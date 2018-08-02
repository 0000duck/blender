/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "device/device.h"
#include "render/image.h"
#include "render/scene.h"

#include "util/util_foreach.h"
#include "util/util_logging.h"
#include "util/util_path.h"
#include "util/util_progress.h"
#include "util/util_sparse_grid.h"
#include "util/util_texture.h"

#ifdef WITH_OSL
#include <OSL/oslexec.h>
#endif

#ifdef WITH_OPENVDB
#include "render/openvdb.h"
#endif

CCL_NAMESPACE_BEGIN

/* Some helpers to silence warning in templated function. */
static bool isfinite(uchar /*value*/)
{
	return false;
}
static bool isfinite(half /*value*/)
{
	return false;
}

ImageManager::ImageManager(const DeviceInfo& info)
{
	need_update = true;
	osl_texture_system = NULL;
	animation_frame = 0;

	/* Set image limits */
	max_num_images = TEX_NUM_MAX;
	has_half_images = info.has_half_images;

	for(size_t type = 0; type < IMAGE_DATA_NUM_TYPES; type++) {
		tex_num_images[type] = 0;
	}
}

ImageManager::~ImageManager()
{
	for(size_t type = 0; type < IMAGE_DATA_NUM_TYPES; type++) {
		for(size_t slot = 0; slot < images[type].size(); slot++)
			assert(!images[type][slot]);
	}
}

void ImageManager::set_osl_texture_system(void *texture_system)
{
	osl_texture_system = texture_system;
}

bool ImageManager::set_animation_frame_update(int frame)
{
	if(frame != animation_frame) {
		animation_frame = frame;

		for(size_t type = 0; type < IMAGE_DATA_NUM_TYPES; type++) {
			for(size_t slot = 0; slot < images[type].size(); slot++) {
				if(images[type][slot] && images[type][slot]->animated)
					return true;
			}
		}
	}

	return false;
}

device_memory *ImageManager::image_memory(int flat_slot)
{
	   ImageDataType type;
	   int slot = flattened_slot_to_type_index(flat_slot, &type);

	   Image *img = images[type][slot];

	   return img->mem;
}

bool ImageManager::get_image_metadata(const string& filename,
                                      void *builtin_data,
                                      ImageMetaData& metadata)
{
	string grid_name = metadata.grid_name;
	memset(&metadata, 0, sizeof(metadata));
	if(!grid_name.empty()) {
		metadata.grid_name = grid_name;
	}

	if(builtin_data) {
		if(builtin_image_info_cb) {
			builtin_image_info_cb(filename, builtin_data, metadata);
		}
		else {
			return false;
		}

		if(metadata.is_float) {
			metadata.is_linear = true;
			metadata.type = (metadata.channels > 1) ? IMAGE_DATA_TYPE_FLOAT4 : IMAGE_DATA_TYPE_FLOAT;
		}
		else {
			metadata.type = (metadata.channels > 1) ? IMAGE_DATA_TYPE_BYTE4 : IMAGE_DATA_TYPE_BYTE;
		}

		return true;
	}

	/* Perform preliminary checks, with meaningful logging. */
	if(!path_exists(filename)) {
		VLOG(1) << "File '" << filename << "' does not exist.";
		return false;
	}
	if(path_is_directory(filename)) {
		VLOG(1) << "File '" << filename << "' is a directory, can't use as image.";
		return false;
	}

#ifdef WITH_OPENVDB
	if(string_endswith(filename, ".vdb")) {
		if(!openvdb_has_grid(filename, metadata.grid_name)) {
			VLOG(1) << "File '" << filename << "' does not have grid '" << metadata.grid_name << "'.";
			return false;
		}
		int3 resolution = openvdb_get_resolution(filename);
		metadata.width = resolution.x;
		metadata.height = resolution.y;
		metadata.depth = resolution.z;
		metadata.is_float = true;
		metadata.is_half = false;

		if(metadata.grid_name == Attribute::standard_name(ATTR_STD_VOLUME_COLOR) ||
		   metadata.grid_name == Attribute::standard_name(ATTR_STD_VOLUME_VELOCITY)) {
			metadata.channels = 4;
			metadata.type = IMAGE_DATA_TYPE_FLOAT4;
		}
		else {
			metadata.channels = 1;
			metadata.type = IMAGE_DATA_TYPE_FLOAT;
		}

		return true;
	}
#endif

	ImageInput *in = ImageInput::create(filename);

	if(!in) {
		return false;
	}

	ImageSpec spec;
	if(!in->open(filename, spec)) {
		delete in;
		return false;
	}

	metadata.width = spec.width;
	metadata.height = spec.height;
	metadata.depth = spec.depth;

	/* check the main format, and channel formats;
	 * if any take up more than one byte, we'll need a float texture slot */
	if(spec.format.basesize() > 1) {
		metadata.is_float = true;
		metadata.is_linear = true;
	}

	for(size_t channel = 0; channel < spec.channelformats.size(); channel++) {
		if(spec.channelformats[channel].basesize() > 1) {
			metadata.is_float = true;
			metadata.is_linear = true;
		}
	}

	/* check if it's half float */
	if(spec.format == TypeDesc::HALF)
		metadata.is_half = true;

	/* basic color space detection, not great but better than nothing
	 * before we do OpenColorIO integration */
	if(metadata.is_float) {
		string colorspace = spec.get_string_attribute("oiio:ColorSpace");

		metadata.is_linear = !(colorspace == "sRGB" ||
							   colorspace == "GammaCorrected" ||
							   (colorspace == "" &&
								   (strcmp(in->format_name(), "png") == 0 ||
									strcmp(in->format_name(), "tiff") == 0 ||
									strcmp(in->format_name(), "dpx") == 0 ||
									strcmp(in->format_name(), "jpeg2000") == 0)));
	}
	else {
		metadata.is_linear = false;
	}

	/* set type and channels */
	metadata.channels = spec.nchannels;

	if(metadata.is_half) {
		metadata.type = (metadata.channels > 1) ? IMAGE_DATA_TYPE_HALF4 : IMAGE_DATA_TYPE_HALF;
	}
	else if(metadata.is_float) {
		metadata.type = (metadata.channels > 1) ? IMAGE_DATA_TYPE_FLOAT4 : IMAGE_DATA_TYPE_FLOAT;
	}
	else {
		metadata.type = (metadata.channels > 1) ? IMAGE_DATA_TYPE_BYTE4 : IMAGE_DATA_TYPE_BYTE;
	}

	in->close();
	delete in;

	return true;
}

