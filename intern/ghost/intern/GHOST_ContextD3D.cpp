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

#include <iostream>
#include <string>

#include <GL/glew.h>
#include <GL/wglew.h>

#include "GHOST_ContextWGL.h" /* For shared drawing */
#include "GHOST_ContextD3D.h"

//#define USE_DRAW_D3D_TEST_TRIANGLE

HMODULE GHOST_ContextD3D::s_d3d_lib = NULL;
PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN GHOST_ContextD3D::s_D3D11CreateDeviceAndSwapChainFn = NULL;

#ifdef USE_DRAW_D3D_TEST_TRIANGLE
static void drawTestTriangle(ID3D11Device *m_device,
                             ID3D11DeviceContext *m_device_ctx,
                             GHOST_TInt32 width,
                             GHOST_TInt32 height);
#endif

GHOST_ContextD3D::GHOST_ContextD3D(bool stereoVisual, HWND hWnd)
    : GHOST_Context(GHOST_kDrawingContextTypeD3D, stereoVisual), m_hWnd(hWnd)
{
}

GHOST_ContextD3D::~GHOST_ContextD3D()
{
  m_swapchain->Release();
  m_backbuffer_view->Release();
  m_device->Release();
  m_device_ctx->ClearState();
  m_device_ctx->Release();
}

GHOST_TSuccess GHOST_ContextD3D::swapBuffers()
{
  HRESULT res = m_swapchain->Present(0, 0);
  return (res == S_OK) ? GHOST_kSuccess : GHOST_kFailure;
}

GHOST_TSuccess GHOST_ContextD3D::activateDrawingContext()
{
  return GHOST_kFailure;
}

GHOST_TSuccess GHOST_ContextD3D::releaseDrawingContext()
{
  return GHOST_kFailure;
}

