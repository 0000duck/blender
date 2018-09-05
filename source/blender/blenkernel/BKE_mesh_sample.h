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
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_MESH_SAMPLE_H__
#define __BKE_MESH_SAMPLE_H__

/** \file BKE_mesh_sample.h
 *  \ingroup bke
 */

struct Mesh;
struct Key;
struct KeyBlock;
struct Mesh;
struct MFace;
struct MVert;
struct MPoly;
struct MCol;

struct MeshSample;
struct MeshSampleGenerator;

typedef struct MeshSampleGenerator MeshSampleGenerator;

typedef void* (*MeshSampleThreadContextCreateFp)(void *userdata, int start);
typedef void (*MeshSampleThreadContextFreeFp)(void *userdata, void *thread_ctx);
typedef bool (*MeshSampleRayFp)(void *userdata, void *thread_ctx, float ray_start[3], float ray_end[3]);

/* ==== Utility Functions ==== */

float* BKE_mesh_sample_calc_triangle_weights(struct Mesh *mesh, const float *loop_weights, float *r_area);

void BKE_mesh_sample_weights_from_loc(struct MeshSample *sample, struct Mesh *mesh, int looptri_index, const float loc[3]);


/* ==== Evaluate ==== */

bool BKE_mesh_sample_is_valid(const struct MeshSample *sample);
bool BKE_mesh_sample_is_volume_sample(const struct MeshSample *sample);

/* Evaluate position and normal on the given mesh */
bool BKE_mesh_sample_eval(const struct Mesh *mesh, const struct MeshSample *sample, float loc[3], float nor[3], float tang[3]);

/* Evaluate UV coordinates on the given mesh */
bool BKE_mesh_sample_eval_uv(const struct Mesh *mesh, const struct MeshSample *sample, int uv_layer, float uv[2]);
/* Evaluate vertex color on the given mesh */
bool BKE_mesh_sample_eval_col(const struct Mesh *mesh, const struct MeshSample *sample, int col_layer, struct MLoopCol *col);

/* Evaluate position for the given shapekey */
bool BKE_mesh_sample_shapekey(struct Key *key, struct KeyBlock *kb, const struct MeshSample *sample, float loc[3]);

void BKE_mesh_sample_clear(struct MeshSample *sample);


/* ==== Generator Types ==== */

struct MeshSampleGenerator *BKE_mesh_sample_gen_surface_vertices(void);

/* vertex_weight_cb is optional */
struct MeshSampleGenerator *BKE_mesh_sample_gen_surface_random(unsigned int seed, bool use_area_weight, const float *loop_weights);

struct MeshSampleGenerator *BKE_mesh_sample_gen_surface_raycast(
        MeshSampleThreadContextCreateFp thread_context_create_cb,
        MeshSampleThreadContextFreeFp thread_context_free_cb,
        MeshSampleRayFp ray_cb,
        void *userdata);

struct MeshSampleGenerator *BKE_mesh_sample_gen_surface_poissondisk(unsigned int seed, float mindist, unsigned int max_samples, const float *loop_weights);

struct MeshSampleGenerator *BKE_mesh_sample_gen_volume_random_bbray(unsigned int seed, float density);

void BKE_mesh_sample_free_generator(struct MeshSampleGenerator *gen);


/* ==== Sampling ==== */

void BKE_mesh_sample_generator_bind(struct MeshSampleGenerator *gen, struct Mesh *mesh);
void BKE_mesh_sample_generator_unbind(struct MeshSampleGenerator *gen);

unsigned int BKE_mesh_sample_gen_get_max_samples(const struct MeshSampleGenerator *gen);

/* Generate a single sample.
 * Not threadsafe!
 */
bool BKE_mesh_sample_generate(struct MeshSampleGenerator *gen, struct MeshSample *sample);

/* Generate a large number of samples.
 */
int BKE_mesh_sample_generate_batch_ex(struct MeshSampleGenerator *gen,
                                      void *output_buffer, int output_stride, int count,
                                      bool use_threads);

int BKE_mesh_sample_generate_batch(struct MeshSampleGenerator *gen,
                                   MeshSample *output_buffer, int count);

/* ==== Utilities ==== */

struct ParticleSystem;
struct ParticleData;
struct BVHTreeFromMesh;

bool BKE_mesh_sample_from_particle(struct MeshSample *sample, struct ParticleSystem *psys, struct Mesh *mesh, struct ParticleData *pa);
bool BKE_mesh_sample_to_particle(struct MeshSample *sample, struct ParticleSystem *psys, struct Mesh *mesh, struct BVHTreeFromMesh *bvhtree, struct ParticleData *pa);

#endif  /* __BKE_MESH_SAMPLE_H__ */
