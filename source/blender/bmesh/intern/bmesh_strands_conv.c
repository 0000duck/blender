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
 * Contributor(s): Lukas Toenne.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/intern/bmesh_strands_conv.c
 *  \ingroup bmesh
 *
 * BM mesh conversion functions.
 */

#include "DNA_cache_library_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_key_types.h"
#include "DNA_strands_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"

#include "BKE_cache_library.h"
#include "BKE_customdata.h"
#include "BKE_key.h"
#include "BKE_main.h"
#include "BKE_mesh_sample.h"
#include "BKE_strands.h"
#include "BKE_particle.h"

#include "bmesh.h"
#include "intern/bmesh_private.h" /* for element checking */

const char *CD_HAIR_SEGMENT_LENGTH = "HAIR_SEGMENT_LENGTH";
const char *CD_HAIR_MASS = "HAIR_MASS";
const char *CD_HAIR_WEIGHT = "HAIR_WEIGHT";
const char *CD_HAIR_ROOT_LOCATION = "HAIR_ROOT_LOCATION";

/* ------------------------------------------------------------------------- */

/**
 * Currently this is only used for Python scripts
 * which may fail to keep matching UV/TexFace layers.
 *
 * \note This should only perform any changes in exceptional cases,
 * if we need this to be faster we could inline #BM_data_layer_add and only
 * call #update_data_blocks once at the end.
 */
void BM_strands_cd_validate(BMesh *UNUSED(bm))
{
}

void BM_strands_cd_flag_ensure(BMesh *bm, const char cd_flag)
{
	const char cd_flag_all = BM_strands_cd_flag_from_bmesh(bm) | cd_flag;
	BM_strands_cd_flag_apply(bm, cd_flag_all);
}

void BM_strands_cd_flag_apply(BMesh *bm, const char UNUSED(cd_flag))
{
	/* CustomData_bmesh_init_pool() must run first */
	BLI_assert(bm->vdata.totlayer == 0 || bm->vdata.pool != NULL);
	BLI_assert(bm->edata.totlayer == 0 || bm->edata.pool != NULL);
	
	if (CustomData_get_named_layer_index(&bm->vdata, CD_PROP_FLT, CD_HAIR_MASS) < 0) {
		BM_data_layer_add_named(bm, &bm->vdata, CD_PROP_FLT, CD_HAIR_MASS);
	}
	if (CustomData_get_named_layer_index(&bm->vdata, CD_PROP_FLT, CD_HAIR_WEIGHT) < 0) {
		BM_data_layer_add_named(bm, &bm->vdata, CD_PROP_FLT, CD_HAIR_WEIGHT);
	}
	if (CustomData_get_named_layer_index(&bm->vdata, CD_MSURFACE_SAMPLE, CD_HAIR_ROOT_LOCATION) < 0) {
		BM_data_layer_add_named(bm, &bm->vdata, CD_MSURFACE_SAMPLE, CD_HAIR_ROOT_LOCATION);
	}
	if (CustomData_get_named_layer_index(&bm->vdata, CD_PROP_FLT, CD_HAIR_SEGMENT_LENGTH) < 0) {
		BM_data_layer_add_named(bm, &bm->vdata, CD_PROP_FLT, CD_HAIR_SEGMENT_LENGTH);
	}
}

char BM_strands_cd_flag_from_bmesh(BMesh *UNUSED(bm))
{
	char cd_flag = 0;
	return cd_flag;
}

/* ------------------------------------------------------------------------- */
/* CacheLibrary */

static KeyBlock *bm_set_shapekey_from_strands_key(BMesh *bm, Strands *strands, Key *key, int act_key_nr)
{
	int totvert = strands->totverts;
	KeyBlock *actkey, *block;
	int i, j;
	
	if (!key) {
		return NULL;
	}
	
	if (act_key_nr != 0)
		actkey = BLI_findlink(&key->block, act_key_nr - 1);
	else
		actkey = NULL;
	
	CustomData_add_layer(&bm->vdata, CD_SHAPE_KEYINDEX, CD_ASSIGN, NULL, 0);
	
	if (actkey && actkey->totelem == totvert) {
		bm->shapenr = act_key_nr;
	}
	
	for (i = 0, block = key->block.first; block; block = block->next, i++) {
		CustomData_add_layer_named(&bm->vdata, CD_SHAPEKEY,
		                           CD_ASSIGN, NULL, 0, block->name);
		
		j = CustomData_get_layer_index_n(&bm->vdata, CD_SHAPEKEY, i);
		bm->vdata.layers[j].uid = block->uid;
	}
	
	return actkey;
}

