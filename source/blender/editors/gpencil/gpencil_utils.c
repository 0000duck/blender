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
 * The Original Code is Copyright (C) 2014, Blender Foundation
 *
 * Contributor(s): Joshua Leung, Antonio Vazquez
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/gpencil/gpencil_utils.c
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
#include "BLI_rand.h"

#include "DNA_gpencil_types.h"
#include "DNA_brush_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"

#include "BKE_context.h"
#include "BKE_gpencil.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_tracking.h"
#include "BKE_action.h"
#include "BKE_screen.h"

#include "WM_api.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "UI_resources.h"
#include "UI_view2d.h"

#include "ED_gpencil.h"
#include "ED_clip.h"
#include "ED_view3d.h"
#include "ED_object.h"
#include "ED_screen.h"

#include "GPU_immediate.h"
#include "GPU_immediate_util.h"

#include "DEG_depsgraph.h"

#include "gpencil_intern.h"

/* ******************************************************** */
/* Context Wrangling... */

/* Get pointer to active Grease Pencil datablock, and an RNA-pointer to trace back to whatever owns it,
 * when context info is not available.
 */
bGPdata **ED_gpencil_data_get_pointers_direct(ID *screen_id, Scene *scene, ScrArea *sa, Object *ob, PointerRNA *ptr)
{
	/* if there's an active area, check if the particular editor may
	 * have defined any special Grease Pencil context for editing...
	 */
	if (sa) {
		SpaceLink *sl = sa->spacedata.first;
		
		switch (sa->spacetype) {
			case SPACE_VIEW3D: /* 3D-View */
			case SPACE_BUTS: /* properties */
			case SPACE_INFO: /* header info (needed after workspaces merge) */
			{
				/* return obgpencil datablock */
				if (ob && (ob->type == OB_GPENCIL)) {
					if (ptr) RNA_id_pointer_create(&ob->id, ptr);
					return (bGPdata **)&ob->data;
				}
				else {
					return NULL;
				}

				break;
			}
			case SPACE_NODE: /* Nodes Editor */
			{
				SpaceNode *snode = (SpaceNode *)sl;
				
				/* return the GP data for the active node block/node */
				if (snode && snode->nodetree) {
					/* for now, as long as there's an active node tree, default to using that in the Nodes Editor */
					if (ptr) RNA_id_pointer_create(&snode->nodetree->id, ptr);
					return &snode->nodetree->gpd;
				}
				
				/* even when there is no node-tree, don't allow this to flow to scene */
				return NULL;
			}
			case SPACE_SEQ: /* Sequencer */
			{
				SpaceSeq *sseq = (SpaceSeq *)sl;
			
				/* for now, Grease Pencil data is associated with the space (actually preview region only) */
				/* XXX our convention for everything else is to link to data though... */
				if (ptr) RNA_pointer_create(screen_id, &RNA_SpaceSequenceEditor, sseq, ptr);
				return &sseq->gpd;
			}
			case SPACE_IMAGE: /* Image/UV Editor */
			{
				SpaceImage *sima = (SpaceImage *)sl;
				
				/* for now, Grease Pencil data is associated with the space... */
				/* XXX our convention for everything else is to link to data though... */
				if (ptr) RNA_pointer_create(screen_id, &RNA_SpaceImageEditor, sima, ptr);
				return &sima->gpd;
			}
			case SPACE_CLIP: /* Nodes Editor */
			{
				SpaceClip *sc = (SpaceClip *)sl;
				MovieClip *clip = ED_space_clip_get_clip(sc);
				
				if (clip) {
					if (sc->gpencil_src == SC_GPENCIL_SRC_TRACK) {
						MovieTrackingTrack *track = BKE_tracking_track_get_active(&clip->tracking);
						
						if (!track)
							return NULL;
						
						if (ptr)
							RNA_pointer_create(&clip->id, &RNA_MovieTrackingTrack, track, ptr);
						
						return &track->gpd;
					}
					else {
						if (ptr)
							RNA_id_pointer_create(&clip->id, ptr);
						
						return &clip->gpd;
					}
				}
				break;
			}
			default: /* unsupported space */
				return NULL;
		}
	}
	
	/* just fall back on the scene's GP data */
	if (ptr) RNA_id_pointer_create((ID *)scene, ptr);
	return (scene) ? &scene->gpd : NULL;
}

/* Get pointer to active Grease Pencil datablock, and an RNA-pointer to trace back to whatever owns it */
bGPdata **ED_gpencil_data_get_pointers(const bContext *C, PointerRNA *ptr)
{
	ID *screen_id = (ID *)CTX_wm_screen(C);
	Scene *scene = CTX_data_scene(C);
	ScrArea *sa = CTX_wm_area(C);
	Object *ob = CTX_data_active_object(C);

	return ED_gpencil_data_get_pointers_direct(screen_id, scene, sa, ob, ptr);
}

/* -------------------------------------------------------- */

/* Get the active Grease Pencil datablock, when context is not available */
bGPdata *ED_gpencil_data_get_active_direct(ID *screen_id, Scene *scene, ScrArea *sa, Object *ob)
{
	bGPdata **gpd_ptr = ED_gpencil_data_get_pointers_direct(screen_id, scene, sa, ob, NULL);
	return (gpd_ptr) ? *(gpd_ptr) : NULL;
}

/* Get the active Grease Pencil datablock */
bGPdata *ED_gpencil_data_get_active(const bContext *C)
{
	bGPdata **gpd_ptr = ED_gpencil_data_get_pointers(C, NULL);
	return (gpd_ptr) ? *(gpd_ptr) : NULL;
}

/* -------------------------------------------------------- */

// XXX: this should be removed... We really shouldn't duplicate logic like this!
bGPdata *ED_gpencil_data_get_active_v3d(Scene *scene, View3D *v3d)
{
	BaseLegacy *base = scene->basact;
	bGPdata *gpd = NULL;
	/* We have to make sure active object is actually visible and selected, else we must use default scene gpd,
	 * to be consistent with ED_gpencil_data_get_active's behavior.
	 */
	
	if (base && TESTBASE(v3d, base)) {
		gpd = base->object->gpd;
	}
	return gpd ? gpd : scene->gpd;
}

/* ******************************************************** */
/* Keyframe Indicator Checks */

/* Check whether there's an active GP keyframe on the current frame */
bool ED_gpencil_has_keyframe_v3d(Scene *UNUSED(scene), Object *ob, int cfra)
{
	if (ob && ob->data) {
		bGPDlayer *gpl = BKE_gpencil_layer_getactive(ob->data);
		if (gpl) {
			if (gpl->actframe) {
				// XXX: assumes that frame has been fetched already
				return (gpl->actframe->framenum == cfra);
			}
			else {
				/* XXX: disabled as could be too much of a penalty */
				/* return BKE_gpencil_layer_find_frame(gpl, cfra); */
			}
		}
	}
	
	return false;
}

