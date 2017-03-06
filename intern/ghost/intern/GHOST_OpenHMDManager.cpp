/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "GHOST_OpenHMDManager.h"
#include "GHOST_EventOpenHMD.h"
#include "GHOST_WindowManager.h"

#include "include/openhmd.h"

#ifdef WITH_OPENHMD_DYNLOAD
#  include "udew.h"
#endif

#define CONTEXT_ASSERT GHOST_ASSERT(m_context, "No OpenHMD context found")

GHOST_OpenHMDManager::GHOST_OpenHMDManager(GHOST_System& sys)
	: m_system(sys),
	  m_context(NULL),
	  m_device(NULL),
	  m_deviceIndex(-1),
	  m_projection_params(NULL)
{
	// context can be pre-created. the device can be opened later at will
	createContext();
}

GHOST_OpenHMDManager::~GHOST_OpenHMDManager()
{
	closeDevice();
	destroyContext();
}

bool GHOST_OpenHMDManager::processEvents()
{
	GHOST_IWindow *window = m_system.getWindowManager()->getActiveWindow();

	if (!window)
		return false;


	GHOST_TUns64 now = m_system.getMilliSeconds();
	static int num_devices_prev = 0;
	const int num_devices = getNumDevices();
	bool anyProcessed = false;

	/* DeviceNumChanged event
	 * Would be nicer if OpenHMD could handle this for us. */
	if (num_devices_prev != num_devices) {
		GHOST_EventOpenHMD *event = new GHOST_EventOpenHMD(now, GHOST_kDeviceNumChanged, window);
		m_system.pushEvent(event);

		num_devices_prev = num_devices;
		anyProcessed = true;
	}
	/* OrientationUpdate event
	 * We might want to add a timeout check here to avoid too many updates. */
	if (m_device) {
		GHOST_EventOpenHMD *event = new GHOST_EventOpenHMD(now, GHOST_kOrientationUpdate, window);
		GHOST_TEventOpenHMDData *data = (GHOST_TEventOpenHMDData*)event->getData();

		ohmd_ctx_update(m_context);
		if (!getRotationQuat(data->orientation))
			return false;

		m_system.pushEvent(event);
		anyProcessed = true;
	}

	return anyProcessed;
}

bool GHOST_OpenHMDManager::available() const
{
	return (m_device != NULL);
}

bool GHOST_OpenHMDManager::createContext()
{
	if (m_context != NULL)
		return true;

#ifdef WITH_OPENHMD_DYNLOAD
	static bool udew_initialized = false;
	static bool udew_success = true;
	if (!udew_initialized) {
		udew_initialized = true;
		int result = udewInit();
		if (result != UDEW_SUCCESS) {
			udew_success = false;
			fprintf(stderr, "Failed to open udev library\n");
		}
	}
	if (!udew_success) {
		return false;
	}
#endif

	return (m_context = ohmd_ctx_create());
}

void GHOST_OpenHMDManager::destroyContext()
{
	ohmd_ctx_destroy(m_context);
	m_context = NULL;
}

bool GHOST_OpenHMDManager::openDevice(const char *requested_vendor_name, const char *requested_device_name)
{
	// Create the context if it hasn't been created yet.
	// Do not check for m_available as that indicates both the context and device
	// are valid, which isn't the case if the context isn't available.
	if (!createContext()) {
		return false;
	}

	bool success = false;
	int num_devices = ohmd_ctx_probe(m_context);
	for (int i = 0; i < num_devices; ++i) {
		const char* device_name = ohmd_list_gets(m_context, i, OHMD_PRODUCT);
		const char* vendor_name = ohmd_list_gets(m_context, i, OHMD_VENDOR);

		if (strcmp(device_name, requested_device_name) == 0 && strcmp(vendor_name, requested_vendor_name) == 0) {
			success = openDevice(i);
			break;
		}
	}

	return success;
}

