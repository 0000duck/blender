/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup GHOST
 */

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <sstream>

#include "GHOST_C-api.h"

#include "GHOST_IXrGraphicsBinding.h"
#include "GHOST_Xr_intern.h"
#include "GHOST_XrContext.h"
#include "GHOST_XrException.h"

#include "GHOST_XrSession.h"

struct OpenXRSessionData {
  XrSystemId system_id{XR_NULL_SYSTEM_ID};
  XrSession session{XR_NULL_HANDLE};
  XrSessionState session_state{XR_SESSION_STATE_UNKNOWN};

  // Only stereo rendering supported now.
  const XrViewConfigurationType view_type{XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO};
  XrSpace reference_space;
  std::vector<XrView> views;
  std::vector<XrSwapchain> swapchains;
  std::map<XrSwapchain, std::vector<XrSwapchainImageBaseHeader *>> swapchain_images;
  int32_t swapchain_image_width, swapchain_image_height;
};

struct GHOST_XrDrawFrame {
  XrFrameState frame_state;
};

/* -------------------------------------------------------------------- */
/** \name Create, Initialize and Destruct
 *
 * \{ */

GHOST_XrSession::GHOST_XrSession(GHOST_XrContext *xr_context)
    : m_context(xr_context), m_oxr(new OpenXRSessionData())
{
}

GHOST_XrSession::~GHOST_XrSession()
{
  // TODO OpenXR calls here can fail, but we should not throw an exception in the destructor.

  unbindGraphicsContext();

  for (XrSwapchain &swapchain : m_oxr->swapchains) {
    xrDestroySwapchain(swapchain);
  }
  m_oxr->swapchains.clear();
  if (m_oxr->reference_space != XR_NULL_HANDLE) {
    xrDestroySpace(m_oxr->reference_space);
  }
  if (m_oxr->session != XR_NULL_HANDLE) {
    xrDestroySession(m_oxr->session);
  }

  m_oxr->session = XR_NULL_HANDLE;
  m_oxr->session_state = XR_SESSION_STATE_UNKNOWN;
}

/**
 * A system in OpenXR the combination of some sort of HMD plus controllers and whatever other
 * devices are managed through OpenXR. So this attempts to init the HMD and the other devices.
 */
void GHOST_XrSession::initSystem()
{
  assert(m_context->getInstance() != XR_NULL_HANDLE);
  assert(m_oxr->system_id == XR_NULL_SYSTEM_ID);

  XrSystemGetInfo system_info{};
  system_info.type = XR_TYPE_SYSTEM_GET_INFO;
  system_info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

  CHECK_XR(xrGetSystem(m_context->getInstance(), &system_info, &m_oxr->system_id),
           "Failed to get device information. Is a device plugged in?");
}

/** \} */ /* Create, Initialize and Destruct */

/* -------------------------------------------------------------------- */
/** \name State Management
 *
 * \{ */

