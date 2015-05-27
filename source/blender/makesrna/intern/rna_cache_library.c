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
 * Contributor(s): Blender Foundation (2015).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_cache_library.c
 *  \ingroup RNA
 */

#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "DNA_cache_library_types.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "WM_types.h"

EnumPropertyItem cache_library_data_type_items[] = {
    {CACHE_TYPE_OBJECT,         "OBJECT",			ICON_OBJECT_DATA,       "Object", "Object base properties"},
    {CACHE_TYPE_DERIVED_MESH,   "DERIVED_MESH",     ICON_OUTLINER_OB_MESH,  "Derived Mesh", "Mesh result from modifiers"},
    {CACHE_TYPE_HAIR,           "HAIR",             ICON_PARTICLE_POINT,    "Hair", "Hair parent strands"},
    {CACHE_TYPE_HAIR_PATHS,     "HAIR_PATHS",       ICON_PARTICLE_PATH,     "Hair Paths", "Full hair paths"},
    {CACHE_TYPE_PARTICLES,      "PARTICLES",        ICON_PARTICLES,         "Particles", "Emitter particles"},
    {0, NULL, 0, NULL, NULL}
};

EnumPropertyItem cache_library_read_result_items[] = {
    {CACHE_READ_SAMPLE_INVALID,         "INVALID",      ICON_ERROR,             "Invalid",      "No valid sample found"},
    {CACHE_READ_SAMPLE_EXACT,           "EXACT",        ICON_SPACE3,            "Exact",        "Found sample for requested frame"},
    {CACHE_READ_SAMPLE_INTERPOLATED,    "INTERPOLATED", ICON_TRIA_DOWN_BAR,     "Interpolated", "Enclosing samples found for interpolation"},
    {CACHE_READ_SAMPLE_EARLY,           "EARLY",        ICON_TRIA_RIGHT_BAR,    "Early",        "Requested frame before the first sample"},
    {CACHE_READ_SAMPLE_LATE,            "LATE",         ICON_TRIA_LEFT_BAR,     "Late",         "Requested frame after the last sample"},
    {0, NULL, 0, NULL, NULL}
};

EnumPropertyItem cache_modifier_type_items[] = {
    {eCacheModifierType_HairSimulation, "HAIR_SIMULATION", ICON_HAIR, "Hair Simulation", ""},
    {eCacheModifierType_ForceField, "FORCE_FIELD", ICON_FORCE_FORCE, "Force Field", ""},
    {eCacheModifierType_ShrinkWrap, "SHRINK_WRAP", ICON_MOD_SHRINKWRAP, "Shrink Wrap", ""},
    {eCacheModifierType_StrandsKey, "STRANDS_KEY", ICON_SHAPEKEY_DATA, "Strands Key", "Shape key for strands"},
    {eCacheModifierType_Haircut, "HAIRCUT", ICON_HAIR, "Hair Cut", "Cut strands where they intersect with an object"},
    {0, NULL, 0, NULL, NULL}
};

#ifdef RNA_RUNTIME

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_string.h"

#include "DNA_key_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"

#include "BKE_animsys.h"
#include "BKE_cache_library.h"
#include "BKE_depsgraph.h"
#include "BKE_main.h"

#include "RNA_access.h"

#include "WM_api.h"

/* ========================================================================= */

static void rna_CacheLibrary_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	CacheLibrary *cachelib = ptr->data;
	DAG_id_tag_update(&cachelib->id, OB_RECALC_DATA);
	WM_main_add_notifier(NC_WINDOW, NULL);
}

static void rna_CacheArchiveInfo_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *UNUSED(ptr))
{
//	CacheLibrary *cachelib = ptr->id.data;
//	WM_main_add_notifier(NC_OBJECT | ND_DRAW, NULL);
}

/* ========================================================================= */

static void rna_CacheModifier_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *UNUSED(ptr))
{
}

#if 0 /* unused */
static void rna_CacheModifier_dependency_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	rna_CacheModifier_update(bmain, scene, ptr);
	DAG_relations_tag_update(bmain);
}
#endif


static StructRNA *rna_CacheModifier_refine(struct PointerRNA *ptr)
{
	CacheModifier *md = (CacheModifier *)ptr->data;

	switch ((eCacheModifier_Type)md->type) {
		case eCacheModifierType_HairSimulation:
			return &RNA_HairSimulationCacheModifier;
		case eCacheModifierType_ForceField:
			return &RNA_ForceFieldCacheModifier;
		case eCacheModifierType_ShrinkWrap:
			return &RNA_ShrinkWrapCacheModifier;
		case eCacheModifierType_StrandsKey:
			return &RNA_StrandsKeyCacheModifier;
		case eCacheModifierType_Haircut:
			return &RNA_HaircutCacheModifier;
			
		/* Default */
		case eCacheModifierType_None:
		case NUM_CACHE_MODIFIER_TYPES:
			return &RNA_CacheLibraryModifier;
	}

	return &RNA_CacheLibraryModifier;
}

