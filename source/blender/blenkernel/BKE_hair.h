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
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_HAIR_H__
#define __BKE_HAIR_H__

/** \file blender/blenkernel/BKE_hair.h
 *  \ingroup bke
 */

#include "BLI_utildefines.h"

static const unsigned int HAIR_STRAND_INDEX_NONE = 0xFFFFFFFF;

struct HairFollicle;
struct HairPattern;
struct HairSystem;
struct HairDrawSettings;
struct HairGuideData;
struct Mesh;
struct MeshSample;
struct MLoop;
struct Object;

/* Create a new hair system instance */
struct HairSystem* BKE_hair_new(void);
/* Copy an existing hair system */
struct HairSystem* BKE_hair_copy(struct HairSystem *hsys);
/* Delete a hair system */
void BKE_hair_free(struct HairSystem *hsys);

/* === Guide Strands === */

/* Allocate buffers for defining guide curves
 * \param totcurves Number of guide curves to allocate
 */
void BKE_hair_guide_curves_begin(struct HairSystem *hsys, int totcurves);

/* Set properties of a guide curve
 * \param index Index of the guide guide curve
 * \param mesh_sample Origin of the guide curve on the scalp mesh.
 * \param numverts Number of vertices in this guide curve
 */
void BKE_hair_set_guide_curve(struct HairSystem *hsys, int index, const struct MeshSample *mesh_sample, int numverts,
                              float taper_length, float taper_thickness);

/* Finalize guide curve update */
void BKE_hair_guide_curves_end(struct HairSystem *hsys);

/* Set properties of a guide curve vertex
 * \param index Index of the guide curve vertex.
 * \param flag Flags to set on the vertex.
 * \param co Location of the vertex in object space.
 */
void BKE_hair_set_guide_vertex(struct HairSystem *hsys, int index, int flag, const float co[3]);

/* Set the hair guide data used by the hair system.
 */
void BKE_hair_set_hair_guides(struct HairSystem *hsys, struct HairGuideData *guides);

/* Remove all guide curves.
 */
void BKE_hair_clear_guides(struct HairSystem *hsys);

/* === Follicles === */

/* Calculate surface area of a scalp mesh */
float BKE_hair_calc_surface_area(const struct Mesh *scalp);

/* Calculate a density value based on surface area and sample count */
float BKE_hair_calc_density_from_count(float area, int count);
/* Calculate maximum sample count based on surface area and density */
int BKE_hair_calc_max_count_from_density(float area, float density);

/* Calculate a density value based on a minimum distance */
float BKE_hair_calc_density_from_min_distance(float min_distance);
/* Calculate a minimum distance based on density */
float BKE_hair_calc_min_distance_from_density(float density);

/* Distribute hair follicles on a scalp mesh */
void BKE_hair_generate_follicles(
        struct HairSystem* hsys,
        struct Mesh *scalp,
        unsigned int seed,
        int count);

/* Distribute hair follicles on a scalp mesh.
 * Optional per-loop weights control follicle density on the scalp.
 */
void BKE_hair_generate_follicles_ex(
        struct HairSystem* hsys,
        struct Mesh *scalp,
        unsigned int seed,
        int count,
        const float *loop_weights);

bool BKE_hair_bind_follicles(struct HairSystem *hsys, const struct Mesh *scalp);

/* === Draw Settings === */

struct HairDrawSettings* BKE_hair_draw_settings_new(void);
struct HairDrawSettings* BKE_hair_draw_settings_copy(struct HairDrawSettings *draw_settings);
void BKE_hair_draw_settings_free(struct HairDrawSettings *draw_settings);

/* === Export === */

/* Intermediate data for export */
typedef struct HairExportCache
{
	/* Per guide curve data */
	int totguidecurves;
	struct HairGuideCurve *guide_curves;
	
	/* Per guide vertex data */
	int totguideverts;
	struct HairGuideVertex *guide_verts;
	float (*guide_tangents)[3];             /* Tangent vectors on guide curves */
	float (*guide_normals)[3];              /* Normal vectors on guide curves */
	
	/* Per fiber data */
	int totfibercurves;
	int totfiberverts;
	int *fiber_numverts;                    /* Number of vertices in each fiber */
	float (*fiber_root_position)[3];        /* Root position of each fiber */
	
	const struct HairFollicle *follicles;
} HairExportCache;

