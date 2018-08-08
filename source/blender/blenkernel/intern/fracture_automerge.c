#include "MEM_guardedalloc.h"

#include "BKE_fracture.h"
#include "BKE_customdata.h"
#include "BKE_mesh.h"
#include "BKE_pointcache.h"

#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_fracture_types.h"
#include "DNA_group_types.h"
#include "DNA_scene_types.h"
#include "DNA_rigidbody_types.h"

#include "BLI_math.h"
#include "BLI_kdtree.h"
#include "BLI_ghash.h"
#include "BLI_listbase.h"

#include "bmesh.h"

static void do_match_normals(MPoly *mp, MPoly *other_mp, MVert *mvert, MLoop *mloop)
{
	MLoop ml, ml2;
	MVert *v, *v2;
	short sno[3];
	float fno[3], fno2[3];
	int j;

	if (mp->totloop == other_mp->totloop) //mpoly+index
	{
		for (j = 0; j < mp->totloop; j++)
		{
			ml = mloop[mp->loopstart + j];
			ml2 = mloop[other_mp->loopstart + j];
			v = mvert + ml.v;
			v2 = mvert + ml2.v;

			normal_short_to_float_v3(fno, v->no);
			normal_short_to_float_v3(fno2, v2->no);
			add_v3_v3(fno, fno2);
			mul_v3_fl(fno, 0.5f);
			normal_float_to_short_v3(sno, fno);
			copy_v3_v3_short(v->no, sno);
			copy_v3_v3_short(v2->no, sno);
		}
	}
}

void BKE_fracture_face_pairs(FractureModifierData *fmd, Mesh *dm, Object *ob)
{
	/* make kdtree of all faces of dm, then find closest face for each face*/
	MPoly *mp = NULL;
	MPoly *mpoly = dm->mpoly;
	MLoop* mloop = dm->mloop;
	MVert* mvert = dm->mvert;
	int totpoly = dm->totpoly;
	KDTree *tree = BLI_kdtree_new(totpoly);
	int i = 0;

	//TODO, work with poly customdata int layer maybe to store "Innerness" ?
	int inner_index = 1;// BKE_object_material_slot_find_index(ob, fmd->inner_material) - 1;

	//printf("Make Face Pairs\n");
	int faces = 0, pairs = 0;

	for (i = 0, mp = mpoly; i < totpoly; mp++, i++) {
		float co[3];
		BKE_fracture_face_calc_center_mean(dm, mp, co);
		if (mp->mat_nr == inner_index)
		{
			BLI_kdtree_insert(tree, i, co);
			faces++;
		}
	}

	BLI_kdtree_balance(tree);

	/*now find pairs of close faces*/

	for (i = 0, mp = mpoly; i < totpoly; mp++, i++) {
		if (mp->mat_nr == inner_index) { /* treat only inner faces ( with inner material) */
			int index = -1, j = 0, r = 0;
			KDTreeNearest *n;
			float co[3];

			BKE_fracture_face_calc_center_mean(dm, mp, co);
			r = BLI_kdtree_range_search(tree, co, &n, fmd->autohide_dist);
			//r = BLI_kdtree_find_nearest_n(tree, co, n, 2);
			/*2nd nearest means not ourselves...*/
			if (r == 0)
				continue;

			index = n[0].index;
			while ((j < r) && i == index) {
				index = n[j].index;
				//printf("I, INDEX %d %d %f\n", i, index, n[j].dist);
				j++;
			}

			if (!BLI_ghash_haskey(fmd->shared->face_pairs, SET_INT_IN_POINTER(index))) {
				BLI_ghash_insert(fmd->shared->face_pairs, SET_INT_IN_POINTER(i), SET_INT_IN_POINTER(index));
				pairs++;
				/*match normals...*/
				if (fmd->fix_normals) {
					do_match_normals(mp, mpoly+index, mvert, mloop);
				}
			}

			if (n != NULL) {
				MEM_freeN(n);
			}
		}
	}

	if (faces == 0 || pairs == 0) {
		BLI_ghash_free(fmd->shared->face_pairs, NULL, NULL);
		fmd->shared->face_pairs = NULL;
	}

	printf("faces, pairs: %d %d\n", faces, pairs);
	BLI_kdtree_free(tree);
}

