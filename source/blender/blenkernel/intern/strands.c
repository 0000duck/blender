/*
 * Copyright 2015, Blender Foundation.
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
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_string.h"

#include "BKE_strands.h"

Strands *BKE_strands_new(int curves, int verts)
{
	Strands *strands = MEM_mallocN(sizeof(Strands), "strands");
	
	strands->totcurves = curves;
	strands->curves = MEM_mallocN(sizeof(StrandsCurve) * curves, "strand curves");
	
	strands->totverts = verts;
	strands->verts = MEM_mallocN(sizeof(StrandsVertex) * verts, "strand vertices");
	
	/* must be added explicitly */
	strands->state = NULL;
	
	return strands;
}

Strands *BKE_strands_copy(Strands *strands)
{
	Strands *new_strands = MEM_dupallocN(strands);
	if (new_strands->curves)
		new_strands->curves = MEM_dupallocN(new_strands->curves);
	if (new_strands->verts)
		new_strands->verts = MEM_dupallocN(new_strands->verts);
	if (new_strands->state)
		new_strands->state = MEM_dupallocN(new_strands->state);
	return new_strands;
}

void BKE_strands_free(Strands *strands)
{
	if (strands) {
		if (strands->curves)
			MEM_freeN(strands->curves);
		if (strands->verts)
			MEM_freeN(strands->verts);
		if (strands->state)
			MEM_freeN(strands->state);
		MEM_freeN(strands);
	}
}

/* copy the rest positions to initialize the motion state */
void BKE_strands_state_copy_rest_positions(Strands *strands)
{
	if (strands->state) {
		int i;
		for (i = 0; i < strands->totverts; ++i) {
			copy_v3_v3(strands->state[i].co, strands->verts[i].co);
		}
	}
}

/* copy the rest positions to initialize the motion state */
void BKE_strands_state_clear_velocities(Strands *strands)
{
	if (strands->state) {
		int i;
		
		for (i = 0; i < strands->totverts; ++i) {
			zero_v3(strands->state[i].vel);
		}
	}
}

void BKE_strands_add_motion_state(Strands *strands)
{
	if (!strands->state) {
		int i;
		
		strands->state = MEM_mallocN(sizeof(StrandsMotionState) * strands->totverts, "strand motion states");
		
		BKE_strands_state_copy_rest_positions(strands);
		BKE_strands_state_clear_velocities(strands);
		
		/* initialize normals */
		for (i = 0; i < strands->totverts; ++i) {
			copy_v3_v3(strands->state[i].nor, strands->verts[i].nor);
		}
	}
}

void BKE_strands_remove_motion_state(Strands *strands)
{
	if (strands) {
		if (strands->state) {
			MEM_freeN(strands->state);
			strands->state = NULL;
		}
	}
}

static void calc_normals(Strands *strands, bool use_motion_state)
{
	StrandIterator it_strand;
	for (BKE_strand_iter_init(&it_strand, strands); BKE_strand_iter_valid(&it_strand); BKE_strand_iter_next(&it_strand)) {
		StrandEdgeIterator it_edge;
		int numverts = it_strand.curve->numverts;
		if (use_motion_state) {
			for (BKE_strand_edge_iter_init(&it_edge, &it_strand); BKE_strand_edge_iter_valid(&it_edge); BKE_strand_edge_iter_next(&it_edge)) {
				sub_v3_v3v3(it_edge.state0->nor, it_edge.state1->co, it_edge.state0->co);
				normalize_v3(it_edge.state0->nor);
			}
			if (numverts > 1)
				copy_v3_v3(it_strand.state[numverts-1].nor, it_strand.state[numverts-2].nor);
		}
		else {
			for (BKE_strand_edge_iter_init(&it_edge, &it_strand); BKE_strand_edge_iter_valid(&it_edge); BKE_strand_edge_iter_next(&it_edge)) {
				sub_v3_v3v3(it_edge.vertex0->nor, it_edge.vertex1->co, it_edge.vertex0->co);
				normalize_v3(it_edge.vertex0->nor);
			}
			if (numverts > 1)
				copy_v3_v3(it_strand.verts[numverts-1].nor, it_strand.verts[numverts-2].nor);
		}
	}
}