static void rna_CacheLibraryModifier_name_set(PointerRNA *ptr, const char *value)
{
	CacheModifier *md = ptr->data;
	char oldname[sizeof(md->name)];
	
	/* make a copy of the old name first */
	BLI_strncpy(oldname, md->name, sizeof(md->name));
	
	/* copy the new name into the name slot */
	BLI_strncpy_utf8(md->name, value, sizeof(md->name));
	
	/* make sure the name is truly unique */
	if (ptr->id.data) {
		CacheLibrary *cachelib = ptr->id.data;
		BKE_cache_modifier_unique_name(&cachelib->modifiers, md);
	}
	
	/* fix all the animation data which may link to this */
	BKE_animdata_fix_paths_rename_all(NULL, "modifiers", oldname, md->name);
}

static char *rna_CacheLibraryModifier_path(PointerRNA *ptr)
{
	CacheModifier *md = ptr->data;
	char name_esc[sizeof(md->name) * 2];

	BLI_strescape(name_esc, md->name, sizeof(name_esc));
	return BLI_sprintfN("modifiers[\"%s\"]", name_esc);
}

static CacheModifier *rna_CacheLibrary_modifier_new(CacheLibrary *cachelib, bContext *UNUSED(C), ReportList *UNUSED(reports),
                                                    const char *name, int type)
{
	return BKE_cache_modifier_add(cachelib, name, type);
}

static void rna_CacheLibrary_modifier_remove(CacheLibrary *cachelib, bContext *UNUSED(C), ReportList *UNUSED(reports), PointerRNA *md_ptr)
{
	CacheModifier *md = md_ptr->data;
	
	BKE_cache_modifier_remove(cachelib, md);

	RNA_POINTER_INVALIDATE(md_ptr);
}

static void rna_CacheLibrary_modifier_clear(CacheLibrary *cachelib, bContext *UNUSED(C))
{
	BKE_cache_modifier_clear(cachelib);
}

/* ------------------------------------------------------------------------- */

static int rna_CacheLibraryModifier_mesh_object_poll(PointerRNA *UNUSED(ptr), PointerRNA value)
{
	/*HairSimCacheModifier *hsmd = ptr->data;*/
	Object *ob = value.data;
	
	return ob->type == OB_MESH && ob->data != NULL;
}

static int rna_CacheLibraryModifier_hair_object_poll(PointerRNA *UNUSED(ptr), PointerRNA value)
{
	/*HairSimCacheModifier *hsmd = ptr->data;*/
	Object *ob = value.data;
	ParticleSystem *psys;
	bool has_hair_system = false;
	
	for (psys = ob->particlesystem.first; psys; psys = psys->next) {
		if (psys->part && psys->part->type == PART_HAIR) {
			has_hair_system = true;
			break;
		}
	}
	return has_hair_system;
}

static PointerRNA rna_HairSimulationCacheModifier_hair_system_get(PointerRNA *ptr)
{
	HairSimCacheModifier *hsmd = ptr->data;
	ParticleSystem *psys = hsmd->object ? BLI_findlink(&hsmd->object->particlesystem, hsmd->hair_system) : NULL;
	PointerRNA value;
	
	RNA_pointer_create(ptr->id.data, &RNA_ParticleSystem, psys, &value);
	return value;
}

static void rna_HairSimulationCacheModifier_hair_system_set(PointerRNA *ptr, PointerRNA value)
{
	HairSimCacheModifier *hsmd = ptr->data;
	ParticleSystem *psys = value.data;
	hsmd->hair_system = hsmd->object ? BLI_findindex(&hsmd->object->particlesystem, psys) : -1;
}

static int rna_HairSimulationCacheModifier_hair_system_poll(PointerRNA *ptr, PointerRNA value)
{
	HairSimCacheModifier *hsmd = ptr->data;
	ParticleSystem *psys = value.data;
	
	if (!hsmd->object)
		return false;
	if (BLI_findindex(&hsmd->object->particlesystem, psys) == -1)
		return false;
	if (!psys->part || psys->part->type != PART_HAIR)
		return false;
	return true;
}

static PointerRNA rna_ShrinkWrapCacheModifier_hair_system_get(PointerRNA *ptr)
{
	ShrinkWrapCacheModifier *smd = ptr->data;
	ParticleSystem *psys = smd->object ? BLI_findlink(&smd->object->particlesystem, smd->hair_system) : NULL;
	PointerRNA value;
	
	RNA_pointer_create(ptr->id.data, &RNA_ParticleSystem, psys, &value);
	return value;
}

