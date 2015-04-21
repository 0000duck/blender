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

#ifndef PTC_ABC_READER_H
#define PTC_ABC_READER_H

#include <string>

#include <Alembic/Abc/IArchive.h>
#include <Alembic/Abc/IObject.h>
#include <Alembic/Abc/ISampleSelector.h>

#include "reader.h"

#include "abc_frame_mapper.h"

#include "util_error_handler.h"
#include "util_types.h"

struct Scene;

namespace PTC {

using namespace Alembic;

class AbcReaderArchive : public ReaderArchive, public FrameMapper {
public:
	virtual ~AbcReaderArchive();
	
	static AbcReaderArchive *open(Scene *scene, const std::string &filename, ErrorHandler *error_handler);
	
	bool use_render() const { return m_use_render; }
	void use_render(bool enable) { m_use_render = enable; }
	
	Abc::IObject root();
	
	Abc::IObject get_id_object(ID *id);
	bool has_id_object(ID *id);
	
	bool get_frame_range(int &start_frame, int &end_frame);
	Abc::ISampleSelector get_frame_sample_selector(float frame);
	
	void get_info_stream(void (*stream)(void *, const char *), void *userdata);
	void get_info_nodes(CacheArchiveInfo *info);
	
protected:
	AbcReaderArchive(Scene *scene, ErrorHandler *error_handler, Abc::IArchive abc_archive);
	
protected:
	ErrorHandler *m_error_handler;
	bool m_use_render;
	
	Abc::IArchive m_abc_archive;
	Abc::IObject m_abc_root;
	Abc::IObject m_abc_root_render;
};

class AbcReader : public Reader {
public:
	AbcReader() :
	    m_abc_archive(0)
	{}
	
	void init(ReaderArchive *archive)
	{
		BLI_assert(dynamic_cast<AbcReaderArchive*>(archive));
		m_abc_archive = static_cast<AbcReaderArchive*>(archive);
	}
	
	virtual void init_abc(Abc::IObject object) {}
	
	AbcReaderArchive *abc_archive() const { return m_abc_archive; }
	
	bool get_frame_range(int &start_frame, int &end_frame);
	PTCReadSampleResult test_sample(float frame);
	
private:
	AbcReaderArchive *m_abc_archive;
};

} /* namespace PTC */

#endif  /* PTC_READER_H */
