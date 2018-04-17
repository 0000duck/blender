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
 * The Original Code is Copyright (C) 2008, Blender Foundation, Joshua Leung
 * This is a new part of Blender
 *
 * Contributor(s): Joshua Leung, Antonio Vazquez
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * Operators for dealing with GP datablocks and layers
 */

/** \file blender/editors/gpencil/gpencil_data.c
 *  \ingroup edgpencil
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_ghash.h"
#include "BLI_math.h"
#include "BLI_string_utils.h"

#include "BLT_translation.h"

#include "DNA_anim_types.h"
#include "DNA_brush_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"

#include "BKE_main.h"
#include "BKE_brush.h"
#include "BKE_animsys.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_fcurve.h"
#include "BKE_global.h"
#include "BKE_gpencil.h"
#include "BKE_library.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "ED_object.h"
#include "ED_gpencil.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "gpencil_intern.h"

/* ************************************************ */
/* Datablock Operators */

/* ******************* Add New Data ************************ */

/* add new datablock - wrapper around API */
static int gp_data_add_exec(bContext *C, wmOperator *op)
{
	bGPdata **gpd_ptr = ED_gpencil_data_get_pointers(C, NULL);

	if (gpd_ptr == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Nowhere for grease pencil data to go");
		return OPERATOR_CANCELLED;
	}
	else {
		/* decrement user count and add new datablock */
		/* TODO: if a datablock exists, we should make a copy of it instead of starting fresh (as in other areas) */
		Main *bmain = CTX_data_main(C);
		bGPdata *gpd = (*gpd_ptr);
		
		id_us_min((ID *)gpd);
		*gpd_ptr = BKE_gpencil_data_addnew(bmain, DATA_("GPencil"));

		/* add default sets of colors and brushes */
		ED_gpencil_add_defaults(C);
	}
	
	/* notifiers */
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_data_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Grease Pencil Add New";
	ot->idname = "GPENCIL_OT_data_add";
	ot->description = "Add new Grease Pencil data-block";
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* callbacks */
	ot->exec = gp_data_add_exec;
	ot->poll = gp_add_poll;
}

/* ******************* Unlink Data ************************ */

/* poll callback for adding data/layers - special */
static int gp_data_unlink_poll(bContext *C)
{
	bGPdata **gpd_ptr = ED_gpencil_data_get_pointers(C, NULL);
	
	/* if we have access to some active data, make sure there's a datablock before enabling this */
	return (gpd_ptr && *gpd_ptr);
}