static void find_other_face(FractureModifierData *fmd, int i, BMesh* bm, Object* ob, BMFace ***faces, int *del_faces)
{
	float f_centr[3], f_centr_other[3];
	BMFace *f1, *f2;
	int other = GET_INT_FROM_POINTER(BLI_ghash_lookup(fmd->shared->face_pairs, SET_INT_IN_POINTER(i)));
	int inner_index = 1;// BKE_object_material_slot_find_index(ob, fmd->inner_material) - 1;

	if ((other == i) && (fmd->fracture_mode != MOD_FRACTURE_DYNAMIC))
	{
		//printf("other == i %d \n", i);
		f1 = BM_face_at_index(bm, i);

		if (f1->mat_nr == inner_index)
		{
			/*is this a remainder face ? */
			*faces = MEM_reallocN(*faces, sizeof(BMFace *) * ((*del_faces) + 1));
			(*faces)[*del_faces] = f1;
			(*del_faces) += 1;
		}

		return;
	}

	if (other >= bm->totface) {
		return;
	}

	f1 = BM_face_at_index(bm, i);
	f2 = BM_face_at_index(bm, other);

	if ((f1 == NULL) || (f2 == NULL)) {
		return;
	}

	BM_face_calc_center_mean(f1, f_centr);
	BM_face_calc_center_mean(f2, f_centr_other);


	if ((len_squared_v3v3(f_centr, f_centr_other) < (fmd->autohide_dist)) && (f1 != f2) &&
		(f1->mat_nr == inner_index) && (f2->mat_nr == inner_index))
	{
		bool in_filter = false;

		/*filter out face pairs, if we have an autohide filter group */
		if (fmd->autohide_filter_group){
			CollectionObject *go;
			for (go = fmd->autohide_filter_group->gobject.first; go; go = go->next) {
				/*check location and scale (maximum size if nonuniform) for now */
				/*if not in any filter range, delete... else keep */
				Object* obj = go->ob;
				float f1_loc[3], f2_loc[3];
				float radius = MAX3(obj->size[0], obj->size[1], obj->size[2]);

				/* TODO XXX watch out if go->ob is parented to ob (Transformation error ?) */
				mul_v3_m4v3(f1_loc, ob->obmat, f_centr);
				mul_v3_m4v3(f2_loc, ob->obmat, f_centr_other);
				radius = radius * radius;

				if ((len_squared_v3v3(f1_loc, obj->loc) < radius) &&
					(len_squared_v3v3(f2_loc, obj->loc) < radius))
				{
					in_filter = true;
					break;
				}
				else
				{
					in_filter = false;
				}
			}
		}

		if (!fmd->autohide_filter_group || !in_filter)
		{
			/*intact face pairs */
			*faces = MEM_reallocN(*faces, sizeof(BMFace *) * ((*del_faces) + 2));
			(*faces)[*del_faces] = f1;
			(*faces)[(*del_faces) + 1] = f2;
			(*del_faces) += 2;
		}
	}
}

static void reset_automerge(FractureModifierData *fmd)
{
	SharedVert *sv;
	SharedVertGroup *vg;

	for (vg = fmd->shared->shared_verts.first; vg; vg = vg->next) {
		vg->exceeded = false;
		//vg->excession_frame = -1;
		//vg->moved = false;
		zero_v3(vg->delta);
		vg->deltas_set = false;

		for (sv = vg->verts.first; sv; sv = sv->next)
		{
			//sv->excession_frame = -1;
			sv->exceeded = false;
			//sv->moved = false;
			zero_v3(sv->delta);
			sv->deltas_set = false;
		}
	}
}

static void calc_delta(SharedVert* sv, BMVert *v)
{
	//apply deltas
	float a[3], b[3], delta[3], quat[3], co[3];
	copy_v3_v3(co, v->co);
	normalize_v3_v3(a, sv->rest_co);
	normalize_v3_v3(b, v->co);
	rotation_between_vecs_to_quat(quat, a, b);

	copy_v3_v3(delta, sv->delta);
	mul_qt_v3(quat, delta);
	add_v3_v3(co, delta);
	copy_v3_v3(v->co, co);
}