/* ******************************************************** */
/* Poll Callbacks */

/* poll callback for adding data/layers - special */
int gp_add_poll(bContext *C)
{
	/* the base line we have is that we have somewhere to add Grease Pencil data */
	return ED_gpencil_data_get_pointers(C, NULL) != NULL;
}

/* poll callback for checking if there is an active layer */
int gp_active_layer_poll(bContext *C)
{
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	bGPDlayer *gpl = BKE_gpencil_layer_getactive(gpd);
	
	return (gpl != NULL);
}

/* poll callback for checking if there is an active brush */
int gp_active_brush_poll(bContext *C)
{
	ToolSettings *ts = CTX_data_tool_settings(C);
	bGPDbrush *brush = BKE_gpencil_brush_getactive(ts);

	return (brush != NULL);
}

/* ******************************************************** */
/* Dynamic Enums of GP Layers */
/* NOTE: These include an option to create a new layer and use that... */

/* Just existing layers */
const EnumPropertyItem *ED_gpencil_layers_enum_itemf(
        bContext *C, PointerRNA *UNUSED(ptr), PropertyRNA *UNUSED(prop), bool *r_free)
{
	bGPdata *gpd = CTX_data_gpencil_data(C);
	bGPDlayer *gpl;
	EnumPropertyItem *item = NULL, item_tmp = {0};
	int totitem = 0;
	int i = 0;
	
	if (ELEM(NULL, C, gpd)) {
		return DummyRNA_DEFAULT_items;
	}
	
	/* Existing layers */
	for (gpl = gpd->layers.first; gpl; gpl = gpl->next, i++) {
		item_tmp.identifier = gpl->info;
		item_tmp.name = gpl->info;
		item_tmp.value = i;
		
		if (gpl->flag & GP_LAYER_ACTIVE)
			item_tmp.icon = ICON_GREASEPENCIL;
		else 
			item_tmp.icon = ICON_NONE;
		
		RNA_enum_item_add(&item, &totitem, &item_tmp);
	}
	
	RNA_enum_item_end(&item, &totitem);
	*r_free = true;

	return item;
}

/* Existing + Option to add/use new layer */
const EnumPropertyItem *ED_gpencil_layers_with_new_enum_itemf(
        bContext *C, PointerRNA *UNUSED(ptr), PropertyRNA *UNUSED(prop), bool *r_free)
{
	bGPdata *gpd = CTX_data_gpencil_data(C);
	bGPDlayer *gpl;
	EnumPropertyItem *item = NULL, item_tmp = {0};
	int totitem = 0;
	int i = 0;
	
	if (ELEM(NULL, C, gpd)) {
		return DummyRNA_DEFAULT_items;
	}
	
	/* Create new layer */
	/* TODO: have some way of specifying that we don't want this? */
	{
		/* active Keying Set */
		item_tmp.identifier = "__CREATE__";
		item_tmp.name = "New Layer";
		item_tmp.value = -1;
		item_tmp.icon = ICON_ZOOMIN;
		RNA_enum_item_add(&item, &totitem, &item_tmp);
		
		/* separator */
		RNA_enum_item_add_separator(&item, &totitem);
	}
	
	/* Existing layers */
	for (gpl = gpd->layers.first, i = 0; gpl; gpl = gpl->next, i++) {
		item_tmp.identifier = gpl->info;
		item_tmp.name = gpl->info;
		item_tmp.value = i;
		
		if (gpl->flag & GP_LAYER_ACTIVE)
			item_tmp.icon = ICON_GREASEPENCIL;
		else 
			item_tmp.icon = ICON_NONE;
		
		RNA_enum_item_add(&item, &totitem, &item_tmp);
	}
	
	RNA_enum_item_end(&item, &totitem);
	*r_free = true;

	return item;
}



/* ******************************************************** */
/* Brush Tool Core */

/**
 * Check whether a given stroke segment is inside a circular brush
 *
 * \param mval     The current screen-space coordinates (midpoint) of the brush
 * \param mvalo    The previous screen-space coordinates (midpoint) of the brush (NOT CURRENTLY USED)
 * \param rad      The radius of the brush
 *
 * \param x0, y0   The screen-space x and y coordinates of the start of the stroke segment
 * \param x1, y1   The screen-space x and y coordinates of the end of the stroke segment
 */
bool gp_stroke_inside_circle(const int mval[2], const int UNUSED(mvalo[2]),
                             int rad, int x0, int y0, int x1, int y1)
{
	/* simple within-radius check for now */
	const float mval_fl[2]     = {mval[0], mval[1]};
	const float screen_co_a[2] = {x0, y0};
	const float screen_co_b[2] = {x1, y1};
	
	if (edge_inside_circle(mval_fl, rad, screen_co_a, screen_co_b)) {
		return true;
	}
	
	/* not inside */
	return false;
}

/* ******************************************************** */
/* Stroke Validity Testing */

/* Check whether given stroke can be edited given the supplied context */
// XXX: do we need additional flags for screenspace vs dataspace?
bool ED_gpencil_stroke_can_use_direct(const ScrArea *sa, const bGPDstroke *gps)
{
	/* sanity check */
	if (ELEM(NULL, sa, gps))
		return false;

	/* filter stroke types by flags + spacetype */
	if (gps->flag & GP_STROKE_3DSPACE) {
		/* 3D strokes - only in 3D view */
		return ((sa->spacetype == SPACE_VIEW3D) || (sa->spacetype == SPACE_BUTS));
	}
	else if (gps->flag & GP_STROKE_2DIMAGE) {
		/* Special "image" strokes - only in Image Editor */
		return (sa->spacetype == SPACE_IMAGE);
	}
	else if (gps->flag & GP_STROKE_2DSPACE) {
		/* 2D strokes (dataspace) - for any 2D view (i.e. everything other than 3D view) */
		return (sa->spacetype != SPACE_VIEW3D);
	}
	else {
		/* view aligned - anything goes */
		return true;
	}
}

/* Check whether given stroke can be edited in the current context */
bool ED_gpencil_stroke_can_use(const bContext *C, const bGPDstroke *gps)
{
	ScrArea *sa = CTX_wm_area(C);
	return ED_gpencil_stroke_can_use_direct(sa, gps);
}