/* unlink datablock - wrapper around API */
static int gp_data_unlink_exec(bContext *C, wmOperator *op)
{
	bGPdata **gpd_ptr = ED_gpencil_data_get_pointers(C, NULL);
	
	if (gpd_ptr == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Nowhere for grease pencil data to go");
		return OPERATOR_CANCELLED;
	}
	else {
		/* just unlink datablock now, decreasing its user count */
		bGPdata *gpd = (*gpd_ptr);

		id_us_min(&gpd->id);
		*gpd_ptr = NULL;
	}
	
	/* notifiers */
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_data_unlink(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Grease Pencil Unlink";
	ot->idname = "GPENCIL_OT_data_unlink";
	ot->description = "Unlink active Grease Pencil data-block";
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* callbacks */
	ot->exec = gp_data_unlink_exec;
	ot->poll = gp_data_unlink_poll;
}


/* ************************************************ */
/* Layer Operators */

/* ******************* Add New Layer ************************ */

/* add new layer - wrapper around API */
static int gp_layer_add_exec(bContext *C, wmOperator *op)
{
	bGPdata **gpd_ptr = ED_gpencil_data_get_pointers(C, NULL);

	/* if there's no existing Grease-Pencil data there, add some */
	if (gpd_ptr == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Nowhere for grease pencil data to go");
		return OPERATOR_CANCELLED;
	}
	if (*gpd_ptr == NULL) {
		*gpd_ptr = BKE_gpencil_data_addnew(CTX_data_main(C), DATA_("GPencil"));
	}
	
	/* add default sets of colors and brushes */
	ED_gpencil_add_defaults(C);

	/* add new layer now */
	BKE_gpencil_layer_addnew(*gpd_ptr, DATA_("GP_Layer"), true);
	
	/* notifiers */
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_layer_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add New Layer";
	ot->idname = "GPENCIL_OT_layer_add";
	ot->description = "Add new Grease Pencil layer for the active Grease Pencil data-block";
	
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* callbacks */
	ot->exec = gp_layer_add_exec;
	ot->poll = gp_add_poll;
}

/* ******************* Remove Active Layer ************************* */

static int gp_layer_remove_exec(bContext *C, wmOperator *op)
{
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	bGPDlayer *gpl = BKE_gpencil_layer_getactive(gpd);
	
	/* sanity checks */
	if (ELEM(NULL, gpd, gpl))
		return OPERATOR_CANCELLED;
	
	if (gpl->flag & GP_LAYER_LOCKED) {
		BKE_report(op->reports, RPT_ERROR, "Cannot delete locked layers");
		return OPERATOR_CANCELLED;
	}
	
	/* make the layer before this the new active layer
	 * - use the one after if this is the first
	 * - if this is the only layer, this naturally becomes NULL
	 */
	if (gpl->prev)
		BKE_gpencil_layer_setactive(gpd, gpl->prev);
	else
		BKE_gpencil_layer_setactive(gpd, gpl->next);
	
	/* delete the layer now... */
	BKE_gpencil_layer_delete(gpd, gpl);
	
	/* notifiers */
	BKE_gpencil_batch_cache_dirty(gpd);
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_layer_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove Layer";
	ot->idname = "GPENCIL_OT_layer_remove";
	ot->description = "Remove active Grease Pencil layer";
	
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* callbacks */
	ot->exec = gp_layer_remove_exec;
	ot->poll = gp_active_layer_poll;
}

/* ******************* Move Layer Up/Down ************************** */

enum {
	GP_LAYER_MOVE_UP   = -1,
	GP_LAYER_MOVE_DOWN = 1
};

static int gp_layer_move_exec(bContext *C, wmOperator *op)
{
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	bGPDlayer *gpl = BKE_gpencil_layer_getactive(gpd);
	
	int direction = RNA_enum_get(op->ptr, "type");
	
	/* sanity checks */
	if (ELEM(NULL, gpd, gpl))
		return OPERATOR_CANCELLED;
	
	BLI_assert(ELEM(direction, -1, 0, 1)); /* we use value below */
	if (BLI_listbase_link_move(&gpd->layers, gpl, direction)) {
		BKE_gpencil_batch_cache_dirty(gpd);
		WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
	}
	
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_layer_move(wmOperatorType *ot)
{
	static const EnumPropertyItem slot_move[] = {
		{GP_LAYER_MOVE_UP, "UP", 0, "Up", ""},
		{GP_LAYER_MOVE_DOWN, "DOWN", 0, "Down", ""},
		{0, NULL, 0, NULL, NULL}
	};
	
	/* identifiers */
	ot->name = "Move Grease Pencil Layer";
	ot->idname = "GPENCIL_OT_layer_move";
	ot->description = "Move the active Grease Pencil layer up/down in the list";
	
	/* api callbacks */
	ot->exec = gp_layer_move_exec;
	ot->poll = gp_active_layer_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	ot->prop = RNA_def_enum(ot->srna, "type", slot_move, 0, "Type", "");
}

/* ********************* Duplicate Layer ************************** */

static int gp_layer_copy_exec(bContext *C, wmOperator *UNUSED(op))
{
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	bGPDlayer *gpl = BKE_gpencil_layer_getactive(gpd);
	bGPDlayer *new_layer;
	
	/* sanity checks */
	if (ELEM(NULL, gpd, gpl))
		return OPERATOR_CANCELLED;
	
	/* make copy of layer, and add it immediately after the existing layer */
	new_layer = BKE_gpencil_layer_duplicate(gpl);
	BLI_insertlinkafter(&gpd->layers, gpl, new_layer);
	
	/* ensure new layer has a unique name, and is now the active layer */
	BLI_uniquename(&gpd->layers, new_layer, DATA_("GP_Layer"), '.', offsetof(bGPDlayer, info), sizeof(new_layer->info));
	BKE_gpencil_layer_setactive(gpd, new_layer);
	
	/* notifiers */
	BKE_gpencil_batch_cache_dirty(gpd);
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_layer_duplicate(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Duplicate Layer";
	ot->idname = "GPENCIL_OT_layer_duplicate";
	ot->description = "Make a copy of the active Grease Pencil layer";
	
	/* callbacks */
	ot->exec = gp_layer_copy_exec;
	ot->poll = gp_active_layer_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ********************* Duplicate Frame ************************** */
enum {
	GP_FRAME_DUP_ACTIVE = 0,
	GP_FRAME_DUP_ALL = 1
};

static int gp_frame_duplicate_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	bGPDlayer *gpl = BKE_gpencil_layer_getactive(gpd);

	int mode = RNA_enum_get(op->ptr, "mode");
	
	/* sanity checks */
	if (ELEM(NULL, gpd, gpl))
		return OPERATOR_CANCELLED;

	if (mode == 0) {
		BKE_gpencil_frame_addcopy(gpl, CFRA);
	}
	else {
		for (gpl = gpd->layers.first; gpl; gpl = gpl->next) {
			if ((gpl->flag & GP_LAYER_LOCKED) == 0) {
				BKE_gpencil_frame_addcopy(gpl, CFRA);
			}
		}

	}
	/* notifiers */
	BKE_gpencil_batch_cache_dirty(gpd);
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);

	return OPERATOR_FINISHED;
}

void GPENCIL_OT_frame_duplicate(wmOperatorType *ot)
{
	static const EnumPropertyItem duplicate_mode[] = {
		{ GP_FRAME_DUP_ACTIVE, "ACTIVE", 0, "Active", "Duplicate frame in active layer only" },
		{ GP_FRAME_DUP_ALL, "ALL", 0, "All", "Duplicate active frames in all layers" },
		{ 0, NULL, 0, NULL, NULL }
		};


	/* identifiers */
	ot->name = "Duplicate Frame";
	ot->idname = "GPENCIL_OT_frame_duplicate";
	ot->description = "Make a copy of the active Grease Pencil Frame";

	/* callbacks */
	ot->exec = gp_frame_duplicate_exec;
	ot->poll = gp_active_layer_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ot->prop = RNA_def_enum(ot->srna, "mode", duplicate_mode, GP_FRAME_DUP_ACTIVE, "Mode", "");
}

/* ********************* Clean Fill Boundaries on Frame ************************** */
enum {
	GP_FRAME_CLEAN_FILL_ACTIVE = 0,
	GP_FRAME_CLEAN_FILL_ALL = 1
};

static int gp_frame_clean_fill_exec(bContext *C, wmOperator *op)
{
	bool changed = false;
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	int mode = RNA_enum_get(op->ptr, "mode");

	CTX_DATA_BEGIN(C, bGPDlayer *, gpl, editable_gpencil_layers)
	{
		bGPDframe *init_gpf = gpl->actframe;
		if (mode == GP_FRAME_CLEAN_FILL_ALL) {
			init_gpf = gpl->frames.first;
		}

		for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
			if ((gpf == gpl->actframe) || (mode == GP_FRAME_CLEAN_FILL_ALL)) {
				bGPDstroke *gps, *gpsn;

				if (gpf == NULL)
					continue;

				/* simply delete strokes which are no fill */
				for (gps = gpf->strokes.first; gps; gps = gpsn) {
					gpsn = gps->next;

					/* skip strokes that are invalid for current view */
					if (ED_gpencil_stroke_can_use(C, gps) == false)
						continue;

					/* free stroke */
					if (gps->flag & GP_STROKE_NOFILL) {
						/* free stroke memory arrays, then stroke itself */
						if (gps->points) {
							BKE_gpencil_free_stroke_weights(gps);
							MEM_freeN(gps->points);
						}
						MEM_SAFE_FREE(gps->triangles);
						BLI_freelinkN(&gpf->strokes, gps);

						changed = true;
					}
				}
			}
		}
	}
	CTX_DATA_END;

	/* notifiers */
	if (changed) {
		BKE_gpencil_batch_cache_dirty(gpd);
		WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
	}

	return OPERATOR_FINISHED;
}

void GPENCIL_OT_frame_clean_fill(wmOperatorType *ot)
{
	static const EnumPropertyItem duplicate_mode[] = {
	{ GP_FRAME_CLEAN_FILL_ACTIVE, "ACTIVE", 0, "Active Frame Only", "Clean active frame only" },
	{ GP_FRAME_CLEAN_FILL_ALL, "ALL", 0, "All Frames", "Clean all frames in all layers" },
	{ 0, NULL, 0, NULL, NULL }
	};

	/* identifiers */
	ot->name = "Clean Fill Boundaries";
	ot->idname = "GPENCIL_OT_frame_clean_fill";
	ot->description = "Remove 'no fill' boundary strokes";

	/* callbacks */
	ot->exec = gp_frame_clean_fill_exec;
	ot->poll = gp_active_layer_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ot->prop = RNA_def_enum(ot->srna, "mode", duplicate_mode, GP_FRAME_DUP_ACTIVE, "Mode", "");
}

/* *********************** Hide Layers ******************************** */

static int gp_hide_exec(bContext *C, wmOperator *op)
{
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	bGPDlayer *layer = BKE_gpencil_layer_getactive(gpd);
	bool unselected = RNA_boolean_get(op->ptr, "unselected");
	
	/* sanity checks */
	if (ELEM(NULL, gpd, layer))
		return OPERATOR_CANCELLED;
	
	if (unselected) {
		bGPDlayer *gpl;
		
		/* hide unselected */
		for (gpl = gpd->layers.first; gpl; gpl = gpl->next) {
			if (gpl != layer) {
				gpl->flag |= GP_LAYER_HIDE;
			}
		}
	}
	else {
		/* hide selected/active */
		layer->flag |= GP_LAYER_HIDE;
	}
	
	/* notifiers */
	BKE_gpencil_batch_cache_dirty(gpd);
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_hide(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Hide Layer(s)";
	ot->idname = "GPENCIL_OT_hide";
	ot->description = "Hide selected/unselected Grease Pencil layers";
	
	/* callbacks */
	ot->exec = gp_hide_exec;
	ot->poll = gp_active_layer_poll; /* NOTE: we need an active layer to play with */
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* props */
	RNA_def_boolean(ot->srna, "unselected", 0, "Unselected", "Hide unselected rather than selected layers");
}

/* ********************** Show All Layers ***************************** */

/* poll callback for showing layers */
static int gp_reveal_poll(bContext *C)
{
	return ED_gpencil_data_get_active(C) != NULL;
}

static void gp_reveal_select_frame(bContext *C, bGPDframe *frame, bool select)
{
	bGPDstroke *gps;
	for (gps = frame->strokes.first; gps; gps = gps->next) {

		/* only deselect strokes that are valid in this view */
		if (ED_gpencil_stroke_can_use(C, gps)) {

			/* (de)select points */
			int i;
			bGPDspoint *pt;
			for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
				SET_FLAG_FROM_TEST(pt->flag, select, GP_SPOINT_SELECT);
			}

			/* (de)select stroke */
			SET_FLAG_FROM_TEST(gps->flag, select, GP_STROKE_SELECT);
		}
	}
}

static int gp_reveal_exec(bContext *C, wmOperator *op)
{
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	bGPDlayer *gpl;
	const bool select = RNA_boolean_get(op->ptr, "select");

	/* sanity checks */
	if (gpd == NULL)
		return OPERATOR_CANCELLED;
	
	for (gpl = gpd->layers.first; gpl; gpl = gpl->next) {

		if (gpl->flag & GP_LAYER_HIDE) {
			gpl->flag &= ~GP_LAYER_HIDE;

			/* select or deselect if requested, only on hidden layers */
			if (gpd->flag & GP_DATA_STROKE_EDITMODE) {
				if (select) {
					/* select all strokes on active frame only (same as select all operator) */
					if (gpl->actframe) {
						gp_reveal_select_frame(C, gpl->actframe, true);
					}
				}
				else {
					/* deselect strokes on all frames (same as deselect all operator) */
					bGPDframe *gpf;
					for (gpf = gpl->frames.first; gpf; gpf = gpf->next) {
						gp_reveal_select_frame(C, gpf, false);
					}
				}
			}
		}
	}
	
	/* notifiers */
	BKE_gpencil_batch_cache_dirty(gpd);
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_reveal(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Show All Layers";
	ot->idname = "GPENCIL_OT_reveal";
	ot->description = "Show all Grease Pencil layers";
	
	/* callbacks */
	ot->exec = gp_reveal_exec;
	ot->poll = gp_reveal_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* props */
	RNA_def_boolean(ot->srna, "select", true, "Select", "");
}

/* ***************** Lock/Unlock All Layers ************************ */

static int gp_lock_all_exec(bContext *C, wmOperator *UNUSED(op))
{
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	bGPDlayer *gpl;
	
	/* sanity checks */
	if (gpd == NULL)
		return OPERATOR_CANCELLED;
	
	/* make all layers non-editable */
	for (gpl = gpd->layers.first; gpl; gpl = gpl->next) {
		gpl->flag |= GP_LAYER_LOCKED;
	}
	
	/* notifiers */
	BKE_gpencil_batch_cache_dirty(gpd);
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_lock_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Lock All Layers";
	ot->idname = "GPENCIL_OT_lock_all";
	ot->description = "Lock all Grease Pencil layers to prevent them from being accidentally modified";
	
	/* callbacks */
	ot->exec = gp_lock_all_exec;
	ot->poll = gp_reveal_poll; /* XXX: could use dedicated poll later */
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* -------------------------- */

static int gp_unlock_all_exec(bContext *C, wmOperator *UNUSED(op))
{
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	bGPDlayer *gpl;
	
	/* sanity checks */
	if (gpd == NULL)
		return OPERATOR_CANCELLED;
	
	/* make all layers editable again */
	for (gpl = gpd->layers.first; gpl; gpl = gpl->next) {
		gpl->flag &= ~GP_LAYER_LOCKED;
	}
	
	/* notifiers */
	BKE_gpencil_batch_cache_dirty(gpd);
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_unlock_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Unlock All Layers";
	ot->idname = "GPENCIL_OT_unlock_all";
	ot->description = "Unlock all Grease Pencil layers so that they can be edited";
	
	/* callbacks */
	ot->exec = gp_unlock_all_exec;
	ot->poll = gp_reveal_poll; /* XXX: could use dedicated poll later */
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ********************** Isolate Layer **************************** */

static int gp_isolate_layer_exec(bContext *C, wmOperator *op)
{
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	bGPDlayer *layer = BKE_gpencil_layer_getactive(gpd);
	bGPDlayer *gpl;
	int flags = GP_LAYER_LOCKED;
	bool isolate = false;
	
	if (RNA_boolean_get(op->ptr, "affect_visibility"))
		flags |= GP_LAYER_HIDE;
		
	if (ELEM(NULL, gpd, layer)) {
		BKE_report(op->reports, RPT_ERROR, "No active layer to isolate");
		return OPERATOR_CANCELLED;
	}
	
	/* Test whether to isolate or clear all flags */
	for (gpl = gpd->layers.first; gpl; gpl = gpl->next) {
		/* Skip if this is the active layer */
		if (gpl == layer)
			continue;
		
		/* If the flags aren't set, that means that the layer is
		 * not alone, so we have some layers to isolate still
		 */
		if ((gpl->flag & flags) == 0) {
			isolate = true;
			break;
		}
	}
	
	/* Set/Clear flags as appropriate */
	/* TODO: Include onionskinning on this list? */
	if (isolate) {
		/* Set flags on all "other" layers */
		for (gpl = gpd->layers.first; gpl; gpl = gpl->next) {
			if (gpl == layer)
				continue;
			else
				gpl->flag |= flags;
		}
	}
	else {
		/* Clear flags - Restore everything else */
		for (gpl = gpd->layers.first; gpl; gpl = gpl->next) {
			gpl->flag &= ~flags;
		}
	}
	
	/* notifiers */
	BKE_gpencil_batch_cache_dirty(gpd);
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_layer_isolate(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Isolate Layer";
	ot->idname = "GPENCIL_OT_layer_isolate";
	ot->description = "Toggle whether the active layer is the only one that can be edited and/or visible";
	
	/* callbacks */
	ot->exec = gp_isolate_layer_exec;
	ot->poll = gp_active_layer_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean(ot->srna, "affect_visibility", false, "Affect Visibility",
	                "In addition to toggling the editability, also affect the visibility");
}

/* ********************** Merge Layer with the next layer **************************** */

static int gp_merge_layer_exec(bContext *C, wmOperator *op)
{
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	bGPDlayer *gpl_current = BKE_gpencil_layer_getactive(gpd);
	bGPDlayer *gpl_next = gpl_current->next;

	if (ELEM(NULL, gpd, gpl_current, gpl_next)) {
		BKE_report(op->reports, RPT_ERROR, "No layers to merge");
		return OPERATOR_CANCELLED;
	}

	/* Collect frames of gpl_current in hash table to avoid O(n^2) lookups */
	GHash *gh_frames_cur = BLI_ghash_int_new_ex(__func__, 64);
	for (bGPDframe *gpf = gpl_current->frames.first; gpf; gpf = gpf->next) {
		BLI_ghash_insert(gh_frames_cur, SET_INT_IN_POINTER(gpf->framenum), gpf);
	}

	/* read all frames from next layer and add any missing in current layer */
	for (bGPDframe *gpf = gpl_next->frames.first; gpf; gpf = gpf->next) {
		/* try to find frame in current layer */
		bGPDframe *frame = BLI_ghash_lookup(gh_frames_cur, SET_INT_IN_POINTER(gpf->framenum));
		if (!frame) {
			bGPDframe *actframe = BKE_gpencil_layer_getframe(gpl_current, gpf->framenum, GP_GETFRAME_USE_PREV);
			frame = BKE_gpencil_frame_addnew(gpl_current, gpf->framenum);
			/* duplicate strokes of current active frame */
			if (actframe) {
				BKE_gpencil_frame_copy_strokes(actframe, frame);
			}
		}
		/* add to tail all strokes */
		BLI_movelisttolist(&frame->strokes, &gpf->strokes);
	}

	/* Now delete next layer */
	BKE_gpencil_layer_delete(gpd, gpl_next);
	BLI_ghash_free(gh_frames_cur, NULL, NULL);

	/* notifiers */
	BKE_gpencil_batch_cache_dirty(gpd);
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);

	return OPERATOR_FINISHED;
}

void GPENCIL_OT_layer_merge(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Merge Down";
	ot->idname = "GPENCIL_OT_layer_merge";
	ot->description = "Merge the current layer with the layer below";

	/* callbacks */
	ot->exec = gp_merge_layer_exec;
	ot->poll = gp_active_layer_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ********************** Change Layer ***************************** */

static int gp_layer_change_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(evt))
{
	uiPopupMenu *pup;
	uiLayout *layout;
	
	/* call the menu, which will call this operator again, hence the canceled */
	pup = UI_popup_menu_begin(C, op->type->name, ICON_NONE);
	layout = UI_popup_menu_layout(pup);
	uiItemsEnumO(layout, "GPENCIL_OT_layer_change", "layer");
	UI_popup_menu_end(C, pup);
	
	return OPERATOR_INTERFACE;
}

static int gp_layer_change_exec(bContext *C, wmOperator *op)
{
	bGPdata *gpd = CTX_data_gpencil_data(C);
	bGPDlayer *gpl = NULL;
	int layer_num = RNA_enum_get(op->ptr, "layer");
	
	/* Get layer or create new one */
	if (layer_num == -1) {
		/* Create layer */
		gpl = BKE_gpencil_layer_addnew(gpd, DATA_("GP_Layer"), true);
	}
	else {
		/* Try to get layer */
		gpl = BLI_findlink(&gpd->layers, layer_num);
		
		if (gpl == NULL) {
			BKE_reportf(op->reports, RPT_ERROR, "Cannot change to non-existent layer (index = %d)", layer_num);
			return OPERATOR_CANCELLED;
		}
	}
	
	/* Set active layer */
	BKE_gpencil_layer_setactive(gpd, gpl);
	
	/* updates */
	BKE_gpencil_batch_cache_dirty(gpd);
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_layer_change(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Change Layer";
	ot->idname = "GPENCIL_OT_layer_change";
	ot->description = "Change active Grease Pencil layer";
	
	/* callbacks */
	ot->invoke = gp_layer_change_invoke;
	ot->exec = gp_layer_change_exec;
	ot->poll = gp_active_layer_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* gp layer to use (dynamic enum) */
	ot->prop = RNA_def_enum(ot->srna, "layer", DummyRNA_DEFAULT_items, 0, "Grease Pencil Layer", "");
	RNA_def_enum_funcs(ot->prop, ED_gpencil_layers_with_new_enum_itemf);
}

/* ************************************************ */

/* ******************* Arrange Stroke Up/Down in drawing order ************************** */

enum {
	GP_STROKE_MOVE_UP = -1,
	GP_STROKE_MOVE_DOWN = 1,
	GP_STROKE_MOVE_TOP = 2,
	GP_STROKE_MOVE_BOTTOM = 3
};

static int gp_stroke_arrange_exec(bContext *C, wmOperator *op)
{
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	bGPDlayer *gpl = BKE_gpencil_layer_getactive(gpd);
	bGPDstroke *gps;

	/* sanity checks */
	if (ELEM(NULL, gpd, gpl, gpl->actframe)) {
		return OPERATOR_CANCELLED;
	}

	const int direction = RNA_enum_get(op->ptr, "direction");

	for (gpl = gpd->layers.first; gpl; gpl = gpl->next) {
		/* temp listbase to store selected strokes by layer */
		ListBase selected = { NULL };
		bGPDframe *gpf = gpl->actframe;
		if (gpl->flag & GP_LAYER_LOCKED) {
			continue;
		}

		if (gpf == NULL) {
			continue;
		}
		bool gpf_lock = false;
		/* verify if any selected stroke is in the extreme of the stack and select to move */
		for (gps = gpf->strokes.first; gps; gps = gps->next) {
			/* only if selected */
			if (gps->flag & GP_STROKE_SELECT) {
				/* skip strokes that are invalid for current view */
				if (ED_gpencil_stroke_can_use(C, gps) == false) {
					continue;
				}
				/* check if the color is editable */
				if (ED_gpencil_stroke_color_use(gpl, gps) == false) {
					continue;
				}
				/* some stroke is already at front*/
				if ((direction == GP_STROKE_MOVE_TOP) || (direction == GP_STROKE_MOVE_UP)) {
					if (gps == gpf->strokes.last) {
						gpf_lock = true;
						continue;
					}
				}
				/* some stroke is already at botom */
				if ((direction == GP_STROKE_MOVE_BOTTOM) || (direction == GP_STROKE_MOVE_DOWN)) {
					if (gps == gpf->strokes.first) {
						gpf_lock = true;
						continue;
					}
				}
				/* add to list (if not locked) */
				if (!gpf_lock) {
					BLI_addtail(&selected, BLI_genericNodeN(gps));
				}
			}
		}
		/* Now do the movement of the stroke */
		if (!gpf_lock) {
			switch (direction) {
				/* Bring to Front */
				case GP_STROKE_MOVE_TOP:
					for (LinkData *link = selected.first; link; link = link->next) {
						gps = link->data;
						BLI_remlink(&gpf->strokes, gps);
						BLI_addtail(&gpf->strokes, gps);
					}
					break;
					/* Bring Forward */
				case GP_STROKE_MOVE_UP:
					for (LinkData *link = selected.last; link; link = link->prev) {
						gps = link->data;
						BLI_listbase_link_move(&gpf->strokes, gps, 1);
					}
					break;
					/* Send Backward */
				case GP_STROKE_MOVE_DOWN:
					for (LinkData *link = selected.first; link; link = link->next) {
						gps = link->data;
						BLI_listbase_link_move(&gpf->strokes, gps, -1);
					}
					break;
					/* Send to Back */
				case GP_STROKE_MOVE_BOTTOM:
					for (LinkData *link = selected.last; link; link = link->prev) {
						gps = link->data;
						BLI_remlink(&gpf->strokes, gps);
						BLI_addhead(&gpf->strokes, gps);
					}
					break;
				default:
					BLI_assert(0);
					break;
			}
		}
		BLI_freelistN(&selected);
	}

	/* notifiers */
	BKE_gpencil_batch_cache_dirty(gpd);
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);

	return OPERATOR_FINISHED;
}

void GPENCIL_OT_stroke_arrange(wmOperatorType *ot)
{
	static const EnumPropertyItem slot_move[] = {
		{GP_STROKE_MOVE_UP, "UP", 0, "Bring Forward", ""},
		{GP_STROKE_MOVE_DOWN, "DOWN", 0, "Send Backward", ""},
		{GP_STROKE_MOVE_TOP, "TOP", 0, "Bring to Front", ""},
		{GP_STROKE_MOVE_BOTTOM, "BOTTOM", 0, "Send to Back", ""},
		{0, NULL, 0, NULL, NULL }
	};

	/* identifiers */
	ot->name = "Arrange Stroke";
	ot->idname = "GPENCIL_OT_stroke_arrange";
	ot->description = "Arrange selected strokes up/down in the drawing order of the active layer";

	/* callbacks */
	ot->exec = gp_stroke_arrange_exec;
	ot->poll = gp_active_layer_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "direction", slot_move, GP_STROKE_MOVE_UP, "Direction", "");
}

/* ******************* Move Stroke to new palette ************************** */

static int gp_stroke_change_palette_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	const int type = RNA_enum_get(op->ptr, "type");

	bGPdata *gpd = ED_gpencil_data_get_active(C);
	bGPDpaletteref *palslot = BKE_gpencil_paletteslot_get_active(gpd);
	Palette *palette;
	PaletteColor *palcolor;

	/* sanity checks */
	if (ELEM(NULL, gpd, palslot)) {
		return OPERATOR_CANCELLED;
	}

	palette = palslot->palette;
	if (ELEM(NULL, palette)) {
		return OPERATOR_CANCELLED;
	}

	/* loop all strokes */
	for (bGPDlayer *gpl = gpd->layers.first; gpl; gpl = gpl->next) {
		/* only editable and visible layers are considered */
		if (!gpencil_layer_is_editable(gpl))
			continue;
		for (bGPDframe *gpf = gpl->frames.first; gpf; gpf = gpf->next) {
			/* check frame if frame range */
			if ((type == GP_MOVE_PALETTE_BEFORE) && (gpf->framenum >= scene->r.cfra))
				continue;
			if ((type == GP_MOVE_PALETTE_AFTER) && (gpf->framenum < scene->r.cfra))
				continue;
			if ((type == GP_MOVE_PALETTE_CURRENT) && (gpf->framenum != scene->r.cfra))
				continue;

			for (bGPDstroke *gps = gpf->strokes.last; gps; gps = gps->prev) {
				/* only if selected */
				if (((gps->flag & GP_STROKE_SELECT) == 0) && (type == GP_MOVE_PALETTE_SELECT))
					continue;
				/* skip strokes that are invalid for current view */
				if (ED_gpencil_stroke_can_use(C, gps) == false)
					continue;
				/* check if the color is editable */
				if (ED_gpencil_stroke_color_use(gpl, gps) == false)
					continue;

				/* look for new color */
				palcolor = BKE_palette_color_getbyname(palette, gps->colorname);
				/* if the color does not exist, create a new one to keep stroke */
				if (palcolor == NULL) {
					palcolor = BKE_palette_color_add_name(palette, gps->colorname);
					copy_v4_v4(palcolor->rgb, gps->palcolor->rgb);
					copy_v4_v4(palcolor->fill, gps->palcolor->fill);
					/* duplicate flags */
					palcolor->flag = gps->palcolor->flag;
					palcolor->stroke_style = gps->palcolor->stroke_style;
					palcolor->fill_style = gps->palcolor->fill_style;
				}

				/* asign new color */
				BLI_strncpy(gps->colorname, palcolor->info, sizeof(gps->colorname));
				gps->palette = palette;
				gps->palcolor = palcolor;
			}
		}
	}
	/* notifiers */
	BKE_gpencil_batch_cache_dirty(gpd);
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);

	return OPERATOR_FINISHED;
}

void GPENCIL_OT_stroke_change_palette(wmOperatorType *ot)
{
	static EnumPropertyItem palette_move_type[] = {
		{ GP_MOVE_PALETTE_SELECT, "SELECTED", 0, "Change Strokes Selected", "Move to new palette any stroke selected in any frame" },
		{ GP_MOVE_PALETTE_ALL, "ALL", 0, "Change All Frames", "Move all strokes in all frames to new palette" },
		{ GP_MOVE_PALETTE_BEFORE, "BEFORE", 0, "Change Frames Before", "Move all strokes in frames before current frame to new palette" },
		{ GP_MOVE_PALETTE_AFTER, "AFTER", 0, "Change Frames After", "Move all strokes in frames greater or equal current frame to new palette" },
		{ GP_MOVE_PALETTE_CURRENT, "CURRENT", 0, "Change Current Frame", "Move all strokes in current frame to new palette" },
		{ 0, NULL, 0, NULL, NULL }
	};

	/* identifiers */
	ot->name = "Change Stroke Palette";
	ot->idname = "GPENCIL_OT_stroke_change_palette";
	ot->description = "Move strokes to active palette";

	/* callbacks */
	ot->exec = gp_stroke_change_palette_exec;
	ot->poll = gp_active_layer_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "type", palette_move_type, GP_MOVE_PALETTE_SELECT, "Type", "");

}