static void clamp_delta(SharedVert *sv, FractureModifierData *fmd)
{
	float factor = (fmd->automerge_dist * fmd->automerge_dist) / len_squared_v3(sv->delta);
	if (factor < 1.0f)
	{
		mul_v3_fl(sv->delta, factor);
	}
}

static void handle_vertex(FractureModifierData *fmd, BMesh* bm, SharedVert *sv, float co[3], float no[3],
						  int cd_edge_crease_offset)
{
	bool do_calc_delta = fmd->keep_distort;
	float dist = fmd->autohide_dist;
	BMEdge *e = NULL;
	Scene *sc = NULL; //fmd->modifier.scene; TODO store "exceededness" in BMVert customdatalayer ? not framebased
	int frame = sc ? (int)BKE_scene_frame_get(sc) : 1;
	BMVert *v = bm->vtable[sv->index];
	bool exceeded = (frame >= sv->excession_frame) && (sv->excession_frame > -1);

	if ((len_squared_v3v3(co, v->co) > (dist * dist)))
	{
		sv->moved = true;
	}

	if ((len_squared_v3v3(co, v->co) <= fmd->automerge_dist * fmd->automerge_dist) && !exceeded)
	{
		copy_v3_v3(v->co, co);
		copy_v3_v3(v->no, no);
	}
	else {

		if (sv->excession_frame == -1)
		{
			sv->excession_frame = frame;
		}

		if (!sv->deltas_set) {
			sub_v3_v3v3(sv->delta, co, v->co);
			clamp_delta(sv, fmd);
			sv->deltas_set = true;
		}
	}

	if (exceeded)
	{
		BMIter iter;
		if (do_calc_delta && sv->deltas_set)
		{
			calc_delta(sv, v);
		}

		BM_ITER_ELEM(e, &iter, v, BM_EDGES_OF_VERT)
		{
			BM_ELEM_CD_SET_FLOAT(e, cd_edge_crease_offset, fmd->inner_crease);
		}
	}
}

static void prepare_automerge(FractureModifierData *fmd, BMesh *bm, Scene* sc)
{
	SharedVert *sv;
	SharedVertGroup *vg;
	int frame = sc ? BKE_scene_frame_get(sc) : 1;

	int cd_edge_crease_offset = CustomData_get_offset(&bm->edata, CD_CREASE);
	if (cd_edge_crease_offset == -1) {
		BM_data_layer_add(bm, &bm->edata, CD_CREASE);
		cd_edge_crease_offset = CustomData_get_offset(&bm->edata, CD_CREASE);
	}

	for (vg = fmd->shared->shared_verts.first; vg; vg = vg->next) {
		BMVert* v1, *v2;
		float co[3], no[3], inverse;
		int verts = 0;

		v1 = bm->vtable[vg->index];
		copy_v3_v3(co, v1->co);
		copy_v3_v3(no, v1->no);
		verts = 1;

		for (sv = vg->verts.first; sv; sv = sv->next)
		{
			bool exceeded = (frame >= sv->excession_frame) && (sv->excession_frame > -1);
			if (!exceeded)
			{
				v2 = bm->vtable[sv->index];
				add_v3_v3(co, v2->co);
				add_v3_v3(no, v2->no);
				verts++;
			}
		}

		inverse = 1.0f/(float)verts;
		mul_v3_fl(co, inverse);
		mul_v3_fl(no, inverse);
		verts = 0;

		handle_vertex(fmd, bm, (SharedVert*)vg, co, no, cd_edge_crease_offset);

		for (sv = vg->verts.first; sv; sv = sv->next)
		{
			handle_vertex(fmd, bm, sv, co, no, cd_edge_crease_offset);
		}
	}
}

