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

#ifndef PTC_WRITER_H
#define PTC_WRITER_H

#include <string>

#include <Alembic/Abc/OArchive.h>

#include "util/util_error_handler.h"
#include "util/util_frame_mapper.h"

struct ID;
struct PointCache;
struct Scene;

namespace PTC {

using namespace Alembic;

class Writer : public FrameMapper {
public:
	Writer(Scene *scene, ID *id, PointCache *cache);
	virtual ~Writer();
	
	void set_error_handler(ErrorHandler *handler);
	
	uint32_t add_frame_sampling();
	
	virtual void write_sample() = 0;
	
protected:
	Abc::OArchive m_archive;
	ErrorHandler *m_error_handler;
	
	Scene *m_scene;
};

} /* namespace PTC */

#endif  /* PTC_WRITER_H */