static void rna_ShrinkWrapCacheModifier_hair_system_set(PointerRNA *ptr, PointerRNA value)
{
	ShrinkWrapCacheModifier *smd = ptr->data;
	ParticleSystem *psys = value.data;
	smd->hair_system = smd->object ? BLI_findindex(&smd->object->particlesystem, psys) : -1;
}

static int rna_ShrinkWrapCacheModifier_hair_system_poll(PointerRNA *ptr, PointerRNA value)
{
	ShrinkWrapCacheModifier *smd = ptr->data;
	ParticleSystem *psys = value.data;
	
	if (!smd->object)
		return false;
	if (BLI_findindex(&smd->object->particlesystem, psys) == -1)
		return false;
	if (!psys->part || psys->part->type != PART_HAIR)
		return false;
	return true;
}

static PointerRNA rna_StrandsKeyCacheModifier_hair_system_get(PointerRNA *ptr)
{
	StrandsKeyCacheModifier *skmd = ptr->data;
	ParticleSystem *psys = skmd->object ? BLI_findlink(&skmd->object->particlesystem, skmd->hair_system) : NULL;
	PointerRNA value;
	
	RNA_pointer_create(ptr->id.data, &RNA_ParticleSystem, psys, &value);
	return value;
}

static void rna_StrandsKeyCacheModifier_hair_system_set(PointerRNA *ptr, PointerRNA value)
{
	StrandsKeyCacheModifier *skmd = ptr->data;
	ParticleSystem *psys = value.data;
	skmd->hair_system = skmd->object ? BLI_findindex(&skmd->object->particlesystem, psys) : -1;
}

static int rna_StrandsKeyCacheModifier_hair_system_poll(PointerRNA *ptr, PointerRNA value)
{
	StrandsKeyCacheModifier *skmd = ptr->data;
	ParticleSystem *psys = value.data;
	
	if (!skmd->object)
		return false;
	if (BLI_findindex(&skmd->object->particlesystem, psys) == -1)
		return false;
	if (!psys->part || psys->part->type != PART_HAIR)
		return false;
	return true;
}

static void rna_StrandsKeyCacheModifier_active_shape_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
#if 0 // TODO
	StrandsKeyCacheModifier *skmd = ptr->data;
	
	if (scene->obedit == ob) {
		/* exit/enter editmode to get new shape */
		switch (ob->type) {
			case OB_MESH:
				EDBM_mesh_load(ob);
				EDBM_mesh_make(scene->toolsettings, ob);
				EDBM_mesh_normals_update(((Mesh *)ob->data)->edit_btmesh);
				BKE_editmesh_tessface_calc(((Mesh *)ob->data)->edit_btmesh);
				break;
			case OB_CURVE:
			case OB_SURF:
				load_editNurb(ob);
				make_editNurb(ob);
				break;
			case OB_LATTICE:
				load_editLatt(ob);
				make_editLatt(ob);
				break;
		}
	}
#endif
	
	rna_CacheModifier_update(bmain, scene, ptr);
}

static void rna_StrandsKeyCacheModifier_active_shape_key_index_range(PointerRNA *ptr, int *min, int *max, int *UNUSED(softmin), int *UNUSED(softmax))
{
	StrandsKeyCacheModifier *skmd = ptr->data;
	Key *key = skmd->key;

	*min = 0;
	if (key) {
		*max = BLI_listbase_count(&key->block) - 1;
		if (*max < 0) *max = 0;
	}
	else {
		*max = 0;
	}
}

static int rna_StrandsKeyCacheModifier_active_shape_key_index_get(PointerRNA *ptr)
{
	StrandsKeyCacheModifier *skmd = ptr->data;

	return max_ii(skmd->shapenr - 1, 0);
}

static void rna_StrandsKeyCacheModifier_active_shape_key_index_set(PointerRNA *ptr, int value)
{
	StrandsKeyCacheModifier *skmd = ptr->data;

	skmd->shapenr = value + 1;
}

static PointerRNA rna_StrandsKeyCacheModifier_active_shape_key_get(PointerRNA *ptr)
{
	StrandsKeyCacheModifier *skmd = ptr->data;
	Key *key = skmd->key;
	KeyBlock *kb;
	PointerRNA keyptr;

	if (key == NULL)
		return PointerRNA_NULL;
	
	kb = BLI_findlink(&key->block, skmd->shapenr - 1);
	RNA_pointer_create((ID *)key, &RNA_ShapeKey, kb, &keyptr);
	return keyptr;
}

static PointerRNA rna_HaircutCacheModifier_hair_system_get(PointerRNA *ptr)
{
	HaircutCacheModifier *hmd = ptr->data;
	ParticleSystem *psys = hmd->object ? BLI_findlink(&hmd->object->particlesystem, hmd->hair_system) : NULL;
	PointerRNA value;
	
	RNA_pointer_create(ptr->id.data, &RNA_ParticleSystem, psys, &value);
	return value;
}