int ImageManager::max_flattened_slot(ImageDataType type)
{
	if(tex_num_images[type] == 0) {
		/* No textures for the type, no slots needs allocation. */
		return 0;
	}
	return type_index_to_flattened_slot(tex_num_images[type], type);
}

/* The lower three bits of a device texture slot number indicate its type.
 * These functions convert the slot ids from ImageManager "images" ones
 * to device ones and vice verse.
 */
int ImageManager::type_index_to_flattened_slot(int slot, ImageDataType type)
{
	return (slot << IMAGE_DATA_TYPE_SHIFT) | (type);
}

int ImageManager::flattened_slot_to_type_index(int flat_slot, ImageDataType *type)
{
	*type = (ImageDataType)(flat_slot & IMAGE_DATA_TYPE_MASK);
	return flat_slot >> IMAGE_DATA_TYPE_SHIFT;
}

string ImageManager::name_from_type(int type)
{
	if(type == IMAGE_DATA_TYPE_FLOAT4)
		return "float4";
	else if(type == IMAGE_DATA_TYPE_FLOAT)
		return "float";
	else if(type == IMAGE_DATA_TYPE_BYTE)
		return "byte";
	else if(type == IMAGE_DATA_TYPE_HALF4)
		return "half4";
	else if(type == IMAGE_DATA_TYPE_HALF)
		return "half";
	else
		return "byte4";
}

string ImageManager::name_from_grid_type(int type)
{
	if(type == IMAGE_GRID_TYPE_SPARSE)
		return "sparse";
	else if(type == IMAGE_GRID_TYPE_OPENVDB)
		return "OpenVDB";
	else
		return "default";
}

static bool image_equals(ImageManager::Image *image,
                         const string& filename,
                         void *builtin_data,
                         InterpolationType interpolation,
                         ExtensionType extension,
                         bool use_alpha,
                         const string& grid_name)
{
	return image->filename == filename &&
	       image->builtin_data == builtin_data &&
	       image->interpolation == interpolation &&
	       image->extension == extension &&
	       image->use_alpha == use_alpha &&
	       image->grid_name == grid_name;
}