/* Check whether given stroke can be edited for the current color */
bool ED_gpencil_stroke_color_use(const bGPDlayer *gpl, const bGPDstroke *gps)
{
	/* check if the color is editable */
	PaletteColor *palcolor = gps->palcolor;
	if ((gps->palette) && (palcolor != NULL)) {
		if (palcolor->flag & PC_COLOR_HIDE)
			return false;
		if (((gpl->flag & GP_LAYER_UNLOCK_COLOR) == 0) && (palcolor->flag & PC_COLOR_LOCKED))
			return false;
	}
	
	return true;
}

/* ******************************************************** */
/* Space Conversion */

/**
 * Init settings for stroke point space conversions
 *
 * \param r_gsc: [out] The space conversion settings struct, populated with necessary params
 */
void gp_point_conversion_init(bContext *C, GP_SpaceConversion *r_gsc)
{
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = CTX_wm_region(C);
	
	/* zero out the storage (just in case) */
	memset(r_gsc, 0, sizeof(GP_SpaceConversion));
	unit_m4(r_gsc->mat);
	
	/* store settings */
	r_gsc->sa = sa;
	r_gsc->ar = ar;
	r_gsc->v2d = &ar->v2d;
	
	/* init region-specific stuff */
	if (sa->spacetype == SPACE_VIEW3D) {
		wmWindow *win = CTX_wm_window(C);
		Scene *scene = CTX_data_scene(C);
		struct Depsgraph *graph = CTX_data_depsgraph(C);
		View3D *v3d = (View3D *)CTX_wm_space_data(C);
		RegionView3D *rv3d = ar->regiondata;
		
		EvaluationContext eval_ctx;
		CTX_data_eval_ctx(C, &eval_ctx);

		/* init 3d depth buffers */
		view3d_operator_needs_opengl(C);
		
		view3d_region_operator_needs_opengl(win, ar);
		ED_view3d_autodist_init(&eval_ctx, graph, ar, v3d, 0);
		
		/* for camera view set the subrect */
		if (rv3d->persp == RV3D_CAMOB) {
			ED_view3d_calc_camera_border(scene, ar, v3d, rv3d, &r_gsc->subrect_data, true); /* no shift */
			r_gsc->subrect = &r_gsc->subrect_data;
		}
	}
}

/**
 * Convert point to parent space
 *
 * \param pt         Original point
 * \param diff_mat   Matrix with the difference between original parent matrix
 * \param[out] r_pt  Pointer to new point after apply matrix
 */
void gp_point_to_parent_space(bGPDspoint *pt, float diff_mat[4][4], bGPDspoint *r_pt) 
{
	float fpt[3];

	mul_v3_m4v3(fpt, diff_mat, &pt->x);
	copy_v3_v3(&r_pt->x, fpt);
}

/**
 * Change position relative to parent object 
 */
void gp_apply_parent(Object *obact, bGPdata *gpd, bGPDlayer *gpl, bGPDstroke *gps)
{
	bGPDspoint *pt;
	int i;

	/* undo matrix */
	float diff_mat[4][4];
	float inverse_diff_mat[4][4];
	float fpt[3];

	ED_gpencil_parent_location(obact, gpd, gpl, diff_mat);
	invert_m4_m4(inverse_diff_mat, diff_mat);

	for (i = 0; i < gps->totpoints; i++) {
		pt = &gps->points[i];
		mul_v3_m4v3(fpt, inverse_diff_mat, &pt->x);
		copy_v3_v3(&pt->x, fpt);
	}
}

/** 
 * Change point position relative to parent object 
 */
void gp_apply_parent_point(Object *obact, bGPdata *gpd, bGPDlayer *gpl, bGPDspoint *pt)
{
	/* undo matrix */
	float diff_mat[4][4];
	float inverse_diff_mat[4][4];
	float fpt[3];

	ED_gpencil_parent_location(obact, gpd, gpl, diff_mat);
	invert_m4_m4(inverse_diff_mat, diff_mat);

	mul_v3_m4v3(fpt, inverse_diff_mat, &pt->x);
	copy_v3_v3(&pt->x, fpt);
}

/**
 * Convert a Grease Pencil coordinate (i.e. can be 2D or 3D) to screenspace (2D)
 *
 * \param[out] r_x  The screen-space x-coordinate of the point
 * \param[out] r_y  The screen-space y-coordinate of the point
 *
 * \warning This assumes that the caller has already checked whether the stroke in question can be drawn.
 */
void gp_point_to_xy(GP_SpaceConversion *gsc, bGPDstroke *gps, bGPDspoint *pt,
                    int *r_x, int *r_y)
{
	ARegion *ar = gsc->ar;
	View2D *v2d = gsc->v2d;
	rctf *subrect = gsc->subrect;
	int xyval[2];
	
	/* sanity checks */
	BLI_assert(!(gps->flag & GP_STROKE_3DSPACE) || (gsc->sa->spacetype == SPACE_VIEW3D));
	BLI_assert(!(gps->flag & GP_STROKE_2DSPACE) || (gsc->sa->spacetype != SPACE_VIEW3D));
	
	
	if (gps->flag & GP_STROKE_3DSPACE) {
		if (ED_view3d_project_int_global(ar, &pt->x, xyval, V3D_PROJ_TEST_NOP) == V3D_PROJ_RET_OK) {
			*r_x = xyval[0];
			*r_y = xyval[1];
		}
		else {
			*r_x = V2D_IS_CLIPPED;
			*r_y = V2D_IS_CLIPPED;
		}
	}
	else if (gps->flag & GP_STROKE_2DSPACE) {
		float vec[3] = {pt->x, pt->y, 0.0f};
		mul_m4_v3(gsc->mat, vec);
		UI_view2d_view_to_region_clip(v2d, vec[0], vec[1], r_x, r_y);
	}
	else {
		if (subrect == NULL) {
			/* normal 3D view (or view space) */
			*r_x = (int)(pt->x / 100 * ar->winx);
			*r_y = (int)(pt->y / 100 * ar->winy);
		}
		else {
			/* camera view, use subrect */
			*r_x = (int)((pt->x / 100) * BLI_rctf_size_x(subrect)) + subrect->xmin;
			*r_y = (int)((pt->y / 100) * BLI_rctf_size_y(subrect)) + subrect->ymin;
		}
	}
}

/**
 * Convert a Grease Pencil coordinate (i.e. can be 2D or 3D) to screenspace (2D)
 *
 * Just like gp_point_to_xy(), except the resulting coordinates are floats not ints.
 * Use this version to solve "stair-step" artifacts which may arise when roundtripping the calculations.
 *
 * \param r_x: [out] The screen-space x-coordinate of the point
 * \param r_y: [out] The screen-space y-coordinate of the point
 *
 * \warning This assumes that the caller has already checked whether the stroke in question can be drawn
 */