/* create vertex and edge data for BMesh based on strand data */
static void bm_make_strands(BMesh *bm, Strands *strands, Key *key, struct DerivedMesh *UNUSED(emitter_dm), float (*keyco)[3], int cd_shape_keyindex_offset)
{
	KeyBlock *block;
	StrandIterator it_strand;
	
	int vindex, eindex;
	BMVert *v = NULL, *v_prev;
	BMEdge *e;
	
	vindex = 0;
	eindex = 0;
	for (BKE_strand_iter_init(&it_strand, strands); BKE_strand_iter_valid(&it_strand); BKE_strand_iter_next(&it_strand)) {
		StrandVertexIterator it_vert;
		
		for (BKE_strand_vertex_iter_init(&it_vert, &it_strand); BKE_strand_vertex_iter_valid(&it_vert); BKE_strand_vertex_iter_next(&it_vert)) {
			float co[3];
			
			copy_v3_v3(co, keyco ? keyco[vindex] : it_vert.vertex->co);
			
			v_prev = v;
			v = BM_vert_create(bm, co, NULL, BM_CREATE_SKIP_CD);
			BM_elem_index_set(v, vindex); /* set_ok */
			
			/* transfer flag */
//			v->head.hflag = BM_vert_flag_from_mflag(mvert->flag & ~SELECT);
			
			/* this is necessary for selection counts to work properly */
//			if (hkey->editflag & SELECT) {
//				BM_vert_select_set(bm, v, true);
//			}
			
//			normal_short_to_float_v3(v->no, mvert->no);
			
			/* Copy Custom Data */
//			CustomData_to_bmesh_block(&me->vdata, &bm->vdata, vindex, &v->head.data, true);
			CustomData_bmesh_set_default(&bm->vdata, &v->head.data);
			
//			BM_elem_float_data_named_set(&bm->vdata, v, CD_PROP_FLT, CD_HAIR_MASS, mass);
			BM_elem_float_data_named_set(&bm->vdata, v, CD_PROP_FLT, CD_HAIR_WEIGHT, it_vert.vertex->weight);
			
			/* root */
			BM_elem_meshsample_data_named_set(&bm->vdata, v, CD_MSURFACE_SAMPLE, CD_HAIR_ROOT_LOCATION, &it_strand.curve->msurf);
			
			/* set shapekey data */
			if (key) {
				int k;
				
				/* set shape key original index */
				if (cd_shape_keyindex_offset != -1)
					BM_ELEM_CD_SET_INT(v, cd_shape_keyindex_offset, vindex);
				
				for (block = key->block.first, k = 0; block; block = block->next, k++) {
					float *co = CustomData_bmesh_get_n(&bm->vdata, v->head.data, CD_SHAPEKEY, k);
					
					if (co) {
						copy_v3_v3(co, ((float *)block->data) + 3 * vindex);
					}
				}
			}
			
			vindex += 1;
			
			if (it_vert.index > 0) {
				e = BM_edge_create(bm, v_prev, v, NULL, BM_CREATE_SKIP_CD);
				BM_elem_index_set(e, eindex); /* set_ok; one less edge than vertices for each particle */
				
				/* transfer flags */
//				e->head.hflag = BM_edge_flag_from_mflag(medge->flag & ~SELECT);
				
				/* this is necessary for selection counts to work properly */
//				if (medge->flag & SELECT) {
//					BM_edge_select_set(bm, e, true);
//				}
				
				/* Copy Custom Data */
//				CustomData_to_bmesh_block(&me->edata, &bm->edata, eindex, &e->head.data, true);
				CustomData_bmesh_set_default(&bm->edata, &e->head.data);
				
				eindex += 1;
			}
		}
		
	}
	
	bm->elem_index_dirty &= ~(BM_VERT | BM_EDGE); /* added in order, clear dirty flag */
}

/**
 * \brief ParticleSystem -> BMesh
 */
void BM_strands_bm_from_strands(BMesh *bm, Strands *strands, Key *key, struct DerivedMesh *emitter_dm, float mat[4][4],
                            const bool set_key, int act_key_nr)
{
	BMIter iter;
	BMVert *v;
	KeyBlock *actkey;
	float (*keyco)[3] = NULL;
	int totvert, totedge;
	
	int cd_shape_keyindex_offset;
	
	/* free custom data */
	/* this isnt needed in most cases but do just incase */
	CustomData_free(&bm->vdata, bm->totvert);
	CustomData_free(&bm->edata, bm->totedge);
	CustomData_free(&bm->ldata, bm->totloop);
	CustomData_free(&bm->pdata, bm->totface);
	
	totvert = strands->totverts;
	totedge = strands->totverts - strands->totcurves;
	
	if (!strands || !totvert || !totedge) {
		if (strands) { /*no verts? still copy customdata layout*/
			CustomData_bmesh_init_pool(&bm->vdata, totvert, BM_VERT);
			CustomData_bmesh_init_pool(&bm->edata, totedge, BM_EDGE);
			CustomData_bmesh_init_pool(&bm->ldata, 0, BM_LOOP);
			CustomData_bmesh_init_pool(&bm->pdata, 0, BM_FACE);
		}
		return; /* sanity check */
	}

	actkey = bm_set_shapekey_from_strands_key(bm, strands, key, act_key_nr);
	if (actkey)
		keyco = actkey->data;

	CustomData_bmesh_init_pool(&bm->vdata, totvert, BM_VERT);
	CustomData_bmesh_init_pool(&bm->edata, totedge, BM_EDGE);

	BM_strands_cd_flag_apply(bm, 0);

	cd_shape_keyindex_offset = key ? CustomData_get_offset(&bm->vdata, CD_SHAPE_KEYINDEX) : -1;

	bm_make_strands(bm, strands, key, emitter_dm, set_key ? keyco : NULL, cd_shape_keyindex_offset);

	/* transform to duplicator local space */
	BM_ITER_MESH(v, &iter, bm, BM_VERTS_OF_MESH) {
		mul_m4_v3(mat, v->co);
	}

#if 0 /* TODO */
	if (me->mselect && me->totselect != 0) {

		BMVert **vert_array = MEM_mallocN(sizeof(BMVert *) * bm->totvert, "VSelConv");
		BMEdge **edge_array = MEM_mallocN(sizeof(BMEdge *) * bm->totedge, "ESelConv");
		BMFace **face_array = MEM_mallocN(sizeof(BMFace *) * bm->totface, "FSelConv");
		MSelect *msel;

#pragma omp parallel sections if (bm->totvert + bm->totedge + bm->totface >= BM_OMP_LIMIT)
		{
#pragma omp section
			{ BM_iter_as_array(bm, BM_VERTS_OF_MESH, NULL, (void **)vert_array, bm->totvert); }
#pragma omp section
			{ BM_iter_as_array(bm, BM_EDGES_OF_MESH, NULL, (void **)edge_array, bm->totedge); }
#pragma omp section
			{ BM_iter_as_array(bm, BM_FACES_OF_MESH, NULL, (void **)face_array, bm->totface); }
		}

		for (i = 0, msel = me->mselect; i < me->totselect; i++, msel++) {
			switch (msel->type) {
				case ME_VSEL:
					BM_select_history_store(bm, (BMElem *)vert_array[msel->index]);
					break;
				case ME_ESEL:
					BM_select_history_store(bm, (BMElem *)edge_array[msel->index]);
					break;
				case ME_FSEL:
					BM_select_history_store(bm, (BMElem *)face_array[msel->index]);
					break;
			}
		}

		MEM_freeN(vert_array);
		MEM_freeN(edge_array);
		MEM_freeN(face_array);
	}
	else {
		me->totselect = 0;
		if (me->mselect) {
			MEM_freeN(me->mselect);
			me->mselect = NULL;
		}
	}
#endif
}