static void optimize_automerge(FractureModifierData *fmd)
{
	SharedVertGroup *vg = fmd->shared->shared_verts.first, *next = NULL;
	SharedVert* sv = NULL;
	int removed = 0, count = 0;

	while(vg) {
		bool intact = true;
		sv = vg->verts.first;
		while (sv) {
			intact = intact && !sv->moved;
			sv = sv->next;
		}

		intact = intact && !vg->moved;

		next = vg->next;

		if (intact) {
			while(vg->verts.first) {
				sv = vg->verts.first;
				BLI_remlink(&vg->verts, sv);
				MEM_freeN(sv);
				sv = NULL;
			}


			BLI_remlink(&fmd->shared->shared_verts, vg);
			MEM_freeN(vg);
			removed++;
		}

		vg = next;
	}

	count = BLI_listbase_count(&fmd->shared->shared_verts);
	printf("remaining | removed groups: %d | %d\n", count, removed);
}
static Mesh* centroids_to_verts(FractureModifierData* fmd, BMesh* bm, Object* ob)
{
	BMIter viter;
	Mesh *dm = NULL;
	MVert *mv = NULL;
	BMVert *v = NULL;
	MeshIsland *mi;
	//only add verts where centroids are...
	float imat[4][4];
	float *velX, *velY, *velZ;
	int i = 0;
	int dm_totvert = BLI_listbase_count(&fmd->shared->meshIslands);
	int totvert = dm_totvert + bm->totvert;


	invert_m4_m4(imat, ob->obmat);

	dm = BKE_mesh_new_nomain(totvert, 0, 0, 0, 0);

	mv = dm->mvert;
	velX = CustomData_add_layer_named(&dm->vdata, CD_PROP_FLT, CD_CALLOC, NULL, totvert, "velX");
	velY = CustomData_add_layer_named(&dm->vdata, CD_PROP_FLT, CD_CALLOC, NULL, totvert, "velY");
	velZ = CustomData_add_layer_named(&dm->vdata, CD_PROP_FLT, CD_CALLOC, NULL, totvert, "velZ");

	for (mi = fmd->shared->meshIslands.first; mi; mi = mi->next)
	{
		RigidBodyOb *rbo = mi->rigidbody;
		mul_v3_m4v3(mv[i].co, imat, mi->rigidbody->pos);
		velX[i] = rbo->lin_vel[0] + rbo->ang_vel[0];
		velY[i] = rbo->lin_vel[1] + rbo->ang_vel[1];
		velZ[i] = rbo->lin_vel[2] + rbo->ang_vel[2];
		i++;
	}

	i = 0;
	BM_ITER_MESH_INDEX(v, &viter, bm, BM_VERTS_OF_MESH, i)
	{
		copy_v3_v3(mv[i + dm_totvert].co, v->co);
		velX[i + dm_totvert] = BM_elem_float_data_get_named(&bm->vdata, v, CD_PROP_FLT, "velX");
		velY[i + dm_totvert] = BM_elem_float_data_get_named(&bm->vdata, v, CD_PROP_FLT, "velY");
		velZ[i + dm_totvert] = BM_elem_float_data_get_named(&bm->vdata, v, CD_PROP_FLT, "velZ");
	}

	return dm;
}

