/*
 * Copyright 2015, Blender Foundation.
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

#ifndef PTC_ALEMBIC_H
#define PTC_ALEMBIC_H

#include <string>

#include <Alembic/Abc/IArchive.h>

struct CacheArchiveInfo;

namespace PTC {

void abc_archive_info_stream(Alembic::Abc::IArchive &archive, void (*stream)(void *, const char *), void *userdata);
void abc_archive_info_nodes(Alembic::Abc::IArchive &archive, CacheArchiveInfo *info, bool calc_nodes, bool calc_bytes_size);

void abc_archive_slice(Alembic::Abc::IArchive in, Alembic::Abc::OArchive out, Alembic::Abc::TimeSamplingPtr time_sampling, Alembic::Abc::chrono_t start, Alembic::Abc::chrono_t end);

} /* namespace PTC */

#endif  /* PTC_CLOTH_H */