/* ******************* Move Stroke to new color ************************** */

static int gp_stroke_change_color_exec(bContext *C, wmOperator *UNUSED(op))
{
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	bGPDpaletteref *palslot = BKE_gpencil_paletteslot_get_active(gpd);
	Palette *palette;
	PaletteColor *color;

	/* sanity checks */
	if (ELEM(NULL, gpd, palslot)) {
		return OPERATOR_CANCELLED;
	}

	bool is_multiedit = (bool)GPENCIL_MULTIEDIT_SESSIONS_ON(gpd);
	palette = palslot->palette;
	color = BKE_palette_color_get_active(palette);
	if (ELEM(NULL, color)) {
		return OPERATOR_CANCELLED;
	}

	/* loop all strokes */
	CTX_DATA_BEGIN(C, bGPDlayer *, gpl, editable_gpencil_layers)
	{
		bGPDframe *init_gpf = gpl->actframe;
		if (is_multiedit) {
			init_gpf = gpl->frames.first;
		}

		for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
			if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {
				if (gpf == NULL)
					continue;

				for (bGPDstroke *gps = gpf->strokes.first; gps; gps = gps->next) {
					/* only if selected */
					if (gps->flag & GP_STROKE_SELECT) {
						/* skip strokes that are invalid for current view */
						if (ED_gpencil_stroke_can_use(C, gps) == false)
							continue;
						/* check if the color is editable */
						if (ED_gpencil_stroke_color_use(gpl, gps) == false)
							continue;

						/* asign new color (only if different) */
						if ((STREQ(gps->colorname, color->info) == false) || (gps->palcolor != color)) {
							BLI_strncpy(gps->colorname, color->info, sizeof(gps->colorname));
							gps->palette = palette;
							gps->palcolor = color;
						}
					}
				}
			}
		}
	}
	CTX_DATA_END;

	/* notifiers */
	BKE_gpencil_batch_cache_dirty(gpd);
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);

	return OPERATOR_FINISHED;
}