/* ------------------------------------------------------------------------- */

#if 0
/**
 * \brief BMesh -> Mesh
 */
static BMVert **bm_to_mesh_vertex_map(BMesh *bm, int ototvert)
{
	const int cd_shape_keyindex_offset = CustomData_get_offset(&bm->vdata, CD_SHAPE_KEYINDEX);
	BMVert **vertMap = NULL;
	BMVert *eve;
	int i = 0;
	BMIter iter;

	/* caller needs to ensure this */
	BLI_assert(ototvert > 0);

	vertMap = MEM_callocN(sizeof(*vertMap) * ototvert, "vertMap");
	if (cd_shape_keyindex_offset != -1) {
		BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, i) {
			const int keyi = BM_ELEM_CD_GET_INT(eve, cd_shape_keyindex_offset);
			if ((keyi != ORIGINDEX_NONE) && (keyi < ototvert)) {
				vertMap[keyi] = eve;
			}
		}
	}
	else {
		BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, i) {
			if (i < ototvert) {
				vertMap[i] = eve;
			}
			else {
				break;
			}
		}
	}

	return vertMap;
}

/**
 * returns customdata shapekey index from a keyblock or -1
 * \note could split this out into a more generic function */
static int bm_to_mesh_shape_layer_index_from_kb(BMesh *bm, KeyBlock *currkey)
{
	int i;
	int j = 0;

	for (i = 0; i < bm->vdata.totlayer; i++) {
		if (bm->vdata.layers[i].type == CD_SHAPEKEY) {
			if (currkey->uid == bm->vdata.layers[i].uid) {
				return j;
			}
			j++;
		}
	}
	return -1;
}

BLI_INLINE void bmesh_quick_edgedraw_flag(MEdge *med, BMEdge *e)
{
	/* this is a cheap way to set the edge draw, its not precise and will
	 * pick the first 2 faces an edge uses.
	 * The dot comparison is a little arbitrary, but set so that a 5 subd
	 * IcoSphere won't vanish but subd 6 will (as with pre-bmesh blender) */


	if ( /* (med->flag & ME_EDGEDRAW) && */ /* assume to be true */
	     (e->l && (e->l != e->l->radial_next)) &&
	     (dot_v3v3(e->l->f->no, e->l->radial_next->f->no) > 0.9995f))
	{
		med->flag &= ~ME_EDGEDRAW;
	}
	else {
		med->flag |= ME_EDGEDRAW;
	}
}
#endif

static void bm_strands_make_strand(BMesh *bm, BMVert *root, Strands *UNUSED(strands), Key *UNUSED(key),
                                   struct DerivedMesh *emitter_dm, struct BVHTreeFromMesh *UNUSED(emitter_bvhtree),
                                   StrandIterator *it_strand)
{
	int numverts = BM_strands_keys_count(root);
	
	BMVert *v;
	BMIter iter;
	StrandVertexIterator it_vert;
	
	it_strand->curve->numverts = numverts;
	/* init root matrix, fully constructed below for non-degenerate strands */
	unit_m3(it_strand->curve->root_matrix);
	
	BKE_strand_vertex_iter_init(&it_vert, it_strand);
	BM_ITER_STRANDS_ELEM(v, &iter, root, BM_VERTS_OF_STRAND) {
		BLI_assert(BKE_strand_vertex_iter_valid(&it_vert));
		
		/* root */
		if (it_vert.index == 0) {
			float loc[3], nor[3], tang[3];
			BM_elem_meshsample_data_named_get(&bm->vdata, v, CD_MSURFACE_SAMPLE, CD_HAIR_ROOT_LOCATION, &it_strand->curve->msurf);
			BKE_mesh_sample_eval(emitter_dm, &it_strand->curve->msurf, loc, nor, tang);
			
			/* construct root matrix */
			copy_v3_v3(it_strand->curve->root_matrix[2], nor);
			copy_v3_v3(it_strand->curve->root_matrix[0], tang);
			cross_v3_v3v3(it_strand->curve->root_matrix[1], it_strand->curve->root_matrix[2], it_strand->curve->root_matrix[0]);
		}
		
		copy_v3_v3(it_vert.vertex->co, v->co);
		it_vert.vertex->time = numverts > 0 ? (float)it_vert.index / (float)(numverts - 1) : 0.0f;
		
		if (it_vert.index == 0) {
			/* weight 1.0 is used for pinning hair roots in particles */
			it_vert.vertex->weight = 1.0f;
		}
		else {
			it_vert.vertex->weight = BM_elem_float_data_named_get(&bm->vdata, v, CD_PROP_FLT, CD_HAIR_WEIGHT);
		}
		
		BKE_strand_vertex_iter_next(&it_vert);
		
		BM_CHECK_ELEMENT(v);
	}
}

/**
 * returns customdata shapekey index from a keyblock or -1
 * \note could split this out into a more generic function */
static int bm_shape_layer_index_from_kb(BMesh *bm, KeyBlock *currkey)
{
	int i;
	int j = 0;

	for (i = 0; i < bm->vdata.totlayer; i++) {
		if (bm->vdata.layers[i].type == CD_SHAPEKEY) {
			if (currkey->uid == bm->vdata.layers[i].uid) {
				return j;
			}
			j++;
		}
	}
	return -1;
}

/* go through and find any shapekey customdata layers
 * that might not have corresponding KeyBlocks, and add them if
 * necessary */
static void bm_strands_add_missing_shapekeys(BMesh *bm, Key *key)
{
	KeyBlock *currkey;
	int i;
	
	for (i = 0; i < bm->vdata.totlayer; i++) {
		const CustomDataLayer *layer = &bm->vdata.layers[i];
		if (layer->type != CD_SHAPEKEY)
			continue;
		
		for (currkey = key->block.first; currkey; currkey = currkey->next) {
			if (currkey->uid == layer->uid)
				break;
		}
		
		if (!currkey) {
			currkey = BKE_keyblock_add(key, layer->name);
			currkey->uid = layer->uid;
		}
	}
}

