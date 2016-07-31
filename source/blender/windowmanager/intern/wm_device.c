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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/intern/wm_device.c
 *  \ingroup wm
 *
 * Data functions for physical devices (GHOST wrappers).
 */

#ifdef WITH_INPUT_HMD

#include "BKE_context.h"

#include "DNA_userdef_types.h"

#include "GHOST_C-api.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm.h"


/* -------------------------------------------------------------------- */
/* HMDs */

/** \name Head Mounted Displays
 * \{ */

int WM_device_HMD_num_devices_get(void)
{
	return GHOST_HMDgetNumDevices();
}

/**
 * Enable or disable an HMD.
 */
void WM_device_HMD_state_set(const int device, const bool enable)
{
	BLI_assert(device < MAX_HMD_DEVICES);
	if (enable && (device >= 0)) {
		/* GHOST closes previously opened device if needed */
		GHOST_HMDopenDevice(device);
	}
	else {
		GHOST_HMDcloseDevice();
	}
}

/**
 * Get index of currently open device.
 */
int WM_device_HMD_current_get(void)
{
	return GHOST_HMDgetOpenDeviceIndex();
}

const char *WM_device_HMD_name_get(int index)
{
	BLI_assert(index < MAX_HMD_DEVICES);
	return GHOST_HMDgetDeviceName(index);
}

const char *WM_device_HMD_vendor_get(int index)
{
	BLI_assert(index < MAX_HMD_DEVICES);
	return GHOST_HMDgetVendorName(index);
}

/**
 * Get IPD from currently opened HMD.
 */
float WM_device_HMD_IPD_get(void)
{
	return GHOST_HMDgetDeviceIPD();
}

/**
 * Get left eye modelview matrix from currently opened HMD.
 */
void WM_device_HMD_left_modelview_matrix_get(float leftMatrix[16])
{
	GHOST_HMDgetLeftModelviewMatrix(leftMatrix);
}

/**
 * Get right eye modelview matrix from currently opened HMD.
 */
void WM_device_HMD_right_modelview_matrix_get(float rightMatrix[16])
{
	GHOST_HMDgetRightModelviewMatrix(rightMatrix);
}

/**
 * Get left eye projection matrix from currently opened HMD.
 */
void WM_device_HMD_left_projection_matrix_get(float leftMatrix[16])
{
	GHOST_HMDgetLeftProjectionMatrix(leftMatrix);
}

/**
 * Get right eye projection matrix from currently opened HMD.
 */
void WM_device_HMD_right_projection_matrix_get(float rightMatrix[16])
{
	GHOST_HMDgetRightProjectionMatrix(rightMatrix);
}
/** \} */

#endif