void GPENCIL_OT_stroke_change_color(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Change Stroke Color";
	ot->idname = "GPENCIL_OT_stroke_change_color";
	ot->description = "Move selected strokes to active color";

	/* callbacks */
	ot->exec = gp_stroke_change_color_exec;
	ot->poll = gp_active_layer_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ******************* Lock color of non selected Strokes colors ************************** */

static int gp_stroke_lock_color_exec(bContext *C, wmOperator *UNUSED(op))
{
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	bGPDpaletteref *palslot = BKE_gpencil_paletteslot_get_active(gpd);
	Palette *palette;

	/* sanity checks */
	if (ELEM(NULL, gpd, palslot))
		return OPERATOR_CANCELLED;

	palette = palslot->palette;
	if (ELEM(NULL, palette))
		return OPERATOR_CANCELLED;
	
	/* first lock all colors */
	for (PaletteColor *palcolor = palette->colors.first; palcolor; palcolor = palcolor->next) {
		palcolor->flag |= PC_COLOR_LOCKED;
	}

	/* loop all selected strokes and unlock any color */
	for (bGPDlayer *gpl = gpd->layers.first; gpl; gpl = gpl->next) {
		/* only editable and visible layers are considered */
		if (gpencil_layer_is_editable(gpl) && (gpl->actframe != NULL)) {
			for (bGPDstroke *gps = gpl->actframe->strokes.last; gps; gps = gps->prev) {
				/* only if selected */
				if (gps->flag & GP_STROKE_SELECT) {
					/* skip strokes that are invalid for current view */
					if (ED_gpencil_stroke_can_use(C, gps) == false) {
						continue;
					}
					/* unlock color */
					if (gps->palcolor != NULL) {
						gps->palcolor->flag &= ~PC_COLOR_LOCKED;
					}
				}
			}
		}
	}
	/* notifiers */
	BKE_gpencil_batch_cache_dirty(gpd);
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);

	return OPERATOR_FINISHED;
}