/* returns offset of the edit against the active shape, so other shapes can compensate accordingly to avoid deformation */
static void bm_strands_get_basiskey_offset(BMesh *bm, Strands *strands, Key *key, int cd_shape_keyindex_offset, float (**r_offset)[3])
{
	*r_offset = NULL;
	
	/* only need offsets for relative shape keys */
	if (key->type == KEY_RELATIVE) {
		
		KeyBlock *actkey = BLI_findlink(&key->block, bm->shapenr - 1);
		/* unlikely, but the active key may not be valid if the bmesh and the mesh are out of sync */
		if (!actkey)
			return;
		
		/* only if active key is a base */
		if (BKE_keyblock_is_basis(key, bm->shapenr - 1) && cd_shape_keyindex_offset >= 0) {
			float (*fp)[3] = actkey->data;
			float (*ofs)[3] = MEM_callocN(sizeof(float) * 3 * bm->totvert,  "currkey->data");
			BMIter iter;
			BMVert *eve;
			StrandsVertex *svert;
			int i;
			
			svert = strands->verts;
			BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, i) {
				const int keyi = BM_ELEM_CD_GET_INT(eve, cd_shape_keyindex_offset);
				
				if (keyi != ORIGINDEX_NONE) {
					sub_v3_v3v3(ofs[i], svert->co, fp[keyi]);
				}
				else {
					/* if there are new vertices in the mesh, we can't propagate the offset
					 * because it will only work for the existing vertices and not the new
					 * ones, creating a mess when doing e.g. subdivide + translate */
					MEM_freeN(ofs);
					ofs = NULL;
					break;
				}
				
				svert++;
			}
			
			*r_offset = ofs;
		}
	}
}

static float *bm_strands_apply_keyblock(BMesh *bm, Strands *strands, StrandsVertex *oldverts, Key *key, int cd_shape_keyindex_offset,
                                        KeyBlock *kb, KeyBlock *actkb, float (*oldkey)[3], float (*offset)[3])
{
	const bool apply_offset = (offset && (kb != actkb) && (bm->shapenr - 1 == kb->relative));
	const int shape_layer_index = bm_shape_layer_index_from_kb(bm, kb);
	const int cd_shape_offset = CustomData_get_n_offset(&bm->vdata, CD_SHAPEKEY, shape_layer_index);
	
	float *newkey, *fp;
	BMIter iter;
	BMVert *eve;
	StrandsVertex *svert;
	int keyi;
	float (*ofs_pt)[3] = offset;
	
	fp = newkey = MEM_callocN(key->elemsize * bm->totvert,  "currkey->data");
	
	svert = strands->verts;
	BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
		
		if (kb == actkb) {
			copy_v3_v3(fp, eve->co);
			
			if (actkb != key->refkey) { /* important see bug [#30771] */
				if (cd_shape_keyindex_offset != -1) {
					if (oldverts) {
						keyi = BM_ELEM_CD_GET_INT(eve, cd_shape_keyindex_offset);
						if (keyi != ORIGINDEX_NONE && keyi < kb->totelem) { /* valid old vertex */
							copy_v3_v3(svert->co, oldverts[keyi].co);
						}
					}
				}
			}
		}
		else if (shape_layer_index != -1) {
			/* in most cases this runs */
			copy_v3_v3(fp, BM_ELEM_CD_GET_VOID_P(eve, cd_shape_offset));
		}
		else if ((oldkey != NULL) &&
		         (cd_shape_keyindex_offset != -1) &&
		         ((keyi = BM_ELEM_CD_GET_INT(eve, cd_shape_keyindex_offset)) != ORIGINDEX_NONE) &&
		         (keyi < kb->totelem))
		{
			/* old method of reconstructing keys via vertice's original key indices,
			 * currently used if the new method above fails (which is theoretically
			 * possible in certain cases of undo) */
			copy_v3_v3(fp, oldkey[keyi]);
		}
		else {
			/* fail! fill in with dummy value */
			copy_v3_v3(fp, svert->co);
		}
		
		/* propagate edited basis offsets to other shapes */
		if (apply_offset) {
			add_v3_v3(fp, *ofs_pt++);
		}
		
		fp += 3;
		svert++;
	}
	
	return newkey;
}

static void bm_strands_apply_shapekeys(BMesh *bm, Strands *strands, StrandsVertex *oldverts, Key *key)
{
	const int cd_shape_keyindex_offset = CustomData_get_offset(&bm->vdata, CD_SHAPE_KEYINDEX);
	KeyBlock *actkb = BLI_findlink(&key->block, bm->shapenr - 1);
	KeyBlock *kb;
	float (*offset)[3] = NULL;

	bm_strands_add_missing_shapekeys(bm, key);

	if (oldverts)
		bm_strands_get_basiskey_offset(bm, strands, key, cd_shape_keyindex_offset, &offset);
	
	for (kb = key->block.first; kb; kb = kb->next) {
		float *newkey;
		
		newkey = bm_strands_apply_keyblock(bm, strands, oldverts, key, cd_shape_keyindex_offset, kb, actkb, kb->data, offset);
		
		kb->totelem = bm->totvert;
		if (kb->data) {
			MEM_freeN(kb->data);
		}
		kb->data = newkey;
	}
	
	if (offset)
		MEM_freeN(offset);
}