GHOST_TSuccess GHOST_ContextD3D::setDefaultFramebufferSize(GHOST_TUns32 width, GHOST_TUns32 height)
{
  RECT rect = {0, 0, (long)width, (long)height};
  RECT winrect;

  /* To use swapchain buffers/textures with custom size, the hidden window has to be resized. */

  GetWindowRect(m_hWnd, &winrect);

  WIN32_CHK(AdjustWindowRectEx(&rect, WS_OVERLAPPEDWINDOW, false, 0));

  width = rect.right - rect.left;
  height = rect.bottom - rect.top;

  if (((winrect.right - winrect.left) != width) || ((winrect.bottom - winrect.top) != height)) {
    return SetWindowPos(m_hWnd,
                        HWND_TOP,
                        0,
                        0,
                        width,
                        height,
                        SWP_NOACTIVATE | SWP_NOREDRAW | SWP_NOMOVE | SWP_NOZORDER) ?
               GHOST_kSuccess :
               GHOST_kFailure;
  }

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextD3D::updateSwapchain(GHOST_TUns32 width, GHOST_TUns32 height)
{
  HRESULT hres;
  DXGI_SWAP_CHAIN_DESC swapchain_desc;

  m_swapchain->GetDesc(&swapchain_desc);

  if ((swapchain_desc.BufferDesc.Width == width) && (swapchain_desc.BufferDesc.Height == height)) {
    // Nothing to do.
    return GHOST_kSuccess;
  }

#define CHECK_HRES \
  if (hres != S_OK) { \
    printf("Error updating swapchain (error code %x): %s line %i\n", hres, __FILE__, __LINE__); \
  } \
  (void)0

  m_device_ctx->OMSetRenderTargets(0, nullptr, nullptr);

  m_backbuffer_view->Release();
  m_device_ctx->ClearState();

  hres = m_swapchain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
  CHECK_HRES;

  ID3D11Texture2D *buf;
  hres = m_swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&buf);
  CHECK_HRES;

  hres = m_device->CreateRenderTargetView(buf, NULL, &m_backbuffer_view);
  CHECK_HRES;
  buf->Release();

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextD3D::setupD3DLib()
{
  if (s_d3d_lib == NULL) {
    s_d3d_lib = LoadLibraryA("d3d11.dll");

    WIN32_CHK(s_d3d_lib != NULL);

    if (s_d3d_lib == NULL) {
      fprintf(stderr, "LoadLibrary(\"d3d11.dll\") failed!\n");
      return GHOST_kFailure;
    }
  }

  if (s_D3D11CreateDeviceAndSwapChainFn == NULL) {
    s_D3D11CreateDeviceAndSwapChainFn = (PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN)GetProcAddress(
        s_d3d_lib, "D3D11CreateDeviceAndSwapChain");

    WIN32_CHK(s_D3D11CreateDeviceAndSwapChainFn != NULL);

    if (s_D3D11CreateDeviceAndSwapChainFn == NULL) {
      fprintf(stderr, "GetProcAddress(s_d3d_lib, \"D3D11CreateDeviceAndSwapChain\") failed!\n");
      return GHOST_kFailure;
    }
  }

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextD3D::initializeDrawingContext()
{
  if (setupD3DLib() == GHOST_kFailure) {
    return GHOST_kFailure;
  }

  DXGI_SWAP_CHAIN_DESC sd{};

  sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  sd.SampleDesc.Count = 1;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.BufferCount = 3;
  sd.OutputWindow = m_hWnd;
  sd.Windowed = TRUE;
  sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

  HRESULT hres = s_D3D11CreateDeviceAndSwapChainFn(NULL,
                                                   D3D_DRIVER_TYPE_HARDWARE,
                                                   NULL,
                                                   // D3D11_CREATE_DEVICE_DEBUG,
                                                   0,
                                                   NULL,
                                                   0,
                                                   D3D11_SDK_VERSION,
                                                   &sd,
                                                   &m_swapchain,
                                                   &m_device,
                                                   NULL,
                                                   &m_device_ctx);
  WIN32_CHK(hres == S_OK);

  ID3D11Texture2D *back_buffer;
  m_swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&back_buffer);

  m_device->CreateRenderTargetView(back_buffer, nullptr, &m_backbuffer_view);
  back_buffer->Release();

  m_swapchain->Present(0, 0);

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextD3D::releaseNativeHandles()
{
  return GHOST_kFailure;
}

GHOST_TSuccess GHOST_ContextD3D::blitOpenGLOffscreenContext(GHOST_Context * /*offscreen_ctx*/,
                                                            GHOST_TInt32 width,
                                                            GHOST_TInt32 height)
{
  if (updateSwapchain(width, height) == GHOST_kSuccess) {
    GHOST_SharedOpenGLResource *shared_res = createSharedOpenGLResource(width, height);

    if (shared_res) {
      GHOST_TSuccess ret = blitFromOpenGLContext(shared_res, width, height);
      disposeSharedOpenGLResource(shared_res);
      return ret;
    }
  }

  return GHOST_kFailure;
}

class GHOST_SharedOpenGLResource {
  struct SharedData {
    HANDLE device;
    GLuint fbo;
    HANDLE render_buf{nullptr};
  } m_shared;

 public:
  GHOST_SharedOpenGLResource(ID3D11Device *device,
                             ID3D11DeviceContext *device_ctx,
                             unsigned int width,
                             unsigned int height,
                             ID3D11RenderTargetView *render_target = nullptr)
      : m_device(device), m_device_ctx(device_ctx), m_cur_width(width), m_cur_height(height)
  {
    ID3D11Resource *backbuffer_res;

    if (!render_target) {
      D3D11_TEXTURE2D_DESC texDesc{};
      D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc{};
      ID3D11Texture2D *tex;

      texDesc.Width = width;
      texDesc.Height = height;
      texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
      texDesc.SampleDesc.Count = 1;
      texDesc.ArraySize = 1;
      texDesc.MipLevels = 1;
      texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

      device->CreateTexture2D(&texDesc, NULL, &tex);

      renderTargetViewDesc.Format = texDesc.Format;
      renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
      renderTargetViewDesc.Texture2D.MipSlice = 0;

      device->CreateRenderTargetView(tex, &renderTargetViewDesc, &render_target);

      tex->Release();
    }

    m_render_target = render_target;
    m_render_target->GetResource(&backbuffer_res);
    backbuffer_res->QueryInterface<ID3D11Texture2D>(&m_render_target_tex);
    backbuffer_res->Release();
  }

  ~GHOST_SharedOpenGLResource()
  {
    m_render_target_tex->Release();
    m_render_target->Release();

    if (m_is_initialized) {
      if (m_shared.render_buf) {
        wglDXUnregisterObjectNV(m_shared.device, m_shared.render_buf);
      }
      if (m_shared.device) {
        wglDXCloseDeviceNV(m_shared.device);
      }
      glDeleteFramebuffers(1, &m_shared.fbo);
      glDeleteRenderbuffers(1, &m_gl_render_buf);
    }
  }

  void reregisterSharedObject()
  {
    if (m_shared.render_buf) {
      wglDXUnregisterObjectNV(m_shared.device, m_shared.render_buf);
    }

    m_shared.render_buf = wglDXRegisterObjectNV(m_shared.device,
                                                m_render_target_tex,
                                                m_gl_render_buf,
                                                GL_RENDERBUFFER,
                                                WGL_ACCESS_READ_WRITE_NV);
    if (!m_shared.render_buf) {
      fprintf(stderr, "Error registering shared object using wglDXRegisterObjectNV()\n");
      return;
    }
  }

  GHOST_TSuccess initialize()
  {
    m_shared.device = wglDXOpenDeviceNV(m_device);
    if (m_shared.device == NULL) {
      fprintf(stderr, "Error opening shared device using wglDXOpenDeviceNV()\n");
      return GHOST_kFailure;
    }

    /* Build the renderbuffer. */
    glGenRenderbuffers(1, &m_gl_render_buf);
    glBindRenderbuffer(GL_RENDERBUFFER, m_gl_render_buf);

    reregisterSharedObject();

    /* Build the framebuffer */
    glGenFramebuffers(1, &m_shared.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_shared.fbo);
    glFramebufferRenderbuffer(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_gl_render_buf);
    m_is_initialized = true;

    return GHOST_kSuccess;
  }

  void ensureUpdated(unsigned int width, unsigned int height)
  {
    if (m_is_initialized == false) {
      initialize();
    }

    if ((m_cur_width != width) || (m_cur_height != height)) {
      m_cur_width = width;
      m_cur_height = height;
      reregisterSharedObject();
    }
  }

  GHOST_TSuccess blit()
  {
    GLint fbo;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fbo);

    const float clear_col[] = {0.8f, 0.5f, 1.0f, 1.0f};
    m_device_ctx->ClearRenderTargetView(m_render_target, clear_col);
    m_device_ctx->OMSetRenderTargets(1, &m_render_target, nullptr);

    beginGLOnly();

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_shared.fbo);
    GLenum err = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (err != GL_FRAMEBUFFER_COMPLETE) {
      fprintf(stderr, "Error: Framebuffer incomplete %u\n", err);
      return GHOST_kFailure;
    }

    /* No glBlitNamedFramebuffer, gotta be 3.3 compatible. */
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_shared.fbo);
    glBlitFramebuffer(0,
                      0,
                      m_cur_width,
                      m_cur_height,
                      0,
                      0,
                      m_cur_width,
                      m_cur_height,
                      GL_COLOR_BUFFER_BIT,
                      GL_LINEAR);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    endGLOnly();

#ifdef USE_DRAW_D3D_TEST_TRIANGLE
    drawTestTriangle(m_device, m_device_ctx, m_cur_width, m_cur_height);
#endif

    return GHOST_kSuccess;
  }

  ID3D11RenderTargetView *m_render_target;
  ID3D11Texture2D *m_render_target_tex;

 private:
  void beginGLOnly()
  {
    wglDXLockObjectsNV(m_shared.device, 1, &m_shared.render_buf);
  }
  void endGLOnly()
  {
    wglDXUnlockObjectsNV(m_shared.device, 1, &m_shared.render_buf);
  }

  ID3D11Device *m_device;
  ID3D11DeviceContext *m_device_ctx;
  GLuint m_gl_render_buf;
  unsigned int m_cur_width, m_cur_height;
  bool m_is_initialized{false};
};

