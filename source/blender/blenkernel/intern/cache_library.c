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
 * The Original Code is Copyright (C) 2015 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/cache_library.c
 *  \ingroup bke
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_fileops.h"
#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_cache_library_types.h"
#include "DNA_group_types.h"
#include "DNA_key_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_force.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"

#include "BKE_anim.h"
#include "BKE_bvhutils.h"
#include "BKE_cache_library.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_editstrands.h"
#include "BKE_effect.h"
#include "BKE_global.h"
#include "BKE_group.h"
#include "BKE_key.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_strands.h"

#include "BLF_translation.h"

#include "PTC_api.h"

#include "BPH_mass_spring.h"

CacheLibrary *BKE_cache_library_add(Main *bmain, const char *name)
{
	CacheLibrary *cachelib;
	char basename[MAX_NAME];

	cachelib = BKE_libblock_alloc(bmain, ID_CL, name);

	BLI_strncpy(basename, cachelib->id.name+2, sizeof(basename));
	BLI_filename_make_safe(basename);
	BLI_snprintf(cachelib->output_filepath, sizeof(cachelib->output_filepath), "//cache/%s.%s", basename, PTC_get_default_archive_extension());

	cachelib->source_mode = CACHE_LIBRARY_SOURCE_SCENE;
	cachelib->display_mode = CACHE_LIBRARY_DISPLAY_MODIFIERS;
	cachelib->display_flag = CACHE_LIBRARY_DISPLAY_MOTION | CACHE_LIBRARY_DISPLAY_CHILDREN;
	cachelib->render_flag = CACHE_LIBRARY_RENDER_MOTION | CACHE_LIBRARY_RENDER_CHILDREN;
	cachelib->eval_mode = CACHE_LIBRARY_EVAL_REALTIME | CACHE_LIBRARY_EVAL_RENDER;

	/* cache everything by default */
	cachelib->data_types = CACHE_TYPE_ALL;

	return cachelib;
}

CacheLibrary *BKE_cache_library_copy(CacheLibrary *cachelib)
{
	CacheLibrary *cachelibn;
	
	cachelibn = BKE_libblock_copy(&cachelib->id);
	
	if (cachelibn->filter_group)
		id_us_plus(&cachelibn->filter_group->id);
	
	{
		CacheModifier *md;
		BLI_listbase_clear(&cachelibn->modifiers);
		for (md = cachelib->modifiers.first; md; md = md->next) {
			BKE_cache_modifier_copy(cachelibn, md);
		}
	}
	
	cachelibn->archive_info = NULL;
	
	if (cachelib->id.lib) {
		BKE_id_lib_local_paths(G.main, cachelib->id.lib, &cachelibn->id);
	}
	
	return cachelibn;
}

void BKE_cache_library_free(CacheLibrary *cachelib)
{
	BKE_cache_modifier_clear(cachelib);
	
	if (cachelib->filter_group)
		id_us_min(&cachelib->filter_group->id);
	
	if (cachelib->archive_info)
		BKE_cache_archive_info_free(cachelib->archive_info);
}

void BKE_cache_library_unlink(CacheLibrary *UNUSED(cachelib))
{
}

/* ========================================================================= */

const char *BKE_cache_item_name_prefix(int type)
{
	/* note: avoid underscores and the like here,
	 * the prefixes must be unique and safe when combined with arbitrary strings!
	 */
	switch (type) {
		case CACHE_TYPE_OBJECT: return "OBJECT";
		case CACHE_TYPE_DERIVED_MESH: return "MESH";
		case CACHE_TYPE_HAIR: return "HAIR";
		case CACHE_TYPE_HAIR_PATHS: return "HAIRPATHS";
		case CACHE_TYPE_PARTICLES: return "PARTICLES";
		default: BLI_assert(false); return NULL; break;
	}
}

void BKE_cache_item_name(Object *ob, int type, int index, char *name)
{
	if (index >= 0)
		sprintf(name, "%s_%s_%d", BKE_cache_item_name_prefix(type), ob->id.name+2, index);
	else
		sprintf(name, "%s_%s", BKE_cache_item_name_prefix(type), ob->id.name+2);
}

int BKE_cache_item_name_length(Object *ob, int type, int index)
{
	char *str_dummy = (char *)"";
	if (index >= 0)
		return BLI_snprintf(str_dummy, 0, "%s_%s_%d", BKE_cache_item_name_prefix(type), ob->id.name + 2, index);
	else
		return BLI_snprintf(str_dummy, 0, "%s_%s", BKE_cache_item_name_prefix(type), ob->id.name + 2);
}

eCacheReadSampleResult BKE_cache_read_result(int ptc_result)
{
	switch (ptc_result) {
		case PTC_READ_SAMPLE_INVALID: return CACHE_READ_SAMPLE_INVALID;
		case PTC_READ_SAMPLE_EARLY: return CACHE_READ_SAMPLE_EARLY;
		case PTC_READ_SAMPLE_LATE: return CACHE_READ_SAMPLE_LATE;
		case PTC_READ_SAMPLE_EXACT: return CACHE_READ_SAMPLE_EXACT;
		case PTC_READ_SAMPLE_INTERPOLATED: return CACHE_READ_SAMPLE_INTERPOLATED;
		default: BLI_assert(false); break; /* should never happen, enums out of sync? */
	}
	return CACHE_READ_SAMPLE_INVALID;
}

bool BKE_cache_library_validate_item(CacheLibrary *cachelib, Object *ob, int type, int index)
{
	if (!cachelib)
		return false;
	
	if (ELEM(type, CACHE_TYPE_DERIVED_MESH)) {
		if (ob->type != OB_MESH)
			return false;
	}
	else if (ELEM(type, CACHE_TYPE_PARTICLES, CACHE_TYPE_HAIR, CACHE_TYPE_HAIR_PATHS)) {
		ParticleSystem *psys = BLI_findlink(&ob->particlesystem, index);
		
		if (!psys)
			return false;
		
		if (ELEM(type, CACHE_TYPE_PARTICLES)) {
			if (psys->part->type != PART_EMITTER)
				return false;
		}
		
		if (ELEM(type, CACHE_TYPE_HAIR, CACHE_TYPE_HAIR_PATHS)) {
			if (psys->part->type != PART_HAIR)
				return false;
		}
	}
	
	return true;
}

/* ========================================================================= */

/* unused
 * filtering now only tags objects to exclude them from storing data,
 * but does not actually remove them from the duplilist.
 */
#if 0
void BKE_cache_library_filter_duplilist(CacheLibrary *cachelib, ListBase *duplilist)
{
	if (cachelib->filter_group) {
		GroupObject *gob;
		
		/* tag only filter group objects as valid */
		BKE_main_id_tag_idcode(G.main, ID_OB, false);
		for (gob = cachelib->filter_group->gobject.first; gob; gob = gob->next)
			gob->ob->id.flag |= LIB_DOIT;
	}
	else {
		/* all objects valid */
		BKE_main_id_tag_idcode(G.main, ID_OB, true);
	}
	
	{
		/* remove invalid duplis */
		DupliObject *dob, *dob_next;
		for (dob = duplilist->first; dob; dob = dob_next) {
			dob_next = dob->next;
			
			if (!(dob->ob->id.flag & LIB_DOIT)) {
				BLI_remlink(duplilist, dob);
				MEM_freeN(dob);
			}
		}
	}
	
	/* clear LIB_DOIT tags */
	BKE_main_id_tag_idcode(G.main, ID_OB, false);
}
#endif

void BKE_cache_library_tag_used_objects(CacheLibrary *cachelib)
{
	if (cachelib->filter_group) {
		GroupObject *gob;
		
		/* tag only filter group objects as valid */
		BKE_main_id_tag_idcode(G.main, ID_OB, false);
		for (gob = cachelib->filter_group->gobject.first; gob; gob = gob->next)
			gob->ob->id.flag |= LIB_DOIT;
	}
	else {
		/* all objects valid */
		BKE_main_id_tag_idcode(G.main, ID_OB, true);
	}
}

/* ========================================================================= */

BLI_INLINE bool path_is_dirpath(const char *path)
{
	/* last char is a slash? */
	const char *last_slash = BLI_last_slash(path);
	return last_slash ? (*(last_slash + 1) == '\0') : false;
}

bool BKE_cache_archive_path_test(CacheLibrary *cachelib, const char *path)
{
	if (BLI_path_is_rel(path)) {
		if (!(G.relbase_valid || cachelib->id.lib))
			return false;
	}
	
	return true;
	
}

void BKE_cache_archive_path_ex(const char *path, Library *lib, const char *default_filename, char *result, int max)
{
	char abspath[FILE_MAX];
	
	result[0] = '\0';
	
	if (BLI_path_is_rel(path)) {
		if (G.relbase_valid || lib) {
			const char *relbase = lib ? lib->filepath : G.main->name;
			
			BLI_strncpy(abspath, path, sizeof(abspath));
			BLI_path_abs(abspath, relbase);
		}
		else {
			/* can't construct a valid path */
			return;
		}
	}
	else {
		BLI_strncpy(abspath, path, sizeof(abspath));
	}
	
	if (abspath[0] != '\0') {
		if (path_is_dirpath(abspath) || BLI_is_dir(abspath)) {
			if (default_filename && default_filename[0] != '\0')
				BLI_join_dirfile(result, max, abspath, default_filename);
		}
		else {
			BLI_strncpy(result, abspath, max);
		}
	}
}

void BKE_cache_archive_input_path(CacheLibrary *cachelib, char *result, int max)
{
	BKE_cache_archive_path_ex(cachelib->input_filepath, cachelib->id.lib, NULL, result, max);
}

