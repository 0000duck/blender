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

#ifndef PTC_EXPORT_H
#define PTC_EXPORT_H

#include "thread.h"

struct Main;
struct Scene;

namespace PTC {

class Writer;

class Exporter
{
public:
	Exporter(Main *bmain, Scene *scene);
	
	void bake(Writer *writer, int start_frame, int end_frame);

	bool cancel() const;
	void cancel(bool value);

private:
	thread_mutex m_mutex;
	
	Main *m_bmain;
	Scene *m_scene;
	
	bool m_cancel;
};

} /* namespace PTC */

#endif  /* PTC_EXPORT_H */
