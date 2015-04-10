//-*****************************************************************************
//
// Copyright (c) 2009-2013,
//  Sony Pictures Imageworks, Inc. and
//  Industrial Light & Magic, a division of Lucasfilm Entertainment Company Ltd.
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// *       Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// *       Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
// *       Neither the name of Sony Pictures Imageworks, nor
// Industrial Light & Magic nor the names of their contributors may be used
// to endorse or promote products derived from this software without specific
// prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//-*****************************************************************************

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

#include <Alembic/AbcGeom/All.h>
#include <Alembic/AbcCoreAbstract/All.h>
#include <Alembic/AbcCoreFactory/All.h>
#include <Alembic/Util/All.h>
#include <Alembic/Abc/TypedPropertyTraits.h>

#include <sstream>

#include "alembic.h"

extern "C" {
#include "BLI_utildefines.h"
}

using namespace ::Alembic::AbcGeom;

namespace PTC {

struct stringstream {
	stringstream(void (*cb)(void *, const char *), void *userdata) :
	    cb(cb)
	{
	}
	
	void (*cb)(void *, const char *);
	void *userdata;
	
	template <typename T>
	friend stringstream& operator << (stringstream &stream, T s);
};

template <typename T>
stringstream& operator << (stringstream &stream, T s)
{
	std::stringstream ss;
	ss << s;
	stream.cb(stream.userdata, ss.str().c_str());
	return stream;
}

static const std::string g_sep(";");
static const std::string g_endl("\n");

static void visitProperties(stringstream &ss, ICompoundProperty, std::string &);

template <class PROP>
static void visitSimpleArrayProperty(stringstream &ss, PROP iProp, const std::string &iIndent)
{
	std::string ptype = "ArrayProperty ";
	size_t asize = 0;
	
	AbcA::ArraySamplePtr samp;
	index_t maxSamples = iProp.getNumSamples();
	for (index_t i = 0 ; i < maxSamples; ++i) {
		iProp.get(samp, ISampleSelector(i));
		asize = samp->size();
	}
	
	std::string mdstring = "interpretation=";
	mdstring += iProp.getMetaData().get("interpretation");
	
	std::stringstream dtype;
	dtype << "datatype=";
	dtype << iProp.getDataType();
	
	std::stringstream asizestr;
	asizestr << ";arraysize=";
	asizestr << asize;
	
	mdstring += g_sep;
	
	mdstring += dtype.str();
	
	mdstring += asizestr.str();
	
	ss << iIndent << "  " << ptype << "name=" << iProp.getName()
	   << g_sep << mdstring << g_sep << "numsamps="
	   << iProp.getNumSamples() << g_endl;
}

template <class PROP>
static void visitSimpleScalarProperty(stringstream &ss, PROP iProp, const std::string &iIndent)
{
	std::string ptype = "ScalarProperty ";
	size_t asize = 0;
	
	const AbcA::DataType &dt = iProp.getDataType();
	const Alembic::Util ::uint8_t extent = dt.getExtent();
	Alembic::Util::Dimensions dims(extent);
	AbcA::ArraySamplePtr samp = AbcA::AllocateArraySample( dt, dims );
	index_t maxSamples = iProp.getNumSamples();
	for (index_t i = 0 ; i < maxSamples; ++i) {
		iProp.get(const_cast<void *>(samp->getData()), ISampleSelector(i));
		asize = samp->size();
	}
	
	std::string mdstring = "interpretation=";
	mdstring += iProp.getMetaData().get("interpretation");
	
	std::stringstream dtype;
	dtype << "datatype=";
	dtype << dt;
	
	std::stringstream asizestr;
	asizestr << ";arraysize=";
	asizestr << asize;
	
	mdstring += g_sep;
	
	mdstring += dtype.str();
	
	mdstring += asizestr.str();
	
	ss << iIndent << "  " << ptype << "name=" << iProp.getName()
	   << g_sep << mdstring << g_sep << "numsamps="
	   << iProp.getNumSamples() << g_endl;
}

static void visitCompoundProperty(stringstream &ss, ICompoundProperty iProp, std::string &ioIndent)
{
	std::string oldIndent = ioIndent;
	ioIndent += "  ";
	
	std::string interp = "schema=";
	interp += iProp.getMetaData().get("schema");
	
	ss << ioIndent << "CompoundProperty " << "name=" << iProp.getName()
	   << g_sep << interp << g_endl;
	
	visitProperties(ss, iProp, ioIndent);
	
	ioIndent = oldIndent;
}

static void visitProperties(stringstream &ss, ICompoundProperty iParent, std::string &ioIndent )
{
	std::string oldIndent = ioIndent;
	for (size_t i = 0 ; i < iParent.getNumProperties() ; i++) {
		PropertyHeader header = iParent.getPropertyHeader(i);
		
		if (header.isCompound()) {
			visitCompoundProperty(ss, ICompoundProperty(iParent, header.getName()), ioIndent);
		}
		else if (header.isScalar()) {
			visitSimpleScalarProperty(ss, IScalarProperty(iParent, header.getName()), ioIndent);
		}
		else {
			BLI_assert(header.isArray());
			visitSimpleArrayProperty(ss, IArrayProperty(iParent, header.getName()), ioIndent);
		}
	}
	
	ioIndent = oldIndent;
}

static void visitObject(stringstream &ss, IObject iObj, std::string iIndent)
{
	// Object has a name, a full name, some meta data,
	// and then it has a compound property full of properties.
	std::string path = iObj.getFullName();
	
	if (iObj.isInstanceRoot()) {
		if (path != "/") {
			ss << "Object " << "name=" << path
			   << " [Instance " << iObj.instanceSourcePath() << "]"
			   << g_endl;
		}
	}
	else if (iObj.isInstanceDescendant()) {
		/* skip non-root instances to avoid repetition */
		return;
	}
	else {
		if (path != "/") {
			ss << "Object " << "name=" << path << g_endl;
		}
		
		// Get the properties.
		ICompoundProperty props = iObj.getProperties();
		visitProperties(ss, props, iIndent);
		
		// now the child objects
		for (size_t i = 0 ; i < iObj.getNumChildren() ; i++) {
			visitObject(ss, IObject(iObj, iObj.getChildHeader(i).getName()), iIndent);
		}
	}
}

void abc_archive_info(IArchive &archive, void (*stream)(void *, const char *), void *userdata)
{
	stringstream ss(stream, userdata);
	
	ss << "Alembic Archive Info for "
	   << Alembic::AbcCoreAbstract::GetLibraryVersion()
	   << g_endl;
	
	std::string appName;
	std::string libraryVersionString;
	Alembic::Util::uint32_t libraryVersion;
	std::string whenWritten;
	std::string userDescription;
	GetArchiveInfo(archive,
	               appName,
	               libraryVersionString,
	               libraryVersion,
	               whenWritten,
	               userDescription);
	
	if (appName != "") {
		ss << "  file written by: " << appName << g_endl;
		ss << "  using Alembic : " << libraryVersionString << g_endl;
		ss << "  written on : " << whenWritten << g_endl;
		ss << "  user description : " << userDescription << g_endl;
		ss << g_endl;
	}
	else {
//		ss << argv[1] << g_endl;
		ss << "  (file doesn't have any ArchiveInfo)"
		   << g_endl;
		ss << g_endl;
	}
	
	visitObject(ss, archive.getTop(), "");
}

} /* namespace PTC */