void BKE_strands_ensure_normals(Strands *strands)
{
	const bool use_motion_state = (strands->state);
	
	calc_normals(strands, false);
	
	if (use_motion_state)
		calc_normals(strands, true);
}

void BKE_strands_get_minmax(Strands *strands, float min[3], float max[3], bool use_motion_state)
{
	int numverts = strands->totverts;
	int i;
	
	if (use_motion_state && strands->state) {
		for (i = 0; i < numverts; ++i) {
			minmax_v3v3_v3(min, max, strands->state[i].co);
		}
	}
	else {
		for (i = 0; i < numverts; ++i) {
			minmax_v3v3_v3(min, max, strands->verts[i].co);
		}
	}
}

/* ------------------------------------------------------------------------- */

StrandsChildren *BKE_strands_children_new(int curves, int verts)
{
	StrandsChildren *strands = MEM_mallocN(sizeof(StrandsChildren), "strands children");
	
	strands->totcurves = curves;
	strands->curves = MEM_mallocN(sizeof(StrandsChildCurve) * curves, "strand children curves");
	
	strands->totverts = verts;
	strands->verts = MEM_mallocN(sizeof(StrandsChildVertex) * verts, "strand children vertices");
	
	/* must be added explicitly */
	strands->curve_uvs = NULL;
	strands->numuv = 0;
	strands->curve_vcols = NULL;
	strands->numvcol = 0;
	
	return strands;
}

StrandsChildren *BKE_strands_children_copy(StrandsChildren *strands)
{
	StrandsChildren *new_strands = MEM_dupallocN(strands);
	if (new_strands->curves)
		new_strands->curves = MEM_dupallocN(new_strands->curves);
	if (new_strands->curve_uvs)
		new_strands->curve_uvs = MEM_dupallocN(new_strands->curve_uvs);
	if (new_strands->curve_vcols)
		new_strands->curve_vcols = MEM_dupallocN(new_strands->curve_vcols);
	if (new_strands->verts)
		new_strands->verts = MEM_dupallocN(new_strands->verts);
	return new_strands;
}

void BKE_strands_children_free(StrandsChildren *strands)
{
	if (strands) {
		if (strands->curves)
			MEM_freeN(strands->curves);
		if (strands->curve_uvs)
			MEM_freeN(strands->curve_uvs);
		if (strands->curve_vcols)
			MEM_freeN(strands->curve_vcols);
		if (strands->verts)
			MEM_freeN(strands->verts);
		MEM_freeN(strands);
	}
}

void BKE_strands_children_add_uvs(StrandsChildren *strands, int num_layers)
{
	if (strands->curve_uvs && strands->numuv != num_layers) {
		MEM_freeN(strands->curve_uvs);
		strands->curve_uvs = NULL;
		strands->numuv = 0;
	}
	
	if (!strands->curve_uvs) {
		strands->curve_uvs = MEM_callocN(sizeof(StrandsChildCurveUV) * strands->totcurves * num_layers, "strands children uv layers");
		strands->numuv = num_layers;
	}
}

void BKE_strands_children_add_vcols(StrandsChildren *strands, int num_layers)
{
	if (strands->curve_vcols && strands->numvcol != num_layers) {
		MEM_freeN(strands->curve_vcols);
		strands->curve_vcols = NULL;
		strands->numvcol = 0;
	}
	
	if (!strands->curve_vcols) {
		strands->curve_vcols = MEM_callocN(sizeof(StrandsChildCurveVCol) * strands->totcurves * num_layers, "strands children vcol layers");
		strands->numvcol = num_layers;
	}
}

int BKE_strands_children_max_length(StrandsChildren *strands)
{
	StrandChildIterator it_strand;
	int maxlen = 0;
	for (BKE_strand_child_iter_init(&it_strand, strands); BKE_strand_child_iter_valid(&it_strand); BKE_strand_child_iter_next(&it_strand)) {
		if (maxlen < it_strand.curve->numverts)
			maxlen = it_strand.curve->numverts;
	}
	return maxlen;
}