Strands *BM_strands_bm_to_strands(BMesh *bm, Strands *strands, Key *key, float mat[4][4], struct DerivedMesh *emitter_dm, struct BVHTreeFromMesh *emitter_bvhtree)
{
	Strands *oldstrands;
	int ntotcurves;
	
	BMVert *root;
	BMIter iter;
	StrandIterator it_strand;
	
	ntotcurves = BM_strands_count(bm);
	
	/* lets save the old strands just in case we are actually working on
	 * a key ... we now do processing of the keys at the end */
	oldstrands = strands;
	
	strands = BKE_strands_new(ntotcurves, bm->totvert);
	
//	strands->cd_flag = BM_strands_cd_flag_from_bmesh(bm);
	
	BKE_strand_iter_init(&it_strand, strands);
	BM_ITER_STRANDS(root, &iter, bm, BM_STRANDS_OF_MESH) {
		BLI_assert(BKE_strand_iter_valid(&it_strand));
		
		bm_strands_make_strand(bm, root, strands, key, emitter_dm, emitter_bvhtree, &it_strand);
		
		BKE_strand_iter_next(&it_strand);
	}
	bm->elem_index_dirty &= ~BM_VERT;
	
	/* transform from edit space (duplicator local space) back to the original object space */
	{
		float imat[4][4];
		int i;
		
		invert_m4_m4(imat, mat);
		
		for (i = 0; i < strands->totverts; ++i)
			mul_m4_v3(imat, strands->verts[i].co);
	}
	
	BKE_strands_ensure_normals(strands);


#if 0 // TODO
	{
		BMEditSelection *selected;
		me->totselect = BLI_listbase_count(&(bm->selected));

		if (me->mselect) MEM_freeN(me->mselect);

		me->mselect = MEM_callocN(sizeof(MSelect) * me->totselect, "Mesh selection history");


		for (i = 0, selected = bm->selected.first; selected; i++, selected = selected->next) {
			if (selected->htype == BM_VERT) {
				me->mselect[i].type = ME_VSEL;

			}
			else if (selected->htype == BM_EDGE) {
				me->mselect[i].type = ME_ESEL;

			}
			else if (selected->htype == BM_FACE) {
				me->mselect[i].type = ME_FSEL;
			}

			me->mselect[i].index = BM_elem_index_get(selected->ele);
		}
	}
#endif

	if (key) {
		bm_strands_apply_shapekeys(bm, strands, oldstrands ? oldstrands->verts : NULL, key);
	}

	if (oldstrands) {
		BKE_strands_free(oldstrands);
	}
	
	return strands;
}

/* ------------------------------------------------------------------------- */
/* ParticleSystem */

int BM_strands_count_psys_keys(ParticleSystem *psys)
{
	ParticleData *pa;
	int p;
	int totkeys = 0;
	
	for (p = 0, pa = psys->particles; p < psys->totpart; ++p, ++pa)
		totkeys += pa->totkey;
	
	return totkeys;
}

#if 0
static KeyBlock *bm_set_shapekey_from_psys(BMesh *bm, ParticleSystem *psys, int totvert, int act_key_nr)
{
	KeyBlock *actkey, *block;
	int i, j;
	
	if (!psys->key) {
		return NULL;
	}
	
	if (act_key_nr != 0)
		actkey = BLI_findlink(&psys->key->block, act_key_nr - 1);
	else
		actkey = NULL;
	
	CustomData_add_layer(&bm->vdata, CD_SHAPE_KEYINDEX, CD_ASSIGN, NULL, 0);
	
	/* check if we need to generate unique ids for the shapekeys.
		 * this also exists in the file reading code, but is here for
		 * a sanity check */
	if (!psys->key->uidgen) {
		fprintf(stderr,
		        "%s had to generate shape key uid's in a situation we shouldn't need to! "
		        "(bmesh internal error)\n",
		        __func__);
		
		psys->key->uidgen = 1;
		for (block = psys->key->block.first; block; block = block->next) {
			block->uid = psys->key->uidgen++;
		}
	}
	
	if (actkey && actkey->totelem == totvert) {
		bm->shapenr = act_key_nr;
	}
	
	for (i = 0, block = psys->key->block.first; block; block = block->next, i++) {
		CustomData_add_layer_named(&bm->vdata, CD_SHAPEKEY,
		                           CD_ASSIGN, NULL, 0, block->name);
		
		j = CustomData_get_layer_index_n(&bm->vdata, CD_SHAPEKEY, i);
		bm->vdata.layers[j].uid = block->uid;
	}
	
	return actkey;
}
#endif

/* create vertex and edge data for BMesh based on particle hair keys */
static void bm_make_particles(BMesh *bm, Object *ob, ParticleSystem *psys, struct DerivedMesh *emitter_dm, float (*keyco)[3], int cd_shape_keyindex_offset)
{
//	KeyBlock *block;
	ParticleData *pa;
	HairKey *hkey;
	int p, k;
	
	int vindex, eindex;
	BMVert *v = NULL, *v_prev;
	BMEdge *e;
	
	float hairmat[4][4];
	
	/* XXX currently all particles and keys have the same mass, this may change */
	float mass = psys->part->mass;
	
	vindex = 0;
	eindex = 0;
	for (p = 0, pa = psys->particles; p < psys->totpart; ++p, ++pa) {
		
		/* hair keys are in a local "hair space", but edit data should be in object space */
		psys_mat_hair_to_object(ob, emitter_dm, psys->part->from, pa, hairmat);
		
		for (k = 0, hkey = pa->hair; k < pa->totkey; ++k, ++hkey) {
			float co[3];
			
			copy_v3_v3(co, keyco ? keyco[vindex] : hkey->co);
			mul_m4_v3(hairmat, co);
			
			v_prev = v;
			v = BM_vert_create(bm, co, NULL, BM_CREATE_SKIP_CD);
			BM_elem_index_set(v, vindex); /* set_ok */
			
			/* transfer flag */
//			v->head.hflag = BM_vert_flag_from_mflag(mvert->flag & ~SELECT);
			
			/* this is necessary for selection counts to work properly */
//			if (hkey->editflag & SELECT) {
//				BM_vert_select_set(bm, v, true);
//			}
			
//			normal_short_to_float_v3(v->no, mvert->no);
			
			/* Copy Custom Data */
//			CustomData_to_bmesh_block(&me->vdata, &bm->vdata, vindex, &v->head.data, true);
			CustomData_bmesh_set_default(&bm->vdata, &v->head.data);
			
			BM_elem_float_data_named_set(&bm->vdata, v, CD_PROP_FLT, CD_HAIR_MASS, mass);
			BM_elem_float_data_named_set(&bm->vdata, v, CD_PROP_FLT, CD_HAIR_WEIGHT, hkey->weight);
			
			/* root */
			if (k == 0) {
				MSurfaceSample root_loc;
				if (BKE_mesh_sample_from_particle(&root_loc, psys, emitter_dm, pa)) {
					BM_elem_meshsample_data_named_set(&bm->vdata, v, CD_MSURFACE_SAMPLE, CD_HAIR_ROOT_LOCATION, &root_loc);
				}
			}
			
#if 0
			/* set shapekey data */
			if (psys->key) {
				/* set shape key original index */
				if (cd_shape_keyindex_offset != -1) BM_ELEM_CD_SET_INT(v, cd_shape_keyindex_offset, vindex);
				
				for (block = psys->key->block.first, j = 0; block; block = block->next, j++) {
					float *co = CustomData_bmesh_get_n(&bm->vdata, v->head.data, CD_SHAPEKEY, j);
					
					if (co) {
						copy_v3_v3(co, ((float *)block->data) + 3 * vindex);
					}
				}
			}
#else
			(void)cd_shape_keyindex_offset;
#endif
			
			vindex += 1;
			
			if (k > 0) {
				e = BM_edge_create(bm, v_prev, v, NULL, BM_CREATE_SKIP_CD);
				BM_elem_index_set(e, eindex); /* set_ok; one less edge than vertices for each particle */
				
				/* transfer flags */
//				e->head.hflag = BM_edge_flag_from_mflag(medge->flag & ~SELECT);
				
				/* this is necessary for selection counts to work properly */
//				if (medge->flag & SELECT) {
//					BM_edge_select_set(bm, e, true);
//				}
				
				/* Copy Custom Data */
//				CustomData_to_bmesh_block(&me->edata, &bm->edata, eindex, &e->head.data, true);
				CustomData_bmesh_set_default(&bm->edata, &e->head.data);
				
				eindex += 1;
			}
			
		} /* hair keys */
	
	} /* particles */
	
	bm->elem_index_dirty &= ~(BM_VERT | BM_EDGE); /* added in order, clear dirty flag */
}