int ImageManager::add_image(const string& filename,
                            void *builtin_data,
                            bool animated,
                            float frame,
                            InterpolationType interpolation,
                            ExtensionType extension,
                            bool use_alpha,
                            bool is_volume,
                            float isovalue,
                            ImageMetaData& metadata)
{
	Image *img;
	size_t slot;

	get_image_metadata(filename, builtin_data, metadata);
	ImageDataType type = metadata.type;

	thread_scoped_lock device_lock(device_mutex);

	/* No half textures on OpenCL, use full float instead. */
	if(!has_half_images) {
		if(type == IMAGE_DATA_TYPE_HALF4) {
			type = IMAGE_DATA_TYPE_FLOAT4;
		}
		else if(type == IMAGE_DATA_TYPE_HALF) {
			type = IMAGE_DATA_TYPE_FLOAT;
		}
	}

	/* Fnd existing image. */
	for(slot = 0; slot < images[type].size(); slot++) {
		img = images[type][slot];
		if(img && image_equals(img,
		                       filename,
		                       builtin_data,
		                       interpolation,
		                       extension,
		                       use_alpha,
		                       metadata.grid_name))
		{
			if(img->frame != frame) {
				img->frame = frame;
				img->need_load = true;
			}
			if(img->use_alpha != use_alpha) {
				img->use_alpha = use_alpha;
				img->need_load = true;
			}
			img->users++;
			return type_index_to_flattened_slot(slot, type);
		}
	}

	/* Find free slot. */
	for(slot = 0; slot < images[type].size(); slot++) {
		if(!images[type][slot])
			break;
	}

	/* Count if we're over the limit.
	 * Very unlikely, since max_num_images is insanely big. But better safe than sorry. */
	int tex_count = 0;
	for(int type = 0; type < IMAGE_DATA_NUM_TYPES; type++) {
		tex_count += tex_num_images[type];
	}
	if(tex_count > max_num_images) {
		printf("ImageManager::add_image: Reached image limit (%d), skipping '%s'\n",
			max_num_images, filename.c_str());
		return -1;
	}

	if(slot == images[type].size()) {
		images[type].resize(images[type].size() + 1);
	}

	/* Add new image. */
	img = new Image();
	img->filename = filename;
	img->grid_name = metadata.grid_name;
	img->builtin_data = builtin_data;
	img->builtin_free_cache = metadata.builtin_free_cache;
	img->need_load = true;
	img->animated = animated;
	img->frame = frame;
	img->interpolation = interpolation;
	img->extension = extension;
	img->users = 1;
	img->use_alpha = use_alpha;
	img->is_volume = is_volume;
	img->isovalue = isovalue;
	img->mem = NULL;

	images[type][slot] = img;

	++tex_num_images[type];

	need_update = true;

	return type_index_to_flattened_slot(slot, type);
}

void ImageManager::remove_image(int flat_slot)
{
	ImageDataType type;
	int slot = flattened_slot_to_type_index(flat_slot, &type);

	Image *image = images[type][slot];
	assert(image && image->users >= 1);

	/* decrement user count */
	image->users--;

	/* don't remove immediately, rather do it all together later on. one of
	 * the reasons for this is that on shader changes we add and remove nodes
	 * that use them, but we do not want to reload the image all the time. */
	if(image->users == 0)
		need_update = true;
}

void ImageManager::remove_image(const string& filename,
                                void *builtin_data,
                                InterpolationType interpolation,
                                ExtensionType extension,
                                bool use_alpha,
                                const string& grid_name)
{
	size_t slot;

	for(int type = 0; type < IMAGE_DATA_NUM_TYPES; type++) {
		for(slot = 0; slot < images[type].size(); slot++) {
			if(images[type][slot] && image_equals(images[type][slot],
			                                      filename,
			                                      builtin_data,
			                                      interpolation,
			                                      extension,
			                                      use_alpha,
			                                      grid_name))
			{
				remove_image(type_index_to_flattened_slot(slot, (ImageDataType)type));
				return;
			}
		}
	}
}

/* TODO(sergey): Deduplicate with the iteration above, but make it pretty,
 * without bunch of arguments passing around making code readability even
 * more cluttered.
 */
void ImageManager::tag_reload_image(const string& filename,
                                    void *builtin_data,
                                    InterpolationType interpolation,
                                    ExtensionType extension,
                                    bool use_alpha,
                                    const string& grid_name)
{
	for(size_t type = 0; type < IMAGE_DATA_NUM_TYPES; type++) {
		for(size_t slot = 0; slot < images[type].size(); slot++) {
			if(images[type][slot] && image_equals(images[type][slot],
			                                      filename,
			                                      builtin_data,
			                                      interpolation,
			                                      extension,
			                                      use_alpha,
			                                      grid_name))
			{
				images[type][slot]->need_load = true;
				break;
			}
		}
	}
}