int *BKE_strands_calc_vertex_start(Strands *strands)
{
	int *vertstart = MEM_mallocN(sizeof(int) * strands->totcurves, "strand curves vertex start");
	StrandIterator it_strand;
	int start;
	
	start = 0;
	for (BKE_strand_iter_init(&it_strand, strands); BKE_strand_iter_valid(&it_strand); BKE_strand_iter_next(&it_strand)) {
		vertstart[it_strand.index] = start;
		start += it_strand.curve->numverts;
	}
	
	return vertstart;
}

/* shortens the last visible segment to have exact cutoff length */
static void strands_children_apply_cutoff(StrandChildIterator *it_strand)
{
	StrandsChildCurve *curve = it_strand->curve;
	const float cutoff = curve->cutoff;
	
	int last, end;
	float *a, *b;
	float t;
	
	if (cutoff < 0 || cutoff >= (float)(curve->numverts-1))
		return;
	
	last = (int)cutoff;
	end = last + 1;
	BLI_assert(last < curve->numverts);
	BLI_assert(end < curve->numverts);
	
	a = it_strand->verts[last].co;
	b = it_strand->verts[end].co;
	t = cutoff - floorf(cutoff);
	interp_v3_v3v3(b, a, b, t);
}


/* 'out' is an optional array to write final positions to, instead of writing back to vertex locations.
 * It must be at least as large as the number of vertices.
 */
static void strands_children_strand_deform_intern(StrandChildIterator *it_strand, Strands *parents, int *vertstart, bool use_motion, float (*out)[3])
{
	int i;
	
	if (!parents || !vertstart)
		return;
	
	if (!parents->state)
		use_motion = false;
	
	for (i = 0; i < 4; ++i) {
		int p = it_strand->curve->parents[i];
		float w = it_strand->curve->parent_weights[i];
		if (p >= 0 && w > 0.0f) {
			StrandsCurve *parent = &parents->curves[p];
			StrandsVertex *pverts;
			StrandsMotionState *pstate;
			int pv0, pv1;
			StrandChildVertexIterator it_vert;
			
			if (parent->numverts <= 0)
				continue;
			
			pverts = &parents->verts[vertstart[p]];
			pstate = &parents->state[vertstart[p]];
			pv0 = 0;
			for (BKE_strand_child_vertex_iter_init(&it_vert, it_strand); BKE_strand_child_vertex_iter_valid(&it_vert); BKE_strand_child_vertex_iter_next(&it_vert)) {
				float time = it_vert.vertex->time;
				float dt, x;
				float poffset0[3], poffset1[3], offset[3];
				
				/* advance to the matching parent edge for interpolation */
				while (pv0 < parent->numverts-1 && pverts[pv0+1].time < time)
					++pv0;
				pv1 = (pv0 < parent->numverts-1)? pv0+1 : pv0;
				
				if (use_motion) {
					sub_v3_v3v3(poffset0, pstate[pv0].co, pverts[pv0].base);
					sub_v3_v3v3(poffset1, pstate[pv1].co, pverts[pv1].base);
				}
				else {
					sub_v3_v3v3(poffset0, pverts[pv0].co, pverts[pv0].base);
					sub_v3_v3v3(poffset1, pverts[pv1].co, pverts[pv1].base);
				}
				
				dt = pverts[pv1].time - pverts[pv0].time;
				x = dt > 0.0f ? (time - pverts[pv0].time) / dt : 0.0f;
				CLAMP(x, 0.0f, 1.0f);
				interp_v3_v3v3(offset, poffset0, poffset1, x);
				
				if (out)
					madd_v3_v3fl(out[it_vert.index], offset, w);
				else
					madd_v3_v3fl(it_vert.vertex->co, offset, w);
			}
		}
	}
}

/* Deform a single strand on-the-fly for intermediate processing.
 * 'out' is an optional array to write final positions to, instead of writing back to vertex locations.
 * It must be at least as large as the number of vertices.
 */