void GPENCIL_OT_stroke_lock_color(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Lock Unused Colors";
	ot->idname = "GPENCIL_OT_stroke_lock_color";
	ot->description = "Lock any color not used in any selected stroke";

	/* api callbacks */
	ot->exec = gp_stroke_lock_color_exec;
	ot->poll = gp_active_layer_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ************************************************ */
/* Palette Slot Operators */

/* ********************* Add Palette SLot ************************* */

static int gp_paletteslot_add_exec(bContext *C, wmOperator *UNUSED(op))
{
	bGPdata *gpd = CTX_data_gpencil_data(C);
	
	/* just add an empty slot */
	BKE_gpencil_paletteslot_add(gpd, NULL);
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_ADDED, NULL);
	
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_palette_slot_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Palette Slot";
	ot->idname = "GPENCIL_OT_palette_slot_add";
	ot->description = "Add new Palette Slot to refer to a Palette used by this Grease Pencil object";
	
	/* callbacks */
	ot->exec = gp_paletteslot_add_exec;
	ot->poll = gp_active_layer_poll; // XXX
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ******************* Remove Palette Slot *********************** */

static int gp_paletteslot_active_poll(bContext *C)
{
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	bGPDpaletteref *palslot = BKE_gpencil_paletteslot_get_active(gpd);

	return (palslot != NULL);
}

static int gp_paletteslot_remove_exec(bContext *C, wmOperator *op)
{
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	bGPDpaletteref *palslot = BKE_gpencil_paletteslot_get_active(gpd);
	
	/* 1) Check if palette is still used anywhere */
	if (BKE_gpencil_paletteslot_has_users(gpd, palslot)) {
		/* XXX: Change strokes to the new active slot's palette instead? */
		BKE_report(op->reports, RPT_ERROR, "Cannot remove, Palette still in use");
		return OPERATOR_CANCELLED;
	}
	
	/* 2) Remove the slot (will unlink user and free it) */
	if ((palslot->next == NULL) && (gpd->active_palette_slot > 0)) {
		/* fix active slot index */
		gpd->active_palette_slot--;
	}
	
	BKE_gpencil_palette_slot_free(gpd, palslot);
		
	/* updates */
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_REMOVED, NULL);
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_palette_slot_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove Palette Slot";
	ot->idname = "GPENCIL_OT_palette_slot_remove";
	ot->description = "Remove active Palette Slot to refer to a Palette used by this Grease Pencil object";
	
	/* callbacks */
	ot->exec = gp_paletteslot_remove_exec;
	ot->poll = gp_paletteslot_active_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}


/* ************************************************ */
/* Drawing Brushes Operators */

/* ******************* Brush create presets ************************** */
static int gp_brush_presets_create_exec(bContext *C, wmOperator *UNUSED(op))
{
	BKE_brush_gpencil_presets(C);

	/* notifiers */
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);

	return OPERATOR_FINISHED;
}