/* Identifiers for data stored in hair export caches */
typedef enum eHairExportCacheUpdateFlags
{
	/* Follicle placement on the scalp mesh */
	HAIR_EXPORT_FIBER_ROOT_POSITIONS      = (1 << 0),
	/* Fiber vertex counts */
	HAIR_EXPORT_FIBER_VERTEX_COUNTS     = (1 << 1),
	/* Follicle parent indices and weights */
	HAIR_EXPORT_FOLLICLE_BINDING        = (1 << 2),
	/* Guide vertex positions (deform only) */
	HAIR_EXPORT_GUIDE_VERTICES          = (1 << 3),
	/* Guide curve number and vertex counts (topology changes) */
	HAIR_EXPORT_GUIDE_CURVES            = (1 << 4),
	
	HAIR_EXPORT_ALL                     =
	    HAIR_EXPORT_FIBER_ROOT_POSITIONS |
	    HAIR_EXPORT_FIBER_VERTEX_COUNTS |
	    HAIR_EXPORT_FOLLICLE_BINDING |
	    HAIR_EXPORT_GUIDE_VERTICES |
	    HAIR_EXPORT_GUIDE_CURVES,
	HAIR_EXPORT_GUIDES                  =
	    HAIR_EXPORT_GUIDE_VERTICES |
	    HAIR_EXPORT_GUIDE_CURVES,
	HAIR_EXPORT_FOLLICLES               =
	    HAIR_EXPORT_FIBER_ROOT_POSITIONS |
	    HAIR_EXPORT_FIBER_VERTEX_COUNTS |
	    HAIR_EXPORT_FOLLICLE_BINDING,
} eHairExportCacheUpdateFlags;

/* Create a new export cache.
 * This can be used to construct full fiber data for rendering.
 */
struct HairExportCache* BKE_hair_export_cache_new(void);

/* Update an existing export cache to ensure it contains the requested data.
 * Returns flags for data that has been updated.
 */
int BKE_hair_export_cache_update(struct HairExportCache *cache, const struct HairSystem *hsys,
                                 int subdiv, struct Mesh *scalp, int requested_data);

/* Free the given export cache */
void BKE_hair_export_cache_free(struct HairExportCache *cache);

/* Invalidate all data in a hair export cache */
void BKE_hair_export_cache_clear(struct HairExportCache *cache);

/* Invalidate part of the data in a hair export cache.
 *
 * Note some parts may get invalidated automatically based on internal dependencies.
 */
void BKE_hair_export_cache_invalidate(struct HairExportCache *cache, int invalidate);

/* === Draw Cache === */

enum {
	BKE_HAIR_BATCH_DIRTY_FIBERS = (1 << 0),
	BKE_HAIR_BATCH_DIRTY_STRANDS = (1 << 1),
	BKE_HAIR_BATCH_DIRTY_ALL = 0xFFFF,
};
void BKE_hair_batch_cache_dirty(struct HairSystem* hsys, int mode);
void BKE_hair_batch_cache_free(struct HairSystem* hsys);

void BKE_hair_get_texture_buffer_size(
        const struct HairExportCache *cache,
        int *r_size,
        int *r_strand_map_start,
        int *r_strand_vertex_start,
        int *r_fiber_start);
void BKE_hair_get_texture_buffer(
        const struct HairExportCache *cache,
        void *texbuffer);

/* === Render API === */

/* Calculate required size for render buffers. */
void BKE_hair_render_get_buffer_size(
        const struct HairExportCache* cache,
        int *r_totcurves,
        int *r_totverts);

/* Create render data in existing buffers.
 * Buffers must be large enough according to BKE_hair_get_render_buffer_size.
 */
void BKE_hair_render_fill_buffers(
        const struct HairExportCache* cache,
        int vertco_stride,
        int *r_curvestart,
        int *r_curvelen,
        float *r_vertco);

#endif
