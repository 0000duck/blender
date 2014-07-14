/*
 * Copyright 2014, Blender Foundation.
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
 * Contributor: 
 *		Jeroen Bakker 
 *		Monique Dewanchand
 */

class MemoryBufferValue;

#ifndef _COM_MemoryBufferValue_h_
#define _COM_MemoryBufferValue_h_

#include "COM_MemoryBuffer.h"
#include "COM_Sampler.h"

class MemoryBufferValue: public MemoryBuffer
{
private:
	SamplerNearestValue *m_sampler_nearest;
	SamplerNearestNoCheckValue *m_sampler_nocheck;
	SamplerBilinearValue *m_sampler_bilinear;

protected:
	/**
	 * @brief construct new MemoryBuffer for a chunk
	 */
	MemoryBufferValue(MemoryProxy *memoryProxy, unsigned int chunkNumber, rcti *rect);
	
	/**
	 * @brief construct new temporarily MemoryBuffer for an area
	 */
	MemoryBufferValue(MemoryProxy *memoryProxy, rcti *rect);
	MemoryBufferValue(DataType datatype, rcti *rect);


public:
	void writePixel(int x, int y, const float *color);
	void addPixel(int x, int y, const float *color);
	void init_samplers();
	void deinit_samplers();
	void read(float *result, int x, int y,
				MemoryBufferExtend extend_x = COM_MB_CLIP,
				MemoryBufferExtend extend_y = COM_MB_CLIP);

	void readNoCheck(float *result, int x, int y,
					MemoryBufferExtend extend_x = COM_MB_CLIP,
					MemoryBufferExtend extend_y = COM_MB_CLIP);

	void readBilinear(float *result, float x, float y,
					 MemoryBufferExtend extend_x = COM_MB_CLIP,
					 MemoryBufferExtend extend_y = COM_MB_CLIP);

	float getMaximumValue() const;
	MemoryBuffer *duplicate();

	friend class MemoryBuffer;
	
#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("COM:MemoryBufferValue")
#endif
};
#endif