void GPENCIL_OT_brush_presets_create(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Create Preset Brushes";
	ot->idname = "GPENCIL_OT_brush_presets_create";
	ot->description = "Create a set of predefined Grease Pencil drawing brushes";

	/* api callbacks */
	ot->exec = gp_brush_presets_create_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

}

/* ***************** Select Brush ************************ */

static int gp_brush_select_exec(bContext *C, wmOperator *op)
{
	ToolSettings *ts = CTX_data_tool_settings(C);
	Main *bmain = CTX_data_main(C);

	/* if there's no existing container */
	if (ts == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Nowhere to go");
		return OPERATOR_CANCELLED;
	}

	const int index = RNA_int_get(op->ptr, "index");
	
	Paint *paint = BKE_brush_get_gpencil_paint(ts);
	int i = 0;
	for (Brush *brush = bmain->brush.first; brush; brush = brush->id.next) {
		if (brush->ob_mode == OB_MODE_GPENCIL_PAINT) {
			if (i == index) {
				BKE_paint_brush_set(paint, brush);

				/* notifiers */
				WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
				return OPERATOR_FINISHED;
			}
			i++;
		}
	}

	return OPERATOR_CANCELLED;
}

void GPENCIL_OT_brush_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Brush";
	ot->idname = "GPENCIL_OT_brush_select";
	ot->description = "Select a Grease Pencil drawing brush";

	/* callbacks */
	ot->exec = gp_brush_select_exec;
	ot->poll = gp_active_brush_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_int(ot->srna, "index", 0, 0, INT_MAX, "Index", "Index of Drawing Brush", 0, INT_MAX);
}

/* ***************** Select Sculpt Brush ************************ */

static int gp_sculpt_select_exec(bContext *C, wmOperator *op)
{
	ToolSettings *ts = CTX_data_tool_settings(C);

	/* if there's no existing container */
	if (ts == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Nowhere to go");
		return OPERATOR_CANCELLED;
	}

	const int index = RNA_int_get(op->ptr, "index");
	GP_BrushEdit_Settings *gp_sculpt = &ts->gp_sculpt;
	/* sanity checks */
	if (ELEM(NULL, gp_sculpt)) {
		return OPERATOR_CANCELLED;
	}

	if (index < TOT_GP_EDITBRUSH_TYPES - 1) {
		gp_sculpt->brushtype = index;
	}
	/* notifiers */
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);

	return OPERATOR_FINISHED;
}

void GPENCIL_OT_sculpt_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Sculpt Brush";
	ot->idname = "GPENCIL_OT_sculpt_select";
	ot->description = "Select a Grease Pencil sculpt brush";

	/* callbacks */
	ot->exec = gp_sculpt_select_exec;
	ot->poll = gp_add_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_int(ot->srna, "index", 0, 0, INT_MAX, "Index", "Index of Sculpt Brush", 0, INT_MAX);
}

/* ******************* Convert animation data ************************ */

static int gp_convert_old_palettes_poll(bContext *C)
{
	/* TODO: need better poll */
	Main *bmain = CTX_data_main(C);
	return bmain->gpencil.first != NULL;
}

/* convert old animation data to new format */
static int gp_convert_old_palettes_exec(bContext *C, wmOperator *UNUSED(op))
{
	Main *bmain = CTX_data_main(C);
	for (bGPdata *gpd = bmain->gpencil.first; gpd; gpd = gpd->id.next) {
		BKE_gpencil_move_animdata_to_palettes(C, gpd);
	}
	/* notifiers */
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);

	return OPERATOR_FINISHED;
}

void GPENCIL_OT_convert_old_palettes(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Convert Old Palettes";
	ot->idname = "GPENCIL_OT_convert_old_palettes";
	ot->description = "Convert old gpencil palettes animation data to blender palettes";

	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* callbacks */
	ot->exec = gp_convert_old_palettes_exec;
	ot->poll = gp_convert_old_palettes_poll;
}

/* ******************* Convert scene gp data to gp object ************************ */

static int gp_convert_scene_to_object_poll(bContext *C)
{
	Scene *scene = CTX_data_scene(C);
	if (scene->gpd) {
		return true;
	}
	else {
		return false;
	}
}

/* convert scene datablock to gpencil object */
static int gp_convert_scene_to_object_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);
	bGPdata *gpd = scene->gpd;
	float loc[3] = { 0.0f, 0.0f, 0.0f };

	Object *ob = ED_add_gpencil_object(C, scene, loc); /* always in origin */
	
	// FIXME: This loses the datablock created above...
	ob->data = gpd;
	scene->gpd = NULL;

	/* notifiers */
	BKE_gpencil_batch_cache_dirty(gpd);
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED | ND_SPACE_PROPERTIES, NULL);

	return OPERATOR_FINISHED;
}

void GPENCIL_OT_convert_scene_to_object(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Convert Scene Datablock to gpencil Object";
	ot->idname = "GPENCIL_OT_convert_scene_to_object";
	ot->description = "Convert scene grease pencil datablock to gpencil object";

	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* callbacks */
	ot->exec = gp_convert_scene_to_object_exec;
	ot->poll = gp_convert_scene_to_object_poll;
}

/*********************** Vertex Groups ***********************************/

static int gpencil_vertex_group_poll(bContext *C)
{
	Object *ob = CTX_data_active_object(C);

	if ((ob) && (ob->type == OB_GPENCIL)) {
		if (!ID_IS_LINKED(ob) && !ID_IS_LINKED(ob->data) && ob->defbase.first) {
			if (ELEM(ob->mode,
				     OB_MODE_GPENCIL_EDIT,
				     OB_MODE_GPENCIL_SCULPT))
			{
				return true;	
			}
		}
	}

	return false;
}

static int gpencil_vertex_group_weight_poll(bContext *C)
{
	Object *ob = CTX_data_active_object(C);

	if ((ob) && (ob->type == OB_GPENCIL)) {
		if (!ID_IS_LINKED(ob) && !ID_IS_LINKED(ob->data) && ob->defbase.first) {
			if (ob->mode == OB_MODE_GPENCIL_WEIGHT)
			{
				return true;	
			}
		}
	}

	return false;
}

static int gpencil_vertex_group_assign_exec(bContext *C, wmOperator *UNUSED(op))
{
	ToolSettings *ts = CTX_data_tool_settings(C);
	Object *ob = CTX_data_active_object(C);

	/* sanity checks */
	if (ELEM(NULL, ts, ob, ob->data))
		return OPERATOR_CANCELLED;

	ED_gpencil_vgroup_assign(C, ob, ts->vgroup_weight);

	/* notifiers */
	BKE_gpencil_batch_cache_dirty(ob->data);
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED | ND_SPACE_PROPERTIES, NULL);

	return OPERATOR_FINISHED;
}

void GPENCIL_OT_vertex_group_assign(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Assign to Vertex Group";
	ot->idname = "GPENCIL_OT_vertex_group_assign";
	ot->description = "Assign the selected vertices to the active vertex group";

	/* api callbacks */
	ot->poll = gpencil_vertex_group_poll;
	ot->exec = gpencil_vertex_group_assign_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* remove point from vertex group */
static int gpencil_vertex_group_remove_from_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = CTX_data_active_object(C);

	/* sanity checks */
	if (ELEM(NULL, ob, ob->data))
		return OPERATOR_CANCELLED;

	ED_gpencil_vgroup_remove(C, ob);

	/* notifiers */
	BKE_gpencil_batch_cache_dirty(ob->data); // XXX: Review this (aligorith)
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED | ND_SPACE_PROPERTIES, NULL);

	return OPERATOR_FINISHED;
}

void GPENCIL_OT_vertex_group_remove_from(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove from Vertex Group";
	ot->idname = "GPENCIL_OT_vertex_group_remove_from";
	ot->description = "Remove the selected vertices from active or all vertex group(s)";

	/* api callbacks */
	ot->poll = gpencil_vertex_group_poll;
	ot->exec = gpencil_vertex_group_remove_from_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

}

static int gpencil_vertex_group_select_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = CTX_data_active_object(C);

	/* sanity checks */
	if (ELEM(NULL, ob, ob->data))
		return OPERATOR_CANCELLED;

	ED_gpencil_vgroup_select(C, ob);

	/* notifiers */
	BKE_gpencil_batch_cache_dirty(ob->data);
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED | ND_SPACE_PROPERTIES, NULL);

	return OPERATOR_FINISHED;
}