void BKE_strands_children_strand_deform(StrandChildIterator *it_strand, Strands *parents, int *vertstart, bool use_motion, float (*out)[3])
{
	StrandChildVertexIterator it_vert;
	
	/* move child strands from their local root space to object space */
	if (out) {
		StrandChildVertexIterator it_vert;
		for (BKE_strand_child_vertex_iter_init(&it_vert, it_strand); BKE_strand_child_vertex_iter_valid(&it_vert); BKE_strand_child_vertex_iter_next(&it_vert)) {
			mul_v3_m4v3(out[it_vert.index], it_strand->curve->root_matrix, it_vert.vertex->co);
		}
	}
	else {
		for (BKE_strand_child_vertex_iter_init(&it_vert, it_strand); BKE_strand_child_vertex_iter_valid(&it_vert); BKE_strand_child_vertex_iter_next(&it_vert)) {
			mul_m4_v3(it_strand->curve->root_matrix, it_vert.vertex->co);
		}
	}
	
	strands_children_strand_deform_intern(it_strand, parents, vertstart, use_motion, out);
}

void BKE_strands_children_deform(StrandsChildren *strands, Strands *parents, bool use_motion)
{
	int *vertstart = NULL;
	StrandChildIterator it_strand;
	
	if (parents)
		vertstart = BKE_strands_calc_vertex_start(parents);
	
	for (BKE_strand_child_iter_init(&it_strand, strands); BKE_strand_child_iter_valid(&it_strand); BKE_strand_child_iter_next(&it_strand)) {
		/* move child strands from their local root space to object space */
		StrandChildVertexIterator it_vert;
		for (BKE_strand_child_vertex_iter_init(&it_vert, &it_strand); BKE_strand_child_vertex_iter_valid(&it_vert); BKE_strand_child_vertex_iter_next(&it_vert)) {
			mul_m4_v3(it_strand.curve->root_matrix, it_vert.vertex->co);
		}
		
		strands_children_apply_cutoff(&it_strand);
		
		strands_children_strand_deform_intern(&it_strand, parents, vertstart, use_motion, NULL);
	}
	
	if (vertstart)
		MEM_freeN(vertstart);
}

static void calc_child_normals(StrandsChildren *strands)
{
	StrandChildIterator it_strand;
	for (BKE_strand_child_iter_init(&it_strand, strands); BKE_strand_child_iter_valid(&it_strand); BKE_strand_child_iter_next(&it_strand)) {
		StrandChildEdgeIterator it_edge;
		int numverts = it_strand.curve->numverts;
		for (BKE_strand_child_edge_iter_init(&it_edge, &it_strand); BKE_strand_child_edge_iter_valid(&it_edge); BKE_strand_child_edge_iter_next(&it_edge)) {
			sub_v3_v3v3(it_edge.vertex0->nor, it_edge.vertex1->co, it_edge.vertex0->co);
			normalize_v3(it_edge.vertex0->nor);
		}
		if (numverts > 1)
			copy_v3_v3(it_strand.verts[numverts-1].nor, it_strand.verts[numverts-2].nor);
	}
}

void BKE_strands_children_ensure_normals(StrandsChildren *strands)
{
	calc_child_normals(strands);
}

void BKE_strands_children_get_minmax(StrandsChildren *strands, float min[3], float max[3])
{
	int numverts = strands->totverts;
	int i;
	
	for (i = 0; i < numverts; ++i) {
		minmax_v3v3_v3(min, max, strands->verts[i].co);
	}
}

/* ------------------------------------------------------------------------- */

void BKE_strand_bend_iter_transform_rest(StrandBendIterator *iter, float mat[3][3])
{
	float dir0[3], dir1[3];
	
	sub_v3_v3v3(dir0, iter->vertex1->co, iter->vertex0->co);
	sub_v3_v3v3(dir1, iter->vertex2->co, iter->vertex1->co);
	normalize_v3(dir0);
	normalize_v3(dir1);
	
	/* rotation between segments */
	rotation_between_vecs_to_mat3(mat, dir0, dir1);
}

void BKE_strand_bend_iter_transform_state(StrandBendIterator *iter, float mat[3][3])
{
	if (iter->state0) {
		float dir0[3], dir1[3];
		
		sub_v3_v3v3(dir0, iter->state1->co, iter->state0->co);
		sub_v3_v3v3(dir1, iter->state2->co, iter->state1->co);
		normalize_v3(dir0);
		normalize_v3(dir1);
		
		/* rotation between segments */
		rotation_between_vecs_to_mat3(mat, dir0, dir1);
	}
	else
		unit_m3(mat);
}