GHOST_SharedOpenGLResource *GHOST_ContextD3D::createSharedOpenGLResource(
    unsigned int width, unsigned int height, ID3D11RenderTargetView *render_target)
{
  if (!(WGL_NV_DX_interop && WGL_NV_DX_interop2)) {
    fprintf(stderr,
            "Error: Can't render OpenGL framebuffer using Direct3D. NV_DX_interop extension not "
            "available.");
    return nullptr;
  }
  GHOST_SharedOpenGLResource *shared_res = new GHOST_SharedOpenGLResource(
      m_device, m_device_ctx, width, height, render_target);

  return shared_res;
}
GHOST_SharedOpenGLResource *GHOST_ContextD3D::createSharedOpenGLResource(unsigned int width,
                                                                         unsigned int height)
{
  return createSharedOpenGLResource(width, height, nullptr);
}

void GHOST_ContextD3D::disposeSharedOpenGLResource(GHOST_SharedOpenGLResource *shared_res)
{
  delete shared_res;
}

GHOST_TSuccess GHOST_ContextD3D::blitFromOpenGLContext(GHOST_SharedOpenGLResource *shared_res,
                                                       unsigned int width,
                                                       unsigned int height)
{
  shared_res->ensureUpdated(width, height);
  return shared_res->blit();
}

