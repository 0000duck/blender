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
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * Operators for editing Grease Pencil strokes
 */

/** \file blender/editors/gpencil/gpencil_edit.c
 *  \ingroup edgpencil
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"
#include "DNA_gpencil_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_gpencil.h"
#include "BKE_library.h"
#include "BKE_report.h"
#include "BKE_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "UI_view2d.h"

#include "ED_gpencil.h"
#include "ED_object.h"
#include "ED_view3d.h"

#include "gpencil_intern.h"

/* ************************************************ */
/* Stroke Edit Mode Management */

static int gpencil_editmode_toggle_poll(bContext *C)
{
	return ED_gpencil_data_get_active(C) != NULL;
}

static int gpencil_editmode_toggle_exec(bContext *C, wmOperator *op)
{
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	Scene *scene = CTX_data_scene(C);
	Object *ob = OBACT;
	ScrArea *sa = CTX_wm_area(C);
	
	/* Toggle editmode flag... */
	if (gpd == NULL)
		return OPERATOR_CANCELLED;
	
	/* Grease Pencil in the 3D view should be synced with the active object's
	 * mode setting (ob->mode) which now reflects GPencil status...
	 *
	 * Other editors though can just toggle the mode
	 */
	if (((sa == NULL) || (sa->spacetype == SPACE_VIEW3D)) && 
	    (ob != NULL)) 
	{
		const int mode_flag = OB_MODE_GPENCIL;
		const bool is_mode_set = (ob->mode & mode_flag) != 0;
		
		if (!is_mode_set) {
			if (!ED_object_mode_compat_set(C, ob, mode_flag, op->reports)) {
				return OPERATOR_CANCELLED;
			}
		}
		
		/* Toggle editmode and sync with object mode */
		ob->restore_mode = ob->mode;
		
		if (is_mode_set) {
			gpd->flag &= ~GP_DATA_STROKE_EDITMODE;
			ob->mode &= ~mode_flag;
		}
		else {
			gpd->flag |= GP_DATA_STROKE_EDITMODE;
			ob->mode |= mode_flag;
		}
		
		WM_event_add_notifier(C, NC_SCENE | ND_MODE, NULL);
	}
	else {
		/* Just toggle editmode */
		gpd->flag ^= GP_DATA_STROKE_EDITMODE;
	}
	
	/* GP editmode should have changed if we reach this point... */
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | ND_GPENCIL_EDITMODE, NULL);
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_editmode_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Strokes Edit Mode Toggle";
	ot->idname = "GPENCIL_OT_editmode_toggle";
	ot->description = "Enter/Exit edit mode for Grease Pencil strokes";
	
	/* callbacks */
	ot->exec = gpencil_editmode_toggle_exec;
	ot->poll = gpencil_editmode_toggle_poll;
	
	/* flags */
	ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER;
}

/* ************************************************ */
/* Stroke Editing Operators */

/* poll callback for all stroke editing operators */
static int gp_stroke_edit_poll(bContext *C)
{
	/* NOTE: this is a bit slower, but is the most accurate... */
	return CTX_DATA_COUNT(C, editable_gpencil_strokes) != 0;
}

/* ************** Duplicate Selected Strokes **************** */

/* Make copies of selected point segments in a selected stroke */
static void gp_duplicate_points(const bGPDstroke *gps, ListBase *new_strokes)
{
	bGPDspoint *pt;
	int i;
	
	int start_idx = -1;
	
	
	/* Step through the original stroke's points:
	 * - We accumulate selected points (from start_idx to current index)
	 *   and then convert that to a new stroke
	 */
	for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
		/* searching for start, are waiting for end? */
		if (start_idx == -1) {
			/* is this the first selected point for a new island? */
			if (pt->flag & GP_SPOINT_SELECT) {
				start_idx = i;
			}
		}
		else {
			size_t len = 0;
			
			/* is this the end of current island yet?
			 * 1) Point i-1 was the last one that was selected
			 * 2) Point i is the last in the array
			 */
			if ((pt->flag & GP_SPOINT_SELECT) == 0) {
				len = i - start_idx;
			}
			else if (i == gps->totpoints - 1) {
				len = i - start_idx + 1;
			}
			//printf("copying from %d to %d = %d\n", start_idx, i, len);
		
			/* make copies of the relevant data */
			if (len) {
				bGPDstroke *gpsd;
				
				/* make a stupid copy first of the entire stroke (to get the flags too) */
				gpsd = MEM_dupallocN(gps);
				
				/* now, make a new points array, and copy of the relevant parts */
				gpsd->points = MEM_callocN(sizeof(bGPDspoint) * len, "gps stroke points copy");
				memcpy(gpsd->points, gps->points + start_idx, sizeof(bGPDspoint) * len);
				gpsd->totpoints = len;
				
				/* add to temp buffer */
				gpsd->next = gpsd->prev = NULL;
				BLI_addtail(new_strokes, gpsd);
				
				/* cleanup + reset for next */
				start_idx = -1;
			}
		}
	}
}