void BKE_cache_archive_output_path(CacheLibrary *cachelib, char *result, int max)
{
	BKE_cache_archive_path_ex(cachelib->output_filepath, cachelib->id.lib, cachelib->id.name+2, result, max);
}

static bool has_active_cache(CacheLibrary *cachelib)
{
	const bool is_baking = cachelib->flag & CACHE_LIBRARY_BAKING;
	
	/* don't read results from output archive when baking */
	if (!is_baking) {
		if (cachelib->display_mode == CACHE_LIBRARY_DISPLAY_RESULT) {
			return true;
		}
	}
	
	if (ELEM(cachelib->source_mode, CACHE_LIBRARY_SOURCE_CACHE, CACHE_LIBRARY_DISPLAY_MODIFIERS)) {
		return true;
	}
	
	return false;
}

static struct PTCReaderArchive *find_active_cache(Scene *scene, CacheLibrary *cachelib)
{
	char filename[FILE_MAX];
	struct PTCReaderArchive *archive = NULL;
	
	const bool is_baking = cachelib->flag & CACHE_LIBRARY_BAKING;
	
	/* don't read results from output archive when baking */
	if (!is_baking) {
		if (cachelib->display_mode == CACHE_LIBRARY_DISPLAY_RESULT) {
			/* try using the output cache */
			BKE_cache_archive_output_path(cachelib, filename, sizeof(filename));
			archive = PTC_open_reader_archive(scene, filename);
		}
	}
	
	if (!archive && ELEM(cachelib->source_mode, CACHE_LIBRARY_SOURCE_CACHE, CACHE_LIBRARY_DISPLAY_MODIFIERS)) {
		BKE_cache_archive_input_path(cachelib, filename, sizeof(filename));
		archive = PTC_open_reader_archive(scene, filename);
	}
	
	return archive;
}

void BKE_cache_library_get_read_flags(CacheLibrary *cachelib, eCacheLibrary_EvalMode eval_mode, bool for_display,
                                      bool *read_strands_motion, bool *read_strands_children)
{
	if (for_display) {
		switch (eval_mode) {
			case CACHE_LIBRARY_EVAL_REALTIME:
				*read_strands_motion = cachelib->display_flag & CACHE_LIBRARY_DISPLAY_MOTION;
				*read_strands_children = cachelib->display_flag & CACHE_LIBRARY_DISPLAY_CHILDREN;
				break;
			case CACHE_LIBRARY_EVAL_RENDER:
				*read_strands_motion = cachelib->render_flag & CACHE_LIBRARY_RENDER_MOTION;
				*read_strands_children = cachelib->render_flag & CACHE_LIBRARY_RENDER_CHILDREN;
				break;
			default:
				*read_strands_motion = false;
				*read_strands_children = false;
				break;
		}
	}
	else {
		*read_strands_motion = true;
		*read_strands_children = true;
	}
}

bool BKE_cache_read_dupli_cache(CacheLibrary *cachelib, DupliCache *dupcache,
                                Scene *scene, Group *dupgroup, float frame, eCacheLibrary_EvalMode eval_mode, bool for_display)
{
	bool read_strands_motion, read_strands_children, read_simdebug = G.debug & G_DEBUG_SIMDATA;
	struct PTCReaderArchive *archive;
	struct PTCReader *reader;
	
	if (!dupcache)
		return false;
	
	dupcache->result = CACHE_READ_SAMPLE_INVALID;
	
	if (!dupgroup || !cachelib)
		return false;
	if (!(cachelib->eval_mode & eval_mode))
		return false;
	
	archive = find_active_cache(scene, cachelib);
	if (!archive)
		return false;
	
	PTC_reader_archive_use_render(archive, eval_mode == CACHE_LIBRARY_EVAL_RENDER);
	
	BKE_cache_library_get_read_flags(cachelib, eval_mode, for_display, &read_strands_motion, &read_strands_children);
	// TODO duplicache reader should only overwrite data that is not sequentially generated by modifiers (simulations) ...
	reader = PTC_reader_duplicache(dupgroup->id.name, dupgroup, dupcache,
	                               read_strands_motion, read_strands_children, read_simdebug);
	PTC_reader_init(reader, archive);
	
	dupcache->result = BKE_cache_read_result(PTC_read_sample(reader, frame));
	
	PTC_reader_free(reader);
	PTC_close_reader_archive(archive);
	
	return (dupcache->result != CACHE_READ_SAMPLE_INVALID);
}

bool BKE_cache_read_dupli_object(CacheLibrary *cachelib, DupliObjectData *data,
                                 Scene *scene, Object *ob, float frame, eCacheLibrary_EvalMode eval_mode, bool for_display)
{
	bool read_strands_motion, read_strands_children;
	struct PTCReaderArchive *archive;
	struct PTCReader *reader;
	/*eCacheReadSampleResult result;*/ /* unused */
	
	if (!data || !ob || !cachelib)
		return false;
	if (!(cachelib->eval_mode & eval_mode))
		return false;
	
	archive = find_active_cache(scene, cachelib);
	if (!archive)
		return false;
	
	PTC_reader_archive_use_render(archive, eval_mode == CACHE_LIBRARY_EVAL_RENDER);
	
	BKE_cache_library_get_read_flags(cachelib, eval_mode, for_display, &read_strands_motion, &read_strands_children);
	reader = PTC_reader_duplicache_object(ob->id.name, ob, data, read_strands_motion, read_strands_children);
	PTC_reader_init(reader, archive);
	
	/*result = */BKE_cache_read_result(PTC_read_sample(reader, frame));
	
	PTC_reader_free(reader);
	PTC_close_reader_archive(archive);
	
	return true;
}


void BKE_cache_library_dag_recalc_tag(EvaluationContext *eval_ctx, Main *bmain)
{
	CacheLibrary *cachelib;
	eCacheLibrary_EvalMode eval_mode = (eval_ctx->mode == DAG_EVAL_RENDER) ? CACHE_LIBRARY_EVAL_RENDER : CACHE_LIBRARY_EVAL_REALTIME;
	
	for (cachelib = bmain->cache_library.first; cachelib; cachelib = cachelib->id.next) {
		if (cachelib->eval_mode & eval_mode) {
			if (has_active_cache(cachelib))
				DAG_id_tag_update(&cachelib->id, OB_RECALC_DATA | OB_RECALC_TIME);
		}
	}
}

/* ========================================================================= */

CacheModifierTypeInfo cache_modifier_types[NUM_CACHE_MODIFIER_TYPES];

static CacheModifierTypeInfo *cache_modifier_type_get(eCacheModifier_Type type)
{
	return &cache_modifier_types[type];
}

static void cache_modifier_type_set(eCacheModifier_Type type, CacheModifierTypeInfo *mti)
{
	memcpy(&cache_modifier_types[type], mti, sizeof(CacheModifierTypeInfo));
}

const char *BKE_cache_modifier_type_name(eCacheModifier_Type type)
{
	return cache_modifier_type_get(type)->name;
}

const char *BKE_cache_modifier_type_struct_name(eCacheModifier_Type type)
{
	return cache_modifier_type_get(type)->struct_name;
}

int BKE_cache_modifier_type_struct_size(eCacheModifier_Type type)
{
	return cache_modifier_type_get(type)->struct_size;
}

/* ------------------------------------------------------------------------- */

bool BKE_cache_modifier_unique_name(ListBase *modifiers, CacheModifier *md)
{
	if (modifiers && md) {
		CacheModifierTypeInfo *mti = cache_modifier_type_get(md->type);

		return BLI_uniquename(modifiers, md, DATA_(mti->name), '.', offsetof(CacheModifier, name), sizeof(md->name));
	}
	return false;
}

CacheModifier *BKE_cache_modifier_add(CacheLibrary *cachelib, const char *name, eCacheModifier_Type type)
{
	CacheModifierTypeInfo *mti = cache_modifier_type_get(type);
	
	CacheModifier *md = MEM_callocN(mti->struct_size, "cache modifier");
	md->type = type;
	
	if (!name)
		name = mti->name;
	BLI_strncpy_utf8(md->name, name, sizeof(md->name));
	/* make sure modifier has unique name */
	BKE_cache_modifier_unique_name(&cachelib->modifiers, md);
	
	if (mti->init)
		mti->init(md);
	
	BLI_addtail(&cachelib->modifiers, md);
	
	return md;
}

void BKE_cache_modifier_remove(CacheLibrary *cachelib, CacheModifier *md)
{
	CacheModifierTypeInfo *mti = cache_modifier_type_get(md->type);
	
	BLI_remlink(&cachelib->modifiers, md);
	
	if (mti->free)
		mti->free(md);
	
	MEM_freeN(md);
}

void BKE_cache_modifier_clear(CacheLibrary *cachelib)
{
	CacheModifier *md, *md_next;
	for (md = cachelib->modifiers.first; md; md = md_next) {
		CacheModifierTypeInfo *mti = cache_modifier_type_get(md->type);
		md_next = md->next;
		
		if (mti->free)
			mti->free(md);
		
		MEM_freeN(md);
	}
	
	BLI_listbase_clear(&cachelib->modifiers);
}

CacheModifier *BKE_cache_modifier_copy(CacheLibrary *cachelib, CacheModifier *md)
{
	CacheModifierTypeInfo *mti = cache_modifier_type_get(md->type);
	
	CacheModifier *tmd = MEM_dupallocN(md);
	
	if (mti->copy)
		mti->copy(md, tmd);
	
	BLI_addtail(&cachelib->modifiers, tmd);
	
	return tmd;
}