bool ImageManager::allocate_sparse_index(Device *device,
                                         device_memory *tex_img,
                                         vector<int> *sparse_index,
                                         string mem_name)
{
	mem_name += "_index";
	device_vector<int> *tex_index =
	        new device_vector<int>(device, mem_name.c_str(), MEM_TEXTURE);

	int *ti;
	{
		thread_scoped_lock device_lock(device_mutex);
		ti = (int*)tex_index->alloc(sparse_index->size());
	}

	if(ti == NULL) {
		return false;
	}

	memcpy(ti, &(*sparse_index)[0], sparse_index->size() * sizeof(int));

	tex_img->grid_info = static_cast<void*>(tex_index);
	tex_img->grid_type = IMAGE_GRID_TYPE_SPARSE;

	return true;
}

bool ImageManager::file_load_image_generic(Image *img,
                                           ImageInput **in,
                                           int &width,
                                           int &height,
                                           int &depth,
                                           int &components)
{
	if(img->filename == "")
		return false;

	if(img->builtin_data) {
		/* load image using builtin images callbacks */
		if(!builtin_image_info_cb || !builtin_image_pixels_cb)
			return false;

		ImageMetaData metadata;
		builtin_image_info_cb(img->filename, img->builtin_data, metadata);

		width = metadata.width;
		height = metadata.height;
		depth = metadata.depth;
		components = metadata.channels;
	}
#ifdef WITH_OPENVDB
	else if(string_endswith(img->filename, ".vdb")) {
		/* NOTE: Error logging is done in meta data acquisition. */
		if(!path_exists(img->filename) || path_is_directory(img->filename)) {
			return false;
		}
		if(!openvdb_has_grid(img->filename, img->grid_name)) {
			return false;
		}

		int3 resolution = openvdb_get_resolution(img->filename);
		width = resolution.x;
		height = resolution.y;
		depth = resolution.z;

		if(img->grid_name == Attribute::standard_name(ATTR_STD_VOLUME_COLOR) ||
		   img->grid_name == Attribute::standard_name(ATTR_STD_VOLUME_VELOCITY)) {
			components = 4;
		}
		else {
			components = 1;
		}
	}
#endif
	else {
		/* NOTE: Error logging is done in meta data acquisition. */
		if(!path_exists(img->filename) || path_is_directory(img->filename)) {
			return false;
		}

		/* load image from file through OIIO */
		*in = ImageInput::create(img->filename);

		if(!*in)
			return false;

		ImageSpec spec = ImageSpec();
		ImageSpec config = ImageSpec();

		if(img->use_alpha == false)
			config.attribute("oiio:UnassociatedAlpha", 1);

		if(!(*in)->open(img->filename, spec, config)) {
			delete *in;
			*in = NULL;
			return false;
		}

		width = spec.width;
		height = spec.height;
		depth = spec.depth;
		components = spec.nchannels;
	}

	/* we only handle certain number of components */
	if(!(components >= 1 && components <= 4)) {
		if(*in) {
			(*in)->close();
			delete *in;
			*in = NULL;
		}

		return false;
	}

	return true;
}

template<typename DeviceType>
void ImageManager::file_load_failed(Image *img,
                                    ImageDataType type,
			                        device_vector<DeviceType> *tex_img)
{
	VLOG(1) << "Failed to load "
	        << path_filename(img->filename) << " ("
	        << img->mem_name << ")";

	/* On failure to load, we set a 1x1 pixels pink image. */
	thread_scoped_lock device_lock(device_mutex);
	DeviceType *device_pixels = tex_img->alloc(1, 1);

	switch(type) {
		case IMAGE_DATA_TYPE_FLOAT4:
		{
			float4 *pixels = (float4*)device_pixels;
			pixels[0].x = TEX_IMAGE_MISSING_R;
			pixels[0].y = TEX_IMAGE_MISSING_G;
			pixels[0].z = TEX_IMAGE_MISSING_B;
			pixels[0].w = TEX_IMAGE_MISSING_A;
			break;
		}
		case IMAGE_DATA_TYPE_FLOAT:
		{
			float *pixels = (float*)device_pixels;
			pixels[0] = TEX_IMAGE_MISSING_R;
			break;
		}
		case IMAGE_DATA_TYPE_BYTE4:
		{
			uchar4 *pixels = (uchar4*)device_pixels;
			pixels[0].x = (TEX_IMAGE_MISSING_R * 255);
			pixels[0].y = (TEX_IMAGE_MISSING_G * 255);
			pixels[0].z = (TEX_IMAGE_MISSING_B * 255);
			pixels[0].w = (TEX_IMAGE_MISSING_A * 255);
			break;
		}
		case IMAGE_DATA_TYPE_BYTE:
		{
			uchar *pixels = (uchar*)device_pixels;
			pixels[0] = (TEX_IMAGE_MISSING_R * 255);
			break;
		}
		case IMAGE_DATA_TYPE_HALF4:
		{
			half4 *pixels = (half4*)device_pixels;
			pixels[0].x = TEX_IMAGE_MISSING_R;
			pixels[0].y = TEX_IMAGE_MISSING_G;
			pixels[0].z = TEX_IMAGE_MISSING_B;
			pixels[0].w = TEX_IMAGE_MISSING_A;
			break;
		}
		case IMAGE_DATA_TYPE_HALF:
		{
			half *pixels = (half*)device_pixels;
			pixels[0] = TEX_IMAGE_MISSING_R;
			break;
		}
		default:
			assert(0);
	}

	/* Store image. */
	img->mem = tex_img;
	img->mem->interpolation = img->interpolation;
	img->mem->extension = img->extension;
	img->mem->grid_type = IMAGE_GRID_TYPE_DEFAULT;

	tex_img->copy_to_device();
}