void gp_point_to_xy_fl(GP_SpaceConversion *gsc, bGPDstroke *gps, bGPDspoint *pt,
                       float *r_x, float *r_y)
{
	ARegion *ar = gsc->ar;
	View2D *v2d = gsc->v2d;
	rctf *subrect = gsc->subrect;
	float xyval[2];
	
	/* sanity checks */
	BLI_assert(!(gps->flag & GP_STROKE_3DSPACE) || (gsc->sa->spacetype == SPACE_VIEW3D));
	BLI_assert(!(gps->flag & GP_STROKE_2DSPACE) || (gsc->sa->spacetype != SPACE_VIEW3D));
	
	
	if (gps->flag & GP_STROKE_3DSPACE) {
		if (ED_view3d_project_float_global(ar, &pt->x, xyval, V3D_PROJ_TEST_NOP) == V3D_PROJ_RET_OK) {
			*r_x = xyval[0];
			*r_y = xyval[1];
		}
		else {
			*r_x = 0.0f;
			*r_y = 0.0f;
		}
	}
	else if (gps->flag & GP_STROKE_2DSPACE) {
		float vec[3] = {pt->x, pt->y, 0.0f};
		int t_x, t_y;
		
		mul_m4_v3(gsc->mat, vec);
		UI_view2d_view_to_region_clip(v2d, vec[0], vec[1], &t_x, &t_y);
		
		if ((t_x == t_y) && (t_x == V2D_IS_CLIPPED)) {
			/* XXX: Or should we just always use the values as-is? */
			*r_x = 0.0f;
			*r_y = 0.0f;
		}
		else {
			*r_x = (float)t_x;
			*r_y = (float)t_y;
		}
	}
	else {
		if (subrect == NULL) {
			/* normal 3D view (or view space) */
			*r_x = (pt->x / 100.0f * ar->winx);
			*r_y = (pt->y / 100.0f * ar->winy);
		}
		else {
			/* camera view, use subrect */
			*r_x = ((pt->x / 100.0f) * BLI_rctf_size_x(subrect)) + subrect->xmin;
			*r_y = ((pt->y / 100.0f) * BLI_rctf_size_y(subrect)) + subrect->ymin;
		}
	}
}

/**
 * Project screenspace coordinates to 3D-space
 *
 * For use with editing tools where it is easier to perform the operations in 2D,
 * and then later convert the transformed points back to 3D.
 *
 * \param screen_co: The screenspace 2D coordinates to convert to
 * \param r_out: The resulting 3D coordinates of the input point
 *
 * \note We include this as a utility function, since the standard method
 * involves quite a few steps, which are invariably always the same
 * for all GPencil operations. So, it's nicer to just centralize these.
 *
 * \warning Assumes that it is getting called in a 3D view only.
 */
bool gp_point_xy_to_3d(GP_SpaceConversion *gsc, Scene *scene, const float screen_co[2], float r_out[3])
{
	View3D *v3d = gsc->sa->spacedata.first;
	RegionView3D *rv3d = gsc->ar->regiondata;
	float *rvec = ED_view3d_cursor3d_get(scene, v3d);
	float ref[3] = {rvec[0], rvec[1], rvec[2]};
	float zfac = ED_view3d_calc_zfac(rv3d, rvec, NULL);
	
	float mval_f[2], mval_prj[2];
	float dvec[3];
	
	copy_v2_v2(mval_f, screen_co);
	
	if (ED_view3d_project_float_global(gsc->ar, ref, mval_prj, V3D_PROJ_TEST_NOP) == V3D_PROJ_RET_OK) {
		sub_v2_v2v2(mval_f, mval_prj, mval_f);
		ED_view3d_win_to_delta(gsc->ar, mval_f, dvec, zfac);
		sub_v3_v3v3(r_out, rvec, dvec);
		
		return true;
	}
	else {
		zero_v3(r_out);
		
		return false;
	}
}

/**
 * Apply smooth to stroke point
 * \param gps              Stroke to smooth
 * \param i                Point index
 * \param inf              Amount of smoothing to apply
 * \param affect_pressure  Apply smoothing to pressure values too?
 */
bool gp_smooth_stroke(bGPDstroke *gps, int i, float inf, bool affect_pressure)
{
	bGPDspoint *pt = &gps->points[i];
	float pressure = 0.0f;
	float sco[3] = {0.0f};
	
	/* Do nothing if not enough points to smooth out */
	if (gps->totpoints <= 2) {
		return false;
	}
	
	/* Only affect endpoints by a fraction of the normal strength,
	 * to prevent the stroke from shrinking too much
	 */
	if ((i == 0) || (i == gps->totpoints - 1)) {
		inf *= 0.1f;
	}
	
	/* Compute smoothed coordinate by taking the ones nearby */
	/* XXX: This is potentially slow, and suffers from accumulation error as earlier points are handled before later ones */
	{
		// XXX: this is hardcoded to look at 2 points on either side of the current one (i.e. 5 items total)
		const int   steps = 2;
		const float average_fac = 1.0f / (float)(steps * 2 + 1);
		int step;
		
		/* add the point itself */
		madd_v3_v3fl(sco, &pt->x, average_fac);
		
		if (affect_pressure) {
			pressure += pt->pressure * average_fac;
		}
		
		/* n-steps before/after current point */
		// XXX: review how the endpoints are treated by this algorithm
		// XXX: falloff measures should also introduce some weighting variations, so that further-out points get less weight
		for (step = 1; step <= steps; step++) {
			bGPDspoint *pt1, *pt2;
			int before = i - step;
			int after = i + step;
			
			CLAMP_MIN(before, 0);
			CLAMP_MAX(after, gps->totpoints - 1);
			
			pt1 = &gps->points[before];
			pt2 = &gps->points[after];
			
			/* add both these points to the average-sum (s += p[i]/n) */
			madd_v3_v3fl(sco, &pt1->x, average_fac);
			madd_v3_v3fl(sco, &pt2->x, average_fac);
			
#if 0
			/* XXX: Disabled because get weird result */
			/* do pressure too? */
			if (affect_pressure) {
				pressure += pt1->pressure * average_fac;
				pressure += pt2->pressure * average_fac;
			}
#endif
		}
	}
	
	/* Based on influence factor, blend between original and optimal smoothed coordinate */
	interp_v3_v3v3(&pt->x, &pt->x, sco, inf);
	
#if 0
	/* XXX: Disabled because get weird result */
	if (affect_pressure) {
		pt->pressure = pressure;
	}
#endif
	
	return true;
}