ID3D11Texture2D *GHOST_ContextD3D::getSharedTexture2D(GHOST_SharedOpenGLResource *shared_res)
{
  return shared_res->m_render_target_tex;
}

#ifdef USE_DRAW_D3D_TEST_TRIANGLE
#  pragma comment(lib, "D3DCompiler.lib")

const static std::string vertex_shader_str{
    "struct VSOut {"
    "float3 color : Color;"
    "float4 pos : SV_POSITION;"
    "};"

    "VSOut main(float2 pos : Position, float3 color : Color)"
    "{"
    "  VSOut vso;"
    "  vso.pos = float4(pos.x, pos.y, 0.0f, 1.0f);"
    "  vso.color = color;"
    "  return vso;"
    "}"};
const static std::string pixel_shader_str{
    " float4 main(float3 color : Color) : SV_TARGET"
    "{"
    "	return float4(color, 1.0f);"
    "}"};

#  include <wrl/module.h>
#  include <d3dcompiler.h>

static void drawTestTriangle(ID3D11Device *m_device,
                             ID3D11DeviceContext *m_device_ctx,
                             GHOST_TInt32 width,
                             GHOST_TInt32 height)
{
  struct Vertex {
    float x, y;
    unsigned char r, g, b, a;
  };
  Vertex vertices[] = {
      {0.0f, 0.5f, 255, 0, 0},
      {0.5f, -0.5f, 0, 255, 0},
      {-0.5f, -0.5f, 0, 0, 255},

  };
  const unsigned int stride = sizeof(Vertex);
  const unsigned int offset = 0;
  ID3D11Buffer *vertex_buffer;

  D3D11_BUFFER_DESC buffer_desc{};
  buffer_desc.Usage = D3D11_USAGE_DEFAULT;
  buffer_desc.ByteWidth = sizeof(vertices);
  buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

  D3D11_SUBRESOURCE_DATA init_data{};
  init_data.pSysMem = vertices;

  m_device->CreateBuffer(&buffer_desc, &init_data, &vertex_buffer);
  m_device_ctx->IASetVertexBuffers(0, 1, &vertex_buffer, &stride, &offset);

  Microsoft::WRL::ComPtr<ID3DBlob> blob;
  D3DCompile(pixel_shader_str.c_str(),
             pixel_shader_str.length(),
             NULL,
             NULL,
             NULL,
             "main",
             "ps_5_0",
             0,
             0,
             &blob,
             NULL);
  Microsoft::WRL::ComPtr<ID3D11PixelShader> pixel_shader;
  m_device->CreatePixelShader(
      blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &pixel_shader);

  D3DCompile(vertex_shader_str.c_str(),
             vertex_shader_str.length(),
             NULL,
             NULL,
             NULL,
             "main",
             "vs_5_0",
             0,
             0,
             &blob,
             NULL);
  Microsoft::WRL::ComPtr<ID3D11VertexShader> vertex_shader;
  m_device->CreateVertexShader(
      blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &vertex_shader);

  m_device_ctx->PSSetShader(pixel_shader.Get(), NULL, 0);
  m_device_ctx->VSSetShader(vertex_shader.Get(), 0, 0);

  Microsoft::WRL::ComPtr<ID3D11InputLayout> input_layout;
  const D3D11_INPUT_ELEMENT_DESC input_desc[] = {
      {"Position", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"Color", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0},
  };
  m_device->CreateInputLayout(input_desc,
                              (unsigned int)std::size(input_desc),
                              blob->GetBufferPointer(),
                              blob->GetBufferSize(),
                              &input_layout);
  m_device_ctx->IASetInputLayout(input_layout.Get());

  m_device_ctx->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  D3D11_VIEWPORT viewport = {};
  viewport.Width = width;
  viewport.Height = height;
  viewport.MaxDepth = 1;
  m_device_ctx->RSSetViewports(1, &viewport);

  m_device_ctx->Draw(std::size(vertices), 0);
}
#endif