#ifdef WITH_OPENVDB
template<typename DeviceType>
void ImageManager::file_load_extern_vdb(Device *device,
                                        Image *img,
                                        ImageDataType type)
{
	VLOG(1) << "Loading external VDB " << img->filename
	        << ", Grid: " << img->grid_name;

	device_vector<DeviceType> *tex_img =
	        new device_vector<DeviceType>(device,
	                                      img->mem_name.c_str(),
	                                      MEM_TEXTURE);

	/* Retrieve metadata. */
	int width, height, depth, components;
	if(!file_load_image_generic(img, NULL, width, height, depth, components)) {
		file_load_failed<DeviceType>(img, type, tex_img);
		return;
	}

	int sparse_size = -1;
	vector<int> sparse_index;
	openvdb_load_preprocess(img->filename, img->grid_name, components,
							img->isovalue, &sparse_index, sparse_size);

	/* Allocate space for image. */
	float *pixels;
	{
		thread_scoped_lock device_lock(device_mutex);
		if(sparse_size > -1) {
			pixels = (float*)tex_img->alloc(sparse_size);
		}
		else {
			pixels = (float*)tex_img->alloc(width, height, depth);
		}
	}

	if(!pixels) {
		/* Could be that we've run out of memory. */
		file_load_failed<DeviceType>(img, type, tex_img);
		return;
	}

	/* Load image. */
	openvdb_load_image(img->filename, img->grid_name, components, pixels, &sparse_index);

	/* Allocate space for sparse_index if it exists. */
	if(sparse_size > -1) {
		tex_img->grid_type = IMAGE_GRID_TYPE_SPARSE;

		if(!allocate_sparse_index(device, (device_memory*)tex_img,
								  &sparse_index, img->mem_name))
		{
			/* Could be that we've run out of memory. */
			file_load_failed<DeviceType>(img, type, tex_img);
			return;
		}
	}
	else {
		tex_img->grid_type = IMAGE_GRID_TYPE_DEFAULT;
	}

	/* Set metadata and copy. */
	tex_img->real_width = width;
	tex_img->real_height = height;
	tex_img->real_depth = depth;
	tex_img->interpolation = img->interpolation;
	tex_img->extension = img->extension;

	img->mem = tex_img;

	thread_scoped_lock device_lock(device_mutex);
	tex_img->copy_to_device();
}
#endif

template<TypeDesc::BASETYPE FileFormat,
         typename StorageType,
         typename DeviceType>