static void rna_HaircutCacheModifier_hair_system_set(PointerRNA *ptr, PointerRNA value)
{
	HaircutCacheModifier *hmd = ptr->data;
	ParticleSystem *psys = value.data;
	hmd->hair_system = hmd->object ? BLI_findindex(&hmd->object->particlesystem, psys) : -1;
}

static int rna_HaircutCacheModifier_hair_system_poll(PointerRNA *ptr, PointerRNA value)
{
	HaircutCacheModifier *hmd = ptr->data;
	ParticleSystem *psys = value.data;
	
	if (!hmd->object)
		return false;
	if (BLI_findindex(&hmd->object->particlesystem, psys) == -1)
		return false;
	if (!psys->part || psys->part->type != PART_HAIR)
		return false;
	return true;
}

static void rna_CacheArchiveInfoNode_bytes_size_get(PointerRNA *ptr, char *value)
{
	CacheArchiveInfoNode *node = ptr->data;
	BLI_snprintf(value, MAX_NAME, "%lld", (long long int)node->bytes_size);
}

static int rna_CacheArchiveInfoNode_bytes_size_length(PointerRNA *ptr)
{
	char buf[MAX_NAME];
	/* theoretically could do a dummy BLI_snprintf here, but BLI does not allow NULL buffer ... */
	CacheArchiveInfoNode *node = ptr->data;
	return BLI_snprintf(buf, sizeof(buf), "%lld", (long long int)node->bytes_size);
}

#else

static void rna_def_hair_sim_params(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "HairSimulationParameters", NULL);
	RNA_def_struct_sdna(srna, "HairSimParams");
	RNA_def_struct_ui_text(srna, "Hair Simulation Parameters", "Simulation parameters for hair simulation");
	RNA_def_struct_ui_icon(srna, ICON_HAIR);
	
	prop = RNA_def_property(srna, "timescale", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 10.0f, 0.1f, 3);
	RNA_def_property_ui_text(prop, "Time Scale", "Simulation time scale relative to scene time");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "substeps", PROP_INT, PROP_NONE);
	RNA_def_property_range(prop, 1, 80);
	RNA_def_property_ui_text(prop, "Substeps", "Simulation steps per frame");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "effector_weights", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "EffectorWeights");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Effector Weights", "");
	
	prop = RNA_def_property(srna, "mass", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Mass", "Mass of hair vertices");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "drag", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 10.0f, 0.1f, 3);
	RNA_def_property_ui_text(prop, "Drag", "Drag simulating friction with surrounding air");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "goal_stiffness", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 10.0f, 0.1f, 3);
	RNA_def_property_float_default(prop, 0.0f);
	RNA_def_property_ui_text(prop, "Goal Strength", "Goal spring, pulling vertices toward their rest position");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "goal_damping", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.1f, 3);
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_ui_text(prop, "Goal Damping", "Damping factor of goal springs");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "use_goal_stiffness_curve", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", eHairSimParams_Flag_UseGoalStiffnessCurve);
	RNA_def_property_ui_text(prop, "Use Goal Stiffness Curve", "Use a curve to define goal stiffness along the strand");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "goal_stiffness_curve", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "goal_stiffness_mapping");
	RNA_def_property_struct_type(prop, "CurveMapping");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Goal Stiffness Curve", "Stiffness of goal springs along the strand curves");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "use_goal_deflect", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", eHairSimParams_Flag_UseGoalDeflect);
	RNA_def_property_ui_text(prop, "Use Goal Deflect", "Disable goal springs inside deflectors, to avoid unstable deformations");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "use_bend_stiffness_curve", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", eHairSimParams_Flag_UseBendStiffnessCurve);
	RNA_def_property_ui_text(prop, "Use Bend Stiffness Curve", "Use a curve to define bend resistance along the strand");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "bend_stiffness_curve", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "bend_stiffness_mapping");
	RNA_def_property_struct_type(prop, "CurveMapping");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Bend Stiffness Curve", "Resistance to bending along the strand curves");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "stretch_stiffness", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 10000.0f, 0.1f, 3);
	RNA_def_property_float_default(prop, 10000.0f);
	RNA_def_property_ui_text(prop, "Stretch Stiffness", "Resistance to stretching");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "stretch_damping", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.1f, 3);
	RNA_def_property_float_default(prop, 0.1f);
	RNA_def_property_ui_text(prop, "Stretch Damping", "Damping factor of stretch springs");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "bend_stiffness", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 1000.0f, 0.1f, 3);
	RNA_def_property_float_default(prop, 100.0f);
	RNA_def_property_ui_text(prop, "Bend Stiffness", "Resistance to bending of the rest shape");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "bend_damping", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.1f, 3);
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_ui_text(prop, "Bend Damping", "Damping factor of bending springs");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
}