void BKE_cache_modifier_foreachIDLink(struct CacheLibrary *cachelib, struct CacheModifier *md, CacheModifier_IDWalkFunc walk, void *userdata)
{
	CacheModifierTypeInfo *mti = cache_modifier_type_get(md->type);
	
	if (mti->foreachIDLink)
		mti->foreachIDLink(md, cachelib, walk, userdata);
}

void BKE_cache_process_dupli_cache(CacheLibrary *cachelib, CacheProcessData *data,
                                   Scene *scene, Group *dupgroup, float frame_prev, float frame, eCacheLibrary_EvalMode eval_mode)
{
	CacheProcessContext ctx;
	CacheModifier *md;
	
	ctx.bmain = G.main;
	ctx.scene = scene;
	ctx.cachelib = cachelib;
	ctx.group = dupgroup;
	
	for (md = cachelib->modifiers.first; md; md = md->next) {
		CacheModifierTypeInfo *mti = cache_modifier_type_get(md->type);
		
		if (mti->process)
			mti->process(md, &ctx, data, frame, frame_prev, eval_mode);
	}
}

/* ------------------------------------------------------------------------- */

static ForceFieldVertexCache *forcefield_vertex_cache_new(void);
static void forcefield_vertex_cache_free(ForceFieldVertexCache *cache);
static void forcefield_vertex_cache_clear(ForceFieldVertexCache *cache);
static void forcefield_vertex_cache_init(ForceFieldVertexCache *cache, float frame, DerivedMesh *dm);

static void effector_set_mesh(CacheEffector *eff, Object *ob, DerivedMesh *dm, bool create_dm, bool create_bvhtree, bool world_space)
{
	if (create_dm && dm) {
		unsigned int numverts, i;
		MVert *mvert, *mv;
		
		eff->dm = CDDM_copy(dm);
		if (!eff->dm)
			return;
		
		DM_ensure_tessface(eff->dm);
		CDDM_calc_normals(eff->dm);
		
		numverts = eff->dm->getNumVerts(eff->dm);
		mvert = eff->dm->getVertArray(eff->dm);
		
		if (world_space) {
			/* convert to world coordinates */
			for (i = 0, mv = mvert; i < numverts; ++i, ++mv) {
				mul_m4_v3(ob->obmat, mv->co);
			}
		}
		
		if (create_bvhtree) {
			if (eff->treedata)
				free_bvhtree_from_mesh(eff->treedata);
			else
				eff->treedata = MEM_callocN(sizeof(BVHTreeFromMesh), "cache effector bvhtree data");
			
			bvhtree_from_mesh_faces(eff->treedata, eff->dm, 0.0, 2, 6);
		}
	}
}

static void effector_set_instances(CacheEffector *eff, Object *ob, float obmat[4][4], ListBase *duplilist)
{
	DupliObject *dob;
	
	for (dob = duplilist->first; dob; dob = dob->next) {
		CacheEffectorInstance *inst;
		
		if (dob->ob != ob)
			continue;
		
		inst = MEM_callocN(sizeof(CacheEffectorInstance), "cache effector instance");
		mul_m4_m4m4(inst->mat, obmat, dob->mat);
		invert_m4_m4(inst->imat, inst->mat);
		
		BLI_addtail(&eff->instances, inst);
	}
}

static bool forcefield_get_effector(DupliCache *dupcache, float obmat[4][4], ForceFieldCacheModifier *ffmd, CacheEffector *eff)
{
	DupliObjectData *dobdata;
	
	if (!ffmd->object)
		return false;
	
	dobdata = BKE_dupli_cache_find_data(dupcache, ffmd->object);
	if (!dobdata)
		return false;
	
	effector_set_mesh(eff, dobdata->ob, dobdata->dm, true, true, false);
	effector_set_instances(eff, dobdata->ob, obmat, &dupcache->duplilist);
	
	switch (ffmd->type) {
		case eForceFieldCacheModifier_Type_Deflect:
			eff->type = eCacheEffector_Type_Deflect;
			break;
		case eForceFieldCacheModifier_Type_Drag:
			eff->type = eCacheEffector_Type_Drag;
			break;
	}
	
	eff->strength = ffmd->strength;
	eff->falloff = ffmd->falloff;
	eff->mindist = ffmd->min_distance;
	eff->maxdist = ffmd->max_distance;
	eff->double_sided = ffmd->flag & eForceFieldCacheModifier_Flag_DoubleSided;
	eff->vertex_cache = ffmd->vertex_cache;
	
	return true;
}

int BKE_cache_effectors_get(CacheEffector *effectors, int max, CacheLibrary *cachelib, DupliCache *dupcache, float obmat[4][4])
{
	CacheModifier *md;
	int tot = 0;
	
	if (tot >= max)
		return tot;
	
	memset(effectors, 0, sizeof(CacheEffector) * max);
	
	for (md = cachelib->modifiers.first; md; md = md->next) {
		switch (md->type) {
			case eCacheModifierType_ForceField: {
				ForceFieldCacheModifier *ffmd = (ForceFieldCacheModifier *)md;
				if (forcefield_get_effector(dupcache, obmat, ffmd, &effectors[tot]))
					tot++;
				break;
			}
		}
		
		BLI_assert(tot <= max);
		if (tot == max)
			break;
	}
	
	return tot;
}

void BKE_cache_effectors_free(CacheEffector *effectors, int tot)
{
	CacheEffector *eff;
	int i;
	
	for (i = 0, eff = effectors; i < tot; ++i, ++eff) {
		BLI_freelistN(&eff->instances);
		
		if (eff->treedata) {
			free_bvhtree_from_mesh(eff->treedata);
			MEM_freeN(eff->treedata);
		}
		
		if (eff->dm) {
			eff->dm->release(eff->dm);
		}
	}
}

static bool forcefield_velocity_update(DupliCache *dupcache, float obmat[4][4], float frame, ForceFieldCacheModifier *ffmd)
{
	DupliObjectData *dobdata;
	bool use_vertex_cache = false;
	
	if (!ffmd->object)
		return false;
	
	dobdata = BKE_dupli_cache_find_data(dupcache, ffmd->object);
	if (!dobdata)
		return false;
	
	switch (ffmd->type) {
		case eForceFieldCacheModifier_Type_Drag:
			use_vertex_cache = true;
			break;
	}
	
	if (use_vertex_cache) {
		if (!ffmd->vertex_cache) {
			ffmd->vertex_cache = forcefield_vertex_cache_new();
		}
		
		forcefield_vertex_cache_init(ffmd->vertex_cache, frame, dobdata->dm);
		{
			int i;
			for (i = 0; i < ffmd->vertex_cache->totvert; ++i) {
				float x[3], v[3];
				mul_v3_m4v3(x, obmat, ffmd->vertex_cache->co_prev[i]);
				copy_v3_v3(v, ffmd->vertex_cache->vel[i]);
				mul_mat3_m4_v3(obmat, v);
				BKE_sim_debug_data_add_vector(x, v, 1,1,0, "hairsim", 45232, i);
			}
		}
	}
	
	return true;
}

void BKE_cache_effector_velocity_update(CacheLibrary *cachelib, DupliCache *dupcache, float obmat[4][4], float frame)
{
	CacheModifier *md;
	
	for (md = cachelib->modifiers.first; md; md = md->next) {
		switch (md->type) {
			case eCacheModifierType_ForceField:
				forcefield_velocity_update(dupcache, obmat, frame, (ForceFieldCacheModifier *)md);
				break;
		}
	}
}

static bool cache_effector_falloff(const CacheEffector *eff, float distance, float *r_factor)
{
	float mindist = eff->mindist;
	float maxdist = eff->maxdist;
	float falloff = eff->falloff;
	float range = maxdist - mindist;
	
	if (r_factor) *r_factor = 0.0f;
	
	if (range <= 0.0f)
		return false;
	
	if (distance > eff->maxdist)
		return false;
	CLAMP_MIN(distance, eff->mindist);
	CLAMP_MIN(falloff, 0.0f);
	
	if (r_factor) *r_factor = powf(1.0f - (distance - mindist) / range, falloff);
	return true;
}

typedef struct CacheEffectorTessfaceData {
	int face_index;
	MFace *mface;
	MVert *mvert[4];
	float weight[4];
} CacheEffectorTessfaceData;

static void cache_effector_velocity(const CacheEffector *eff, CacheEffectorInstance *inst, CacheEffectorTessfaceData *tessface, float vel[3])
{
	zero_v3(vel);
	
	if (!eff->vertex_cache)
		return;
	
	BLI_assert(eff->vertex_cache->totvert == eff->dm->getNumVerts(eff->dm));
	
	madd_v3_v3fl(vel, eff->vertex_cache->vel[tessface->mface->v1], tessface->weight[0]);
	madd_v3_v3fl(vel, eff->vertex_cache->vel[tessface->mface->v2], tessface->weight[1]);
	madd_v3_v3fl(vel, eff->vertex_cache->vel[tessface->mface->v3], tessface->weight[2]);
	if (tessface->mface->v4)
		madd_v3_v3fl(vel, eff->vertex_cache->vel[tessface->mface->v4], tessface->weight[3]);
	
	/* vertex cache velocities are in local space, effector results are all expected in world space */
	mul_mat3_m4_v3(inst->mat, vel);
}