void GPENCIL_OT_vertex_group_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Vertex Group";
	ot->idname = "GPENCIL_OT_vertex_group_select";
	ot->description = "Select all the vertices assigned to the active vertex group";

	/* api callbacks */
	ot->poll = gpencil_vertex_group_poll;
	ot->exec = gpencil_vertex_group_select_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int gpencil_vertex_group_deselect_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = CTX_data_active_object(C);

	/* sanity checks */
	if (ELEM(NULL, ob, ob->data))
		return OPERATOR_CANCELLED;

	ED_gpencil_vgroup_deselect(C, ob);

	/* notifiers */
	BKE_gpencil_batch_cache_dirty(ob->data);
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED | ND_SPACE_PROPERTIES, NULL);

	return OPERATOR_FINISHED;
}

void GPENCIL_OT_vertex_group_deselect(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Deselect Vertex Group";
	ot->idname = "GPENCIL_OT_vertex_group_deselect";
	ot->description = "Deselect all selected vertices assigned to the active vertex group";

	/* api callbacks */
	ot->poll = gpencil_vertex_group_poll;
	ot->exec = gpencil_vertex_group_deselect_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* invert */
static int gpencil_vertex_group_invert_exec(bContext *C, wmOperator *UNUSED(op))
{
	ToolSettings *ts = CTX_data_tool_settings(C);
	Object *ob = CTX_data_active_object(C);

	/* sanity checks */
	if (ELEM(NULL, ts, ob, ob->data))
		return OPERATOR_CANCELLED;

	bGPDspoint *pt;
	const int def_nr = ob->actdef - 1;
	if (!BLI_findlink(&ob->defbase, def_nr))
		return OPERATOR_CANCELLED;

	CTX_DATA_BEGIN(C, bGPDstroke *, gps, editable_gpencil_strokes)
	{
		for (int i = 0; i < gps->totpoints; i++) {
			pt = &gps->points[i];
			if (pt->weights == NULL) {
				BKE_gpencil_vgroup_add_point_weight(pt, def_nr, 1.0f);
			}
			else if (pt->weights->factor == 1.0f) {
				BKE_gpencil_vgroup_remove_point_weight(pt, def_nr);
			}
			else {
				pt->weights->factor = 1.0f - pt->weights->factor;
			}
		}
	}
	CTX_DATA_END;

	/* notifiers */
	BKE_gpencil_batch_cache_dirty(ob->data);
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED | ND_SPACE_PROPERTIES, NULL);

	return OPERATOR_FINISHED;
}

void GPENCIL_OT_vertex_group_invert(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Invert Vertex Group";
	ot->idname = "GPENCIL_OT_vertex_group_invert";
	ot->description = "Invert weights to the active vertex group";

	/* api callbacks */
	ot->poll = gpencil_vertex_group_weight_poll;
	ot->exec = gpencil_vertex_group_invert_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* smooth */
static int gpencil_vertex_group_smooth_exec(bContext *C, wmOperator *op)
{
	const float fac = RNA_float_get(op->ptr, "factor");
	const int repeat = RNA_int_get(op->ptr, "repeat");

	ToolSettings *ts = CTX_data_tool_settings(C);
	Object *ob = CTX_data_active_object(C);

	/* sanity checks */
	if (ELEM(NULL, ts, ob, ob->data))
		return OPERATOR_CANCELLED;

	bGPDspoint *pta, *ptb, *ptc;

	const int def_nr = ob->actdef - 1;
	if (!BLI_findlink(&ob->defbase, def_nr))
		return OPERATOR_CANCELLED;

	CTX_DATA_BEGIN(C, bGPDstroke *, gps, editable_gpencil_strokes)
	{
		for (int s = 0; s < repeat; s++) {
			for (int i = 0; i < gps->totpoints; i++) {
				/* previous point */
				if (i > 0) {
					pta = &gps->points[i - 1];
				}
				else {
					pta = &gps->points[i];
				}
				/* current */
				ptb = &gps->points[i];
				/* next point */
				if (i + 1 < gps->totpoints) {
					ptc = &gps->points[i + 1];
				}
				else {
					ptc = &gps->points[i];
				}

				float wa = BKE_gpencil_vgroup_use_index(pta, def_nr);
				float wb = BKE_gpencil_vgroup_use_index(ptb, def_nr);
				float wc = BKE_gpencil_vgroup_use_index(ptc, def_nr);
				CLAMP_MIN(wa, 0.0f);
				CLAMP_MIN(wb, 0.0f);
				CLAMP_MIN(wc, 0.0f);

				/* the optimal value is the corresponding to the interpolation of the weight
				*  at the distance of point b
				*/
				const float opfac = line_point_factor_v3(&ptb->x, &pta->x, &ptc->x);
				const float optimal = interpf(wa, wb, opfac);
				/* Based on influence factor, blend between original and optimal */
				wb = interpf(wb, optimal, fac);
				BKE_gpencil_vgroup_add_point_weight(ptb, def_nr, wb);
			}
		}
	}
	CTX_DATA_END;

	/* notifiers */
	BKE_gpencil_batch_cache_dirty(ob->data);
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED | ND_SPACE_PROPERTIES, NULL);

	return OPERATOR_FINISHED;
}

void GPENCIL_OT_vertex_group_smooth(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Smooth Vertex Group";
	ot->idname = "GPENCIL_OT_vertex_group_smooth";
	ot->description = "Smooth weights to the active vertex group";

	/* api callbacks */
	ot->poll = gpencil_vertex_group_weight_poll;
	ot->exec = gpencil_vertex_group_smooth_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_float(ot->srna, "factor", 0.5f, 0.0f, 1.0, "Factor", "", 0.0f, 1.0f);
	RNA_def_int(ot->srna, "repeat", 1, 1, 10000, "Iterations", "", 1, 200);
}

/****************************** Join ***********************************/

/* userdata for joined_gpencil_fix_animdata_cb() */
typedef struct tJoinGPencil_AdtFixData {
	bGPdata *src_gpd;
	bGPdata *tar_gpd;
	
	GHash *names_map;
} tJoinGPencil_AdtFixData;

/* Callback to pass to BKE_fcurves_main_cb() for RNA Paths attached to each F-Curve used in the AnimData */
static void joined_gpencil_fix_animdata_cb(ID *id, FCurve *fcu, void *user_data)
{
	tJoinGPencil_AdtFixData *afd = (tJoinGPencil_AdtFixData *)user_data;
	ID *src_id = &afd->src_gpd->id;
	ID *dst_id = &afd->tar_gpd->id;
	
	GHashIterator gh_iter;
	
	/* Fix paths - If this is the target datablock, it will have some "dirty" paths */
	if ((id == src_id) && fcu->rna_path && strstr(fcu->rna_path, "layers[")) {
		GHASH_ITER(gh_iter, afd->names_map) {
			const char *old_name = BLI_ghashIterator_getKey(&gh_iter);
			const char *new_name = BLI_ghashIterator_getValue(&gh_iter);
			
			/* only remap if changed; this still means there will be some waste if there aren't many drivers/keys */
			if (!STREQ(old_name, new_name) && strstr(fcu->rna_path, old_name)) {
				fcu->rna_path = BKE_animsys_fix_rna_path_rename(id, fcu->rna_path, "layers",
				                                                old_name, new_name, 0, 0, false);
				
				/* we don't want to apply a second remapping on this F-Curve now, 
				 * so stop trying to fix names names
				 */
				break;
			}
		}
	}
	
	/* Fix driver targets */
	if (fcu->driver) {
		/* Fix driver references to invalid ID's */
		for (DriverVar *dvar = fcu->driver->variables.first; dvar; dvar = dvar->next) {
			/* only change the used targets, since the others will need fixing manually anyway */
			DRIVER_TARGETS_USED_LOOPER(dvar)
			{
				/* change the ID's used... */
				if (dtar->id == src_id) {
					dtar->id = dst_id;
					
					/* also check on the subtarget...
					 * XXX: We duplicate the logic from drivers_path_rename_fix() here, with our own
					 *      little twists so that we know that it isn't going to clobber the wrong data
					 */
					if (dtar->rna_path && strstr(dtar->rna_path, "layers[")) {
						GHASH_ITER(gh_iter, afd->names_map) {
							const char *old_name = BLI_ghashIterator_getKey(&gh_iter);
							const char *new_name = BLI_ghashIterator_getValue(&gh_iter);
							
							/* only remap if changed */
							if (!STREQ(old_name, new_name)) {
								if ((dtar->rna_path) && strstr(dtar->rna_path, old_name)) {
									/* Fix up path */
									dtar->rna_path = BKE_animsys_fix_rna_path_rename(id, dtar->rna_path, "layers",
									                                                 old_name, new_name, 0, 0, false);
									break; /* no need to try any more names for layer path */
								}
							}
						}
					}
				}
			}
			DRIVER_TARGETS_LOOPER_END
		}
	}
}