static void rna_def_cache_modifier_hair_simulation(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	rna_def_hair_sim_params(brna);
	
	srna = RNA_def_struct(brna, "HairSimulationCacheModifier", "CacheLibraryModifier");
	RNA_def_struct_sdna(srna, "HairSimCacheModifier");
	RNA_def_struct_ui_text(srna, "Hair Simulation Cache Modifier", "Apply hair dynamics simulation to the cache");
	RNA_def_struct_ui_icon(srna, ICON_HAIR);
	
	prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "object");
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_pointer_funcs(prop, NULL, NULL, NULL, "rna_CacheLibraryModifier_hair_object_poll");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Object", "Object whose cache to simulate");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "hair_system_index", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "hair_system");
	RNA_def_property_ui_text(prop, "Hair System Index", "Hair system cache to simulate");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "hair_system", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_funcs(prop, "rna_HairSimulationCacheModifier_hair_system_get", "rna_HairSimulationCacheModifier_hair_system_set", NULL, "rna_HairSimulationCacheModifier_hair_system_poll");
	RNA_def_property_struct_type(prop, "ParticleSystem");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Hair System", "Hair system cache to simulate");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "parameters", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "sim_params");
	RNA_def_property_struct_type(prop, "HairSimulationParameters");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Simulation Parameters", "Parameters of the simulation");
}

static void rna_def_cache_modifier_force_field(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem force_type_items[] = {
	    {eForceFieldCacheModifier_Type_Deflect, "DEFLECT", ICON_FORCE_FORCE, "Deflect", "Push away from the surface"},
	    {eForceFieldCacheModifier_Type_Drag, "DRAG", ICON_FORCE_DRAG, "Drag", "Adjust velocity to the surface"},
	    {0, NULL, 0, NULL, NULL}
	};
	
	srna = RNA_def_struct(brna, "ForceFieldCacheModifier", "CacheLibraryModifier");
	RNA_def_struct_sdna(srna, "ForceFieldCacheModifier");
	RNA_def_struct_ui_text(srna, "Force Field Cache Modifier", "Use an object as a force field");
	RNA_def_struct_ui_icon(srna, ICON_FORCE_FORCE);
	
	prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "object");
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_pointer_funcs(prop, NULL, NULL, NULL, "rna_CacheLibraryModifier_mesh_object_poll");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Object", "Object whose cache to simulate");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "force_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, force_type_items);
	RNA_def_property_ui_text(prop, "Force Type", "Type of force field");
	
	prop = RNA_def_property(srna, "strength", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_range(prop, -10000.0f, 10000.0f, 0.1, 3);
	RNA_def_property_ui_text(prop, "Strength", "");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "falloff", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 10.0f, 0.1, 3);
	RNA_def_property_ui_text(prop, "Falloff", "");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "min_distance", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_range(prop, -100.0f, 100.0f, 0.1, 4);
	RNA_def_property_ui_text(prop, "Minimum Distance", "");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "max_distance", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_range(prop, -100.0f, 100.0f, 0.1, 4);
	RNA_def_property_ui_text(prop, "Maximum Distance", "");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "use_double_sided", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", eForceFieldCacheModifier_Flag_DoubleSided);
	RNA_def_property_ui_text(prop, "Use Double Sided", "");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
}

static void rna_def_cache_modifier_shrink_wrap(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "ShrinkWrapCacheModifier", "CacheLibraryModifier");
	RNA_def_struct_sdna(srna, "ShrinkWrapCacheModifier");
	RNA_def_struct_ui_text(srna, "Shrink Wrap Cache Modifier", "");
	RNA_def_struct_ui_icon(srna, ICON_HAIR);
	
	prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "object");
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_pointer_funcs(prop, NULL, NULL, NULL, "rna_CacheLibraryModifier_hair_object_poll");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Object", "Object whose cache to simulate");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "hair_system_index", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "hair_system");
	RNA_def_property_ui_text(prop, "Hair System Index", "Hair system cache to simulate");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "hair_system", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_funcs(prop, "rna_ShrinkWrapCacheModifier_hair_system_get", "rna_ShrinkWrapCacheModifier_hair_system_set", NULL, "rna_ShrinkWrapCacheModifier_hair_system_poll");
	RNA_def_property_struct_type(prop, "ParticleSystem");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Hair System", "Hair system cache to simulate");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "target");
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Target", "Mesh object to wrap onto");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
}