/**
 * \brief ParticleSystem -> BMesh
 */
void BM_strands_bm_from_psys(BMesh *bm, Object *ob, ParticleSystem *psys, struct DerivedMesh *emitter_dm,
                             const bool set_key, int act_key_nr)
{
	// KeyBlock *actkey;
	float (*keyco)[3] = NULL;
	int totvert, totedge;
	
	int cd_shape_keyindex_offset;
	
	/* free custom data */
	/* this isnt needed in most cases but do just incase */
	CustomData_free(&bm->vdata, bm->totvert);
	CustomData_free(&bm->edata, bm->totedge);
	CustomData_free(&bm->ldata, bm->totloop);
	CustomData_free(&bm->pdata, bm->totface);
	
	totvert = BM_strands_count_psys_keys(psys);
	totedge = totvert - psys->totpart;
	
	if (!psys || !totvert || !totedge) {
		if (psys) { /*no verts? still copy customdata layout*/
			CustomData_bmesh_init_pool(&bm->vdata, totvert, BM_VERT);
			CustomData_bmesh_init_pool(&bm->edata, totedge, BM_EDGE);
			CustomData_bmesh_init_pool(&bm->ldata, 0, BM_LOOP);
			CustomData_bmesh_init_pool(&bm->pdata, 0, BM_FACE);
		}
		return; /* sanity check */
	}

#if 0
	actkey = bm_set_shapekey_from_psys(bm, psys, totvert, act_key_nr);
	if (actkey)
		keyco = actkey->data;
#else
	(void)act_key_nr;
#endif

	CustomData_bmesh_init_pool(&bm->vdata, totvert, BM_VERT);
	CustomData_bmesh_init_pool(&bm->edata, totedge, BM_EDGE);

	BM_strands_cd_flag_apply(bm, /*psys->cd_flag*/0);

	cd_shape_keyindex_offset = /*psys->key ? CustomData_get_offset(&bm->vdata, CD_SHAPE_KEYINDEX) :*/ -1;

	bm_make_particles(bm, ob, psys, emitter_dm, set_key ? keyco : NULL, cd_shape_keyindex_offset);


#if 0 /* TODO */
	if (me->mselect && me->totselect != 0) {

		BMVert **vert_array = MEM_mallocN(sizeof(BMVert *) * bm->totvert, "VSelConv");
		BMEdge **edge_array = MEM_mallocN(sizeof(BMEdge *) * bm->totedge, "ESelConv");
		BMFace **face_array = MEM_mallocN(sizeof(BMFace *) * bm->totface, "FSelConv");
		MSelect *msel;

#pragma omp parallel sections if (bm->totvert + bm->totedge + bm->totface >= BM_OMP_LIMIT)
		{
#pragma omp section
			{ BM_iter_as_array(bm, BM_VERTS_OF_MESH, NULL, (void **)vert_array, bm->totvert); }
#pragma omp section
			{ BM_iter_as_array(bm, BM_EDGES_OF_MESH, NULL, (void **)edge_array, bm->totedge); }
#pragma omp section
			{ BM_iter_as_array(bm, BM_FACES_OF_MESH, NULL, (void **)face_array, bm->totface); }
		}

		for (i = 0, msel = me->mselect; i < me->totselect; i++, msel++) {
			switch (msel->type) {
				case ME_VSEL:
					BM_select_history_store(bm, (BMElem *)vert_array[msel->index]);
					break;
				case ME_ESEL:
					BM_select_history_store(bm, (BMElem *)edge_array[msel->index]);
					break;
				case ME_FSEL:
					BM_select_history_store(bm, (BMElem *)face_array[msel->index]);
					break;
			}
		}

		MEM_freeN(vert_array);
		MEM_freeN(edge_array);
		MEM_freeN(face_array);
	}
	else {
		me->totselect = 0;
		if (me->mselect) {
			MEM_freeN(me->mselect);
			me->mselect = NULL;
		}
	}
#endif
}

#if 0
/**
 * \brief BMesh -> Mesh
 */