/* join objects called from OBJECT_OT_join */
int ED_gpencil_join_objects_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	Depsgraph *depsgraph = CTX_data_depsgraph(C);
	Object  *obact = CTX_data_active_object(C);
	bGPdata *gpd_dst = NULL;
	bool ok = false;

	/* Ensure we're in right mode and that the active object is correct */
	if (!obact || obact->type != OB_GPENCIL)
		return OPERATOR_CANCELLED;

	bGPdata *gpd = (bGPdata *)obact->data;
	if ((!gpd) || GPENCIL_ANY_MODE(gpd)) {
		return OPERATOR_CANCELLED;
	}

	/* Ensure all rotations are applied before */
	// XXX: Why don't we apply them here instead of warning?
	CTX_DATA_BEGIN(C, Base *, base, selected_editable_bases)
	{
		if (base->object->type == OB_GPENCIL) {
			if ((base->object->rot[0] != 0) || 
				(base->object->rot[1] != 0) || 
				(base->object->rot[2] != 0)) 
			{
				BKE_report(op->reports, RPT_ERROR, "Apply all rotations before join objects");
				return OPERATOR_CANCELLED;
			}
		}
	}
	CTX_DATA_END;

	CTX_DATA_BEGIN(C, Base *, base, selected_editable_bases)
	{
		if (base->object == obact) {
			ok = true;
			break;
		}
	}
	CTX_DATA_END;

	/* that way the active object is always selected */
	if (ok == false) {
		BKE_report(op->reports, RPT_WARNING, "Active object is not a selected grease pencil");
		return OPERATOR_CANCELLED;
	}

	gpd_dst = obact->data;

	/* loop and join all data */
	CTX_DATA_BEGIN(C, Base *, base, selected_editable_bases)
	{
		if ((base->object->type == OB_GPENCIL) && (base->object != obact)) {
			/* we assume that each datablock is not already used in active object */
			if (obact->data != base->object->data) {
				bGPdata *gpd_src = base->object->data;

				/* Apply all GP modifiers before */
				for (ModifierData *md = base->object->modifiers.first; md; md = md->next) {
					const ModifierTypeInfo *mti = modifierType_getInfo(md->type);
					if (mti->bakeModifierGP) {
						mti->bakeModifierGP(C, depsgraph, md, base->object);
					}
				}

				/* copy vertex groups to the base one's */
				int old_idx = 0;
				for (bDeformGroup *dg = base->object->defbase.first; dg; dg = dg->next) {
					bDeformGroup *vgroup = MEM_dupallocN(dg);
					int idx = BLI_listbase_count(&obact->defbase);
					defgroup_unique_name(vgroup, obact);
					BLI_addtail(&obact->defbase, vgroup);
					/* update vertex groups in strokes in original data */
					for (bGPDlayer *gpl_src = gpd->layers.first; gpl_src; gpl_src = gpl_src->next) {
						for (bGPDframe *gpf = gpl_src->frames.first; gpf; gpf = gpf->next) {
							for (bGPDstroke *gps = gpf->strokes.first; gps; gps = gps->next) {
								bGPDspoint *pt;
								int i;
								for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
									if ((pt->weights) && (pt->weights->index == old_idx)) {
										pt->weights->index = idx;
									}
								}
							}
						}
					}
					old_idx++;
				}
				if (obact->defbase.first && obact->actdef == 0)
					obact->actdef = 1;

				/* add missing paletteslots */
				bGPDpaletteref *palslot;
				for (palslot = gpd_src->palette_slots.first; palslot; palslot = palslot->next) {
					if (BKE_gpencil_paletteslot_find(gpd_dst, palslot->palette) == NULL) {
						BKE_gpencil_paletteslot_add(gpd_dst, palslot->palette);
					}
				}

				/* duplicate bGPDlayers  */
				tJoinGPencil_AdtFixData afd = {0};
				afd.src_gpd = gpd_src;
				afd.tar_gpd = gpd_dst;
				afd.names_map = BLI_ghash_str_new("joined_gp_layers_map");
				
				float imat[3][3], bmat[3][3];
				float offset_global[3];
				float offset_local[3];

				sub_v3_v3v3(offset_global, obact->loc, base->object->obmat[3]);
				copy_m3_m4(bmat, obact->obmat);
				invert_m3_m3(imat, bmat);
				mul_m3_v3(imat, offset_global);
				mul_v3_m3v3(offset_local, imat, offset_global);


				for (bGPDlayer *gpl_src = gpd_src->layers.first; gpl_src; gpl_src = gpl_src->next) {
					bGPDlayer *gpl_new = BKE_gpencil_layer_duplicate(gpl_src);
					float diff_mat[4][4];
					float inverse_diff_mat[4][4];

					/* recalculate all stroke points */
					ED_gpencil_parent_location(base->object, gpd_src, gpl_src, diff_mat);
					invert_m4_m4(inverse_diff_mat, diff_mat);

					for (bGPDframe *gpf = gpl_new->frames.first; gpf; gpf = gpf->next) {
						for (bGPDstroke *gps = gpf->strokes.first; gps; gps = gps->next) {
							bGPDspoint *pt;
							int i;
							for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
								float mpt[3];
								mul_v3_m4v3(mpt, inverse_diff_mat, &pt->x);
								sub_v3_v3(mpt, offset_local);
								mul_v3_m4v3(&pt->x, diff_mat, mpt);
							}
						}
					}
					
					/* be sure name is unique in new object */
					BLI_uniquename(&gpd_dst->layers, gpl_new, DATA_("GP_Layer"), '.', offsetof(bGPDlayer, info), sizeof(gpl_new->info));
					BLI_ghash_insert(afd.names_map, BLI_strdup(gpl_src->info), gpl_new->info);
					
					/* add to destination datablock */
					BLI_addtail(&gpd_dst->layers, gpl_new);
				}
				
				/* Fix all the animation data */
				BKE_fcurves_main_cb(bmain, joined_gpencil_fix_animdata_cb, &afd);
				BLI_ghash_free(afd.names_map, MEM_freeN, NULL);
				
				/* Only copy over animdata now, after all the remapping has been done, 
				 * so that we don't have to worry about ambiguities re which datablock
				 * a layer came from!
				 */
				if (base->object->adt) {
					if (obact->adt == NULL) {
						/* no animdata, so just use a copy of the whole thing */
						obact->adt = BKE_animdata_copy(bmain, base->object->adt, false);
					}
					else {
						/* merge in data - we'll fix the drivers manually */
						BKE_animdata_merge_copy(&obact->id, &base->object->id, ADT_MERGECOPY_KEEP_DST, false);
					}
				}
				
				if (gpd_src->adt) {
					if (gpd_dst->adt == NULL) {
						/* no animdata, so just use a copy of the whole thing */
						gpd_dst->adt = BKE_animdata_copy(bmain, gpd_src->adt, false);
					}
					else {
						/* merge in data - we'll fix the drivers manually */
						BKE_animdata_merge_copy(&gpd_dst->id, &gpd_src->id, ADT_MERGECOPY_KEEP_DST, false);
					}
				}
			}

			/* Free the old object */
			ED_object_base_free_and_unlink(bmain, scene, base->object);
		}
	}
	CTX_DATA_END;

	DEG_relations_tag_update(bmain);  /* because we removed object(s) */

	WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);

	return OPERATOR_FINISHED;
}

