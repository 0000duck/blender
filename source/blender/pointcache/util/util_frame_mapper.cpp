/*
 * Copyright 2013, Blender Foundation.
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
 */

#include "util_frame_mapper.h"

extern "C" {
#include "DNA_scene_types.h"
}

namespace PTC {

using namespace Abc;
using namespace AbcCoreAbstract;

FrameMapper::FrameMapper(double fps)
{
	m_frames_per_sec = fps;
	m_sec_per_frame = (fps == 0.0 ? 0.0 : 1.0/fps);
}

FrameMapper::FrameMapper(Scene *scene)
{
	m_frames_per_sec = (scene->r.frs_sec_base == 0.0f ? 0.0 : (double)scene->r.frs_sec / (double)scene->r.frs_sec_base);
	m_sec_per_frame = (scene->r.frs_sec == 0.0f ? 0.0 : (double)scene->r.frs_sec_base / (double)scene->r.frs_sec);
}

chrono_t FrameMapper::frame_to_time(float frame) const
{
	return (double)(frame - 1.0f) * m_sec_per_frame;
}

float FrameMapper::time_to_frame(chrono_t time) const
{
	return (float)(time * m_frames_per_sec) + 1.0f;
}

} /* namespace PTC */