static bool cache_effector_find_nearest(CacheEffector *eff, CacheEffectorInstance *inst, CacheEffectorPoint *point,
                                        float r_vec[3], float r_nor[3], float *r_dist, bool *r_inside,
                                        CacheEffectorTessfaceData *r_tessface)
{
	const bool need_inside = r_dist || r_inside;
	
	BVHTreeNearest nearest = {0, };
	float world_near_co[3], world_near_no[3];
	float co[3], vec[3], dist;
	bool inside;
	
	if (!eff->treedata)
		return false;
	
	nearest.dist_sq = FLT_MAX;
	
	/* lookup in object space */
	mul_v3_m4v3(co, inst->imat, point->x);
	
	BLI_bvhtree_find_nearest(eff->treedata->tree, co, &nearest, eff->treedata->nearest_callback, eff->treedata);
	if (nearest.index < 0)
		return false;
	
	/* convert back to world space */
	mul_v3_m4v3(world_near_co, inst->mat, nearest.co);
	copy_v3_v3(world_near_no, nearest.no);
	mul_mat3_m4_v3(inst->mat, world_near_no);
	
	sub_v3_v3v3(vec, point->x, world_near_co);
	dist = normalize_v3(vec);
	
	if (need_inside) {
		inside = false;
		if (!eff->double_sided) {
			if (dot_v3v3(vec, world_near_no) < 0.0f) {
				dist = -dist;
				inside = true;
			}
		}
	}
	
	if (r_vec) copy_v3_v3(r_vec, vec);
	if (r_nor) copy_v3_v3(r_nor, world_near_no);
	if (r_dist) *r_dist = dist;
	if (r_inside) *r_inside = inside;
	
	if (r_tessface && eff->dm) {
		CacheEffectorTessfaceData *t = r_tessface;
		DerivedMesh *dm = eff->dm;
		MFace *mf = dm->getTessFaceArray(dm) + nearest.index;
		MVert *mverts = dm->getVertArray(dm);
		
		t->face_index = nearest.index;
		t->mface = mf;
		t->mvert[0] = &mverts[mf->v1];
		t->mvert[1] = &mverts[mf->v2];
		t->mvert[2] = &mverts[mf->v3];
		
		if (mf->v4) {
			t->mvert[3] = &mverts[mf->v4];
			interp_weights_face_v3(t->weight, t->mvert[0]->co, t->mvert[1]->co, t->mvert[2]->co, t->mvert[3]->co, nearest.co);
		}
		else {
			t->mvert[3] = NULL;
			interp_weights_face_v3(t->weight, t->mvert[0]->co, t->mvert[1]->co, t->mvert[2]->co, NULL, nearest.co);
		}
	}
	
	return true;
}

static bool cache_effector_deflect(CacheEffector *eff, CacheEffectorInstance *inst, CacheEffectorPoint *point, CacheEffectorResult *result)
{
	float vec[3], dist, falloff;
	bool inside;
	
	if (!cache_effector_find_nearest(eff, inst, point, vec, NULL, &dist, &inside, NULL))
		return false;
	if (!cache_effector_falloff(eff, dist, &falloff))
		return false;
	
	mul_v3_v3fl(result->f, vec, eff->strength * falloff);
	if (inside)
		negate_v3(result->f);
	return true;
}

static bool cache_effector_drag(CacheEffector *eff, CacheEffectorInstance *inst, CacheEffectorPoint *point, CacheEffectorResult *result)
{
	float vec[3], dist, vel[3];
	float falloff;
	CacheEffectorTessfaceData facedata;
	
	if (!cache_effector_find_nearest(eff, inst, point, vec, NULL, &dist, NULL, &facedata))
		return false;
	if (!cache_effector_falloff(eff, dist, &falloff))
		return false;
	
	cache_effector_velocity(eff, inst, &facedata, vel);
	
	/* relative velocity */
	sub_v3_v3v3(vel, point->v, vel);
	
	mul_v3_v3fl(result->f, vel, -eff->strength * falloff);
	
	return true;
}

static void cache_effector_result_init(CacheEffectorResult *result)
{
	zero_v3(result->f);
}

static void cache_effector_result_add(CacheEffectorResult *result, const CacheEffectorResult *other)
{
	add_v3_v3(result->f, other->f);
}

int BKE_cache_effectors_eval_ex(CacheEffector *effectors, int tot, CacheEffectorPoint *point, CacheEffectorResult *result,
                                bool (*filter)(void *, CacheEffector *), void *filter_data)
{
	CacheEffector *eff;
	int i, applied = 0;
	
	cache_effector_result_init(result);
	
	for (i = 0, eff = effectors; i < tot; ++i, ++eff) {
		const eCacheEffector_Type type = eff->type;
		CacheEffectorInstance *inst;
		
		for (inst = eff->instances.first; inst; inst = inst->next) {
			CacheEffectorResult inst_result;
			cache_effector_result_init(&inst_result);
			
			if (filter && !filter(filter_data, eff))
				continue;
			
			switch (type) {
				case eCacheEffector_Type_Deflect:
					if (cache_effector_deflect(eff, inst, point, &inst_result)) {
						cache_effector_result_add(result, &inst_result);
						++applied;
					}
					break;
				case eCacheEffector_Type_Drag:
					if (cache_effector_drag(eff, inst, point, &inst_result)) {
						cache_effector_result_add(result, &inst_result);
						++applied;
					}
					break;
			}
		}
	}
	
	return applied;
}

int BKE_cache_effectors_eval(CacheEffector *effectors, int tot, CacheEffectorPoint *point, CacheEffectorResult *result)
{
	return BKE_cache_effectors_eval_ex(effectors, tot, point, result, NULL, NULL);
}

/* ========================================================================= */

bool BKE_cache_modifier_find_object(DupliCache *dupcache, Object *ob, DupliObjectData **r_data)
{
	DupliObjectData *dobdata;
	
	if (!ob)
		return false;
	dobdata = BKE_dupli_cache_find_data(dupcache, ob);
	if (!dobdata)
		return false;
	
	if (r_data) *r_data = dobdata;
	return true;
}

bool BKE_cache_modifier_find_strands(DupliCache *dupcache, Object *ob, int hair_system, DupliObjectData **r_data, Strands **r_strands, StrandsChildren **r_children, const char **r_name)
{
	DupliObjectData *dobdata;
	ParticleSystem *psys;
	DupliObjectDataStrands *link;
	Strands *strands;
	StrandsChildren *children;
	
	if (!ob)
		return false;
	dobdata = BKE_dupli_cache_find_data(dupcache, ob);
	if (!dobdata)
		return false;
	
	psys = BLI_findlink(&ob->particlesystem, hair_system);
	if (!psys || (psys->part->type != PART_HAIR))
		return false;
	
	strands = NULL;
	children = NULL;
	for (link = dobdata->strands.first; link; link = link->next) {
		if (link->strands && STREQ(link->name, psys->name)) {
			strands = link->strands;
			children = link->strands_children;
			break;
		}
	}
	if ((r_strands && !strands) || (r_children && !children))
		return false;
	
	if (r_data) *r_data = dobdata;
	if (r_strands) *r_strands = strands;
	if (r_children) *r_children = children;
	if (r_name) *r_name = psys->name;
	return true;
}

static void hairsim_params_init(HairSimParams *params)
{
	params->timescale = 1.0f;
	params->substeps = 5;
	
	params->mass = 0.3f;
	params->drag = 0.1f;
	
	params->stretch_stiffness = 10000.0f;
	params->stretch_damping = 0.1f;
	params->bend_stiffness = 100.0f;
	params->bend_damping = 1.0f;
	params->goal_stiffness = 0.0f;
	params->goal_damping = 1.0f;
	{
		CurveMapping *cm = curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
		cm->cm[0].curve[0].x = 0.0f;
		cm->cm[0].curve[0].y = 1.0f;
		cm->cm[0].curve[1].x = 1.0f;
		cm->cm[0].curve[1].y = 0.0f;
		curvemapping_changed_all(cm);
		params->goal_stiffness_mapping = cm;
	}
	{
		CurveMapping *cm = curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
		cm->cm[0].curve[0].x = 0.0f;
		cm->cm[0].curve[0].y = 1.0f;
		cm->cm[0].curve[1].x = 1.0f;
		cm->cm[0].curve[1].y = 1.0f;
		curvemapping_changed_all(cm);
		params->bend_stiffness_mapping = cm;
	}
	
	params->effector_weights = BKE_add_effector_weights(NULL);
}

static void hairsim_init(HairSimCacheModifier *hsmd)
{
	hsmd->object = NULL;
	hsmd->hair_system = -1;
	
	hairsim_params_init(&hsmd->sim_params);
}

static void hairsim_copy(HairSimCacheModifier *hsmd, HairSimCacheModifier *thsmd)
{
	if (hsmd->sim_params.effector_weights)
		thsmd->sim_params.effector_weights = MEM_dupallocN(hsmd->sim_params.effector_weights);
	if (hsmd->sim_params.goal_stiffness_mapping)
		thsmd->sim_params.goal_stiffness_mapping = curvemapping_copy(hsmd->sim_params.goal_stiffness_mapping);
	if (hsmd->sim_params.bend_stiffness_mapping)
		thsmd->sim_params.bend_stiffness_mapping = curvemapping_copy(hsmd->sim_params.bend_stiffness_mapping);
}

static void hairsim_free(HairSimCacheModifier *hsmd)
{
	if (hsmd->sim_params.effector_weights)
		MEM_freeN(hsmd->sim_params.effector_weights);
	if (hsmd->sim_params.goal_stiffness_mapping)
		curvemapping_free(hsmd->sim_params.goal_stiffness_mapping);
	if (hsmd->sim_params.bend_stiffness_mapping)
		curvemapping_free(hsmd->sim_params.bend_stiffness_mapping);
}