static void rna_def_cache_modifier_strands_key(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "StrandsKeyCacheModifier", "CacheLibraryModifier");
	RNA_def_struct_sdna(srna, "StrandsKeyCacheModifier");
	RNA_def_struct_ui_text(srna, "Strands Key Cache Modifier", "");
	RNA_def_struct_ui_icon(srna, ICON_SHAPEKEY_DATA);
	
	prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "object");
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_pointer_funcs(prop, NULL, NULL, NULL, "rna_CacheLibraryModifier_hair_object_poll");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Object", "Object whose cache to simulate");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "hair_system_index", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "hair_system");
	RNA_def_property_ui_text(prop, "Hair System Index", "Hair system cache to simulate");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "hair_system", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_funcs(prop, "rna_StrandsKeyCacheModifier_hair_system_get", "rna_StrandsKeyCacheModifier_hair_system_set", NULL, "rna_StrandsKeyCacheModifier_hair_system_poll");
	RNA_def_property_struct_type(prop, "ParticleSystem");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Hair System", "Hair system cache to simulate");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	/* shape keys */
	prop = RNA_def_property(srna, "shape_keys", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "key");
	RNA_def_property_ui_text(prop, "Shape Keys", "");
	
	prop = RNA_def_property(srna, "use_motion_state", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", eStrandsKeyCacheModifier_Flag_UseMotionState);
	RNA_def_property_ui_text(prop, "Use Motion State", "Apply the shape key to the motion state instead of the base shape");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "show_only_shape_key", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", eStrandsKeyCacheModifier_Flag_ShapeLock);
	RNA_def_property_ui_text(prop, "Shape Key Lock", "Always show the current Shape for this Object");
	RNA_def_property_ui_icon(prop, ICON_UNPINNED, 1);
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "active_shape_key", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "ShapeKey");
	RNA_def_property_pointer_funcs(prop, "rna_StrandsKeyCacheModifier_active_shape_key_get", NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Active Shape Key", "Current shape key");
	
	prop = RNA_def_property(srna, "active_shape_key_index", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "shapenr");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE); /* XXX this is really unpredictable... */
	RNA_def_property_int_funcs(prop, "rna_StrandsKeyCacheModifier_active_shape_key_index_get", "rna_StrandsKeyCacheModifier_active_shape_key_index_set",
	                           "rna_StrandsKeyCacheModifier_active_shape_key_index_range");
	RNA_def_property_ui_text(prop, "Active Shape Key Index", "Current shape key index");
	RNA_def_property_update(prop, 0, "rna_StrandsKeyCacheModifier_active_shape_update");
}

static void rna_def_cache_modifier_haircut(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem cut_mode_items[] = {
	    {eHaircutCacheModifier_CutMode_Enter, "ENTER", 0, "Enter", "Cut strands when entering the target mesh"},
	    {eHaircutCacheModifier_CutMode_Exit, "EXIT", 0, "Exit", "Cut strands when exiting the target mesh"},
	    {0, NULL, 0, NULL, NULL}
	};
	
	srna = RNA_def_struct(brna, "HaircutCacheModifier", "CacheLibraryModifier");
	RNA_def_struct_sdna(srna, "HaircutCacheModifier");
	RNA_def_struct_ui_text(srna, "Hair Cut Cache Modifier", "");
	RNA_def_struct_ui_icon(srna, ICON_HAIR);
	
	prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "object");
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_pointer_funcs(prop, NULL, NULL, NULL, "rna_CacheLibraryModifier_hair_object_poll");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Object", "Object whose cache to simulate");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "hair_system_index", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "hair_system");
	RNA_def_property_ui_text(prop, "Hair System Index", "Hair system cache to simulate");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "hair_system", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_funcs(prop, "rna_HaircutCacheModifier_hair_system_get", "rna_HaircutCacheModifier_hair_system_set", NULL, "rna_HaircutCacheModifier_hair_system_poll");
	RNA_def_property_struct_type(prop, "ParticleSystem");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Hair System", "Hair system cache to simulate");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "target");
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Target", "Mesh object to wrap onto");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "use_internal_target", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", eHaircutCacheModifier_Flag_InternalTarget);
	RNA_def_property_ui_text(prop, "Use Internal Target", "Use a cached object from the group instead of an object in the scene");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "cut_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "cut_mode");
	RNA_def_property_enum_items(prop, cut_mode_items);
	RNA_def_property_flag(prop, PROP_ENUM_FLAG);
	RNA_def_property_ui_text(prop, "Cut Mode", "When to cut strands with the target");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
}