Mesh *BKE_fracture_autohide_do(FractureModifierData *fmd, Mesh *dm, Object *ob, Scene* sc)
{
	int totpoly = dm->totpoly;
	int i = 0;
	BMesh *bm;
	Mesh *result;
	BMFace **faces = MEM_mallocN(sizeof(BMFace *), "faces");
	int del_faces = 0;
	bool do_merge = fmd->do_merge;
	struct BMeshToMeshParams bmt = {.calc_object_remap = 0};

	if (fmd->use_centroids && !fmd->use_vertices)
	{
		BM_mesh_create(&bm_mesh_allocsize_default,  &((struct BMeshCreateParams){.use_toolflags = true,}));
		result = centroids_to_verts(fmd, bm, ob);
		BM_mesh_free(bm);
		MEM_freeN(faces);
		return result;
	}
	else {
	   bm = BKE_fracture_mesh_to_bmesh(dm);
	}

	BM_mesh_elem_index_ensure(bm, BM_FACE | BM_VERT);
	BM_mesh_elem_table_ensure(bm, BM_FACE | BM_VERT);
	BM_mesh_elem_toolflags_ensure(bm);


	if (!fmd->use_centroids)
	{
		RigidBodyWorld *rbw = sc ? sc->rigidbody_world : NULL;
		PointCache *cache = rbw ? rbw->shared->pointcache : NULL;
		int frame = (int)BKE_scene_frame_get(sc);
		int endframe = sc->r.efra;
		int testframe = cache != NULL ? MIN2(cache->endframe, endframe) : endframe;

		if (fmd->automerge_dist > 0)
		{
			//make vert groups together here, if vert is close enough
			prepare_automerge(fmd, bm, sc);
		}

		if (frame == testframe) {
			optimize_automerge(fmd);
		}
	}

	if (fmd->shared->face_pairs && fmd->autohide_dist > 0)
	{
		BM_mesh_elem_hflag_disable_all(bm, BM_FACE | BM_EDGE | BM_VERT , BM_ELEM_SELECT, false);

		for (i = 0; i < totpoly; i++) {
			find_other_face(fmd, i, bm, ob,  &faces, &del_faces);
		}

		for (i = 0; i < del_faces; i++) {
			BMFace *f = faces[i];
			if (f->l_first->e != NULL) { /* a lame check.... */
				BMIter iter;
				BMVert *v;
				BM_ITER_ELEM(v, &iter, f, BM_VERTS_OF_FACE)
				{
					BM_elem_flag_enable(v, BM_ELEM_SELECT);
				}

				BM_elem_flag_enable(f, BM_ELEM_SELECT);
			}
		}

		if (fmd->frac_algorithm != MOD_FRACTURE_BISECT && fmd->frac_algorithm != MOD_FRACTURE_BISECT_FAST)
		{
			BMO_op_callf(bm, (BMO_FLAG_DEFAULTS & ~BMO_FLAG_RESPECT_HIDE), "delete_keep_normals geom=%hf context=%i", BM_ELEM_SELECT, DEL_FACES);
		}
	}

	if (del_faces == 0) {
		/*fallback if you want to merge verts but use no filling method, whose faces could be hidden (and you dont have any selection then) */
		BM_mesh_elem_hflag_enable_all(bm, BM_FACE | BM_EDGE | BM_VERT , BM_ELEM_SELECT, false);
	}

	if (fmd->use_vertices)
	{	//only output verts
		BMO_op_callf(bm, (BMO_FLAG_DEFAULTS & ~BMO_FLAG_RESPECT_HIDE), "delete geom=%aef context=%i", DEL_EDGESFACES);

		if (fmd->use_centroids)
		{
			result = centroids_to_verts(fmd, bm, ob);
			BM_mesh_free(bm);
			MEM_freeN(faces);
			return result;
		}
	}

	if (fmd->automerge_dist > 0 && do_merge) {

		//separate this, because it costs performance and might not work so well with thin objects, but its useful for smooth objects
		if (fmd->frac_algorithm == MOD_FRACTURE_BISECT || fmd->frac_algorithm == MOD_FRACTURE_BISECT_FAST)
		{
			//here we dont expect inner faces and odd interpolation so we can recalc the normals
			BMO_op_callf(bm, (BMO_FLAG_DEFAULTS & ~BMO_FLAG_RESPECT_HIDE),
					 "automerge verts=%hv dist=%f", BM_ELEM_SELECT,
					 0.0001f); /*need to merge larger cracks*/
		}
		else {

			//here we might need to keep the original normals
			BMO_op_callf(bm, (BMO_FLAG_DEFAULTS & ~BMO_FLAG_RESPECT_HIDE),
					 "automerge_keep_normals verts=%hv dist=%f", BM_ELEM_SELECT,
					 0.0001f); /*need to merge larger cracks*/
		}

		if (fmd->fix_normals) {
			/* dissolve sharp edges with limit dissolve
			 * this causes massive flicker with displacements and possibly with glass too when autohide is enabled
			 * so use this only when fix normals has been requested and automerge is enabled
			 * for glass in most cases autohide is enough, for displacements too, fix normals and automerge are for special cases where you
			 * want to clear off nearly all cracks (with smooth objects for example), in those cases you still might experience flickering
			 * when using glass or displacements */
			BMO_op_callf(bm, (BMO_FLAG_DEFAULTS & ~BMO_FLAG_RESPECT_HIDE), "dissolve_limit_keep_normals "
						 "angle_limit=%f use_dissolve_boundaries=%b verts=%av edges=%ae delimit=%i",
						 DEG2RADF(1.0f), false, 0);
		}
	}

	if (!fmd->fix_normals)
	{
		BM_mesh_normals_update(bm);
	}

	BM_mesh_bm_to_me(NULL, bm, result, &bmt);

	BM_mesh_free(bm);
	MEM_freeN(faces);

	return result;
}