/**
* Apply smooth for strength to stroke point
* \param gps              Stroke to smooth
* \param i                Point index
* \param inf              Amount of smoothing to apply
*/
bool gp_smooth_stroke_strength(bGPDstroke *gps, int i, float inf)
{
	bGPDspoint *ptb = &gps->points[i];

	/* Do nothing if not enough points */
	if (gps->totpoints <= 2) {
		return false;
	}

	/* Compute theoretical optimal value using distances */
	bGPDspoint *pta, *ptc;
	int before = i - 1;
	int after = i + 1;

	CLAMP_MIN(before, 0);
	CLAMP_MAX(after, gps->totpoints - 1);

	pta = &gps->points[before];
	ptc = &gps->points[after];

	/* the optimal value is the corresponding to the interpolation of the strength
	 *  at the distance of point b
	 */
	const float fac = line_point_factor_v3(&ptb->x, &pta->x, &ptc->x);
	const float optimal = (1.0f - fac) * pta->strength + fac * ptc->strength;

	/* Based on influence factor, blend between original and optimal */
	ptb->strength = (1.0f - inf) * ptb->strength + inf * optimal;

	return true;
}

/**
* Apply smooth for thickness to stroke point (use pressure)
* \param gps              Stroke to smooth
* \param i                Point index
* \param inf              Amount of smoothing to apply
*/
bool gp_smooth_stroke_thickness(bGPDstroke *gps, int i, float inf)
{
	bGPDspoint *ptb = &gps->points[i];

	/* Do nothing if not enough points */
	if (gps->totpoints <= 2) {
		return false;
	}

	/* Compute theoretical optimal value using distances */
	bGPDspoint *pta, *ptc;
	int before = i - 1;
	int after = i + 1;

	CLAMP_MIN(before, 0);
	CLAMP_MAX(after, gps->totpoints - 1);

	pta = &gps->points[before];
	ptc = &gps->points[after];

	/* the optimal value is the corresponding to the interpolation of the pressure
	 *  at the distance of point b
	 */
	float fac = line_point_factor_v3(&ptb->x, &pta->x, &ptc->x);
	float optimal = (1.0f - fac) * pta->pressure + fac * ptc->pressure;

	/* Based on influence factor, blend between original and optimal */
	ptb->pressure = (1.0f - inf) * ptb->pressure + inf * optimal;

	return true;
}

/**
 * Subdivide a stroke once, by adding a point half way between each pair of existing points
 * \param gps           Stroke data
 * \param new_totpoints Total number of points (after subdividing)
 */
void gp_subdivide_stroke(bGPDstroke *gps, const int new_totpoints)
{
	/* Move points towards end of enlarged points array to leave space for new points */
	int y = 1;
	for (int i = gps->totpoints - 1; i > 0; i--) {
		gps->points[new_totpoints - y] = gps->points[i];
		y += 2;
	}
	
	/* Create interpolated points */
	for (int i = 0; i < new_totpoints - 1; i += 2) {
		bGPDspoint *prev  = &gps->points[i];
		bGPDspoint *pt    = &gps->points[i + 1];
		bGPDspoint *next  = &gps->points[i + 2];
		
		/* Interpolate all values */
		interp_v3_v3v3(&pt->x, &prev->x, &next->x, 0.5f);
		
		pt->pressure = interpf(prev->pressure, next->pressure, 0.5f);
		pt->strength = interpf(prev->strength, next->strength, 0.5f);
		CLAMP(pt->strength, GPENCIL_STRENGTH_MIN, 1.0f);
		pt->time = interpf(prev->time, next->time, 0.5f);
	}
	
	/* Update to new total number of points */
	gps->totpoints = new_totpoints;
}

/**
 * Add randomness to stroke
 * \param gps           Stroke data
 * \param brush         Brush data
 */
void gp_randomize_stroke(bGPDstroke *gps, bGPDbrush *brush)
{
	bGPDspoint *pt1, *pt2, *pt3;
	float v1[3];
	float v2[3];
	if (gps->totpoints < 3) {
		return;
	}

	/* get two vectors using 3 points */
	pt1 = &gps->points[0];
	pt2 = &gps->points[1];
	pt3 = &gps->points[(int)(gps->totpoints * 0.75)];

	sub_v3_v3v3(v1, &pt2->x, &pt1->x);
	sub_v3_v3v3(v2, &pt3->x, &pt2->x);
	normalize_v3(v1);
	normalize_v3(v2);

	/* get normal vector to plane created by two vectors */
	float normal[3];
	cross_v3_v3v3(normal, v1, v2);
	normalize_v3(normal);
	
	/* get orthogonal vector to plane to rotate random effect */
	float ortho[3];
	cross_v3_v3v3(ortho, v1, normal);
	normalize_v3(ortho);
	
	/* Read all points and apply shift vector (first and last point not modified) */
	for (int i = 1; i < gps->totpoints - 1; ++i) {
		bGPDspoint *pt = &gps->points[i];
		/* get vector with shift (apply a division because random is too sensitive */
		const float fac = BLI_frand() * (brush->draw_random_sub / 10.0f);
		float svec[3];
		copy_v3_v3(svec, ortho);
		if (BLI_frand() > 0.5f) {
			mul_v3_fl(svec, -fac);
		}
		else {
			mul_v3_fl(svec, fac);
		}

		/* apply shift */
		add_v3_v3(&pt->x, svec);
	}

}
/* calculate difference matrix */
void ED_gpencil_parent_location(Object *obact, bGPdata *gpd, bGPDlayer *gpl, float diff_mat[4][4])
{
	Object *obparent = gpl->parent;

	/* if not layer parented, try with object parented */
	if (obparent == NULL) {
		if (obact != NULL) {
			/* the gpd can be scene, but a gpobject can be active, so need check gpd */
			if ((obact->type == OB_GPENCIL) && (obact->data == gpd)) {
				copy_m4_m4(diff_mat, obact->obmat);
				return;
			}
		}
		/* not gpencil object */
		unit_m4(diff_mat);
		return;
	}
	else {
		if ((gpl->partype == PAROBJECT) || (gpl->partype == PARSKEL)) {
			mul_m4_m4m4(diff_mat, obparent->obmat, gpl->inverse);
			return;
		}
		else if (gpl->partype == PARBONE) {
			bPoseChannel *pchan = BKE_pose_channel_find_name(obparent->pose, gpl->parsubstr);
			if (pchan) {
				float tmp_mat[4][4];
				mul_m4_m4m4(tmp_mat, obparent->obmat, pchan->pose_mat);
				mul_m4_m4m4(diff_mat, tmp_mat, gpl->inverse);
			}
			else {
				mul_m4_m4m4(diff_mat, obparent->obmat, gpl->inverse); /* if bone not found use object (armature) */
			}
			return;
		}
		else {
			unit_m4(diff_mat); /* not defined type */
		}
	}
}