static void hairsim_foreach_id_link(HairSimCacheModifier *hsmd, CacheLibrary *cachelib, CacheModifier_IDWalkFunc walk, void *userdata)
{
	walk(userdata, cachelib, &hsmd->modifier, (ID **)(&hsmd->object));
	if (hsmd->sim_params.effector_weights)
		walk(userdata, cachelib, &hsmd->modifier, (ID **)(&hsmd->sim_params.effector_weights->group));
}

static void hairsim_process(HairSimCacheModifier *hsmd, CacheProcessContext *ctx, CacheProcessData *data, int frame, int frame_prev, eCacheLibrary_EvalMode UNUSED(eval_mode))
{
#define MAX_CACHE_EFFECTORS 64
	
	Object *ob = hsmd->object;
	Strands *strands;
	float mat[4][4];
	ListBase *effectors;
	CacheEffector cache_effectors[MAX_CACHE_EFFECTORS];
	int tot_cache_effectors;
	struct Implicit_Data *solver_data;
	
	/* only perform hair sim once */
//	if (eval_mode != CACHE_LIBRARY_EVAL_REALTIME)
//		return;
	
	if (!BKE_cache_modifier_find_strands(data->dupcache, ob, hsmd->hair_system, NULL, &strands, NULL, NULL))
		return;
	
	/* Note: motion state data should always be created regardless of actual sim.
	 * This is necessary so the cache writer actually writes the first (empty) sample
	 * and the samples get mapped correctly to frames when reading.
	 */
	BKE_strands_add_motion_state(strands);
	
	/* skip first step and potential backward steps */
	if (frame > frame_prev) {
		if (hsmd->sim_params.flag & eHairSimParams_Flag_UseGoalStiffnessCurve && hsmd->sim_params.goal_stiffness_mapping)
			curvemapping_changed_all(hsmd->sim_params.goal_stiffness_mapping);
		if (hsmd->sim_params.flag & eHairSimParams_Flag_UseBendStiffnessCurve && hsmd->sim_params.bend_stiffness_mapping)
			curvemapping_changed_all(hsmd->sim_params.bend_stiffness_mapping);
		
		if (ob)
			mul_m4_m4m4(mat, data->mat, ob->obmat);
		else
			copy_m4_m4(mat, data->mat);
		
		BKE_cache_effector_velocity_update(ctx->cachelib, data->dupcache, data->mat, (float)frame);
		
		solver_data = BPH_strands_solver_create(strands, &hsmd->sim_params);
		effectors = pdInitEffectors_ex(ctx->scene, ob, NULL, data->lay, hsmd->sim_params.effector_weights, true);
		tot_cache_effectors = BKE_cache_effectors_get(cache_effectors, MAX_CACHE_EFFECTORS, ctx->cachelib, data->dupcache, data->mat);
		
		BPH_strands_solve(strands, mat, solver_data, &hsmd->sim_params, (float)frame, (float)frame_prev, ctx->scene, effectors, cache_effectors, tot_cache_effectors);
		
		pdEndEffectors(&effectors);
		BKE_cache_effectors_free(cache_effectors, tot_cache_effectors);
		BPH_mass_spring_solver_free(solver_data);
	}
	
#undef MAX_CACHE_EFFECTORS
}

CacheModifierTypeInfo cacheModifierType_HairSimulation = {
    /* name */              "HairSimulation",
    /* structName */        "HairSimCacheModifier",
    /* structSize */        sizeof(HairSimCacheModifier),

    /* copy */              (CacheModifier_CopyFunc)hairsim_copy,
    /* foreachIDLink */     (CacheModifier_ForeachIDLinkFunc)hairsim_foreach_id_link,
    /* process */           (CacheModifier_ProcessFunc)hairsim_process,
    /* init */              (CacheModifier_InitFunc)hairsim_init,
    /* free */              (CacheModifier_FreeFunc)hairsim_free,
};

/* ------------------------------------------------------------------------- */

static ForceFieldVertexCache *forcefield_vertex_cache_new(void)
{
	ForceFieldVertexCache *cache = MEM_callocN(sizeof(ForceFieldVertexCache), "force field vertex cache");
	return cache;
}

static void forcefield_vertex_cache_free(ForceFieldVertexCache *cache)
{
	if (cache->co_prev)
		MEM_freeN(cache->co_prev);
	if (cache->vel)
		MEM_freeN(cache->vel);
	MEM_freeN(cache);
}

static void forcefield_vertex_cache_clear(ForceFieldVertexCache *cache)
{
	if (cache->co_prev)
		MEM_freeN(cache->co_prev);
	if (cache->vel)
		MEM_freeN(cache->vel);
	memset(cache, 0, sizeof(ForceFieldVertexCache));
}

static void forcefield_vertex_cache_init(ForceFieldVertexCache *cache, float frame, DerivedMesh *dm)
{
	MVert *mvert = dm->getVertArray(dm);
	float dframe = frame - cache->frame_prev;
	float inv_dframe = dframe > 0.0f ? 1.0f / dframe : 0.0f;
	bool has_co_prev = (cache->co_prev != NULL);
	int totvert = dm->getNumVerts(dm);
	int i;
	
	if (cache->totvert != totvert) {
		forcefield_vertex_cache_clear(cache);
		dframe = 0.0f;
	}
	
	if (!cache->co_prev)
		cache->co_prev = MEM_mallocN(sizeof(float) * 3 * totvert, "force field vertex cache vertices");
	if (!cache->vel)
		cache->vel = MEM_mallocN(sizeof(float) * 3 * totvert, "force field vertex cache vertices");
	
	for (i = 0; i < totvert; ++i) {
		if (has_co_prev) {
			sub_v3_v3v3(cache->vel[i], mvert[i].co, cache->co_prev[i]);
			mul_v3_fl(cache->vel[i], inv_dframe);
		}
		else {
			zero_v3(cache->vel[i]);
		}
		
		copy_v3_v3(cache->co_prev[i], mvert[i].co);
	}
	cache->frame_prev = frame;
	cache->totvert = totvert;
}

static void forcefield_init(ForceFieldCacheModifier *ffmd)
{
	ffmd->object = NULL;
	
	ffmd->vertex_cache = NULL;
	
	ffmd->strength = 0.0f;
	ffmd->falloff = 1.0f;
	ffmd->min_distance = 0.0f;
	ffmd->max_distance = 1.0f;
}

static void forcefield_copy(ForceFieldCacheModifier *UNUSED(ffmd), ForceFieldCacheModifier *tffmd)
{
	tffmd->vertex_cache = NULL;
}

static void forcefield_free(ForceFieldCacheModifier *ffmd)
{
	if (ffmd->vertex_cache) {
		forcefield_vertex_cache_free(ffmd->vertex_cache);
		ffmd->vertex_cache = NULL;
	}
}

static void forcefield_foreach_id_link(ForceFieldCacheModifier *ffmd, CacheLibrary *cachelib, CacheModifier_IDWalkFunc walk, void *userdata)
{
	walk(userdata, cachelib, &ffmd->modifier, (ID **)(&ffmd->object));
}

CacheModifierTypeInfo cacheModifierType_ForceField = {
    /* name */              "ForceField",
    /* structName */        "ForceFieldCacheModifier",
    /* structSize */        sizeof(ForceFieldCacheModifier),

    /* copy */              (CacheModifier_CopyFunc)forcefield_copy,
    /* foreachIDLink */     (CacheModifier_ForeachIDLinkFunc)forcefield_foreach_id_link,
    /* process */           (CacheModifier_ProcessFunc)NULL,
    /* init */              (CacheModifier_InitFunc)forcefield_init,
    /* free */              (CacheModifier_FreeFunc)forcefield_free,
};

/* ------------------------------------------------------------------------- */

static void shrinkwrap_init(ShrinkWrapCacheModifier *smd)
{
	smd->object = NULL;
	smd->hair_system = -1;
}

static void shrinkwrap_copy(ShrinkWrapCacheModifier *UNUSED(smd), ShrinkWrapCacheModifier *UNUSED(tsmd))
{
}

static void shrinkwrap_free(ShrinkWrapCacheModifier *UNUSED(smd))
{
}

static void shrinkwrap_foreach_id_link(ShrinkWrapCacheModifier *smd, CacheLibrary *cachelib, CacheModifier_IDWalkFunc walk, void *userdata)
{
	walk(userdata, cachelib, &smd->modifier, (ID **)(&smd->object));
	walk(userdata, cachelib, &smd->modifier, (ID **)(&smd->target));
}

typedef struct ShrinkWrapCacheData {
	DerivedMesh *dm;
	BVHTreeFromMesh treedata;
	
	ListBase instances;
} ShrinkWrapCacheData;

typedef struct ShrinkWrapCacheInstance {
	struct ShrinkWrapCacheInstance *next, *prev;
	
	float mat[4][4];
	float imat[4][4];
} ShrinkWrapCacheInstance;

static void shrinkwrap_data_get_bvhtree(ShrinkWrapCacheData *data, DerivedMesh *dm, bool create_bvhtree)
{
	data->dm = CDDM_copy(dm);
	if (!data->dm)
		return;
	
	DM_ensure_tessface(data->dm);
	CDDM_calc_normals(data->dm);
	
	if (create_bvhtree) {
		bvhtree_from_mesh_faces(&data->treedata, data->dm, 0.0, 2, 6);
	}
}