bool GHOST_OpenHMDManager::openDevice(int index)
{
	// Create the context if it hasn't been created yet
	// Do not check for m_available as that indicates both the context and device
	// are valid, which isn't the case if the context isn't available.
	if (!createContext()) {
		return false;
	}

	// out of bounds
	if (index >= ohmd_ctx_probe(m_context)) {
		return false;
	}

	// Blender only allows one opened device at a time
	if (getOpenHMDDevice()) {
		closeDevice();
	}

	// can't fail to open the device
	m_deviceIndex = index;

	ohmd_device_settings* settings = ohmd_device_settings_create(m_context);

	// If OHMD_IDS_AUTOMATIC_UPDATE is set to 0, ohmd_ctx_update() must be called at least 10 times per second.
	// It is enabled by default, runs in seperate thread.
	// This allows for correct tracking on low framerates, needed for heavy scenes.

	int auto_update = 1;
	ohmd_device_settings_seti(settings, OHMD_IDS_AUTOMATIC_UPDATE, &auto_update);

	m_device = ohmd_list_open_device_s(m_context, index, settings);
	ohmd_device_settings_destroy(settings); //cleanup settings

	m_projection_params = new OpenHMDDistortionParameters;
	// Set the thing for the stuff
	ohmd_device_getf(m_device, OHMD_SCREEN_HORIZONTAL_SIZE, &(m_projection_params->viewport_scale[0]));
	m_projection_params->viewport_scale[0] /= 2.0f;
	ohmd_device_getf(m_device, OHMD_SCREEN_VERTICAL_SIZE, &(m_projection_params->viewport_scale[1]));
	//distortion coefficients
	ohmd_device_getf(m_device, OHMD_UNIVERSAL_DISTORTION_K, &(m_projection_params->distortion_coeffs[0]));
	ohmd_device_getf(m_device, OHMD_UNIVERSAL_ABERRATION_K, &(m_projection_params->aberr_scale[0]));
	//calculate lens centers (assuming the eye separation is the distance betweenteh lense centers)
	ohmd_device_getf(m_device, OHMD_LENS_HORIZONTAL_SEPARATION, &m_projection_params->sep);
	ohmd_device_getf(m_device, OHMD_LENS_VERTICAL_POSITION, &(m_projection_params->left_lens_center[1]));
	ohmd_device_getf(m_device, OHMD_LENS_VERTICAL_POSITION, &(m_projection_params->right_lens_center[1]));

	return true;
}

void GHOST_OpenHMDManager::closeDevice()
{
	if (!m_device) {
		return;
	}

	ohmd_close_device(m_device);

	delete m_projection_params;
	m_device = NULL;
	m_deviceIndex = -1;
}

int GHOST_OpenHMDManager::getNumDevices()
{
	CONTEXT_ASSERT;
	return ohmd_ctx_probe(m_context);
}

const char *GHOST_OpenHMDManager::getError() const
{
	if (!m_device) {
		return NULL;
	}

	return ohmd_ctx_get_error(m_context);
}

const char *GHOST_OpenHMDManager::getDeviceName() const
{
	if (!m_device)
		return NULL;

	ohmd_ctx_probe(m_context);
	return ohmd_list_gets(m_context, m_deviceIndex, OHMD_PRODUCT);
}

const char *GHOST_OpenHMDManager::getDeviceName(int index)
{
	CONTEXT_ASSERT;
	// Probe to fetch the device information from the hardware
	ohmd_ctx_probe(m_context);
	return ohmd_list_gets(m_context, index, OHMD_PRODUCT);
}

const char *GHOST_OpenHMDManager::getVendorName() const
{
	if (!m_device)
		return NULL;

	ohmd_ctx_probe(m_context);
	return ohmd_list_gets(m_context, m_deviceIndex, OHMD_VENDOR);
}

const char *GHOST_OpenHMDManager::getVendorName(int index)
{
	CONTEXT_ASSERT;
	// Probe to fetch the device information from the hardware
	ohmd_ctx_probe(m_context);
	return ohmd_list_gets(m_context, index, OHMD_VENDOR);
}

const char *GHOST_OpenHMDManager::getPath() const
{
	if (!m_device)
		return NULL;

	return ohmd_list_gets(m_context, m_deviceIndex, OHMD_PATH);
}

bool GHOST_OpenHMDManager::getRotationQuat(float orientation[4]) const
{
	if (!m_device) {
		return false;
	}

	float tmp[4];
	if (ohmd_device_getf(m_device, OHMD_ROTATION_QUAT, tmp) < 0)
		return false;

	// Convert from x, y, z, w space (OpenHMD) to w, x, y, z (Blender)
	orientation[0] = tmp[3];
	orientation[1] = tmp[0];
	orientation[2] = tmp[1];
	orientation[3] = tmp[2];

	return true;
}

void GHOST_OpenHMDManager::getLeftEyeGLModelviewMatrix(float mat[4][4]) const
{
	if (!m_device) {
		return;
	}

	ohmd_device_getf(m_device, OHMD_LEFT_EYE_GL_MODELVIEW_MATRIX, *mat);
}

void GHOST_OpenHMDManager::getRightEyeGLModelviewMatrix(float mat[4][4]) const
{
	if (!m_device) {
		return;
	}

	ohmd_device_getf(m_device, OHMD_RIGHT_EYE_GL_MODELVIEW_MATRIX, *mat);
}

void GHOST_OpenHMDManager::getLeftEyeGLProjectionMatrix(float mat[4][4]) const
{
	if (!m_device) {
		return;
	}

	ohmd_device_getf(m_device, OHMD_LEFT_EYE_GL_PROJECTION_MATRIX, *mat);
}

void GHOST_OpenHMDManager::getRightEyeGLProjectionMatrix(float mat[4][4]) const
{
	if (!m_device) {
		return;
	}

	ohmd_device_getf(m_device, OHMD_RIGHT_EYE_GL_PROJECTION_MATRIX, *mat);
}