static void create_reference_space(OpenXRSessionData *oxr, const GHOST_XrPose *base_pose)
{
  XrReferenceSpaceCreateInfo create_info{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
  create_info.poseInReferenceSpace.orientation.w = 1.0f;

  create_info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
#if 0
  create_info.poseInReferenceSpace.position.x = base_pose->position[0];
  create_info.poseInReferenceSpace.position.y = base_pose->position[2];
  create_info.poseInReferenceSpace.position.z = -base_pose->position[1];
  create_info.poseInReferenceSpace.orientation.x = base_pose->orientation_quat[1];
  create_info.poseInReferenceSpace.orientation.y = base_pose->orientation_quat[3];
  create_info.poseInReferenceSpace.orientation.z = -base_pose->orientation_quat[2];
  create_info.poseInReferenceSpace.orientation.w = base_pose->orientation_quat[0];
#else
  (void)base_pose;
#endif

  CHECK_XR(xrCreateReferenceSpace(oxr->session, &create_info, &oxr->reference_space),
           "Failed to create reference space.");
}

void GHOST_XrSession::start(const GHOST_XrSessionBeginInfo *begin_info)
{
  assert(m_context->getInstance() != XR_NULL_HANDLE);
  assert(m_oxr->session == XR_NULL_HANDLE);
  if (m_context->getCustomFuncs()->gpu_ctx_bind_fn == nullptr) {
    THROW_XR(
        "Invalid API usage: No way to bind graphics context to the XR session. Call "
        "GHOST_XrGraphicsContextBindFuncs() with valid parameters before starting the "
        "session (through GHOST_XrSessionStart()).");
  }

  initSystem();

  bindGraphicsContext();
  if (m_gpu_ctx == nullptr) {
    THROW_XR(
        "Invalid API usage: No graphics context returned through the callback set with "
        "GHOST_XrGraphicsContextBindFuncs(). This is required for session starting (through "
        "GHOST_XrSessionStart()).");
  }

  std::string requirement_str;
  m_gpu_binding = GHOST_XrGraphicsBindingCreateFromType(m_context->getGraphicsBindingType());
  if (!m_gpu_binding->checkVersionRequirements(
          m_gpu_ctx, m_context->getInstance(), m_oxr->system_id, &requirement_str)) {
    std::ostringstream strstream;
    strstream << "Available graphics context version does not meet the following requirements: %s"
              << requirement_str;
    THROW_XR(strstream.str().c_str());
  }
  m_gpu_binding->initFromGhostContext(m_gpu_ctx);

  XrSessionCreateInfo create_info{};
  create_info.type = XR_TYPE_SESSION_CREATE_INFO;
  create_info.systemId = m_oxr->system_id;
  create_info.next = &m_gpu_binding->oxr_binding;

  CHECK_XR(xrCreateSession(m_context->getInstance(), &create_info, &m_oxr->session),
           "Failed to create VR session. The OpenXR runtime may have additional requirements for "
           "the graphics driver that are not met. Other causes are possible too however.\nTip: "
           "The --debug-xr command line option for Blender might allow the runtime to output "
           "detailed error information to the command line.");

  prepareDrawing();
  create_reference_space(m_oxr.get(), &begin_info->base_pose);
}

void GHOST_XrSession::end()
{
  assert(m_oxr->session != XR_NULL_HANDLE);

  CHECK_XR(xrEndSession(m_oxr->session), "Failed to cleanly end the VR session.");
  unbindGraphicsContext();
}

GHOST_XrSession::eLifeExpectancy GHOST_XrSession::handleStateChangeEvent(
    const XrEventDataSessionStateChanged *lifecycle)
{
  m_oxr->session_state = lifecycle->state;

  /* Runtime may send events for apparently destroyed session. Our handle should be NULL then. */
  assert((m_oxr->session == XR_NULL_HANDLE) || (m_oxr->session == lifecycle->session));

  switch (lifecycle->state) {
    case XR_SESSION_STATE_READY: {
      XrSessionBeginInfo begin_info{};

      begin_info.type = XR_TYPE_SESSION_BEGIN_INFO;
      begin_info.primaryViewConfigurationType = m_oxr->view_type;
      CHECK_XR(xrBeginSession(m_oxr->session, &begin_info),
               "Failed to cleanly begin the VR session.");
      break;
    }
    case XR_SESSION_STATE_STOPPING:
      /* Runtime will change state to STATE_EXITING, don't destruct session yet. */
      end();
      break;
    case XR_SESSION_STATE_EXITING:
      return SESSION_DESTROY;
    default:
      break;
  }

  return SESSION_KEEP_ALIVE;
}
/** \} */ /* State Management */

/* -------------------------------------------------------------------- */
/** \name Drawing
 *
 * \{ */

static std::vector<XrSwapchainImageBaseHeader *> swapchain_images_create(
    XrSwapchain swapchain, GHOST_IXrGraphicsBinding *gpu_binding)
{
  std::vector<XrSwapchainImageBaseHeader *> images;
  uint32_t image_count;

  CHECK_XR(xrEnumerateSwapchainImages(swapchain, 0, &image_count, nullptr),
           "Failed to get count of swapchain images to create for the VR session.");
  images = gpu_binding->createSwapchainImages(image_count);
  CHECK_XR(xrEnumerateSwapchainImages(swapchain, images.size(), &image_count, images[0]),
           "Failed to create swapchain images for the VR session.");

  return images;
}

static unique_oxr_ptr<XrSwapchain> swapchain_create(const XrSession session,
                                                    GHOST_IXrGraphicsBinding *gpu_binding,
                                                    const XrViewConfigurationView *xr_view)
{
  XrSwapchainCreateInfo create_info{XR_TYPE_SWAPCHAIN_CREATE_INFO};
  unique_oxr_ptr<XrSwapchain> swapchain(xrDestroySwapchain);
  uint32_t format_count = 0;
  int64_t chosen_format;

  CHECK_XR(xrEnumerateSwapchainFormats(session, 0, &format_count, nullptr),
           "Failed to get count of swapchain image formats.");
  std::vector<int64_t> swapchain_formats(format_count);
  CHECK_XR(xrEnumerateSwapchainFormats(
               session, swapchain_formats.size(), &format_count, swapchain_formats.data()),
           "Failed to get swapchain image formats.");
  assert(swapchain_formats.size() == format_count);

  if (!gpu_binding->chooseSwapchainFormat(swapchain_formats, &chosen_format)) {
    THROW_XR("Error: No format matching OpenXR runtime supported swapchain formats found.");
  }

  create_info.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT |
                           XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
  create_info.format = chosen_format;
  create_info.sampleCount = xr_view->recommendedSwapchainSampleCount;
  create_info.width = xr_view->recommendedImageRectWidth;
  create_info.height = xr_view->recommendedImageRectHeight;
  create_info.faceCount = 1;
  create_info.arraySize = 1;
  create_info.mipCount = 1;
  CHECK_XR(swapchain.construct(xrCreateSwapchain, session, &create_info),
           "Failed to create OpenXR swapchain.");

  return swapchain;
}

void GHOST_XrSession::prepareDrawing()
{
  std::vector<XrViewConfigurationView> view_configs;
  uint32_t view_count;

  CHECK_XR(
      xrEnumerateViewConfigurationViews(
          m_context->getInstance(), m_oxr->system_id, m_oxr->view_type, 0, &view_count, nullptr),
      "Failed to get count of view configurations.");
  view_configs.resize(view_count, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
  CHECK_XR(xrEnumerateViewConfigurationViews(m_context->getInstance(),
                                             m_oxr->system_id,
                                             m_oxr->view_type,
                                             view_configs.size(),
                                             &view_count,
                                             view_configs.data()),
           "Failed to get count of view configurations.");

  for (const XrViewConfigurationView &view : view_configs) {
    unique_oxr_ptr<XrSwapchain> swapchain = swapchain_create(
        m_oxr->session, m_gpu_binding.get(), &view);
    auto images = swapchain_images_create(swapchain.get(), m_gpu_binding.get());

    m_oxr->swapchain_image_width = view.recommendedImageRectWidth;
    m_oxr->swapchain_image_height = view.recommendedImageRectHeight;
    m_oxr->swapchains.push_back(swapchain.get());
    m_oxr->swapchain_images.insert(std::make_pair(swapchain.get(), std::move(images)));

    swapchain.release();
  }

  m_oxr->views.resize(view_count, {XR_TYPE_VIEW});
}

void GHOST_XrSession::beginFrameDrawing()
{
  XrFrameWaitInfo wait_info{XR_TYPE_FRAME_WAIT_INFO};
  XrFrameBeginInfo begin_info{XR_TYPE_FRAME_BEGIN_INFO};
  XrFrameState frame_state{XR_TYPE_FRAME_STATE};

  // TODO Blocking call. Does this intefer with other drawing?
  CHECK_XR(xrWaitFrame(m_oxr->session, &wait_info, &frame_state),
           "Failed to synchronize frame rates between Blender and the device.");

  CHECK_XR(xrBeginFrame(m_oxr->session, &begin_info),
           "Failed to submit frame rendering start state.");

  m_draw_frame = std::unique_ptr<GHOST_XrDrawFrame>(new GHOST_XrDrawFrame());
  m_draw_frame->frame_state = frame_state;
}

void GHOST_XrSession::endFrameDrawing(std::vector<XrCompositionLayerBaseHeader *> *layers)
{
  XrFrameEndInfo end_info{XR_TYPE_FRAME_END_INFO};

  end_info.displayTime = m_draw_frame->frame_state.predictedDisplayTime;
  end_info.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
  end_info.layerCount = layers->size();
  end_info.layers = layers->data();

  CHECK_XR(xrEndFrame(m_oxr->session, &end_info), "Failed to submit rendered frame.");
  m_draw_frame = nullptr;
}

void GHOST_XrSession::draw(void *draw_customdata)
{
  std::vector<XrCompositionLayerProjectionView>
      projection_layer_views;  // Keep alive until xrEndFrame() call!
  XrCompositionLayerProjection proj_layer;
  std::vector<XrCompositionLayerBaseHeader *> layers;

  beginFrameDrawing();

  if (isVisible()) {
    proj_layer = drawLayer(projection_layer_views, draw_customdata);
    layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader *>(&proj_layer));
  }

  endFrameDrawing(&layers);
}

static void ghost_xr_draw_view_info_from_view(const XrView &view, GHOST_XrDrawViewInfo &r_info)
{
#if 0
  /* Set and convert to Blender coodinate space */
  r_info.pose.position[0] = view.pose.position.x;
  r_info.pose.position[1] = -view.pose.position.z;
  r_info.pose.position[2] = view.pose.position.y;
  r_info.pose.orientation_quat[0] = view.pose.orientation.w;
  r_info.pose.orientation_quat[1] = view.pose.orientation.x;
  r_info.pose.orientation_quat[2] = -view.pose.orientation.z;
  r_info.pose.orientation_quat[3] = view.pose.orientation.y;
#else
  r_info.pose.position[0] = view.pose.position.x;
  r_info.pose.position[1] = view.pose.position.y;
  r_info.pose.position[2] = view.pose.position.z;
  r_info.pose.orientation_quat[0] = view.pose.orientation.w;
  r_info.pose.orientation_quat[1] = view.pose.orientation.x;
  r_info.pose.orientation_quat[2] = view.pose.orientation.y;
  r_info.pose.orientation_quat[3] = view.pose.orientation.z;
#endif

  r_info.fov.angle_left = view.fov.angleLeft;
  r_info.fov.angle_right = view.fov.angleRight;
  r_info.fov.angle_up = view.fov.angleUp;
  r_info.fov.angle_down = view.fov.angleDown;
}

void GHOST_XrSession::drawView(XrSwapchain swapchain,
                               XrCompositionLayerProjectionView &proj_layer_view,
                               XrView &view,
                               void *draw_customdata)
{
  XrSwapchainImageAcquireInfo acquire_info{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
  XrSwapchainImageWaitInfo wait_info{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
  XrSwapchainImageReleaseInfo release_info{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
  XrSwapchainImageBaseHeader *swapchain_image;
  GHOST_XrDrawViewInfo draw_view_info{};
  GHOST_ContextHandle draw_ctx;
  uint32_t swapchain_idx;

  CHECK_XR(xrAcquireSwapchainImage(swapchain, &acquire_info, &swapchain_idx),
           "Failed to acquire swapchain image for the VR session.");
  wait_info.timeout = XR_INFINITE_DURATION;
  CHECK_XR(xrWaitSwapchainImage(swapchain, &wait_info),
           "Failed to acquire swapchain image for the VR session.");

  proj_layer_view.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
  proj_layer_view.pose = view.pose;
  proj_layer_view.fov = view.fov;
  proj_layer_view.subImage.swapchain = swapchain;
  proj_layer_view.subImage.imageRect.offset = {0, 0};
  proj_layer_view.subImage.imageRect.extent = {m_oxr->swapchain_image_width,
                                               m_oxr->swapchain_image_height};

  swapchain_image = m_oxr->swapchain_images[swapchain][swapchain_idx];

  draw_view_info.ofsx = proj_layer_view.subImage.imageRect.offset.x;
  draw_view_info.ofsy = proj_layer_view.subImage.imageRect.offset.y;
  draw_view_info.width = proj_layer_view.subImage.imageRect.extent.width;
  draw_view_info.height = proj_layer_view.subImage.imageRect.extent.height;
  ghost_xr_draw_view_info_from_view(view, draw_view_info);

  m_gpu_binding->drawViewBegin(swapchain_image);
  draw_ctx = m_context->getCustomFuncs()->draw_view_fn(&draw_view_info, draw_customdata);
  m_gpu_binding->drawViewEnd(swapchain_image, &draw_view_info, (GHOST_Context *)draw_ctx);

  CHECK_XR(xrReleaseSwapchainImage(swapchain, &release_info),
           "Failed to release swapchain image used to submit VR session frame.");
}

XrCompositionLayerProjection GHOST_XrSession::drawLayer(
    std::vector<XrCompositionLayerProjectionView> &proj_layer_views, void *draw_customdata)
{
  XrViewLocateInfo viewloc_info{XR_TYPE_VIEW_LOCATE_INFO};
  XrViewState view_state{XR_TYPE_VIEW_STATE};
  XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
  uint32_t view_count;

  viewloc_info.displayTime = m_draw_frame->frame_state.predictedDisplayTime;
  viewloc_info.space = m_oxr->reference_space;

  CHECK_XR(xrLocateViews(m_oxr->session,
                         &viewloc_info,
                         &view_state,
                         m_oxr->views.size(),
                         &view_count,
                         m_oxr->views.data()),
           "Failed to query frame view and projection state.");
  assert(m_oxr->swapchains.size() == view_count);

  proj_layer_views.resize(view_count);

  for (uint32_t view_idx = 0; view_idx < view_count; view_idx++) {
    drawView(m_oxr->swapchains[view_idx],
             proj_layer_views[view_idx],
             m_oxr->views[view_idx],
             draw_customdata);
  }

  layer.space = m_oxr->reference_space;
  layer.viewCount = proj_layer_views.size();
  layer.views = proj_layer_views.data();

  return layer;
}

/** \} */ /* Drawing */

/* -------------------------------------------------------------------- */
/** \name State Queries
 *
 * \{ */

bool GHOST_XrSession::isRunning() const
{
  if (m_oxr->session == XR_NULL_HANDLE) {
    return false;
  }
  switch (m_oxr->session_state) {
    case XR_SESSION_STATE_RUNNING:
    case XR_SESSION_STATE_VISIBLE:
    case XR_SESSION_STATE_FOCUSED:
      return true;
    default:
      return false;
  }
}
bool GHOST_XrSession::isVisible() const
{
  if (m_oxr->session == XR_NULL_HANDLE) {
    return false;
  }
  switch (m_oxr->session_state) {
    case XR_SESSION_STATE_VISIBLE:
    case XR_SESSION_STATE_FOCUSED:
      return true;
    default:
      return false;
  }
}

/** \} */ /* State Queries */

/* -------------------------------------------------------------------- */
/** \name Graphics Context Injection
 *
 * Sessions need access to Ghost graphics context information. Additionally, this API allows
 * creating contexts on the fly (created on start, destructed on end). For this, callbacks to bind
 * (potentially create) and unbind (potentially destruct) a Ghost graphics context have to be set,
 * which will be called on session start and end respectively.
 *
 * \{ */

void GHOST_XrSession::bindGraphicsContext()
{
  const GHOST_XrCustomFuncs *custom_funcs = m_context->getCustomFuncs();
  assert(custom_funcs->gpu_ctx_bind_fn);
  m_gpu_ctx = static_cast<GHOST_Context *>(
      custom_funcs->gpu_ctx_bind_fn(m_context->getGraphicsBindingType()));
}
void GHOST_XrSession::unbindGraphicsContext()
{
  const GHOST_XrCustomFuncs *custom_funcs = m_context->getCustomFuncs();
  if (custom_funcs->gpu_ctx_unbind_fn) {
    custom_funcs->gpu_ctx_unbind_fn(m_context->getGraphicsBindingType(), m_gpu_ctx);
  }
  m_gpu_ctx = nullptr;
}

/** \} */ /* Graphics Context Injection */