void ImageManager::file_load_image(Device *device,
                                   Image *img,
                                   ImageDataType type,
                                   int texture_limit)
{
	device_vector<DeviceType> *tex_img =
	        new device_vector<DeviceType>(device,
	                                      img->mem_name.c_str(),
	                                      MEM_TEXTURE);

	tex_img->grid_type = IMAGE_GRID_TYPE_DEFAULT;
	tex_img->interpolation = img->interpolation;
	tex_img->extension = img->extension;

	/* Try to retrieve an ImageInput for reading the image.
	 * Otherwise, retrieve metadata. */
	ImageInput *in = NULL;
	int width, height, depth, components;
	if(!file_load_image_generic(img, &in, width, height, depth, components)) {
		/* Could not retrieve image. */
		file_load_failed<DeviceType>(img, type, tex_img);
		return;
	}

	size_t max_size = max(max(width, height), depth);
	size_t num_pixels = ((size_t)width) * height * depth;
	if(max_size == 0) {
		/* Don't bother with invalid images. */
		file_load_failed<DeviceType>(img, type, tex_img);
		return;
	}

	/* Allocate storage for the image. */
	vector<StorageType> pixels_storage;
	StorageType *pixels;
	if(texture_limit > 0 && max_size > texture_limit) {
		pixels_storage.resize(num_pixels * 4);
		pixels = &pixels_storage[0];
	}
	else {
		thread_scoped_lock device_lock(device_mutex);
		pixels = (StorageType*)tex_img->alloc(width, height, depth);
	}

	if(pixels == NULL) {
		/* Could be that we've run out of memory. */
		file_load_failed<DeviceType>(img, type, tex_img);
		return;
	}

	/* Read RGBA pixels. */
	if(in) {
		StorageType *readpixels = pixels;
		vector<StorageType> tmppixels;
		if(components > 4) {
			tmppixels.resize(((size_t)width)*height*components);
			readpixels = &tmppixels[0];
		}
		if(depth <= 1) {
			size_t scanlinesize = ((size_t)width)*components*sizeof(StorageType);
			in->read_image(FileFormat,
			               (uchar*)readpixels + (height-1)*scanlinesize,
			               AutoStride,
			               -scanlinesize,
			               AutoStride);
		}
		else {
			in->read_image(FileFormat, (uchar*)readpixels);
		}
		if(components > 4) {
			size_t dimensions = ((size_t)width)*height;
			for(size_t i = dimensions-1, pixel = 0; pixel < dimensions; pixel++, i--) {
				pixels[i*4+3] = tmppixels[i*components+3];
				pixels[i*4+2] = tmppixels[i*components+2];
				pixels[i*4+1] = tmppixels[i*components+1];
				pixels[i*4+0] = tmppixels[i*components+0];
			}
			tmppixels.clear();
		}
		in->close();
		delete in;
	}
	else {
		if(FileFormat == TypeDesc::FLOAT) {
			builtin_image_float_pixels_cb(img->filename,
			                              img->builtin_data,
			                              (float*)&pixels[0],
			                              num_pixels * components,
			                              img->builtin_free_cache);
		}
		else if(FileFormat == TypeDesc::UINT8) {
			builtin_image_pixels_cb(img->filename,
			                        img->builtin_data,
			                        (uchar*)&pixels[0],
			                        num_pixels * components,
			                        img->builtin_free_cache);
		}
		else {
			/* TODO(dingto): Support half for ImBuf. */
		}
	}

	/* Image post-processing. */

	/* Check if we actually have a float4 slot, in case components == 1,
	 * but device doesn't support single channel textures.
	 */
	const StorageType alpha_one = (FileFormat == TypeDesc::UINT8)? 255 : 1;
	const bool cmyk = (in ? strcmp(in->format_name(), "jpeg") == 0 && components == 4 : false);
	const bool is_rgba = (type == IMAGE_DATA_TYPE_FLOAT4 ||
	                      type == IMAGE_DATA_TYPE_HALF4 ||
	                      type == IMAGE_DATA_TYPE_BYTE4);

	if(is_rgba) {
		if(cmyk) {
			/* CMYK */
			for(size_t i = num_pixels-1, pixel = 0; pixel < num_pixels; pixel++, i--) {
				pixels[i*4+2] = (pixels[i*4+2]*pixels[i*4+3])/255;
				pixels[i*4+1] = (pixels[i*4+1]*pixels[i*4+3])/255;
				pixels[i*4+0] = (pixels[i*4+0]*pixels[i*4+3])/255;
				pixels[i*4+3] = alpha_one;
			}
		}
		else if(components == 2) {
			/* grayscale + alpha */
			for(size_t i = num_pixels-1, pixel = 0; pixel < num_pixels; pixel++, i--) {
				pixels[i*4+3] = pixels[i*2+1];
				pixels[i*4+2] = pixels[i*2+0];
				pixels[i*4+1] = pixels[i*2+0];
				pixels[i*4+0] = pixels[i*2+0];
			}
		}
		else if(components == 3) {
			/* RGB */
			for(size_t i = num_pixels-1, pixel = 0; pixel < num_pixels; pixel++, i--) {
				pixels[i*4+3] = alpha_one;
				pixels[i*4+2] = pixels[i*3+2];
				pixels[i*4+1] = pixels[i*3+1];
				pixels[i*4+0] = pixels[i*3+0];
			}
		}
		else if(components == 1) {
			/* grayscale */
			for(size_t i = num_pixels-1, pixel = 0; pixel < num_pixels; pixel++, i--) {
				pixels[i*4+3] = alpha_one;
				pixels[i*4+2] = pixels[i];
				pixels[i*4+1] = pixels[i];
				pixels[i*4+0] = pixels[i];
			}
		}
		if(img->use_alpha == false) {
			for(size_t i = num_pixels-1, pixel = 0; pixel < num_pixels; pixel++, i--) {
				pixels[i*4+3] = alpha_one;
			}
		}
	}

	/* Make sure we don't have buggy values. */
	if(FileFormat == TypeDesc::FLOAT) {
		/* For RGBA buffers we put all channels to 0 if either of them is not
		 * finite. This way we avoid possible artifacts caused by fully changed
		 * hue.
		 */
		if(is_rgba) {
			for(size_t i = 0; i < num_pixels; i += 4) {
				StorageType *pixel = &pixels[i*4];
				if(!isfinite(pixel[0]) ||
				   !isfinite(pixel[1]) ||
				   !isfinite(pixel[2]) ||
				   !isfinite(pixel[3]))
				{
					pixel[0] = 0;
					pixel[1] = 0;
					pixel[2] = 0;
					pixel[3] = 0;
				}
			}
		}
		else {
			for(size_t i = 0; i < num_pixels; ++i) {
				StorageType *pixel = &pixels[i];
				if(!isfinite(pixel[0])) {
					pixel[0] = 0;
				}
			}
		}
	}

	/* Scale image down if needed. */
	if(pixels_storage.size() > 0) {
		float scale_factor = 1.0f;
		while(max_size * scale_factor > texture_limit) {
			scale_factor *= 0.5f;
		}
		VLOG(1) << "Scaling image " << img->filename
		        << " by a factor of " << scale_factor << ".";
		vector<StorageType> scaled_pixels;
		size_t scaled_width, scaled_height, scaled_depth;
		util_image_resize_pixels(pixels_storage,
		                         width, height, depth,
		                         is_rgba ? 4 : 1,
		                         scale_factor,
		                         &scaled_pixels,
		                         &scaled_width, &scaled_height, &scaled_depth);

		pixels = &scaled_pixels[0];
		width = scaled_width;
		height = scaled_height;
		depth = scaled_depth;
		max_size = max(max(width, height), depth);
		num_pixels = ((size_t)width) * height * depth;
	}

	/* Compress image if needed. */
	int num_pixels_real = -1;
	if(img->is_volume && device->info.type != DEVICE_CUDA) {
		vector<StorageType> sparse_pixels;
		vector<int> sparse_index;

		if(create_sparse_grid<StorageType>(pixels, width, height, depth,
		                                   components, img->filename,
		                                   img->isovalue, &sparse_pixels,
		                                   &sparse_index))
		{
			pixels = &sparse_pixels[0];
			num_pixels_real = sparse_pixels.size() / components;
			allocate_sparse_index(device, (device_memory*)tex_img,
			                      &sparse_index, img->mem_name);
		}
	}

	/* Store image. */
	StorageType *texture_pixels;
	{
		thread_scoped_lock device_lock(device_mutex);
		if(num_pixels_real > -1) {
			/* For sparse grids, the dimensions of the image do not match the
			 * required storage space. Allocate with num_pixels_real instead. */
			texture_pixels = (StorageType*)tex_img->alloc(num_pixels_real);
		}
		else {
			texture_pixels = (StorageType*)tex_img->alloc(width, height, depth);
			num_pixels_real = num_pixels;
		}
	}

	memcpy(texture_pixels, pixels, num_pixels_real * sizeof(DeviceType));

	tex_img->real_width = width;
	tex_img->real_height = height;
	tex_img->real_depth = depth;

	img->mem = tex_img;

	thread_scoped_lock device_lock(device_mutex);
	tex_img->copy_to_device();
}