static BMVert **bm_to_mesh_vertex_map(BMesh *bm, int ototvert)
{
	const int cd_shape_keyindex_offset = CustomData_get_offset(&bm->vdata, CD_SHAPE_KEYINDEX);
	BMVert **vertMap = NULL;
	BMVert *eve;
	int i = 0;
	BMIter iter;

	/* caller needs to ensure this */
	BLI_assert(ototvert > 0);

	vertMap = MEM_callocN(sizeof(*vertMap) * ototvert, "vertMap");
	if (cd_shape_keyindex_offset != -1) {
		BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, i) {
			const int keyi = BM_ELEM_CD_GET_INT(eve, cd_shape_keyindex_offset);
			if ((keyi != ORIGINDEX_NONE) && (keyi < ototvert)) {
				vertMap[keyi] = eve;
			}
		}
	}
	else {
		BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, i) {
			if (i < ototvert) {
				vertMap[i] = eve;
			}
			else {
				break;
			}
		}
	}

	return vertMap;
}

/**
 * returns customdata shapekey index from a keyblock or -1
 * \note could split this out into a more generic function */
static int bm_to_mesh_shape_layer_index_from_kb(BMesh *bm, KeyBlock *currkey)
{
	int i;
	int j = 0;

	for (i = 0; i < bm->vdata.totlayer; i++) {
		if (bm->vdata.layers[i].type == CD_SHAPEKEY) {
			if (currkey->uid == bm->vdata.layers[i].uid) {
				return j;
			}
			j++;
		}
	}
	return -1;
}

BLI_INLINE void bmesh_quick_edgedraw_flag(MEdge *med, BMEdge *e)
{
	/* this is a cheap way to set the edge draw, its not precise and will
	 * pick the first 2 faces an edge uses.
	 * The dot comparison is a little arbitrary, but set so that a 5 subd
	 * IcoSphere won't vanish but subd 6 will (as with pre-bmesh blender) */


	if ( /* (med->flag & ME_EDGEDRAW) && */ /* assume to be true */
	     (e->l && (e->l != e->l->radial_next)) &&
	     (dot_v3v3(e->l->f->no, e->l->radial_next->f->no) > 0.9995f))
	{
		med->flag &= ~ME_EDGEDRAW;
	}
	else {
		med->flag |= ME_EDGEDRAW;
	}
}
#endif

static void make_particle_hair(BMesh *bm, BMVert *root, Object *ob, ParticleSystem *psys, struct DerivedMesh *emitter_dm, struct BVHTreeFromMesh *emitter_bvhtree, struct ParticleData *pa)
{
	int totkey = BM_strands_keys_count(root);
	HairKey *hair;
	
	BMVert *v;
	BMIter iter;
	HairKey *hkey;
	int k;
	
	float inv_hairmat[4][4];
	
	pa->alive = PARS_ALIVE;
	pa->flag = 0;
	
	pa->time = 0.0f;
	pa->lifetime = 100.0f;
	pa->dietime = 100.0f;
	
	pa->size = psys->part->size;
	
	// TODO define other particle stuff ...
	
	hair = MEM_callocN(totkey * sizeof(HairKey), "hair keys");
	
	hkey = hair;
	k = 0;
	BM_ITER_STRANDS_ELEM(v, &iter, root, BM_VERTS_OF_STRAND) {
		/* root */
		if (k == 0) {
			MSurfaceSample root_loc;
			BM_elem_meshsample_data_named_get(&bm->vdata, v, CD_MSURFACE_SAMPLE, CD_HAIR_ROOT_LOCATION, &root_loc);
			if (!BKE_mesh_sample_to_particle(&root_loc, psys, emitter_dm, emitter_bvhtree, pa)) {
				pa->num = 0;
				pa->num_dmcache = DMCACHE_NOTFOUND;
				zero_v4(pa->fuv);
				pa->foffset = 0.0f;
			}
			
			/* edit data is in object space, hair keys must be converted back into "hair space" */
			psys_mat_hair_to_object(ob, emitter_dm, psys->part->from, pa, inv_hairmat);
			invert_m4(inv_hairmat);
		}
		
		mul_v3_m4v3(hkey->co, inv_hairmat, v->co);
		mul_v3_m4v3(hkey->world_co, ob->obmat, v->co);
		
		hkey->time = totkey > 0 ? (float)k / (float)(totkey - 1) : 0.0f;
		if (k == 0) {
			/* weight 1.0 is used for pinning hair roots in particles */
			hkey->weight = 1.0f;
		}
		else {
			hkey->weight = BM_elem_float_data_named_get(&bm->vdata, v, CD_PROP_FLT, CD_HAIR_WEIGHT);
		}
		
		++hkey;
		++k;
		
		BM_CHECK_ELEMENT(v);
	}
	
	if (pa->hair)
		MEM_freeN(pa->hair);
	
	pa->hair = hair;
	pa->totkey = totkey;
}