void BKE_fracture_normal_find(Mesh *dm, KDTree *tree, float co[3], short no[3], short rno[3], float range)
{
	KDTreeNearest *n = NULL, n2;
	int index = 0, i = 0, count = 0;
	MVert* mvert;
	float fno[3], vno[3];

	normal_short_to_float_v3(fno, no);

	count = BLI_kdtree_range_search(tree, co, &n, range);
	for (i = 0; i < count; i++)
	{
		index = n[i].index;
		mvert = dm->mvert + index;
		normal_short_to_float_v3(vno, mvert->no);
		if ((dot_v3v3(fno, vno) > 0.0f)){
			copy_v3_v3_short(rno, mvert->no);
			if (n != NULL) {
				MEM_freeN(n);
				n = NULL;
			}
			return;
		}
	}

	if (n != NULL) {
		MEM_freeN(n);
		n = NULL;
	}

	/*fallback if no valid normal in searchrange....*/
	BLI_kdtree_find_nearest(tree, co, &n2);
	index = n2.index;
	mvert = dm->mvert + index;
	copy_v3_v3_short(rno, mvert->no);
}

void BKE_fracture_physics_mesh_normals_fix(FractureModifierData *fmd, Shard* s, MeshIsland* mi, int i, Mesh* orig_dm)
{
	MVert *mv, *verts;
	int totvert;
	int j;

	mi->physics_mesh = BKE_fracture_shard_to_mesh(s, true);
	totvert = mi->physics_mesh->totvert;
	verts = mi->physics_mesh->mvert;

	mi->vertco = MEM_mallocN(sizeof(float) * 3 * totvert, "vertco");
	mi->vertno = MEM_mallocN(sizeof(short) * 3 * totvert, "vertno");

	for (mv = verts, j = 0; j < totvert; mv++, j++) {
		short no[3];

		mi->vertco[j * 3] = mv->co[0];
		mi->vertco[j * 3 + 1] = mv->co[1];
		mi->vertco[j * 3 + 2] = mv->co[2];

		/* either take orignormals or take ones from fractured mesh */
		if (fmd->fix_normals) {
		   BKE_fracture_normal_find(orig_dm, fmd->shared->nor_tree, mv->co, mv->no, no, fmd->nor_range);
		}
		else {
			copy_v3_v3_short(no, mv->no);
		}

		mi->vertno[j * 3] = no[0];
		mi->vertno[j * 3 + 1] = no[1];
		mi->vertno[j * 3 + 2] = no[2];

		if (fmd->fix_normals) {
			copy_v3_v3_short(mi->vertices_cached[j]->no, no);
			copy_v3_v3_short(mv->no, no);
		}

		/* then eliminate centroid in vertex coords*/
		sub_v3_v3(mv->co, s->centroid);
	}

	if (fmd->fix_normals)
	{
		printf("Fixing Normals: %d\n", i);
	}
}