void ImageManager::device_load_image(Device *device,
                                     Scene *scene,
                                     ImageDataType type,
                                     int slot,
                                     Progress *progress)
{
	if(progress->get_cancel())
		return;

	Image *img = images[type][slot];

	if(osl_texture_system && !img->builtin_data)
		return;

	string filename = path_filename(img->filename);
	progress->set_status("Updating Images", "Loading " + filename);

	/* Slot assignment */
	int flat_slot = type_index_to_flattened_slot(slot, type);
	img->mem_name = string_printf("__tex_image_%s_%03d", name_from_type(type).c_str(), flat_slot);

	/* Free previous texture(s) in slot. */
	if(img->mem) {
		thread_scoped_lock device_lock(device_mutex);
		if(img->mem->grid_info && img->mem->grid_type == IMAGE_GRID_TYPE_SPARSE) {
			device_memory *info = (device_memory*)img->mem->grid_info;
			delete info;
			img->mem->grid_info = NULL;
		}
		delete img->mem;
		img->mem = NULL;
	}

	/* Create new texture. */
	const int texture_limit = scene->params.texture_limit;
	const bool is_extern_vdb = string_endswith(img->filename, ".vdb");

	switch(type) {
		case IMAGE_DATA_TYPE_FLOAT4:
#ifdef WITH_OPENVDB
			if(is_extern_vdb)
				file_load_extern_vdb<float4>(device, img, type);
			else
#endif
				file_load_image<TypeDesc::FLOAT, float, float4>(device, img, type, texture_limit);
			break;
		case IMAGE_DATA_TYPE_FLOAT:
#ifdef WITH_OPENVDB
			if(is_extern_vdb)
				file_load_extern_vdb<float>(device, img, type);
			else
#endif
				file_load_image<TypeDesc::FLOAT, float, float>(device, img, type, texture_limit);
			break;
		case IMAGE_DATA_TYPE_BYTE4:
			file_load_image<TypeDesc::UINT8, uchar, uchar4>(device, img, type, texture_limit);
			break;
		case IMAGE_DATA_TYPE_BYTE:
			file_load_image<TypeDesc::UINT8, uchar, uchar>(device, img, type, texture_limit);
			break;
		case IMAGE_DATA_TYPE_HALF4:
			file_load_image<TypeDesc::HALF, half, half4>(device, img, type, texture_limit);
			break;
		case IMAGE_DATA_TYPE_HALF:
			file_load_image<TypeDesc::HALF, half, half>(device, img, type, texture_limit);
			break;
		default:
			assert(0);
	}

	img->need_load = false;

	if(img->mem) {
		VLOG(1) << "Loaded " << img->mem_name << " as "
				<< name_from_grid_type(img->mem->grid_type) << " grid.";
	}
}