static void rna_def_cache_modifier(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "CacheLibraryModifier", NULL);
	RNA_def_struct_sdna(srna, "CacheModifier");
	RNA_def_struct_path_func(srna, "rna_CacheLibraryModifier_path");
	RNA_def_struct_refine_func(srna, "rna_CacheModifier_refine");
	RNA_def_struct_ui_text(srna, "Cache Modifier", "Cache Modifier");
	RNA_def_struct_ui_icon(srna, ICON_PHYSICS);
	
	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, cache_modifier_type_items);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Type", "Type of the cache modifier");
	
	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_CacheLibraryModifier_name_set");
	RNA_def_property_ui_text(prop, "Name", "Modifier name");
	RNA_def_property_update(prop, NC_ID | NA_RENAME, NULL);
	RNA_def_struct_name_property(srna, prop);
	
	rna_def_cache_modifier_hair_simulation(brna);
	rna_def_cache_modifier_force_field(brna);
	rna_def_cache_modifier_shrink_wrap(brna);
	rna_def_cache_modifier_strands_key(brna);
	rna_def_cache_modifier_haircut(brna);
}

static void rna_def_cache_library_modifiers(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *parm;
	
	RNA_def_property_srna(cprop, "CacheLibraryModifiers");
	srna = RNA_def_struct(brna, "CacheLibraryModifiers", NULL);
	RNA_def_struct_sdna(srna, "CacheLibrary");
	RNA_def_struct_ui_text(srna, "Cache Modifiers", "Collection of cache modifiers");
	
	/* add modifier */
	func = RNA_def_function(srna, "new", "rna_CacheLibrary_modifier_new");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
	RNA_def_function_ui_description(func, "Add a new modifier");
	parm = RNA_def_string(func, "name", "Name", 0, "", "New name for the modifier");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	/* modifier to add */
	parm = RNA_def_enum(func, "type", cache_modifier_type_items, 1, "", "Modifier type to add");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	/* return type */
	parm = RNA_def_pointer(func, "modifier", "CacheLibraryModifier", "", "Newly created modifier");
	RNA_def_function_return(func, parm);
	
	/* remove modifier */
	func = RNA_def_function(srna, "remove", "rna_CacheLibrary_modifier_remove");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
	RNA_def_function_ui_description(func, "Remove an existing modifier");
	/* modifier to remove */
	parm = RNA_def_pointer(func, "modifier", "CacheLibraryModifier", "", "Modifier to remove");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL | PROP_RNAPTR);
	RNA_def_property_clear_flag(parm, PROP_THICK_WRAP);
	
	/* clear all modifiers */
	func = RNA_def_function(srna, "clear", "rna_CacheLibrary_modifier_clear");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	RNA_def_function_ui_description(func, "Remove all modifiers");
}