/* reset parent matrix for all layers */
void ED_gpencil_reset_layers_parent(Object *obact, bGPdata *gpd)
{
	bGPDspoint *pt;
	int i;
	float diff_mat[4][4];
	float cur_mat[4][4];

	for (bGPDlayer *gpl = gpd->layers.first; gpl; gpl = gpl->next) {
		if (gpl->parent != NULL) {
			/* calculate new matrix */
			if ((gpl->partype == PAROBJECT) || (gpl->partype == PARSKEL)) {
				invert_m4_m4(cur_mat, gpl->parent->obmat);
			}
			else if (gpl->partype == PARBONE) {
				bPoseChannel *pchan = BKE_pose_channel_find_name(gpl->parent->pose, gpl->parsubstr);
				if (pchan) {
					float tmp_mat[4][4];
					mul_m4_m4m4(tmp_mat, gpl->parent->obmat, pchan->pose_mat);
					invert_m4_m4(cur_mat, tmp_mat);
				}
			}

			/* only redo if any change */
			if (!equals_m4m4(gpl->inverse, cur_mat)) {
				/* first apply current transformation to all strokes */
				ED_gpencil_parent_location(obact, gpd, gpl, diff_mat);
				for (bGPDframe *gpf = gpl->frames.first; gpf; gpf = gpf->next) {
					for (bGPDstroke *gps = gpf->strokes.first; gps; gps = gps->next) {
						for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
							mul_m4_v3(diff_mat, &pt->x);
						}
					}
				}
				/* set new parent matrix */
				copy_m4_m4(gpl->inverse, cur_mat);
			}
		}
	}
}
/* ******************************************************** */
bool ED_gpencil_stroke_minmax(
        const bGPDstroke *gps, const bool use_select,
        float r_min[3], float r_max[3])
{
	const bGPDspoint *pt;
	int i;
	bool changed = false;

	for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
		if ((use_select == false) || (pt->flag & GP_SPOINT_SELECT)) {;
			minmax_v3v3_v3(r_min, r_max, &pt->x);
			changed = true;
		}
	}
	return changed;
}

/* Dynamic Enums of GP Brushes */
const EnumPropertyItem *ED_gpencil_brushes_enum_itemf(
        bContext *C, PointerRNA *UNUSED(ptr), PropertyRNA *UNUSED(prop),
        bool *r_free)
{
	ToolSettings *ts = CTX_data_tool_settings(C);
	bGPDbrush *brush;
	EnumPropertyItem *item = NULL, item_tmp = { 0 };
	int totitem = 0;
	int i = 0;

	if (ELEM(NULL, C, ts)) {
		return DummyRNA_DEFAULT_items;
	}

	/* Existing brushes */
	for (brush = ts->gp_brushes.first; brush; brush = brush->next, i++) {
		item_tmp.identifier = brush->info;
		item_tmp.name = brush->info;
		item_tmp.value = i;

		if (brush->flag & GP_BRUSH_ACTIVE)
			item_tmp.icon = ICON_BRUSH_DATA;
		else
			item_tmp.icon = ICON_NONE;

		RNA_enum_item_add(&item, &totitem, &item_tmp);
	}

	RNA_enum_item_end(&item, &totitem);
	*r_free = true;

	return item;
}

/* Dynamic Enums of GP Palettes */
const EnumPropertyItem *ED_gpencil_palettes_enum_itemf(
        bContext *C, PointerRNA *UNUSED(ptr), PropertyRNA *UNUSED(prop),
        bool *r_free)
{
	bGPdata *gpd = CTX_data_gpencil_data(C);
	bGPDpalette *palette;
	EnumPropertyItem *item = NULL, item_tmp = { 0 };
	int totitem = 0;
	int i = 0;

	if (ELEM(NULL, C, gpd)) {
		return DummyRNA_DEFAULT_items;
	}

	/* Existing palettes */
	for (palette = gpd->palettes.first; palette; palette = palette->next, i++) {
		item_tmp.identifier = palette->info;
		item_tmp.name = palette->info;
		item_tmp.value = i;

		if (palette->flag & PL_PALETTE_ACTIVE)
			item_tmp.icon = ICON_COLOR;
		else
			item_tmp.icon = ICON_NONE;

		RNA_enum_item_add(&item, &totitem, &item_tmp);
	}

	RNA_enum_item_end(&item, &totitem);
	*r_free = true;

	return item;
}
/* Helper function to create new OB_GPENCIL Object */
Object *ED_add_gpencil_object(bContext *C, Scene *scene, const float loc[3])
{
	float rot[3];
	zero_v3(rot);

	Object *ob = ED_object_add_type(C, OB_GPENCIL, NULL, loc, rot, false, scene->lay);

	/* define size */
	BKE_object_obdata_size_init(ob, GP_OBGPENCIL_DEFAULT_SIZE);
	/* create default brushes and colors */
	ED_gpencil_add_defaults(C);

	return ob;
}

/* Helper function to create default colors and drawing brushes */
void ED_gpencil_add_defaults(bContext *C)
{
	ToolSettings *ts = CTX_data_tool_settings(C);
	bGPdata *gpd = CTX_data_gpencil_data(C);
	
	/* ensure palettes, colors, and palette slots exist */
	BKE_gpencil_paletteslot_validate(CTX_data_main(C), gpd);

	/* create default brushes */
	if (BLI_listbase_is_empty(&ts->gp_brushes)) {
		BKE_gpencil_brush_init_presets(ts);
	}
}


/* allocate memory for saving gp object to be sorted by zdepth */
tGPencilSort *ED_gpencil_allocate_cache(tGPencilSort *cache, int *gp_cache_size, int gp_cache_used)
{
	tGPencilSort *p = NULL;

	/* By default a cache is created with one block with a predefined number of free slots,
	if the size is not enough, the cache is reallocated adding a new block of free slots.
	This is done in order to keep cache small */
	if (gp_cache_used + 1 > *gp_cache_size) {
		if ((*gp_cache_size == 0) || (cache == NULL)) {
			p = MEM_callocN(sizeof(struct tGPencilSort) * GP_CACHE_BLOCK_SIZE, "tGPencilSort");
			*gp_cache_size = GP_CACHE_BLOCK_SIZE;
		}
		else {
			*gp_cache_size += GP_CACHE_BLOCK_SIZE;
			p = MEM_recallocN(cache, sizeof(struct tGPencilSort) * *gp_cache_size);
		}
		cache = p;
	}
	return cache;
}

