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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef __BKE_ANIM_H__
#define __BKE_ANIM_H__

/** \file BKE_anim.h
 *  \ingroup bke
 *  \author nzc
 *  \since March 2001
 */
struct EvaluationContext;
struct Path;
struct Object;
struct Scene;
struct ListBase;
struct bAnimVizSettings;
struct bMotionPath;
struct bPoseChannel;
struct ReportList;
struct GHash;
struct DupliCache;
struct DupliObject;
struct DupliObjectData;
struct DerivedMesh;
struct Strands;
struct StrandsChildren;
struct DupliCacheIterator;
struct CacheLibrary;

/* ---------------------------------------------------- */
/* Animation Visualization */

void animviz_settings_init(struct bAnimVizSettings *avs);

void animviz_free_motionpath_cache(struct bMotionPath *mpath);
void animviz_free_motionpath(struct bMotionPath *mpath);

struct bMotionPath *animviz_verify_motionpaths(struct ReportList *reports, struct Scene *scene, struct Object *ob, struct bPoseChannel *pchan);

void animviz_get_object_motionpaths(struct Object *ob, ListBase *targets);
void animviz_calc_motionpaths(struct Scene *scene, ListBase *targets);

/* ---------------------------------------------------- */
/* Curve Paths */

void free_path(struct Path *path);
void calc_curvepath(struct Object *ob, struct ListBase *nurbs);
int where_on_path(struct Object *ob, float ctime, float vec[4], float dir[3], float quat[4], float *radius, float *weight);

/* ---------------------------------------------------- */
/* Dupli-Geometry */

struct ListBase *object_duplilist_ex(struct EvaluationContext *eval_ctx, struct Scene *scene, struct Object *ob, bool update);
struct ListBase *object_duplilist(struct EvaluationContext *eval_ctx, struct Scene *scene, struct Object *ob);
struct ListBase *group_duplilist_ex(struct EvaluationContext *eval_ctx, struct Scene *scene, struct Group *group, bool update);
struct ListBase *group_duplilist(struct EvaluationContext *eval_ctx, struct Scene *scene, struct Group *group);
void free_object_duplilist(struct ListBase *lb);
int count_duplilist(struct Object *ob);

void BKE_object_dupli_cache_update(struct Scene *scene, struct Object *ob, struct EvaluationContext *eval_ctx, float frame);
void BKE_object_dupli_cache_clear(struct Object *ob);
void BKE_object_dupli_cache_free(struct Object *ob);
bool BKE_object_dupli_cache_contains(struct Object *ob, struct Object *other);
struct DupliObjectData *BKE_dupli_cache_find_data(struct DupliCache *dupcache, struct Object *ob);

void BKE_dupli_object_data_init(struct DupliObjectData *data, struct Object *ob);
/* does not free data itself */
void BKE_dupli_object_data_clear(struct DupliObjectData *data);
void BKE_dupli_object_data_set_mesh(struct DupliObjectData *data, struct DerivedMesh *dm);
void BKE_dupli_object_data_add_strands(struct DupliObjectData *data, const char *name, struct Strands *strands);
void BKE_dupli_object_data_add_strands_children(struct DupliObjectData *data, const char *name, struct StrandsChildren *children);
struct Strands *BKE_dupli_object_data_find_strands(struct DupliObjectData *data, const char *name);
struct StrandsChildren *BKE_dupli_object_data_find_strands_children(struct DupliObjectData *data, const char *name);
bool BKE_dupli_object_data_acquire_strands(struct DupliObjectData *data, struct Strands *strands);
bool BKE_dupli_object_data_acquire_strands_children(struct DupliObjectData *data, struct StrandsChildren *children);

struct DupliCache *BKE_dupli_cache_new(void);
void BKE_dupli_cache_free(struct DupliCache *dupcache);
void BKE_dupli_cache_clear(struct DupliCache *dupcache);
void BKE_dupli_cache_clear_instances(struct DupliCache *dupcache);
struct DupliObjectData *BKE_dupli_cache_add_object(struct DupliCache *dupcache, struct Object *ob);
void BKE_dupli_cache_add_instance(struct DupliCache *dupcache, float obmat[4][4], struct DupliObjectData *data);
void BKE_dupli_cache_from_group(struct Scene *scene, struct Group *group, struct CacheLibrary *cachelib, struct DupliCache *dupcache, struct EvaluationContext *eval_ctx);

struct DupliCacheIterator *BKE_dupli_cache_iter_new(struct DupliCache *dupcache);
void BKE_dupli_cache_iter_free(struct DupliCacheIterator *iter);
bool BKE_dupli_cache_iter_valid(struct DupliCacheIterator *iter);
void BKE_dupli_cache_iter_next(struct DupliCacheIterator *iter);
struct DupliObjectData *BKE_dupli_cache_iter_get(struct DupliCacheIterator *iter);

typedef struct DupliExtraData {
	float obmat[4][4];
	unsigned int lay;
} DupliExtraData;

typedef struct DupliApplyData {
	int num_objects;
	DupliExtraData *extra;
} DupliApplyData;

DupliApplyData *duplilist_apply(struct Object *ob, struct ListBase *duplilist);
void duplilist_restore(struct ListBase *duplilist, DupliApplyData *apply_data);
void duplilist_free_apply_data(DupliApplyData *apply_data);

#endif