void BM_strands_bm_to_psys(BMesh *bm, Object *ob, ParticleSystem *psys, struct DerivedMesh *emitter_dm, struct BVHTreeFromMesh *emitter_bvhtree)
{
	ParticleData *particles, *oldparticles;
	int ototpart, ototkey, ntotpart;
	
	BMVert *root;
	BMIter iter;
	ParticleData *pa;
	int p;
	
	ototpart = psys->totpart;
	ototkey = BM_strands_count_psys_keys(psys);
	
	ntotpart = BM_strands_count(bm);
	
	/* new particles block */
	if (bm->totvert == 0) particles = NULL;
	else particles = MEM_callocN(ntotpart * sizeof(ParticleData), "particles");
	
	/* lets save the old particles just in case we are actually working on
	 * a key ... we now do processing of the keys at the end */
	oldparticles = psys->particles;
	
	psys->totpart = ntotpart;
	
//	psys->cd_flag = BM_strands_cd_flag_from_bmesh(bm);
	
	pa = particles;
	p = 0;
	BM_ITER_STRANDS(root, &iter, bm, BM_STRANDS_OF_MESH) {
		
		make_particle_hair(bm, root, ob, psys, emitter_dm, emitter_bvhtree, pa);
		
		++pa;
		++p;
	}
	bm->elem_index_dirty &= ~BM_VERT;


#if 0 // TODO
	{
		BMEditSelection *selected;
		me->totselect = BLI_listbase_count(&(bm->selected));

		if (me->mselect) MEM_freeN(me->mselect);

		me->mselect = MEM_callocN(sizeof(MSelect) * me->totselect, "Mesh selection history");


		for (i = 0, selected = bm->selected.first; selected; i++, selected = selected->next) {
			if (selected->htype == BM_VERT) {
				me->mselect[i].type = ME_VSEL;

			}
			else if (selected->htype == BM_EDGE) {
				me->mselect[i].type = ME_ESEL;

			}
			else if (selected->htype == BM_FACE) {
				me->mselect[i].type = ME_FSEL;
			}

			me->mselect[i].index = BM_elem_index_get(selected->ele);
		}
	}
#endif

#if 0 // TODO
	/* see comment below, this logic is in twice */

	if (me->key) {
		const int cd_shape_keyindex_offset = CustomData_get_offset(&bm->vdata, CD_SHAPE_KEYINDEX);

		KeyBlock *currkey;
		KeyBlock *actkey = BLI_findlink(&me->key->block, bm->shapenr - 1);

		float (*ofs)[3] = NULL;

		/* go through and find any shapekey customdata layers
		 * that might not have corresponding KeyBlocks, and add them if
		 * necessary */
		j = 0;
		for (i = 0; i < bm->vdata.totlayer; i++) {
			if (bm->vdata.layers[i].type != CD_SHAPEKEY)
				continue;

			for (currkey = me->key->block.first; currkey; currkey = currkey->next) {
				if (currkey->uid == bm->vdata.layers[i].uid)
					break;
			}

			if (!currkey) {
				currkey = BKE_keyblock_add(me->key, bm->vdata.layers[i].name);
				currkey->uid = bm->vdata.layers[i].uid;
			}

			j++;
		}


		/* editing the base key should update others */
		if ((me->key->type == KEY_RELATIVE) && /* only need offsets for relative shape keys */
		    (actkey != NULL) &&                /* unlikely, but the active key may not be valid if the
		                                        * bmesh and the mesh are out of sync */
		    (oldverts != NULL))                /* not used here, but 'oldverts' is used later for applying 'ofs' */
		{
			const bool act_is_basis = BKE_keyblock_is_basis(me->key, bm->shapenr - 1);

			/* active key is a base */
			if (act_is_basis && (cd_shape_keyindex_offset != -1)) {
				float (*fp)[3] = actkey->data;

				ofs = MEM_callocN(sizeof(float) * 3 * bm->totvert,  "currkey->data");
				mvert = me->mvert;
				BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, i) {
					const int keyi = BM_ELEM_CD_GET_INT(eve, cd_shape_keyindex_offset);

					if (keyi != ORIGINDEX_NONE) {
						sub_v3_v3v3(ofs[i], mvert->co, fp[keyi]);
					}
					else {
						/* if there are new vertices in the mesh, we can't propagate the offset
						 * because it will only work for the existing vertices and not the new
						 * ones, creating a mess when doing e.g. subdivide + translate */
						MEM_freeN(ofs);
						ofs = NULL;
						break;
					}

					mvert++;
				}
			}
		}

		for (currkey = me->key->block.first; currkey; currkey = currkey->next) {
			const bool apply_offset = (ofs && (currkey != actkey) && (bm->shapenr - 1 == currkey->relative));
			int cd_shape_offset;
			int keyi;
			float (*ofs_pt)[3] = ofs;
			float *newkey, (*oldkey)[3], *fp;

			j = bm_to_mesh_shape_layer_index_from_kb(bm, currkey);
			cd_shape_offset = CustomData_get_n_offset(&bm->vdata, CD_SHAPEKEY, j);


			fp = newkey = MEM_callocN(me->key->elemsize * bm->totvert,  "currkey->data");
			oldkey = currkey->data;

			mvert = me->mvert;
			BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {

				if (currkey == actkey) {
					copy_v3_v3(fp, eve->co);

					if (actkey != me->key->refkey) { /* important see bug [#30771] */
						if (cd_shape_keyindex_offset != -1) {
							if (oldverts) {
								keyi = BM_ELEM_CD_GET_INT(eve, cd_shape_keyindex_offset);
								if (keyi != ORIGINDEX_NONE && keyi < currkey->totelem) { /* valid old vertex */
									copy_v3_v3(mvert->co, oldverts[keyi].co);
								}
							}
						}
					}
				}
				else if (j != -1) {
					/* in most cases this runs */
					copy_v3_v3(fp, BM_ELEM_CD_GET_VOID_P(eve, cd_shape_offset));
				}
				else if ((oldkey != NULL) &&
				         (cd_shape_keyindex_offset != -1) &&
				         ((keyi = BM_ELEM_CD_GET_INT(eve, cd_shape_keyindex_offset)) != ORIGINDEX_NONE) &&
				         (keyi < currkey->totelem))
				{
					/* old method of reconstructing keys via vertice's original key indices,
					 * currently used if the new method above fails (which is theoretically
					 * possible in certain cases of undo) */
					copy_v3_v3(fp, oldkey[keyi]);
				}
				else {
					/* fail! fill in with dummy value */
					copy_v3_v3(fp, mvert->co);
				}

				/* propagate edited basis offsets to other shapes */
				if (apply_offset) {
					add_v3_v3(fp, *ofs_pt++);
				}

				fp += 3;
				mvert++;
			}

			currkey->totelem = bm->totvert;
			if (currkey->data) {
				MEM_freeN(currkey->data);
			}
			currkey->data = newkey;
		}

		if (ofs) MEM_freeN(ofs);
	}
#else
	psys->particles = particles;
#endif

	if (oldparticles) {
		ParticleData *pa;
		int p;
		for (p = 0, pa = oldparticles; p < ototpart; ++p, ++pa)
			if (pa->hair)
				MEM_freeN(pa->hair);
		MEM_freeN(oldparticles);
	}
}