/* add gp object to the temporary cache for sorting */
void ED_gpencil_add_to_cache(tGPencilSort *cache, RegionView3D *rv3d, Base *base, int *gp_cache_used)
{
	/* save object */
	cache[*gp_cache_used].base = base;

	/* calculate zdepth from point of view */
	float zdepth = 0.0;
	if (rv3d->is_persp) {
		zdepth = ED_view3d_calc_zfac(rv3d, base->object->loc, NULL);
	}
	else {
		zdepth = -dot_v3v3(rv3d->viewinv[2], base->object->loc);
	}
	cache[*gp_cache_used].zdepth = zdepth;

	/* increase slots used in cache */
	(*gp_cache_used)++;
}

/* reproject the points of the stroke to a plane locked to axis to avoid stroke offset */
void ED_gp_project_stroke_to_plane(Object *ob, RegionView3D *rv3d, bGPDstroke *gps, const float origin[3], const int axis, char type)
{
	float plane_normal[3];
	float vn[3];

	float ray[3];
	float rpoint[3];

	/* normal vector for a plane locked to axis */
	zero_v3(plane_normal);
	plane_normal[axis] = 1.0f;
	/* if object, apply object rotation */
	if (type & GP_TOOL_SOURCE_OBJECT) {
		if (ob && ob->type == OB_GPENCIL) {
			mul_mat3_m4_v3(ob->obmat, plane_normal);
		}
	}

	/* Reproject the points in the plane */
	for (int i = 0; i < gps->totpoints; i++) {
		bGPDspoint *pt = &gps->points[i];

		/* get a vector from the point with the current view direction of the viewport */
		ED_view3d_global_to_vector(rv3d, &pt->x, vn);

		/* calculate line extrem point to create a ray that cross the plane */
		mul_v3_fl(vn, -50.0f);
		add_v3_v3v3(ray, &pt->x, vn);

		/* if the line never intersect, the point is not changed */
		if (isect_line_plane_v3(rpoint, &pt->x, ray, origin, plane_normal)) {
			copy_v3_v3(&pt->x, rpoint);
		}
	}
}

/* reproject one points to a plane locked to axis to avoid stroke offset */
void ED_gp_project_point_to_plane(Object *ob, RegionView3D *rv3d, const float origin[3], const int axis, char type, bGPDspoint *pt)
{
	float plane_normal[3];
	float vn[3];

	float ray[3];
	float rpoint[3];

	/* no need reproject */
	if (axis < 0) {
		return;
	}

	/* normal vector for a plane locked to axis */
	zero_v3(plane_normal);
	plane_normal[axis] = 1.0f;
	/* if object, apply object rotation */
	if (type & GP_TOOL_SOURCE_OBJECT) {
		if (ob && ob->type == OB_GPENCIL) {
			mul_mat3_m4_v3(ob->obmat, plane_normal);
		}
	}

	/* Reproject the points in the plane */
	/* get a vector from the point with the current view direction of the viewport */
	ED_view3d_global_to_vector(rv3d, &pt->x, vn);

	/* calculate line extrem point to create a ray that cross the plane */
	mul_v3_fl(vn, -50.0f);
	add_v3_v3v3(ray, &pt->x, vn);

	/* if the line never intersect, the point is not changed */
	if (isect_line_plane_v3(rpoint, &pt->x, ray, origin, plane_normal)) {
		copy_v3_v3(&pt->x, rpoint);
	}
}

/* get drawing reference for conversion or projection of the stroke */
void ED_gp_get_drawing_reference(ToolSettings *ts, View3D *v3d, Scene *scene, Object *ob, bGPDlayer *gpl, char align_flag, float vec[3])
{
	const float *fp = ED_view3d_cursor3d_get(scene, v3d);

	/* if using a gpencil object at cursor mode, can use the location of the object */
	if ((ts->gpencil_src & GP_TOOL_SOURCE_OBJECT) && (align_flag & GP_PROJECT_VIEWSPACE)) {
		if (ob) {
			if (ob->type == OB_GPENCIL) {
				/* use last stroke position for layer */
				if (gpl && gpl->flag & GP_LAYER_USE_LOCATION) {
					if (gpl->actframe) {
						bGPDframe *gpf = gpl->actframe;
						if (gpf->strokes.last) {
							bGPDstroke *gps = gpf->strokes.last;
							if (gps->totpoints > 0) {
								copy_v3_v3(vec, &gps->points[gps->totpoints - 1].x);
								mul_m4_v3(ob->obmat, vec);
								return;
							}
						}
					}
				}
				/* use cursor */
				if (align_flag & GP_PROJECT_CURSOR) {
					/* use 3D-cursor */
					copy_v3_v3(vec, fp);
				}
				else {
					/* use object location */
					copy_v3_v3(vec, ob->obmat[3]);
				}
			}
		}
	}
	else {
		/* use 3D-cursor */
		copy_v3_v3(vec, fp);
	}
}
/* ******************************************************** */
/* Cursor drawing */

/* check if cursor is in drawing region */
static bool gp_check_cursor_region(bContext *C, int mval[2])
{
	ARegion *ar = CTX_wm_region(C);
	ScrArea *sa = CTX_wm_area(C);
	/* TODO: add more spacetypes */
	if (!ELEM(sa->spacetype, SPACE_VIEW3D)) {
		return false;
	}
	if ((ar) && (ar->regiontype != RGN_TYPE_WINDOW)) {
		return false;
	}
	else if (ar) {
		rcti region_rect;

		/* Perform bounds check using  */
		ED_region_visible_rect(ar, &region_rect);
		return BLI_rcti_isect_pt_v(&region_rect, mval);
	}
	else {
		return false;
	}

	return false;
}