static void shrinkwrap_data_get_instances(ShrinkWrapCacheData *data, Object *ob, float obmat[4][4], ListBase *duplilist)
{
	DupliObject *dob;
	
	for (dob = duplilist->first; dob; dob = dob->next) {
		ShrinkWrapCacheInstance *inst;
		
		if (dob->ob != ob)
			continue;
		
		inst = MEM_callocN(sizeof(ShrinkWrapCacheInstance), "shrink wrap instance");
		mul_m4_m4m4(inst->mat, obmat, dob->mat);
		invert_m4_m4(inst->imat, inst->mat);
		
		BLI_addtail(&data->instances, inst);
	}
}

static void shrinkwrap_data_free(ShrinkWrapCacheData *data)
{
	BLI_freelistN(&data->instances);
	
	free_bvhtree_from_mesh(&data->treedata);
	
	if (data->dm) {
		data->dm->release(data->dm);
	}
}

static void shrinkwrap_apply_vertex(ShrinkWrapCacheModifier *UNUSED(smd), ShrinkWrapCacheData *data, ShrinkWrapCacheInstance *inst, StrandsVertex *vertex, StrandsMotionState *UNUSED(state))
{
//	const float *point = state->co;
//	float *npoint = state->co;
	const float *point = vertex->co;
	float *npoint = vertex->co;
	
	BVHTreeNearest nearest = {0, };
	float co[3];
	
	if (!data->treedata.tree)
		return;
	
	nearest.index = -1;
	nearest.dist_sq = FLT_MAX;
	
	/* lookup in target space */
	mul_v3_m4v3(co, inst->imat, point);
	
	BLI_bvhtree_find_nearest(data->treedata.tree, co, &nearest, data->treedata.nearest_callback, &data->treedata);
	if (nearest.index < 0)
		return;
	
	/* convert back to world space */
	mul_m4_v3(inst->mat, nearest.co);
	mul_mat3_m4_v3(inst->mat, nearest.no);
	
	{
		float vec[3];
		
		sub_v3_v3v3(vec, point, nearest.co);
		
		/* project along the distance vector */
		if (dot_v3v3(vec, nearest.no) < 0.0f) {
			sub_v3_v3v3(npoint, point, vec);
		}
	}
}

static void shrinkwrap_apply(ShrinkWrapCacheModifier *smd, ShrinkWrapCacheData *data, Strands *strands)
{
	StrandIterator it_strand;
	for (BKE_strand_iter_init(&it_strand, strands); BKE_strand_iter_valid(&it_strand); BKE_strand_iter_next(&it_strand)) {
		StrandVertexIterator it_vert;
		for (BKE_strand_vertex_iter_init(&it_vert, &it_strand); BKE_strand_vertex_iter_valid(&it_vert); BKE_strand_vertex_iter_next(&it_vert)) {
			ShrinkWrapCacheInstance *inst;
			
			/* XXX this is not great, the result depends on order of instances in the duplilist ...
			 * but good enough for single instance use case.
			 */
			for (inst = data->instances.first; inst; inst = inst->next) {
				shrinkwrap_apply_vertex(smd, data, inst, it_vert.vertex, it_vert.state);
			}
		}
	}
}

static void shrinkwrap_process(ShrinkWrapCacheModifier *smd, CacheProcessContext *UNUSED(ctx), CacheProcessData *data, int UNUSED(frame), int UNUSED(frame_prev), eCacheLibrary_EvalMode UNUSED(eval_mode))
{
	Object *ob = smd->object;
	DupliObject *dob;
	Strands *strands;
	DupliObjectData *target_data;
	float mat[4][4];
	
	ShrinkWrapCacheData shrinkwrap;
	
	if (!BKE_cache_modifier_find_strands(data->dupcache, ob, smd->hair_system, NULL, &strands, NULL, NULL))
		return;
	if (!BKE_cache_modifier_find_object(data->dupcache, smd->target, &target_data))
		return;
	
	for (dob = data->dupcache->duplilist.first; dob; dob = dob->next) {
		if (dob->ob != ob)
			continue;
		
		/* instances are calculated relative to the strands object */
		invert_m4_m4(mat, dob->mat);
		
		memset(&shrinkwrap, 0, sizeof(shrinkwrap));
		shrinkwrap_data_get_bvhtree(&shrinkwrap, target_data->dm, true);
		shrinkwrap_data_get_instances(&shrinkwrap, smd->target, mat, &data->dupcache->duplilist);
		
		shrinkwrap_apply(smd, &shrinkwrap, strands);
		
		shrinkwrap_data_free(&shrinkwrap);
		
		/* XXX assume a single instance ... otherwise would just overwrite previous strands data */
		break;
	}
}

CacheModifierTypeInfo cacheModifierType_ShrinkWrap = {
    /* name */              "ShrinkWrap",
    /* structName */        "ShrinkWrapCacheModifier",
    /* structSize */        sizeof(ShrinkWrapCacheModifier),

    /* copy */              (CacheModifier_CopyFunc)shrinkwrap_copy,
    /* foreachIDLink */     (CacheModifier_ForeachIDLinkFunc)shrinkwrap_foreach_id_link,
    /* process */           (CacheModifier_ProcessFunc)shrinkwrap_process,
    /* init */              (CacheModifier_InitFunc)shrinkwrap_init,
    /* free */              (CacheModifier_FreeFunc)shrinkwrap_free,
};

/* ------------------------------------------------------------------------- */

static void strandskey_init(StrandsKeyCacheModifier *skmd)
{
	skmd->object = NULL;
	skmd->hair_system = -1;
	
	skmd->key = BKE_key_add_ex(NULL, KEY_OWNER_CACHELIB, -1);
	skmd->key->type = KEY_RELATIVE;
}

static void strandskey_copy(StrandsKeyCacheModifier *skmd, StrandsKeyCacheModifier *tskmd)
{
	tskmd->key = BKE_key_copy(skmd->key);
	
	tskmd->edit = NULL;
}

static void strandskey_free(StrandsKeyCacheModifier *skmd)
{
	BKE_key_free(skmd->key);
	
	if (skmd->edit) {
		BKE_editstrands_free(skmd->edit);
		MEM_freeN(skmd->edit);
		skmd->edit = NULL;
	}
}

static void strandskey_foreach_id_link(StrandsKeyCacheModifier *skmd, CacheLibrary *cachelib, CacheModifier_IDWalkFunc walk, void *userdata)
{
	walk(userdata, cachelib, &skmd->modifier, (ID **)(&skmd->object));
}

static void strandskey_process(StrandsKeyCacheModifier *skmd, CacheProcessContext *UNUSED(ctx), CacheProcessData *data, int UNUSED(frame), int UNUSED(frame_prev), eCacheLibrary_EvalMode UNUSED(eval_mode))
{
	const bool use_motion = skmd->flag & eStrandsKeyCacheModifier_Flag_UseMotionState;
	Object *ob = skmd->object;
	Strands *strands;
	KeyBlock *actkb;
	float *shape;
	
	if (!BKE_cache_modifier_find_strands(data->dupcache, ob, skmd->hair_system, NULL, &strands, NULL, NULL))
		return;
	if (use_motion && !strands->state)
		return;
	
	actkb = BLI_findlink(&skmd->key->block, skmd->shapenr);
	shape = BKE_key_evaluate_strands(strands, skmd->key, actkb, skmd->flag & eStrandsKeyCacheModifier_Flag_ShapeLock, NULL, use_motion);
	if (shape) {
		StrandsVertex *vert = strands->verts;
		StrandsMotionState *state = use_motion ? strands->state : NULL;
		int totvert = strands->totverts;
		int i;
		
		float *fp = shape;
		for (i = 0; i < totvert; ++i) {
			if (state) {
				copy_v3_v3(state->co, fp);
				++state;
			}
			else {
				copy_v3_v3(vert->co, fp);
				++vert;
			}
			fp += 3;
		}
		
		MEM_freeN(shape);
	}
}

CacheModifierTypeInfo cacheModifierType_StrandsKey = {
    /* name */              "StrandsKey",
    /* structName */        "StrandsKeyCacheModifier",
    /* structSize */        sizeof(StrandsKeyCacheModifier),

    /* copy */              (CacheModifier_CopyFunc)strandskey_copy,
    /* foreachIDLink */     (CacheModifier_ForeachIDLinkFunc)strandskey_foreach_id_link,
    /* process */           (CacheModifier_ProcessFunc)strandskey_process,
    /* init */              (CacheModifier_InitFunc)strandskey_init,
    /* free */              (CacheModifier_FreeFunc)strandskey_free,
};

KeyBlock *BKE_cache_modifier_strands_key_insert_key(StrandsKeyCacheModifier *skmd, Strands *strands, const char *name, const bool from_mix)
{
	const bool use_motion = skmd->flag & eStrandsKeyCacheModifier_Flag_UseMotionState;
	Key *key = skmd->key;
	KeyBlock *kb;
	bool newkey = false;
	
	if (key == NULL) {
		key = skmd->key = BKE_key_add_ex(NULL, KEY_OWNER_CACHELIB, -1);
		key->type = KEY_RELATIVE;
		newkey = true;
	}
	else if (BLI_listbase_is_empty(&key->block)) {
		newkey = true;
	}
	
	if (newkey || from_mix == false) {
		/* create from mesh */
		kb = BKE_keyblock_add_ctime(key, name, false);
		BKE_keyblock_convert_from_strands(strands, key, kb, use_motion);
	}
	else {
		/* copy from current values */
		KeyBlock *actkb = BLI_findlink(&skmd->key->block, skmd->shapenr);
		bool shape_lock = skmd->flag & eStrandsKeyCacheModifier_Flag_ShapeLock;
		int totelem;
		float *data = BKE_key_evaluate_strands(strands, key, actkb, shape_lock, &totelem, use_motion);
		
		/* create new block with prepared data */
		kb = BKE_keyblock_add_ctime(key, name, false);
		kb->data = data;
		kb->totelem = totelem;
	}
	
	return kb;
}