static int gp_duplicate_exec(bContext *C, wmOperator *op)
{
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	
	if (gpd == NULL) {
		BKE_report(op->reports, RPT_ERROR, "No Grease Pencil data");
		return OPERATOR_CANCELLED;
	}
	
	/* for each visible (and editable) layer's selected strokes,
	 * copy the strokes into a temporary buffer, then append
	 * once all done
	 */
	CTX_DATA_BEGIN(C, bGPDlayer *, gpl, editable_gpencil_layers)
	{
		ListBase new_strokes = {NULL, NULL};
		bGPDframe *gpf = gpl->actframe;
		bGPDstroke *gps;
		
		if (gpf == NULL)
			continue;
		
		/* make copies of selected strokes, and deselect these once we're done */
		for (gps = gpf->strokes.first; gps; gps = gps->next) {
			/* skip strokes that are invalid for current view */
			if (ED_gpencil_stroke_can_use(C, gps) == false)
				continue;
			
			if (gps->flag & GP_STROKE_SELECT) {
				if (gps->totpoints == 1) {
					/* Special Case: If there's just a single point in this stroke... */
					bGPDstroke *gpsd;
					
					/* make direct copies of the stroke and its points */
					gpsd = MEM_dupallocN(gps);
					gpsd->points = MEM_dupallocN(gps->points);
					
					/* add to temp buffer */
					gpsd->next = gpsd->prev = NULL;
					BLI_addtail(&new_strokes, gpsd);
				}
				else {
					/* delegate to a helper, as there's too much to fit in here (for copying subsets)... */
					gp_duplicate_points(gps, &new_strokes);
				}
				
				/* deselect original stroke, or else the originals get moved too
				 * (when using the copy + move macro)
				 */
				gps->flag &= ~GP_STROKE_SELECT;
			}
		}
		
		/* add all new strokes in temp buffer to the frame (preventing double-copies) */
		BLI_movelisttolist(&gpf->strokes, &new_strokes);
		BLI_assert(new_strokes.first == NULL);
	}
	CTX_DATA_END;
	
	/* updates */
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_duplicate(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Duplicate Strokes";
	ot->idname = "GPENCIL_OT_duplicate";
	ot->description = "Duplicate the selected Grease Pencil strokes";
	
	/* callbacks */
	ot->exec = gp_duplicate_exec;
	ot->poll = gp_stroke_edit_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ******************* Copy/Paste Strokes ************************* */
/* Grease Pencil stroke data copy/paste buffer:
 * - The copy operation collects all segments of selected strokes,
 *   dumping "ready to be copied" copies of the strokes into the buffer.
 * - The paste operation makes a copy of those elements, and adds them
 *   to the active layer. This effectively flattens down the strokes
 *   from several different layers into a single layer.
 */

/* list of bGPDstroke instances */
/* NOTE: is exposed within the editors/gpencil module so that other tools can use it too */
ListBase gp_strokes_copypastebuf = {NULL, NULL};

/* Free copy/paste buffer data */
void ED_gpencil_strokes_copybuf_free(void)
{
	bGPDstroke *gps, *gpsn;
	
	for (gps = gp_strokes_copypastebuf.first; gps; gps = gpsn) {
		gpsn = gps->next;
		
		MEM_freeN(gps->points);
		BLI_freelinkN(&gp_strokes_copypastebuf, gps);
	}
	
	BLI_listbase_clear(&gp_strokes_copypastebuf);
}

/* --------------------- */
/* Copy selected strokes */

static int gp_strokes_copy_exec(bContext *C, wmOperator *op)
{
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	
	if (gpd == NULL) {
		BKE_report(op->reports, RPT_ERROR, "No Grease Pencil data");
		return OPERATOR_CANCELLED;
	}
	
	/* clear the buffer first */
	ED_gpencil_strokes_copybuf_free();
	
	/* for each visible (and editable) layer's selected strokes,
	 * copy the strokes into a temporary buffer, then append
	 * once all done
	 */
	CTX_DATA_BEGIN(C, bGPDlayer *, gpl, editable_gpencil_layers)
	{
		bGPDframe *gpf = gpl->actframe;
		bGPDstroke *gps;
		
		if (gpf == NULL)
			continue;
		
		/* make copies of selected strokes, and deselect these once we're done */
		for (gps = gpf->strokes.first; gps; gps = gps->next) {
			/* skip strokes that are invalid for current view */
			if (ED_gpencil_stroke_can_use(C, gps) == false)
				continue;
			
			if (gps->flag & GP_STROKE_SELECT) {
				if (gps->totpoints == 1) {
					/* Special Case: If there's just a single point in this stroke... */
					bGPDstroke *gpsd;
					
					/* make direct copies of the stroke and its points */
					gpsd = MEM_dupallocN(gps);
					gpsd->points = MEM_dupallocN(gps->points);
					
					/* add to temp buffer */
					gpsd->next = gpsd->prev = NULL;
					BLI_addtail(&gp_strokes_copypastebuf, gpsd);
				}
				else {
					/* delegate to a helper, as there's too much to fit in here (for copying subsets)... */
					gp_duplicate_points(gps, &gp_strokes_copypastebuf);
				}
			}
		}
	}
	CTX_DATA_END;
	
	/* done - no updates needed */
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_copy(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Copy Strokes";
	ot->idname = "GPENCIL_OT_copy";
	ot->description = "Copy selected Grease Pencil points and strokes";
	
	/* callbacks */
	ot->exec = gp_strokes_copy_exec;
	ot->poll = gp_stroke_edit_poll;
	
	/* flags */
	//ot->flag = OPTYPE_REGISTER;
}

/* --------------------- */
/* Paste selected strokes */

static int gp_strokes_paste_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	bGPDlayer *gpl = CTX_data_active_gpencil_layer(C);
	bGPDframe *gpf;
	
	/* check for various error conditions */
	if (gpd == NULL) {
		BKE_report(op->reports, RPT_ERROR, "No Grease Pencil data");
		return OPERATOR_CANCELLED;
	}
	else if (BLI_listbase_is_empty(&gp_strokes_copypastebuf)) {
		BKE_report(op->reports, RPT_ERROR, "No strokes to paste, select and copy some points before trying again");
		return OPERATOR_CANCELLED;
	}
	else if (gpl == NULL) {
		/* no active layer - let's just create one */
		gpl = gpencil_layer_addnew(gpd, DATA_("GP_Layer"), 1);
	}
	else if (gpl->flag & (GP_LAYER_HIDE | GP_LAYER_LOCKED)) {
		BKE_report(op->reports, RPT_ERROR, "Can not paste strokes when active layer is hidden or locked");
		return OPERATOR_CANCELLED;
	}
	else {
		/* Check that some of the strokes in the buffer can be used */
		bGPDstroke *gps;
		bool ok = false;
		
		for (gps = gp_strokes_copypastebuf.first; gps; gps = gps->next) {
			if (ED_gpencil_stroke_can_use(C, gps)) {
				ok = true;
				break;
			}
		}
		
		if (ok == false) {
			/* XXX: this check is not 100% accurate (i.e. image editor is incompatible with normal 2D strokes),
			 * but should be enough to give users a good idea of what's going on
			 */
			if (CTX_wm_area(C)->spacetype == SPACE_VIEW3D)
				BKE_report(op->reports, RPT_ERROR, "Cannot paste 2D strokes in 3D View");
			else
				BKE_report(op->reports, RPT_ERROR, "Cannot paste 3D strokes in 2D editors");
				
			return OPERATOR_CANCELLED;
		}
	}
	
	/* Deselect all strokes first */
	CTX_DATA_BEGIN(C, bGPDstroke *, gps, editable_gpencil_strokes)
	{
		bGPDspoint *pt;
		int i;
		
		for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
			pt->flag &= ~GP_SPOINT_SELECT;
		}
		
		gps->flag &= ~GP_STROKE_SELECT;
	}
	CTX_DATA_END;
	
	/* Ensure we have a frame to draw into
	 * NOTE: Since this is an op which creates strokes,
	 *       we are obliged to add a new frame if one
	 *       doesn't exist already
	 */
	gpf = gpencil_layer_getframe(gpl, CFRA, true);
	
	if (gpf) {
		bGPDstroke *gps;
		
		/* Copy each stroke into the layer */
		for (gps = gp_strokes_copypastebuf.first; gps; gps = gps->next) {
			if (ED_gpencil_stroke_can_use(C, gps)) {
				bGPDstroke *new_stroke = MEM_dupallocN(gps);
				
				new_stroke->points = MEM_dupallocN(gps->points);
				new_stroke->next = new_stroke->prev = NULL;
				
				BLI_addtail(&gpf->strokes, new_stroke);
			}
		}
	}
	
	/* updates */
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_paste(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Paste Strokes";
	ot->idname = "GPENCIL_OT_paste";
	ot->description = "Paste previously copied strokes into active layer";
	
	/* callbacks */
	ot->exec = gp_strokes_paste_exec;
	ot->poll = gp_stroke_edit_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ******************* Move To Layer ****************************** */

static int gp_move_to_layer_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(evt))
{
	uiPopupMenu *pup;
	uiLayout *layout;
	
	/* call the menu, which will call this operator again, hence the canceled */
	pup = UI_popup_menu_begin(C, op->type->name, ICON_NONE);
	layout = UI_popup_menu_layout(pup);
	uiItemsEnumO(layout, "GPENCIL_OT_move_to_layer", "layer");
	UI_popup_menu_end(C, pup);
	
	return OPERATOR_INTERFACE;
}

// FIXME: allow moving partial strokes
static int gp_move_to_layer_exec(bContext *C, wmOperator *op)
{
	bGPdata *gpd = CTX_data_gpencil_data(C);
	bGPDlayer *target_layer = NULL;
	ListBase strokes = {NULL, NULL};
	int layer_num = RNA_enum_get(op->ptr, "layer");
	
	/* Get layer or create new one */
	if (layer_num == -1) {
		/* Create layer */
		target_layer = gpencil_layer_addnew(gpd, DATA_("GP_Layer"), true);
	}
	else {
		/* Try to get layer */
		target_layer = BLI_findlink(&gpd->layers, layer_num);
		
		if (target_layer == NULL) {
			BKE_reportf(op->reports, RPT_ERROR, "There is no layer number %d", layer_num);
			return OPERATOR_CANCELLED;
		}
	}
	
	/* Extract all strokes to move to this layer
	 * NOTE: We need to do this in a two-pass system to avoid conflicts with strokes
	 *       getting repeatedly moved
	 */
	CTX_DATA_BEGIN(C, bGPDlayer *, gpl, editable_gpencil_layers)
	{
		bGPDframe *gpf = gpl->actframe;
		bGPDstroke *gps, *gpsn;
		
		/* skip if no frame with strokes, or if this is the layer we're moving strokes to */
		if ((gpl == target_layer) || (gpf == NULL))
			continue;
		
		/* make copies of selected strokes, and deselect these once we're done */
		for (gps = gpf->strokes.first; gps; gps = gpsn) {
			gpsn = gps->next;
			
			/* skip strokes that are invalid for current view */
			if (ED_gpencil_stroke_can_use(C, gps) == false)
				continue;
			
			/* TODO: Don't just move entire strokes - instead, only copy the selected portions... */
			if (gps->flag & GP_STROKE_SELECT) {
				BLI_remlink(&gpf->strokes, gps);
				BLI_addtail(&strokes, gps);
			}
		}
	}
	CTX_DATA_END;
	
	/* Paste them all in one go */
	if (strokes.first) {
		Scene *scene = CTX_data_scene(C);
		bGPDframe *gpf = gpencil_layer_getframe(target_layer, CFRA, true);
		
		BLI_movelisttolist(&gpf->strokes, &strokes);
		BLI_assert((strokes.first == strokes.last) && (atrokes.first == NULL));
	}
	
	/* updates */
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_move_to_layer(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Move Strokes to Layer";
	ot->idname = "GPENCIL_OT_move_to_layer";
	ot->description = "Move selected strokes to another layer"; // XXX: allow moving individual points too?
	
	/* callbacks */
	ot->invoke = gp_move_to_layer_invoke;
	ot->exec = gp_move_to_layer_exec;
	ot->poll = gp_stroke_edit_poll; // XXX?
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* gp layer to use (dynamic enum) */
	ot->prop = RNA_def_enum(ot->srna, "layer", DummyRNA_DEFAULT_items, 0, "Grease Pencil Layer", "");
	RNA_def_enum_funcs(ot->prop, ED_gpencil_layers_with_new_enum_itemf);
}

/* ******************* Delete Active Frame ************************ */

static int gp_actframe_delete_poll(bContext *C)
{
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	bGPDlayer *gpl = gpencil_layer_getactive(gpd);
	
	/* only if there's an active layer with an active frame */
	return (gpl && gpl->actframe);
}

/* delete active frame - wrapper around API calls */
static int gp_actframe_delete_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	bGPDlayer *gpl = gpencil_layer_getactive(gpd);
	bGPDframe *gpf = gpencil_layer_getframe(gpl, CFRA, 0);
	
	/* if there's no existing Grease-Pencil data there, add some */
	if (gpd == NULL) {
		BKE_report(op->reports, RPT_ERROR, "No grease pencil data");
		return OPERATOR_CANCELLED;
	}
	if (ELEM(NULL, gpl, gpf)) {
		BKE_report(op->reports, RPT_ERROR, "No active frame to delete");
		return OPERATOR_CANCELLED;
	}
	
	/* delete it... */
	gpencil_layer_delframe(gpl, gpf);
	
	/* notifiers */
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_active_frame_delete(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Delete Active Frame";
	ot->idname = "GPENCIL_OT_active_frame_delete";
	ot->description = "Delete the active frame for the active Grease Pencil datablock";
	
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* callbacks */
	ot->exec = gp_actframe_delete_exec;
	ot->poll = gp_actframe_delete_poll;
}

/* ******************* Delete Operator ************************ */

typedef enum eGP_DeleteMode {
	/* delete selected stroke points */
	GP_DELETEOP_POINTS          = 0,
	/* delete selected strokes */
	GP_DELETEOP_STROKES         = 1,
	/* delete active frame */
	GP_DELETEOP_FRAME           = 2,
} eGP_DeleteMode;


/* Delete selected strokes */
static int gp_delete_selected_strokes(bContext *C)
{
	bool changed = false;
	
	CTX_DATA_BEGIN(C, bGPDlayer *, gpl, editable_gpencil_layers)
	{
		bGPDframe *gpf = gpl->actframe;
		bGPDstroke *gps, *gpsn;
		
		if (gpf == NULL)
			continue;
		
		/* simply delete strokes which are selected */
		for (gps = gpf->strokes.first; gps; gps = gpsn) {
			gpsn = gps->next;
			
			/* skip strokes that are invalid for current view */
			if (ED_gpencil_stroke_can_use(C, gps) == false)
				continue;
			
			/* free stroke if selected */
			if (gps->flag & GP_STROKE_SELECT) {
				/* free stroke memory arrays, then stroke itself */
				if (gps->points) MEM_freeN(gps->points);
				BLI_freelinkN(&gpf->strokes, gps);
				
				changed = true;
			}
		}
	}
	CTX_DATA_END;
	
	if (changed) {
		WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

/* Delete selected points but keep the stroke */
static int gp_dissolve_selected_points(bContext *C)
{
	bool changed = false;
	
	CTX_DATA_BEGIN(C, bGPDlayer *, gpl, editable_gpencil_layers)
	{
		bGPDframe *gpf = gpl->actframe;
		bGPDstroke *gps, *gpsn;
		
		if (gpf == NULL)
			continue;
		
		/* simply delete points from selected strokes
		 * NOTE: we may still have to remove the stroke if it ends up having no points!
		 */
		for (gps = gpf->strokes.first; gps; gps = gpsn) {
			gpsn = gps->next;
			
			/* skip strokes that are invalid for current view */
			if (ED_gpencil_stroke_can_use(C, gps) == false)
				continue;
			
			if (gps->flag & GP_STROKE_SELECT) {
				bGPDspoint *pt;
				int i;
				
				int tot = gps->totpoints; /* number of points in new buffer */
				
				/* First Pass: Count how many points are selected (i.e. how many to remove) */
				for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
					if (pt->flag & GP_SPOINT_SELECT) {
						/* selected point - one of the points to remove */
						tot--;
					}
				}
				
				/* if no points are left, we simply delete the entire stroke */
				if (tot <= 0) {
					/* remove the entire stroke */
					MEM_freeN(gps->points);
					BLI_freelinkN(&gpf->strokes, gps);
				}
				else {
					/* just copy all unselected into a smaller buffer */
					bGPDspoint *new_points = MEM_callocN(sizeof(bGPDspoint) * tot, "new gp stroke points copy");
					bGPDspoint *npt        = new_points;
					
					for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
						if ((pt->flag & GP_SPOINT_SELECT) == 0) {
							*npt = *pt;
							npt++;
						}
					}
					
					/* free the old buffer */
					MEM_freeN(gps->points);
					
					/* save the new buffer */
					gps->points = new_points;
					gps->totpoints = tot;
					
					/* deselect the stroke, since none of its selected points will still be selected */
					gps->flag &= ~GP_STROKE_SELECT;
				}
				
				changed = true;
			}
		}
	}
	CTX_DATA_END;
	
	if (changed) {
		WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

/* Split selected strokes into segments, splitting on selected points */
static int gp_delete_selected_points(bContext *C)
{
	bool changed = false;
	
	CTX_DATA_BEGIN(C, bGPDlayer *, gpl, editable_gpencil_layers)
	{
		bGPDframe *gpf = gpl->actframe;
		bGPDstroke *gps, *gpsn;
		
		if (gpf == NULL)
			continue;
		
		/* simply delete strokes which are selected */
		for (gps = gpf->strokes.first; gps; gps = gpsn) {
			gpsn = gps->next;
			
			/* skip strokes that are invalid for current view */
			if (ED_gpencil_stroke_can_use(C, gps) == false)
				continue;
			
			
			if (gps->flag & GP_STROKE_SELECT) {
				bGPDspoint *pt;
				int i;
				
				/* The algorithm used here is as follows:
				 * 1) We firstly identify the number of "islands" of non-selected points
				 *    which will all end up being in new strokes.
				 *    - In the most extreme case (i.e. every other vert is a 1-vert island),
				 *      we have at most n / 2 islands
				 *    - Once we start having larger islands than that, the number required
				 *      becomes much less
				 * 2) Each island gets converted to a new stroke
				 */
				typedef struct tGPDeleteIsland {
					int start_idx;
					int end_idx;
				} tGPDeleteIsland;
				
				tGPDeleteIsland *islands = MEM_callocN(sizeof(tGPDeleteIsland) * (gps->totpoints + 1) / 2, "gp_point_islands");
				bool in_island  = false;
				int num_islands = 0;
				
				/* First Pass: Identify start/end of islands */
				for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
					if (pt->flag & GP_SPOINT_SELECT) {
						/* selected - stop accumulating to island */
						in_island = false;
					}
					else {
						/* unselected - start of a new island? */
						int idx;
						
						if (in_island) {
							/* extend existing island */
							idx = num_islands - 1;
							islands[idx].end_idx = i;
						}
						else {
							/* start of new island */
							in_island = true;
							num_islands++;
							
							idx = num_islands - 1;
							islands[idx].start_idx = islands[idx].end_idx = i;
						}
					}
				}
				
				/* Watch out for special case where No islands = All points selected = Delete Stroke only */
				if (num_islands) {
					/* there are islands, so create a series of new strokes, adding them before the "next" stroke */
					int idx;
					
					/* deselect old stroke, since it will be used as template for the new strokes */
					gps->flag &= ~GP_STROKE_SELECT;
					
					/* create each new stroke... */
					for (idx = 0; idx < num_islands; idx++) {
						tGPDeleteIsland *island = &islands[idx];
						bGPDstroke *new_stroke  = MEM_dupallocN(gps);
						
						/* compute new buffer size (+ 1 needed as the endpoint index is "inclusive") */
						new_stroke->totpoints = island->end_idx - island->start_idx + 1;
						new_stroke->points    = MEM_callocN(sizeof(bGPDspoint) * new_stroke->totpoints, "gp delete stroke fragment");
						
						/* copy over the relevant points */
						memcpy(new_stroke->points, gps->points + island->start_idx, sizeof(bGPDspoint) * new_stroke->totpoints);
						
						/* add new stroke to the frame */
						if (gpsn) {
							BLI_insertlinkbefore(&gpf->strokes, gpsn, new_stroke);
						}
						else {
							BLI_addtail(&gpf->strokes, new_stroke);
						}
					}
				}
				
				/* free islands */
				MEM_freeN(islands);
				
				/* Delete the old stroke */
				MEM_freeN(gps->points);
				BLI_freelinkN(&gpf->strokes, gps);
				
				changed = true;
			}
		}
	}
	CTX_DATA_END;
	
	if (changed) {
		WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}


static int gp_delete_exec(bContext *C, wmOperator *op)
{
	eGP_DeleteMode mode = RNA_enum_get(op->ptr, "type");
	int result = OPERATOR_CANCELLED;
	
	switch (mode) {
		case GP_DELETEOP_STROKES:	/* selected strokes */
			result = gp_delete_selected_strokes(C);
			break;
		
		case GP_DELETEOP_POINTS:	/* selected points (breaks the stroke into segments) */
			result = gp_delete_selected_points(C);
			break;

		case GP_DELETEOP_FRAME:		/* active frame */
			result = gp_actframe_delete_exec(C, op);
			break;
	}
	
	return result;
}

void GPENCIL_OT_delete(wmOperatorType *ot)
{
	static EnumPropertyItem prop_gpencil_delete_types[] = {
		{GP_DELETEOP_POINTS, "POINTS", 0, "Points", "Delete selected points and split strokes into segments"},
		{GP_DELETEOP_STROKES, "STROKES", 0, "Strokes", "Delete selected strokes"},
		{GP_DELETEOP_FRAME, "FRAME", 0, "Frame", "Delete active frame"},
		{0, NULL, 0, NULL, NULL}
	};
	
	/* identifiers */
	ot->name = "Delete...";
	ot->idname = "GPENCIL_OT_delete";
	ot->description = "Delete selected Grease Pencil strokes, vertices, or frames";
	
	/* callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = gp_delete_exec;
	ot->poll = gp_stroke_edit_poll;
	
	/* flags */
	ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER;
	
	/* props */
	ot->prop = RNA_def_enum(ot->srna, "type", prop_gpencil_delete_types, 0, "Type", "Method used for deleting Grease Pencil data");
}

static int gp_dissolve_exec(bContext *C, wmOperator *UNUSED(op))
{
	return gp_dissolve_selected_points(C);
}

void GPENCIL_OT_dissolve(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Dissolve";
	ot->idname = "GPENCIL_OT_dissolve";
	ot->description = "Delete selected points without splitting strokes";

	/* callbacks */
	ot->exec = gp_dissolve_exec;
	ot->poll = gp_stroke_edit_poll;

	/* flags */
	ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER;
}

/* ************************************************ */