/* Helper callback for drawing the cursor itself */
static void gp_brush_drawcursor(bContext *C, int x, int y, void *UNUSED(customdata))
{
	Scene *scene = CTX_data_scene(C);
	GP_BrushEdit_Settings *gset = &scene->toolsettings->gp_sculpt;
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	GP_EditBrush_Data *brush = NULL;
	if ((gpd) && (gpd->flag & GP_DATA_STROKE_WEIGHTMODE)) {
		brush = &gset->brush[gset->weighttype];
	}
	else {
		brush = &gset->brush[gset->brushtype];
	}
	bGPDbrush *paintbrush;

	/* default radius and color */
	float radius = 5.0f;
	float color[3], darkcolor[3];
	ARRAY_SET_ITEMS(color, 1.0f, 1.0f, 1.0f);

	int mval[2];
	ARRAY_SET_ITEMS(mval, x, y);
	/* check if cursor is in drawing region and has valid datablock */
	if ((!gp_check_cursor_region(C, mval)) || (gpd == NULL)) {
		return;
	}

	/* for paint use paint brush size and color */
	if (gpd->flag & GP_DATA_STROKE_PAINTMODE) {
		/* while drawing hide */
		if (gpd->sbuffer_size > 0) {
			return;
		}

		paintbrush = BKE_gpencil_brush_getactive(scene->toolsettings);
		if (paintbrush) {
			if ((paintbrush->flag & GP_BRUSH_ENABLE_CURSOR) == 0) {
				return;
			}
			/* after some testing, display the size of the brush is not practical because 
			 * is too disruptive and the size of cursor does not change with zoom factor.
			 * The decision was to use a fix size, instead of  paintbrush->thickness value. 
			 */
			radius = 3.0f;
			copy_v3_v3(color, paintbrush->curcolor);
		}
	}

	/* for sculpt use sculpt brush size */
	if (GPENCIL_SCULPT_OR_WEIGHT_MODE(gpd)) {
		if (brush) {
			if ((brush->flag & GP_EDITBRUSH_FLAG_ENABLE_CURSOR) == 0) {
				return;
			}

			radius = brush->size;
			if (brush->flag & (GP_EDITBRUSH_FLAG_INVERT | GP_EDITBRUSH_FLAG_TMP_INVERT)) {
				copy_v3_v3(color, brush->curcolor_sub);
			}
			else {
				copy_v3_v3(color, brush->curcolor_add);
			}
		}
	}

	/* draw icon */
	Gwn_VertFormat *format = immVertexFormat();
	unsigned int pos = GWN_vertformat_attr_add(format, "pos", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);
	immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

	glEnable(GL_LINE_SMOOTH);
	glEnable(GL_BLEND);

	/* Inner Ring: Color from UI panel */
	immUniformColor4f(color[0], color[1], color[2], 0.8f);
	imm_draw_circle_wire_2d(pos, x, y, radius, 40);
	/* Outer Ring: Dark color for contrast on light backgrounds (e.g. gray on white) */
	mul_v3_v3fl(darkcolor, color, 0.40f);
	immUniformColor4f(darkcolor[0], darkcolor[1], darkcolor[2], 0.8f);
	imm_draw_circle_wire_2d(pos, x, y, radius + 1, 40);

	immUnbindProgram();

	glDisable(GL_BLEND);
	glDisable(GL_LINE_SMOOTH);
}

/* Turn brush cursor in on/off */
void ED_gpencil_toggle_brush_cursor(bContext *C, bool enable)
{
	Scene *scene = CTX_data_scene(C);
	GP_BrushEdit_Settings *gset = &scene->toolsettings->gp_sculpt;

	if (gset->paintcursor && !enable) {
		/* clear cursor */
		WM_paint_cursor_end(CTX_wm_manager(C), gset->paintcursor);
		gset->paintcursor = NULL;
	}
	else if (enable) {
		/* in some situations cursor could be duplicated, so it is better disable first if exist */
		if (gset->paintcursor) {
			/* clear cursor */
			WM_paint_cursor_end(CTX_wm_manager(C), gset->paintcursor);
			gset->paintcursor = NULL;
		}
		/* enable cursor */
		gset->paintcursor = WM_paint_cursor_activate(CTX_wm_manager(C),
			NULL,
			gp_brush_drawcursor, NULL);
	}
}

/* assign points to vertex group */
void ED_gpencil_vgroup_assign(bContext *C, Object *ob, float weight)
{
	bGPDspoint *pt;
	const int def_nr = ob->actdef - 1;
	if (!BLI_findlink(&ob->defbase, def_nr))
		return;

	CTX_DATA_BEGIN(C, bGPDstroke *, gps, editable_gpencil_strokes)
	{
		if (gps->flag & GP_STROKE_SELECT) {
			for (int i = 0; i < gps->totpoints; ++i) {
				pt = &gps->points[i];
				if (pt->flag & GP_SPOINT_SELECT) {
					BKE_gpencil_vgroup_add_point_weight(pt, def_nr, weight);
				}
			}
		}
	}
	CTX_DATA_END;
}

/* remove points from vertex group */
void ED_gpencil_vgroup_remove(bContext *C, Object *ob)
{
	bGPDspoint *pt;
	const int def_nr = ob->actdef - 1;
	if (!BLI_findlink(&ob->defbase, def_nr))
		return;

	CTX_DATA_BEGIN(C, bGPDstroke *, gps, editable_gpencil_strokes)
	{
		for (int i = 0; i < gps->totpoints; ++i) {
			pt = &gps->points[i];
			if ((pt->flag & GP_SPOINT_SELECT) && (pt->totweight > 0)) {
				BKE_gpencil_vgroup_remove_point_weight(pt, def_nr);
			}
		}
	}
	CTX_DATA_END;
}

/* select points of vertex group */
void ED_gpencil_vgroup_select(bContext *C, Object *ob)
{
	bGPDspoint *pt;
	const int def_nr = ob->actdef - 1;
	if (!BLI_findlink(&ob->defbase, def_nr))
		return;

	CTX_DATA_BEGIN(C, bGPDstroke *, gps, editable_gpencil_strokes)
	{
		for (int i = 0; i < gps->totpoints; ++i) {
			pt = &gps->points[i];
			if (BKE_gpencil_vgroup_use_index(pt, def_nr) > -1.0f) {
				pt->flag |= GP_SPOINT_SELECT;
				gps->flag |= GP_STROKE_SELECT;
			}
		}
	}
	CTX_DATA_END;
}
/* unselect points of vertex group */
void ED_gpencil_vgroup_deselect(bContext *C, Object *ob)
{
	bGPDspoint *pt;
	const int def_nr = ob->actdef - 1;
	if (!BLI_findlink(&ob->defbase, def_nr))
		return;

	CTX_DATA_BEGIN(C, bGPDstroke *, gps, editable_gpencil_strokes)
	{
		for (int i = 0; i < gps->totpoints; ++i) {
			pt = &gps->points[i];
			if (BKE_gpencil_vgroup_use_index(pt, def_nr) > -1.0f) {
				pt->flag &= ~GP_SPOINT_SELECT;
				gps->flag |= GP_STROKE_SELECT;
			}
		}
	}
	CTX_DATA_END;
}