void BKE_fracture_shared_vert_groups(FractureModifierData* fmd, Mesh* dm, ListBase *shared_verts)
{
	/* make kdtree of all verts of dm, then find closest(rangesearch) verts for each vert*/
	MVert* mvert = dm->mvert , *mv = NULL;
	int totvert = dm->totvert;
	KDTree *tree = BLI_kdtree_new(totvert);
	GHash* visit = BLI_ghash_int_new("visited_verts");
	int i = 0;

	//printf("Make Face Pairs\n");
	int groups = 0;

	for (i = 0, mv = mvert; i < totvert; mv++, i++) {
		BLI_kdtree_insert(tree, i, mv->co);
	}

	BLI_kdtree_balance(tree);

	/*now find groups of close verts*/

	for (i = 0, mv = mvert; i < totvert; mv++, i++) {
		int index = -1, j = 0, r = 0;
		KDTreeNearest *n = NULL;

		r = BLI_kdtree_range_search(tree, mv->co, &n, fmd->autohide_dist);
		/*2nd nearest means not ourselves...*/

		if (r > 0) {
			SharedVertGroup *gvert = MEM_mallocN(sizeof(SharedVertGroup), "sharedVertGroup");
			gvert->index = i;
			gvert->verts.first = NULL;
			gvert->verts.last = NULL;
			gvert->exceeded = false;
			gvert->deltas_set = false;
			gvert->moved = false;
			gvert->excession_frame = -1;
			zero_v3(gvert->delta);
			copy_v3_v3(gvert->rest_co, mvert[i].co);

			for (j = 0; j < r; j++)
			{
				index = n[j].index;
				if (!BLI_ghash_haskey(visit, SET_INT_IN_POINTER(index)))
				{
					BLI_ghash_insert(visit, SET_INT_IN_POINTER(index), SET_INT_IN_POINTER(index));

					if (i != index)
					{
						SharedVert *svert = MEM_mallocN(sizeof(SharedVert), "sharedVert");
						svert->index = index;
						svert->exceeded = false;
						svert->deltas_set = false;
						svert->moved = false;
						svert->excession_frame = -1;
						zero_v3(svert->delta);
						copy_v3_v3(svert->rest_co, mvert[index].co);
						BLI_addtail(&gvert->verts, svert);
					}
				}
			}

			if (gvert->verts.first != NULL)
			{
				BLI_addtail(shared_verts, gvert);
				groups++;
			}
			else {
				MEM_freeN(gvert);
			}
		}

		if (n != NULL) {
			MEM_freeN(n);
		}
	}

	printf("shared vert groups: %d\n", groups);
	BLI_ghash_free(visit, NULL, NULL);
	BLI_kdtree_free(tree);
}

static void free_shared_vert_group(SharedVertGroup *vg)
{
	SharedVert *sv;

	while (vg->verts.first) {
		sv = vg->verts.first;
		BLI_remlink(&vg->verts, sv);
		MEM_freeN(sv);
	}
	MEM_freeN(vg);
}

void BKE_fracture_shared_verts_free(ListBase* lb)
{
	SharedVertGroup *vg = lb->first;

	while (vg)
	{
		SharedVertGroup *next;
		next = vg->next;

		BLI_remlink(lb, vg);
		free_shared_vert_group(vg);
		vg = next;
	}

	lb->first = NULL;
	lb->last = NULL;
}

void BKE_fracture_automerge_refresh(FractureModifierData* fmd)
{
	printf("GAH, refreshing automerge\n");
	BKE_fracture_shared_verts_free(&fmd->shared->shared_verts);

	/* in case of re-using existing islands this one might become invalid for automerge, so force fallback */
	if (fmd->shared->dm && fmd->shared->dm->totvert > 0)
	{
		BKE_fracture_shared_vert_groups(fmd, fmd->shared->dm, &fmd->shared->shared_verts);
	}
	else if (fmd->shared->visible_mesh)
	{
		Mesh* fdm = BKE_fracture_bmesh_to_mesh(fmd->shared->visible_mesh);
		BKE_fracture_shared_vert_groups(fmd, fdm, &fmd->shared->shared_verts);

		BKE_mesh_free(fdm);
		fdm = NULL;
	}
}

void BKE_fracture_autohide_refresh(FractureModifierData *fmd, Object *ob)
{
	fmd->refresh_autohide = false;
	/*HERE make a kdtree of the fractured derivedmesh,
	 * store pairs of faces (MPoly) here (will be most likely the inner faces) */
	if (fmd->shared->face_pairs != NULL) {
		BLI_ghash_free(fmd->shared->face_pairs, NULL, NULL);
		fmd->shared->face_pairs = NULL;
	}

	fmd->shared->face_pairs = BLI_ghash_int_new("face_pairs");

	/* in case of re-using existing islands this one might become invalid for autohide, so force fallback */
	if (fmd->shared->dm && fmd->shared->dm->totpoly > 0)
	{
		BKE_fracture_face_pairs(fmd, fmd->shared->dm, ob);
	}
	else if (fmd->shared->visible_mesh)
	{
		Mesh* fdm = BKE_fracture_bmesh_to_mesh(fmd->shared->visible_mesh);
		BKE_fracture_face_pairs(fmd, fdm, ob);

		BKE_mesh_free(fdm);
		fdm = NULL;
	}
}