static void rna_def_cache_library(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem source_mode_items[] = {
	    {CACHE_LIBRARY_SOURCE_SCENE,    "SCENE",        0,      "Scene",        "Use generated scene data as source"},
	    {CACHE_LIBRARY_SOURCE_CACHE,    "CACHE",        0,      "Cache",        "Use cache data as source"},
	    {0, NULL, 0, NULL, NULL}
	};
	
	static EnumPropertyItem display_mode_items[] = {
	    {CACHE_LIBRARY_DISPLAY_SOURCE,  "SOURCE",       0,      "Source",       "Display source data unmodified"},
	    {CACHE_LIBRARY_DISPLAY_MODIFIERS, "MODIFIERS",  0,      "Modifiers",    "Display source data with modifiers applied"},
	    {CACHE_LIBRARY_DISPLAY_RESULT,  "RESULT",       0,      "Result",       "Display resulting data"},
	    {0, NULL, 0, NULL, NULL}
	};
	
	srna = RNA_def_struct(brna, "CacheLibrary", "ID");
	RNA_def_struct_ui_text(srna, "Cache Library", "Cache Library datablock for constructing an archive of caches");
	RNA_def_struct_ui_icon(srna, ICON_PHYSICS);
	
	prop = RNA_def_property(srna, "input_filepath", PROP_STRING, PROP_FILEPATH);
	RNA_def_property_string_sdna(prop, NULL, "input_filepath");
	RNA_def_property_ui_text(prop, "Input File Path", "Path to a cache archive for reading input");
	RNA_def_property_update(prop, 0, "rna_CacheLibrary_update");
	
	prop = RNA_def_property(srna, "output_filepath", PROP_STRING, PROP_FILEPATH);
	RNA_def_property_string_sdna(prop, NULL, "output_filepath");
	RNA_def_property_ui_text(prop, "Output File Path", "Path where cache output is written");
	RNA_def_property_update(prop, 0, "rna_CacheLibrary_update");
	
	prop = RNA_def_property(srna, "source_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "source_mode");
	RNA_def_property_enum_items(prop, source_mode_items);
	RNA_def_property_ui_text(prop, "Source Mode", "Source of the cache library data");
	RNA_def_property_update(prop, 0, "rna_CacheLibrary_update");
	
	prop = RNA_def_property(srna, "display_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "display_mode");
	RNA_def_property_enum_items(prop, display_mode_items);
	RNA_def_property_ui_text(prop, "Display Mode", "What data to display in the viewport");
	RNA_def_property_update(prop, 0, "rna_CacheLibrary_update");
	
	prop = RNA_def_property(srna, "display_motion", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "display_flag", CACHE_LIBRARY_DISPLAY_MOTION);
	RNA_def_property_ui_text(prop, "Display Motion", "Display motion state result from simulation, if available");
	RNA_def_property_update(prop, 0, "rna_CacheLibrary_update");
	
	prop = RNA_def_property(srna, "display_children", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "display_flag", CACHE_LIBRARY_DISPLAY_CHILDREN);
	RNA_def_property_ui_text(prop, "Display Children", "Display child strands, if available");
	RNA_def_property_update(prop, 0, "rna_CacheLibrary_update");
	
	prop = RNA_def_property(srna, "data_types", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "data_types");
	RNA_def_property_enum_items(prop, cache_library_data_type_items);
	RNA_def_property_flag(prop, PROP_ENUM_FLAG);
	RNA_def_property_ui_text(prop, "Data Types", "Types of data to store in the cache");
	RNA_def_property_update(prop, 0, "rna_CacheLibrary_update");
	
	prop = RNA_def_property(srna, "filter_group", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "filter_group");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Filter Group", "If set, only objects in this group will be cached");
	RNA_def_property_update(prop, 0, "rna_CacheLibrary_update");
	
	/* modifiers */
	prop = RNA_def_property(srna, "modifiers", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "CacheLibraryModifier");
	RNA_def_property_ui_text(prop, "Modifiers", "Modifiers applying to the cached data");
	rna_def_cache_library_modifiers(brna, prop);
	
	prop = RNA_def_property(srna, "archive_info", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "CacheArchiveInfo");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Archive Info", "Information about structure and contents of the archive");
}

static void rna_def_cache_archive_info_node(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem type_items[] = {
	    {eCacheArchiveInfoNode_Type_Object, "OBJECT", 0, "Object", "Structural object node forming the hierarchy"},
	    {eCacheArchiveInfoNode_Type_ScalarProperty, "SCALAR_PROPERTY", 0, "Scalar Property", "Property with a single value per sample"},
	    {eCacheArchiveInfoNode_Type_ArrayProperty, "ARRAY_PROPERTY", 0, "Array Property", "Array property with an arbitrary number of values per sample"},
	    {eCacheArchiveInfoNode_Type_CompoundProperty, "COMPOUND_PROPERTY", 0, "Compound Property", "Compound property containing other properties"},
	    {0, NULL, 0, NULL, NULL}
	};
	
	srna = RNA_def_struct(brna, "CacheArchiveInfoNode", NULL);
	RNA_def_struct_ui_text(srna, "Cache Archive Info Node", "Node in the structure of a cache archive");
	
	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, type_items);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Type", "Type of archive node");
	
	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Name", "Name of the archive node");
	RNA_def_struct_name_property(srna, prop);
	
	prop = RNA_def_property(srna, "child_nodes", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "CacheArchiveInfoNode");
	RNA_def_property_ui_text(prop, "Child Nodes", "Nested archive nodes");
	
	prop = RNA_def_property(srna, "expand", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", eCacheArchiveInfoNode_Flag_Expand);
	RNA_def_property_ui_text(prop, "Expand", "Show contents of the node");
	RNA_def_property_update(prop, 0, "rna_CacheArchiveInfo_update");
	
	/* XXX this is a 64bit integer, not supported nicely by RNA,
	 * but string encoding is sufficient for feedback
	 */
	prop = RNA_def_property(srna, "bytes_size", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_CacheArchiveInfoNode_bytes_size_get", "rna_CacheArchiveInfoNode_bytes_size_length", NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Bytes Size", "Overall size of the node data in bytes");
	
	prop = RNA_def_property(srna, "datatype", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "datatype_name");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Datatype", "Type of values stored in the property");
	
	prop = RNA_def_property(srna, "datatype_extent", PROP_INT, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Datatype Extent", "Array extent of a single data element");
	
	prop = RNA_def_property(srna, "samples", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "num_samples");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Samples", "Number of samples stored for the property");
	
	prop = RNA_def_property(srna, "array_size", PROP_INT, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Array Size", "Maximum array size for any sample of the property");
}

static void rna_def_cache_archive_info(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "CacheArchiveInfo", NULL);
	RNA_def_struct_ui_text(srna, "Cache Archive Info", "Information about structure and contents of a cache file");
	
	prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_FILEPATH);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "File Path", "Path to the cache archive");
	
	prop = RNA_def_property(srna, "root_node", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "CacheArchiveInfoNode");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Root Node", "Root node of the archive");
}

void RNA_def_cache_library(BlenderRNA *brna)
{
	rna_def_cache_modifier(brna);
	rna_def_cache_library(brna);
	rna_def_cache_archive_info_node(brna);
	rna_def_cache_archive_info(brna);
}

#endif