void GHOST_OpenHMDManager::getPositionVector(float position[3]) const
{
	if (!m_device) {
		return;
	}

	ohmd_device_getf(m_device, OHMD_POSITION_VECTOR, position);
}

float GHOST_OpenHMDManager::getScreenHorizontalSize() const
{
	if (!m_device) {
		return -1;
	}

	float val = -1;
	ohmd_device_getf(m_device, OHMD_SCREEN_HORIZONTAL_SIZE, &val);
	return val;
}

float GHOST_OpenHMDManager::getScreenVerticalSize() const
{
	if (!m_device) {
		return -1;
	}

	float val = -1;
	ohmd_device_getf(m_device, OHMD_SCREEN_VERTICAL_SIZE, &val);
	return val;
}

float GHOST_OpenHMDManager::getLensHorizontalSeparation() const
{
	if (!m_device) {
		return -1;
	}

	float val = -1;
	ohmd_device_getf(m_device, OHMD_LENS_HORIZONTAL_SEPARATION, &val);
	return val;

}

float GHOST_OpenHMDManager::getLensVerticalPosition() const
{
	if (!m_device) {
		return -1;
	}

	float val = -1;
	ohmd_device_getf(m_device, OHMD_LENS_VERTICAL_POSITION, &val);
	return val;
}

float GHOST_OpenHMDManager::getLeftEyeFOV() const
{
	if (!m_device) {
		return -1;
	}

	float val = -1;
	ohmd_device_getf(m_device, OHMD_LEFT_EYE_FOV, &val);
	return val;
}

float GHOST_OpenHMDManager::getLeftEyeAspectRatio() const
{
	if (!m_device) {
		return -1;
	}

	float val = -1;
	ohmd_device_getf(m_device, OHMD_LEFT_EYE_ASPECT_RATIO, &val);
	return val;
}

float GHOST_OpenHMDManager::getRightEyeFOV() const
{
	if (!m_device) {
		return -1;
	}

	float val = -1;
	ohmd_device_getf(m_device, OHMD_RIGHT_EYE_FOV, &val);
	return val;
}

float GHOST_OpenHMDManager::getRightEyeAspectRatio() const
{
	if (!m_device) {
		return -1;
	}

	float val = -1;
	ohmd_device_getf(m_device, OHMD_RIGHT_EYE_ASPECT_RATIO, &val);
	return val;
}

/**
 * \returns -1 if not found.
 */
float GHOST_OpenHMDManager::getEyeIPD() const
{
	if (!m_device) {
		return -1;
	}

	float val = -1;
	ohmd_device_getf(m_device, OHMD_EYE_IPD, &val);
	return val;
}

float GHOST_OpenHMDManager::getProjectionZFar() const
{
	if (!m_device) {
		return -1;
	}

	float val = -1;
	ohmd_device_getf(m_device, OHMD_PROJECTION_ZFAR, &val);
	return val;
}

float GHOST_OpenHMDManager::getProjectionZNear() const
{
	if (!m_device) {
		return -1;
	}

	float val = -1;
	ohmd_device_getf(m_device, OHMD_PROJECTION_ZNEAR, &val);
	return val;
}

void GHOST_OpenHMDManager::getDistortion(float distortion[6]) const
{
	if (!m_device) {
		return;
	}

	ohmd_device_getf(m_device, OHMD_DISTORTION_K, distortion);
}

int GHOST_OpenHMDManager::getScreenHorizontalResolution() const
{
	if (!m_device) {
		return -1;
	}

	int val = -1;
	ohmd_device_geti(m_device, OHMD_SCREEN_HORIZONTAL_RESOLUTION, &val);
	return val;
}

int GHOST_OpenHMDManager::getScreenVerticalResolution() const
{
	if (!m_device) {
		return -1;
	}

	int val = -1;
	ohmd_device_geti(m_device, OHMD_SCREEN_VERTICAL_RESOLUTION, &val);
	return val;
}

bool GHOST_OpenHMDManager::setEyeIPD(float val)
{
	if (!m_device) {
		return false;
	}

	return ohmd_device_setf(m_device, OHMD_EYE_IPD, &val);
}

bool GHOST_OpenHMDManager::setProjectionZFar(float val)
{
	if (!m_device) {
		return false;
	}

	return ohmd_device_setf(m_device, OHMD_PROJECTION_ZFAR, &val);
}

bool GHOST_OpenHMDManager::setProjectionZNear(float val)
{
	if (!m_device) {
		return false;
	}

	return ohmd_device_setf(m_device, OHMD_PROJECTION_ZNEAR, &val);
}

ohmd_context *GHOST_OpenHMDManager::getOpenHMDContext()
{
	return m_context;
}

ohmd_device *GHOST_OpenHMDManager::getOpenHMDDevice()
{
	return m_device;
}

const int GHOST_OpenHMDManager::getDeviceIndex()
{
	return m_deviceIndex;
}

void* GHOST_OpenHMDManager::getDistortionParameters()
{
	if (!m_device) {
		return NULL;
	}
	return m_projection_params;
}