void ImageManager::device_free_image(Device *, ImageDataType type, int slot)
{
	Image *img = images[type][slot];
	VLOG(1) << "Freeing " << img->mem_name;

	if(img) {
		if(osl_texture_system && !img->builtin_data) {
#ifdef WITH_OSL
			ustring filename(images[type][slot]->filename);
			((OSL::TextureSystem*)osl_texture_system)->invalidate(filename);
#endif
		}

		if(img->mem) {
			thread_scoped_lock device_lock(device_mutex);
			if(img->mem->grid_info && img->mem->grid_type == IMAGE_GRID_TYPE_SPARSE) {
				device_memory *info = (device_memory*)img->mem->grid_info;
				delete info;
				img->mem->grid_info = NULL;
			}
			delete img->mem;
		}

		delete img;
		images[type][slot] = NULL;
		--tex_num_images[type];
	}
}

void ImageManager::device_update(Device *device,
                                 Scene *scene,
                                 Progress& progress)
{
	if(!need_update) {
		return;
	}

	TaskPool pool;
	for(int type = 0; type < IMAGE_DATA_NUM_TYPES; type++) {
		for(size_t slot = 0; slot < images[type].size(); slot++) {
			if(!images[type][slot])
				continue;

			if(images[type][slot]->users == 0) {
				device_free_image(device, (ImageDataType)type, slot);
			}
			else if(images[type][slot]->need_load) {
				if(!osl_texture_system || images[type][slot]->builtin_data) {
					pool.push(function_bind(&ImageManager::device_load_image,
					                        this,
					                        device,
					                        scene,
					                        (ImageDataType)type,
					                        slot,
					                        &progress));
				}
			}
		}
	}

	pool.wait_work();

	need_update = false;
}

void ImageManager::device_update_slot(Device *device,
                                      Scene *scene,
                                      int flat_slot,
                                      Progress *progress)
{
	ImageDataType type;
	int slot = flattened_slot_to_type_index(flat_slot, &type);

	Image *image = images[type][slot];
	assert(image != NULL);

	if(image->users == 0) {
		device_free_image(device, type, slot);
	}
	else if(image->need_load) {
		if(!osl_texture_system || image->builtin_data) {
			device_load_image(device,
			                  scene,
			                  type,
			                  slot,
			                  progress);
		}
	}
}

void ImageManager::device_free_builtin(Device *device)
{
	for(int type = 0; type < IMAGE_DATA_NUM_TYPES; type++) {
		for(size_t slot = 0; slot < images[type].size(); slot++) {
			if(images[type][slot] && images[type][slot]->builtin_data)
				device_free_image(device, (ImageDataType)type, slot);
		}
	}
}

void ImageManager::device_free(Device *device)
{
	for(int type = 0; type < IMAGE_DATA_NUM_TYPES; type++) {
		for(size_t slot = 0; slot < images[type].size(); slot++) {
			device_free_image(device, (ImageDataType)type, slot);
		}
		images[type].clear();
	}
}

CCL_NAMESPACE_END