bool BKE_cache_modifier_strands_key_get(Object *ob, StrandsKeyCacheModifier **r_skmd, DerivedMesh **r_dm, Strands **r_strands, DupliObjectData **r_dobdata, const char **r_name, float r_mat[4][4])
{
	CacheLibrary *cachelib = ob->cache_library;
	CacheModifier *md;
	
	if (!cachelib)
		return false;
	
	/* ignore when the object is not actually using the cachelib */
	if (!((ob->transflag & OB_DUPLIGROUP) && ob->dup_group && ob->dup_cache))
		return false;
	
	for (md = cachelib->modifiers.first; md; md = md->next) {
		if (md->type == eCacheModifierType_StrandsKey) {
			StrandsKeyCacheModifier *skmd = (StrandsKeyCacheModifier *)md;
			DupliObjectData *dobdata;
			
			if (BKE_cache_modifier_find_strands(ob->dup_cache, skmd->object, skmd->hair_system, &dobdata, r_strands, NULL, r_name)) {
				if (r_skmd) *r_skmd = skmd;
				if (r_dm) *r_dm = dobdata->dm;
				if (r_dobdata) *r_dobdata = dobdata;
				
				/* relative transform from the original hair object to the duplicator local space */
				/* XXX bad hack, common problem: we want to display strand edit data in the place of "the" instance,
				 * but in fact there can be multiple instances of the same dupli object data, so this is ambiguous ...
				 * For our basic use case, just pick the first dupli instance, assuming that it's the only one.
				 * ugh ...
				 */
				if (r_mat) {
					DupliObject *dob;
					for (dob = ob->dup_cache->duplilist.first; dob; dob = dob->next) {
						if (dob->ob == skmd->object)
							break;
					}
					if (dob) {
						/* note: plain duplis from the dupli cache list are relative
						 * to the duplicator already! (not in world space like final duplis)
						 */
						copy_m4_m4(r_mat, dob->mat);
					}
					else
						unit_m4(r_mat);
				}
				
				return true;
			}
		}
	}
	
	return false;
}

/* ------------------------------------------------------------------------- */

static void haircut_init(HaircutCacheModifier *hmd)
{
	hmd->object = NULL;
	hmd->hair_system = -1;
}

static void haircut_copy(HaircutCacheModifier *UNUSED(hmd), HaircutCacheModifier *UNUSED(thmd))
{
}

static void haircut_free(HaircutCacheModifier *UNUSED(hmd))
{
}

static void haircut_foreach_id_link(HaircutCacheModifier *smd, CacheLibrary *cachelib, CacheModifier_IDWalkFunc walk, void *userdata)
{
	walk(userdata, cachelib, &smd->modifier, (ID **)(&smd->object));
	walk(userdata, cachelib, &smd->modifier, (ID **)(&smd->target));
}

typedef struct HaircutCacheData {
	DerivedMesh *dm;
	BVHTreeFromMesh treedata;
	
	ListBase instances;
} HaircutCacheData;

typedef struct HaircutCacheInstance {
	struct HaircutCacheInstance *next, *prev;
	
	float mat[4][4];
	float imat[4][4];
} HaircutCacheInstance;

static void haircut_data_get_bvhtree(HaircutCacheData *data, DerivedMesh *dm, bool create_bvhtree)
{
	data->dm = CDDM_copy(dm);
	if (!data->dm)
		return;
	
	DM_ensure_tessface(data->dm);
	CDDM_calc_normals(data->dm);
	
	if (create_bvhtree) {
		bvhtree_from_mesh_faces(&data->treedata, data->dm, 0.0, 2, 6);
	}
}

static void haircut_data_get_instances(HaircutCacheData *data, Object *ob, float obmat[4][4], ListBase *duplilist)
{
	if (duplilist) {
		DupliObject *dob;
		
		for (dob = duplilist->first; dob; dob = dob->next) {
			HaircutCacheInstance *inst;
			
			if (dob->ob != ob)
				continue;
			
			inst = MEM_callocN(sizeof(HaircutCacheInstance), "haircut instance");
			mul_m4_m4m4(inst->mat, obmat, dob->mat);
			invert_m4_m4(inst->imat, inst->mat);
			
			BLI_addtail(&data->instances, inst);
		}
	}
	else {
		HaircutCacheInstance *inst = MEM_callocN(sizeof(HaircutCacheInstance), "haircut instance");
		mul_m4_m4m4(inst->mat, obmat, ob->obmat);
		invert_m4_m4(inst->imat, inst->mat);
		
		BLI_addtail(&data->instances, inst);
	}
}

static void haircut_data_free(HaircutCacheData *data)
{
	BLI_freelistN(&data->instances);
	
	free_bvhtree_from_mesh(&data->treedata);
	
	if (data->dm) {
		data->dm->release(data->dm);
	}
}

/* XXX intersection counting does not work reliably */
#if 0
typedef struct PointInsideBVH {
	BVHTreeFromMesh bvhdata;
	int num_hits;
} PointInsideBVH;

static void point_inside_bvh_cb(void *userdata, int index, const BVHTreeRay *ray, BVHTreeRayHit *hit)
{
	PointInsideBVH *data = userdata;
	
	data->bvhdata.raycast_callback(&data->bvhdata, index, ray, hit);
	
	if (hit->index != -1)
		++data->num_hits;
}

/* true if the point is inside the target mesh */
static bool haircut_test_point(HaircutCacheModifier *hmd, HaircutCacheData *data, HaircutCacheInstance *inst, const float *v)
{
	const float dir[3] = {1.0f, 0.0f, 0.0f};
	float start[3];
	PointInsideBVH userdata;
	
	if (!(hmd->cut_mode & eHaircutCacheModifier_CutMode_Enter))
		return false;
	
	userdata.bvhdata = data->treedata;
	userdata.num_hits = 0;
	
	/* lookup in target space */
	mul_v3_m4v3(start, inst->imat, v);
	
	BLI_bvhtree_ray_cast_all(data->treedata.tree, start, dir, 0.0f, point_inside_bvh_cb, &userdata);
	
	/* for any point inside a watertight mesh the number of hits is uneven */
	return (userdata.num_hits % 2) == 1;
}
#else
/* true if the point is inside the target mesh */
static bool haircut_test_point(HaircutCacheModifier *hmd, HaircutCacheData *data, HaircutCacheInstance *inst, const float *v)
{
	BVHTreeRayHit hit = {0, };
	float start[3], dir[3] = {0.0f, 0.0f, 1.0f};
	bool is_entering;
	
	if (!(hmd->cut_mode & eHaircutCacheModifier_CutMode_Enter))
		return false;
	if (!data->treedata.tree)
		return false;
	
	/* lookup in target space */
	mul_v3_m4v3(start, inst->imat, v);
	
	hit.index = -1;
	hit.dist = FLT_MAX;
	
	BLI_bvhtree_ray_cast(data->treedata.tree, start, dir, 0.0f, &hit, data->treedata.raycast_callback, &data->treedata);
	if (hit.index < 0) {
		return false;
	}
	
	mul_mat3_m4_v3(inst->mat, hit.no);
	
	is_entering = (dot_v3v3(dir, hit.no) < 0.0f);
	
	return !is_entering;
}
#endif

static bool haircut_find_segment_cut(HaircutCacheModifier *hmd, HaircutCacheData *data, HaircutCacheInstance *inst,
                                     const float *v1, const float *v2, float *r_lambda)
{
	BVHTreeRayHit hit = {0, };
	float start[3], dir[3], length;
	bool is_entering;
	
	if (!data->treedata.tree)
		return false;
	
	/* lookup in target space */
	mul_v3_m4v3(start, inst->imat, v1);
	sub_v3_v3v3(dir, v2, v1);
	mul_mat3_m4_v3(inst->imat, dir);
	length = normalize_v3(dir);
	
	if (length == 0.0f)
		return false;
	
	hit.index = -1;
	hit.dist = length;
	
	BLI_bvhtree_ray_cast(data->treedata.tree, start, dir, 0.0f, &hit, data->treedata.raycast_callback, &data->treedata);
	if (hit.index < 0)
		return false;
	
	is_entering = (dot_v3v3(dir, hit.no) < 0.0f);
	if ((hmd->cut_mode & eHaircutCacheModifier_CutMode_Enter && is_entering) ||
	    (hmd->cut_mode & eHaircutCacheModifier_CutMode_Exit && !is_entering))
	{
		if (r_lambda) *r_lambda = len_v3v3(hit.co, start) / length;
		return true;
	}
	
	return false;
}

static bool haircut_find_first_strand_cut(HaircutCacheModifier *hmd, HaircutCacheData *data, StrandChildIterator *it_strand, float (*strand_deform)[3], float *r_cutoff)
{
	StrandChildVertexIterator it_vert;
	int vprev = -1;
	float cutoff = 0.0f;
	
	for (BKE_strand_child_vertex_iter_init(&it_vert, it_strand); BKE_strand_child_vertex_iter_valid(&it_vert); BKE_strand_child_vertex_iter_next(&it_vert)) {
		bool found_cut = false;
		float lambda_min = 1.0f;
		HaircutCacheInstance *inst;
		
		if (it_vert.index == 0) {
			for (inst = data->instances.first; inst; inst = inst->next) {
				/* test root vertex */
				if (haircut_test_point(hmd, data, inst, strand_deform[it_vert.index])) {
					if (r_cutoff) *r_cutoff = 0.0f;
					return true;
				}
			}
		}
		else {
			for (inst = data->instances.first; inst; inst = inst->next) {
				float lambda;
				if (haircut_find_segment_cut(hmd, data, inst, strand_deform[vprev], strand_deform[it_vert.index], &lambda)) {
					found_cut = true;
					if (lambda < lambda_min)
						lambda_min = lambda;
				}
			}
			
			if (found_cut) {
				if (r_cutoff) *r_cutoff = cutoff + lambda_min;
				return true;
			}
		}
		
		cutoff += 1.0f;
		vprev = it_vert.index;
	}
	
	if (r_cutoff) *r_cutoff = -1.0f; /* indicates "no cutoff" */
	return false;
}

