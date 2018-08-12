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
 * Contributor(s): Blender Foundation.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_hair.c
 *  \ingroup RNA
 */

#include <stdlib.h>

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "DNA_hair_types.h"

#include "WM_types.h"

#ifdef RNA_RUNTIME

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

#include "BKE_context.h"
#include "BKE_hair.h"
#include "BKE_main.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

static void UNUSED_FUNCTION(rna_HairSystem_update)(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	DEG_id_tag_update(ptr->id.data, OB_RECALC_DATA);
}

static void rna_HairDrawSettings_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
#if 0
	/* XXX Only need to update render engines
	 * However, that requires finding all hair systems using these draw settings,
	 * then flagging the cache as dirty.
	 */
	BKE_hair_batch_cache_dirty(hsys, BKE_HAIR_BATCH_DIRTY_ALL);
#else
	DEG_id_tag_update(ptr->id.data, OB_RECALC_DATA);
#endif
	WM_main_add_notifier(NC_OBJECT | ND_DRAW, ptr->id.data);
}

static void rna_HairSystem_generate_follicles(
        HairSystem *hsys,
        struct bContext *C,
        Object *scalp,
        int seed,
        int count)
{
	if (!scalp)
	{
		return;
	}
	
	struct Depsgraph *depsgraph = CTX_data_depsgraph(C);
	
	BLI_assert(scalp && scalp->type == OB_MESH);
	Mesh *scalp_mesh = (Mesh *)DEG_get_evaluated_id(depsgraph, scalp->data);
	
	BKE_hair_generate_follicles(hsys, scalp_mesh, (unsigned int)seed, count);
}

#else

static void rna_def_hair_follicle(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "HairFollicle", NULL);
	RNA_def_struct_ui_text(srna, "Hair Follicle", "Single follicle on a surface");
	RNA_def_struct_sdna(srna, "HairFollicle");
	
	prop = RNA_def_property(srna, "mesh_sample", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "MeshSample");
}

static void rna_def_hair_pattern(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "HairPattern", NULL);
	RNA_def_struct_ui_text(srna, "Hair Pattern", "Set of hair follicles distributed on a surface");
	RNA_def_struct_sdna(srna, "HairPattern");
	RNA_def_struct_ui_icon(srna, ICON_STRANDS);
	
	prop = RNA_def_property(srna, "follicles", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "follicles", "num_follicles");
	RNA_def_property_struct_type(prop, "HairFollicle");
	RNA_def_property_ui_text(prop, "Follicles", "Hair fiber follicles");
}

static void rna_def_hair_system(BlenderRNA *brna)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *prop, *parm;
	
	srna = RNA_def_struct(brna, "HairSystem", NULL);
	RNA_def_struct_ui_text(srna, "Hair System", "Hair rendering and deformation data");
	RNA_def_struct_sdna(srna, "HairSystem");
	RNA_def_struct_ui_icon(srna, ICON_STRANDS);
	
	prop = RNA_def_property(srna, "pattern", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "HairPattern");
	RNA_def_property_ui_text(prop, "Pattern", "Hair pattern");
	
	func = RNA_def_function(srna, "generate_follicles", "rna_HairSystem_generate_follicles");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	parm = RNA_def_pointer(func, "scalp", "Object", "Scalp", "Scalp object on which to place hair follicles");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	parm = RNA_def_int(func, "seed", 0, 0, INT_MAX, "Seed", "Seed value for random numbers", 0, INT_MAX);
	parm = RNA_def_int(func, "count", 0, 0, INT_MAX, "Count", "Maximum number of follicles to generate", 1, 1e5);
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
}

static void rna_def_hair_draw_settings(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static const EnumPropertyItem follicle_mode_items[] = {
	    {HAIR_DRAW_FOLLICLE_NONE, "NONE", 0, "None", ""},
	    {HAIR_DRAW_FOLLICLE_POINTS, "POINTS", 0, "Points", "Draw a point for each follicle"},
	    {0, NULL, 0, NULL, NULL}
	};
	
	static const EnumPropertyItem fiber_mode_items[] = {
	    {HAIR_DRAW_FIBER_NONE, "NONE", 0, "None", ""},
	    {HAIR_DRAW_FIBER_CURVES, "CURVES", 0, "Curves", "Draw fiber curves"},
	    {0, NULL, 0, NULL, NULL}
	};
	
	srna = RNA_def_struct(brna, "HairDrawSettings", NULL);
	RNA_def_struct_ui_text(srna, "Hair Draw Settings", "Settings for drawing hair systems");
	RNA_def_struct_sdna(srna, "HairDrawSettings");
	
	prop = RNA_def_property(srna, "follicle_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, follicle_mode_items);
	RNA_def_property_ui_text(prop, "Follicle Mode", "Draw follicles on the scalp surface");
	RNA_def_property_update(prop, 0, "rna_HairDrawSettings_update");
	
	prop = RNA_def_property(srna, "fiber_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, fiber_mode_items);
	RNA_def_property_ui_text(prop, "Fiber Mode", "Draw fiber curves");
	RNA_def_property_update(prop, 0, "rna_HairDrawSettings_update");
	
	/* hair shape */
	prop = RNA_def_property(srna, "use_close_tip", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "shape_flag", HAIR_DRAW_CLOSE_TIP);
	RNA_def_property_ui_text(prop, "Close Tip", "Set tip radius to zero");
	RNA_def_property_update(prop, 0, "rna_HairDrawSettings_update");

	prop = RNA_def_property(srna, "shape", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_range(prop, -1.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Shape", "Strand shape parameter");
	RNA_def_property_update(prop, 0, "rna_HairDrawSettings_update");

	prop = RNA_def_property(srna, "root_radius", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 10.0f, 0.1, 2);
	RNA_def_property_ui_text(prop, "Root", "Strand width at the root");
	RNA_def_property_update(prop, 0, "rna_HairDrawSettings_update");

	prop = RNA_def_property(srna, "tip_radius", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 10.0f, 0.1, 2);
	RNA_def_property_ui_text(prop, "Tip", "Strand width at the tip");
	RNA_def_property_update(prop, 0, "rna_HairDrawSettings_update");

	prop = RNA_def_property(srna, "radius_scale", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 10.0f, 0.1, 2);
	RNA_def_property_ui_text(prop, "Scaling", "Multiplier of radius properties");
	RNA_def_property_update(prop, 0, "rna_HairDrawSettings_update");
}

void RNA_def_hair(BlenderRNA *brna)
{
	rna_def_hair_follicle(brna);
	rna_def_hair_pattern(brna);
	rna_def_hair_system(brna);
	rna_def_hair_draw_settings(brna);
}

#endif