static void haircut_apply(HaircutCacheModifier *hmd, CacheProcessContext *ctx, eCacheLibrary_EvalMode eval_mode, HaircutCacheData *data, Strands *parents, StrandsChildren *strands)
{
	StrandChildIterator it_strand;
	bool do_strands_motion, do_strands_children;
	
	/* Note: the child data here is not yet deformed by parents, so the intersections won't be correct.
	 * We deform each strand individually on-the-fly to avoid duplicating memory.
	 */
	int *vertstart = BKE_strands_calc_vertex_start(parents);
	int maxlen = BKE_strands_children_max_length(strands);
	float (*strand_deform)[3] = (float (*)[3])MEM_mallocN(sizeof(float) * 3 * maxlen, "child strand buffer");
	
	BKE_cache_library_get_read_flags(ctx->cachelib, eval_mode, true, &do_strands_motion, &do_strands_children);
	
	for (BKE_strand_child_iter_init(&it_strand, strands); BKE_strand_child_iter_valid(&it_strand); BKE_strand_child_iter_next(&it_strand)) {
		float cutoff = -1.0f;
		
		BKE_strands_children_strand_deform(&it_strand, parents, vertstart, do_strands_motion, strand_deform);
		
		if (haircut_find_first_strand_cut(hmd, data, &it_strand, strand_deform, &cutoff))
			it_strand.curve->cutoff = cutoff;
	}
	
	if (vertstart)
		MEM_freeN(vertstart);
	if (strand_deform)
		MEM_freeN(strand_deform);
}

static void haircut_process(HaircutCacheModifier *hmd, CacheProcessContext *ctx, CacheProcessData *data, int UNUSED(frame), int UNUSED(frame_prev), eCacheLibrary_EvalMode eval_mode)
{
	const bool dupli_target = hmd->flag & eHaircutCacheModifier_Flag_InternalTarget;
	Object *ob = hmd->object;
	DupliObject *dob;
	Strands *parents;
	StrandsChildren *strands;
	DerivedMesh *target_dm;
	float mat[4][4];
	
	HaircutCacheData haircut;
	
	if (!BKE_cache_modifier_find_strands(data->dupcache, ob, hmd->hair_system, NULL, &parents, &strands, NULL))
		return;
	
	if (dupli_target) {
		DupliObjectData *target_data;
		if (!BKE_cache_modifier_find_object(data->dupcache, hmd->target, &target_data))
			return;
		target_dm = target_data->dm;
	}
	else {
		if (!hmd->target)
			return;
		target_dm = mesh_get_derived_final(ctx->scene, hmd->target, CD_MASK_BAREMESH);
	}
	
	for (dob = data->dupcache->duplilist.first; dob; dob = dob->next) {
		if (dob->ob != ob)
			continue;
		
		memset(&haircut, 0, sizeof(haircut));
		haircut_data_get_bvhtree(&haircut, target_dm, true);
		if (dupli_target) {
			/* instances are calculated relative to the strands object */
			invert_m4_m4(mat, dob->mat);
			haircut_data_get_instances(&haircut, hmd->target, mat, &data->dupcache->duplilist);
		}
		else {
			/* instances are calculated relative to the strands object */
			mul_m4_m4m4(mat, data->mat, dob->mat);
			invert_m4(mat);
			haircut_data_get_instances(&haircut, hmd->target, mat, NULL);
		}
		
		haircut_apply(hmd, ctx, eval_mode, &haircut, parents, strands);
		
		haircut_data_free(&haircut);
		
		/* XXX assume a single instance ... otherwise would just overwrite previous strands data */
		break;
	}
}

CacheModifierTypeInfo cacheModifierType_Haircut = {
    /* name */              "Haircut",
    /* structName */        "HaircutCacheModifier",
    /* structSize */        sizeof(HaircutCacheModifier),

    /* copy */              (CacheModifier_CopyFunc)haircut_copy,
    /* foreachIDLink */     (CacheModifier_ForeachIDLinkFunc)haircut_foreach_id_link,
    /* process */           (CacheModifier_ProcessFunc)haircut_process,
    /* init */              (CacheModifier_InitFunc)haircut_init,
    /* free */              (CacheModifier_FreeFunc)haircut_free,
};

/* ------------------------------------------------------------------------- */

bool BKE_cache_library_uses_key(CacheLibrary *cachelib, Key *key)
{
	CacheModifier *md;
	for (md = cachelib->modifiers.first; md; md = md->next) {
		if (md->type == eCacheModifierType_StrandsKey) {
			StrandsKeyCacheModifier *skmd = (StrandsKeyCacheModifier *)md;
			if (skmd->key == key)
				return true;
		}
	}
	return false;
}

void BKE_cache_modifier_init(void)
{
	cache_modifier_type_set(eCacheModifierType_HairSimulation, &cacheModifierType_HairSimulation);
	cache_modifier_type_set(eCacheModifierType_ForceField, &cacheModifierType_ForceField);
	cache_modifier_type_set(eCacheModifierType_ShrinkWrap, &cacheModifierType_ShrinkWrap);
	cache_modifier_type_set(eCacheModifierType_StrandsKey, &cacheModifierType_StrandsKey);
	cache_modifier_type_set(eCacheModifierType_Haircut, &cacheModifierType_Haircut);
}

/* ========================================================================= */

#if 0
static unsigned int hash_combine(unsigned int kx, unsigned int ky)
{
#define rot(x,k) (((x)<<(k)) | ((x)>>(32-(k))))

	unsigned int a, b, c;

	a = b = c = 0xdeadbeef + (2 << 2) + 13;
	a += kx;
	b += ky;

	c ^= b; c -= rot(b,14);
	a ^= c; a -= rot(c,11);
	b ^= a; b -= rot(a,25);
	c ^= b; c -= rot(b,16);
	a ^= c; a -= rot(c,4);
	b ^= a; b -= rot(a,14);
	c ^= b; c -= rot(b,24);

	return c;

#undef rot
}

static unsigned int cache_archive_info_node_hash(const void *key)
{
	const CacheArchiveInfoNode *node = key;
	
	unsigned int hash = hash_combine(BLI_ghashutil_strhash(node->name), BLI_ghashutil_inthash(node->type));
	if (node->parent_hash != 0)
		hash = hash_combine(hash, node->parent_hash);
	return hash;
}

static bool cache_archive_info_node_cmp(const CacheArchiveInfoNode *a, const CacheArchiveInfoNode *b)
{
	if (a->parent_hash != b->parent_hash)
		return true;
	else if (a->type != b->type)
		return true;
	else if (!STREQ(a->name, b->name))
		return true;
	else
		return false;
}
#endif

static void cache_archive_info_node_free(CacheArchiveInfoNode *node)
{
	CacheArchiveInfoNode *child, *child_next;
	for (child = node->child_nodes.first; child; child = child_next) {
		child_next = child->next;
		cache_archive_info_node_free(child);
	}
	
	MEM_freeN(node);
}

CacheArchiveInfo *BKE_cache_archive_info_new(void)
{
	CacheArchiveInfo *info = MEM_callocN(sizeof(CacheArchiveInfo), "cache archive info");
	
	return info;
}

void BKE_cache_archive_info_free(CacheArchiveInfo *info)
{
	if (info) {
		if (info->root_node)
			cache_archive_info_node_free(info->root_node);
		
		MEM_freeN(info);
	}
}

void BKE_cache_archive_info_clear(CacheArchiveInfo *info)
{
	if (info->root_node) {
		cache_archive_info_node_free(info->root_node);
		info->root_node = NULL;
	}
}

CacheArchiveInfoNode *BKE_cache_archive_info_find_node(CacheArchiveInfo *info, CacheArchiveInfoNode *parent,
                                                       eCacheArchiveInfoNode_Type type, const char *name)
{
	if (parent) {
		CacheArchiveInfoNode *child;
		for (child = parent->child_nodes.first; child; child = child->next) {
			if (STREQ(child->name, name) && child->type == type)
				return child;
		}
	}
	else if (info->root_node) {
		if (STREQ(info->root_node->name, name) && info->root_node->type == type)
			return info->root_node;
	}
	return NULL;
}

CacheArchiveInfoNode *BKE_cache_archive_info_add_node(CacheArchiveInfo *info, CacheArchiveInfoNode *parent,
                                                      eCacheArchiveInfoNode_Type type, const char *name)
{
	CacheArchiveInfoNode *node;
	
	BLI_assert(parent || !info->root_node);
	
	node = MEM_callocN(sizeof(CacheArchiveInfoNode), "cache archive info node");
	node->type = type;
	BLI_strncpy(node->name, name, sizeof(node->name));
	
	/* these values are only optionally calculated, -1 indicates unknown */
	node->bytes_size = -1;
	node->array_size = -1;
	
	if (parent)
		BLI_addtail(&parent->child_nodes, node);
	else
		info->root_node = node;
	
	return node;
}
