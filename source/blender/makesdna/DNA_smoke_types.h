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
 * The Original Code is Copyright (C) 2006 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Daniel Genrich (Genscher)
 *                 Sebastian Barschkis (sebbas)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file DNA_smoke_types.h
 *  \ingroup DNA
 */

#ifndef __DNA_SMOKE_TYPES_H__
#define __DNA_SMOKE_TYPES_H__

/* flags */
enum {
	FLUID_DOMAIN_USE_NOISE = (1 << 1),  /* use noise */
	FLUID_DOMAIN_USE_DISSOLVE = (1 << 2),  /* let smoke dissolve */
	FLUID_DOMAIN_USE_DISSOLVE_LOG = (1 << 3),  /* using 1/x for dissolve */

#ifdef DNA_DEPRECATED
	FLUID_DOMAIN_USE_HIGH_SMOOTH = (1 << 5),  /* -- Deprecated -- */
#endif
	FLUID_DOMAIN_FILE_LOAD = (1 << 6),  /* flag for file load */
	FLUID_DOMAIN_USE_ADAPTIVE_DOMAIN = (1 << 7),
	FLUID_DOMAIN_USE_ADAPTIVE_TIME = (1 << 8), /* adaptive time stepping in domain */
	FLUID_DOMAIN_USE_MESH = (1 << 9),  /* use mesh */
	FLUID_DOMAIN_USE_GUIDING = (1 << 10),  /* use guiding */
	FLUID_DOMAIN_USE_SPEED_VECTORS = (1 << 11),  /* generate mesh speed vectors */
};

/* border collisions */
enum {
	FLUID_DOMAIN_BORDER_FRONT = (1 << 1),
	FLUID_DOMAIN_BORDER_BACK = (1 << 2),
	FLUID_DOMAIN_BORDER_RIGHT = (1 << 3),
	FLUID_DOMAIN_BORDER_LEFT = (1 << 4),
	FLUID_DOMAIN_BORDER_TOP = (1 << 5),
	FLUID_DOMAIN_BORDER_BOTTOM = (1 << 6),
};

/* cache file formats */
enum {
	FLUID_DOMAIN_FILE_UNI = (1 << 0),
	FLUID_DOMAIN_FILE_OPENVDB = (1 << 1),
	FLUID_DOMAIN_FILE_RAW = (1 << 2),
	FLUID_DOMAIN_FILE_OBJECT = (1 << 3),
	FLUID_DOMAIN_FILE_BIN_OBJECT = (1 << 4),
};

/* slice method */
enum {
	FLUID_DOMAIN_SLICE_VIEW_ALIGNED = 0,
	FLUID_DOMAIN_SLICE_AXIS_ALIGNED = 1,
};

/* axis aligned method */
enum {
	AXIS_SLICE_FULL   = 0,
	AXIS_SLICE_SINGLE = 1,
};

/* single slice direction */
enum {
	SLICE_AXIS_AUTO = 0,
	SLICE_AXIS_X    = 1,
	SLICE_AXIS_Y    = 2,
	SLICE_AXIS_Z    = 3,
};

enum {
	VECTOR_DRAW_NEEDLE     = 0,
	VECTOR_DRAW_STREAMLINE = 1,
};

enum {
	FLUID_DOMAIN_FIELD_DENSITY    = 0,
	FLUID_DOMAIN_FIELD_HEAT       = 1,
	FLUID_DOMAIN_FIELD_FUEL       = 2,
	FLUID_DOMAIN_FIELD_REACT      = 3,
	FLUID_DOMAIN_FIELD_FLAME      = 4,
	FLUID_DOMAIN_FIELD_VELOCITY_X = 5,
	FLUID_DOMAIN_FIELD_VELOCITY_Y = 6,
	FLUID_DOMAIN_FIELD_VELOCITY_Z = 7,
	FLUID_DOMAIN_FIELD_COLOR_R    = 8,
	FLUID_DOMAIN_FIELD_COLOR_G    = 9,
	FLUID_DOMAIN_FIELD_COLOR_B    = 10,
	FLUID_DOMAIN_FIELD_FORCE_X    = 11,
	FLUID_DOMAIN_FIELD_FORCE_Y    = 12,
	FLUID_DOMAIN_FIELD_FORCE_Z    = 13,
};

/* domain types */
#define FLUID_DOMAIN_TYPE_GAS    0
#define FLUID_DOMAIN_TYPE_LIQUID 1

/* noise */
#define FLUID_NOISE_TYPE_WAVELET (1<<0)

/* viewport preview types */
#define FLUID_DOMAIN_VIEWPORT_GEOMETRY  0
#define FLUID_DOMAIN_VIEWPORT_PREVIEW   1
#define FLUID_DOMAIN_VIEWPORT_FINAL     2

/* mesh levelset generator types */
#define FLUID_DOMAIN_MESH_IMPROVED    0
#define FLUID_DOMAIN_MESH_UNION       1

/* guiding velocity source */
#define FLUID_DOMAIN_GUIDING_SRC_DOMAIN   0
#define FLUID_DOMAIN_GUIDING_SRC_EFFECTOR 1

/* fluid data fields (active_fields) */
#define FLUID_DOMAIN_ACTIVE_HEAT      (1<<0)
#define FLUID_DOMAIN_ACTIVE_FIRE      (1<<1)
#define FLUID_DOMAIN_ACTIVE_COLORS    (1<<2)
#define FLUID_DOMAIN_ACTIVE_COLOR_SET (1<<3)
#define FLUID_DOMAIN_ACTIVE_OBSTACLE  (1<<4)
#define FLUID_DOMAIN_ACTIVE_GUIDING   (1<<5)
#define FLUID_DOMAIN_ACTIVE_INVEL     (1<<6)

/* particle types */
#define FLUID_DOMAIN_PARTICLE_FLIP   (1<<0)
#define FLUID_DOMAIN_PARTICLE_DROP   (1<<1)
#define FLUID_DOMAIN_PARTICLE_BUBBLE (1<<2)
#define FLUID_DOMAIN_PARTICLE_FLOAT  (1<<3)
#define FLUID_DOMAIN_PARTICLE_TRACER (1<<4)

/* cache options */
#define FLUID_DOMAIN_BAKING_DATA         1
#define FLUID_DOMAIN_BAKED_DATA          2
#define FLUID_DOMAIN_BAKING_NOISE        4
#define FLUID_DOMAIN_BAKED_NOISE         8
#define FLUID_DOMAIN_BAKING_MESH         16
#define FLUID_DOMAIN_BAKED_MESH          32
#define FLUID_DOMAIN_BAKING_PARTICLES    64
#define FLUID_DOMAIN_BAKED_PARTICLES     128
#define FLUID_DOMAIN_BAKING_GUIDING      256
#define FLUID_DOMAIN_BAKED_GUIDING       512

#define FLUID_DOMAIN_DIR_DEFAULT    "cache_fluid"
#define FLUID_DOMAIN_DIR_DATA       "data"
#define FLUID_DOMAIN_DIR_NOISE      "noise"
#define FLUID_DOMAIN_DIR_MESH       "mesh"
#define FLUID_DOMAIN_DIR_PARTICLES  "particles"
#define FLUID_DOMAIN_DIR_GUIDING    "guiding"
#define FLUID_DOMAIN_DIR_SCRIPT     "script"
#define FLUID_DOMAIN_SMOKE_SCRIPT   "smoke_script.py"
#define FLUID_DOMAIN_LIQUID_SCRIPT  "liquid_script.py"

/* Deprecated values (i.e. all defines and enums below this line up until typedefs)*/
/* cache compression */
#define SM_CACHE_LIGHT		0
#define SM_CACHE_HEAVY		1

/* high resolution sampling types */
#define SM_HRES_NEAREST		0
#define SM_HRES_LINEAR		1
#define SM_HRES_FULLSAMPLE	2

enum {
	VDB_COMPRESSION_BLOSC = 0,
	VDB_COMPRESSION_ZIP   = 1,
	VDB_COMPRESSION_NONE  = 2,
};

typedef struct SmokeVertexVelocity {
	float vel[3];
} SmokeVertexVelocity;

typedef struct SmokeDomainSettings {
	struct SmokeModifierData *smd; /* for fast RNA access */
	struct FLUID *fluid;
	struct FLUID_3D *fluid_old; /* adaptive domain needs access to old fluid state */
	void *fluid_mutex;
	struct Group *fluid_group;
	struct Group *eff_group; // UNUSED
	struct Group *coll_group; // collision objects group
	struct GPUTexture *tex;
	struct GPUTexture *tex_wt;
	struct GPUTexture *tex_shadow;
	struct GPUTexture *tex_flame;
	struct Object *guiding_parent;
	struct SmokeVertexVelocity *mesh_velocities; /* vertex velocities of simulated fluid mesh */
	struct EffectorWeights *effector_weights;

	/* domain object data */
	float p0[3]; /* start point of BB in local space (includes sub-cell shift for adaptive domain)*/
	float p1[3]; /* end point of BB in local space */
	float dp0[3]; /* difference from object center to grid start point */
	float cell_size[3]; /* size of simulation cell in local space */
	float global_size[3]; /* global size of domain axises */
	float prev_loc[3];
	int shift[3]; /* current domain shift in simulation cells */
	float shift_f[3]; /* exact domain shift */
	float obj_shift_f[3]; /* how much object has shifted since previous smoke frame (used to "lock" domain while drawing) */
	float imat[4][4]; /* domain object imat */
	float obmat[4][4]; /* domain obmat */
	float fluidmat[4][4]; /* low res fluid matrix */
	float fluidmat_wt[4][4]; /* high res fluid matrix */
	int base_res[3]; /* initial "non-adapted" resolution */
	int res_min[3]; /* cell min */
	int res_max[3]; /* cell max */
	int res[3]; /* data resolution (res_max-res_min) */
	int total_cells;
	float dx; /* 1.0f / res */
	float scale; /* largest domain size */
	char pad_object[4]; /* unused */

	/* adaptive domain options */
	int adapt_margin;
	int adapt_res;
	float adapt_threshold;
	char pad_adaptive[4]; /* unused */

	/* fluid domain options */
	int maxres; /* longest axis on the BB gets this resolution assigned */
	int solver_res;	/* dimension of manta solver, 2d or 3d */
	int border_collisions;	/* How domain border collisions are handled */
	int flags; /* use-mesh, use-noise, etc. */
	float gravity[3];
	int active_fields;
	short type; /* gas, liquid */
	char pad_fluid[6]; /* unused */

	/* smoke domain options */
	float alpha;
	float beta;
	int diss_speed;/* in frames */
	float vorticity;
	float active_color[3]; /* monitor smoke color */
	int highres_sampling;

	/* flame options */
	float burning_rate, flame_smoke, flame_vorticity;
	float flame_ignition, flame_max_temp;
	float flame_smoke_color[3];

	/* noise options */
	float noise_strength;
	float noise_pos_scale;
	float noise_time_anim;
	int res_noise[3];
	int noise_scale;
	short noise_type; /* noise type: wave, curl, anisotropic */
	char pad_noise[2]; /* unused */

	/* liquid domain options */
	float particle_randomness;
	int particle_number;
	int particle_minimum;
	int particle_maximum;
	float particle_radius;
	float particle_band_width;

	/* diffusion options*/
	float surface_tension;
	float viscosity_base;
	int viscosity_exponent;
	float domain_size;

	/* mesh options */
	float mesh_smoothen_upper;
	float mesh_smoothen_lower;
	int mesh_smoothen_pos;
	int mesh_smoothen_neg;
	int mesh_scale;
	int totvert;
	short mesh_generator;
	char pad_mesh[6]; /* unused */

	/* secondary particle options */
	float particle_droplet_threshold;
	float particle_droplet_amount;
	int particle_droplet_life;
	int particle_droplet_max;
	float particle_bubble_rise;
	int particle_bubble_life;
	int particle_bubble_max;
	float particle_floater_amount;
	int particle_floater_life;
	int particle_floater_max;
	float particle_tracer_amount;
	int particle_tracer_life;
	int particle_tracer_max;
	int particle_type;
	int particle_scale;
	char pad_particle[4]; /* unused */

	/* fluid guiding options */
	float guiding_alpha; /* guiding weight scalar (determines strength) */
	int guiding_beta; /* guiding blur radius (affects size of vortices) */
	float guiding_vel_factor; /* multiply guiding velocity by this factor */
	int *guide_res; /* res for velocity guide grids - independent from base res */
	short guiding_source;
	char pad_guiding[6]; /* unused */

	/* cache options */
	int cache_frame_start;
	int cache_frame_end;
	int cache_frame_pause_data;
	int cache_frame_pause_noise;
	int cache_frame_pause_mesh;
	int cache_frame_pause_particles;
	int cache_frame_pause_guiding;
	int cache_flag;
	char cache_mesh_format;
	char cache_data_format;
	char cache_particle_format;
	char cache_noise_format;
	char cache_directory[1024];
	char error[64]; /* Bake error description */
	char pad_cache[4]; /* unused */

	/* viewport display options */
	short viewport_display_mode;
	short render_display_mode;
	char pad_viewport[4];

	/* time options */
	float time_scale;
	float cfl_condition;

	/* display options */
	char slice_method, axis_slice_method;
	char slice_axis, draw_velocity;
	float slice_per_voxel;
	float slice_depth;
	float display_thickness;
	struct ColorBand *coba;
	float vector_scale;
	char vector_draw_type;
	char use_coba;
	char coba_field;  /* simulation field used for the color mapping */
	char pad_display; /* unused */

	/* -- Deprecated / unsed options (below)-- */

	/* view options */
	int viewsettings;
	char pad_view[4]; /* unused */

	/* OpenVDB cache options */
	int openvdb_comp;
	float clipping;
	char data_depth;
	char pad_vdb[7]; /* unused */

	/* pointcache options */
	/* Smoke uses only one cache from now on (index [0]), but keeping the array for now for reading old files. */
	struct PointCache *point_cache[2];	/* definition is in DNA_object_force_types.h */
	struct ListBase ptcaches[2];
	int cache_comp;
	int cache_high_comp;

} SmokeDomainSettings;

/* type */
#define FLUID_FLOW_TYPE_SMOKE     1
#define FLUID_FLOW_TYPE_FIRE      2
#define FLUID_FLOW_TYPE_SMOKEFIRE 3
#define FLUID_FLOW_TYPE_LIQUID    4

/* behavior */
#define FLUID_FLOW_BEHAVIOR_INFLOW   0
#define FLUID_FLOW_BEHAVIOR_OUTFLOW  1
#define FLUID_FLOW_BEHAVIOR_GEOMETRY 2

/* flow source */
#define FLUID_FLOW_SOURCE_PARTICLES 0
#define FLUID_FLOW_SOURCE_MESH      1

/* flow texture type */
#define FLUID_FLOW_TEXTURE_MAP_AUTO 0
#define FLUID_FLOW_TEXTURE_MAP_UV   1

/* flags */
#define FLUID_FLOW_ABSOLUTE (1<<1) /* old style emission */
#define FLUID_FLOW_INITVELOCITY (1<<2) /* passes particles speed to the smoke */
#define FLUID_FLOW_TEXTUREEMIT (1<<3) /* use texture to control emission speed */
#define FLUID_FLOW_USE_PART_SIZE (1<<4) /* use specific size for particles instead of closest cell */
#define FLUID_FLOW_USE_INFLOW (1<<5) /* control when to apply inflow */

typedef struct SmokeFlowSettings {
	struct SmokeModifierData *smd; /* for fast RNA access */
	struct DerivedMesh *dm;
	struct ParticleSystem *psys;
	struct Tex *noise_texture;

	/* initial velocity */
	float *verts_old; /* previous vertex positions in domain space */
	int numverts;
	float vel_multi; // Multiplier for inherited velocity
	float vel_normal;
	float vel_random;

	/* emission */
	float density;
	float color[3];
	float fuel_amount;
	float temp; /* delta temperature (temp - ambient temp) */
	float volume_density; /* density emitted within mesh volume */
	float surface_distance; /* maximum emission distance from mesh surface */
	float particle_size;
	int subframes;

	/* texture control */
	float texture_size;
	float texture_offset;
	int pad;
	char uvlayer_name[64];	/* MAX_CUSTOMDATA_LAYER_NAME */
	short vgroup_density;

	short type; /* smoke, flames, both, outflow, liquid */
	short behavior; /* inflow, outflow, static */
	short source;
	short texture_type;
	short pad2[3];
	int flags; /* absolute emission etc*/
} SmokeFlowSettings;

/* effector types */
#define FLUID_EFFECTOR_TYPE_COLLISION 0
#define FLUID_EFFECTOR_TYPE_GUIDE     1

/* guiding velocity modes */
#define FLUID_EFFECTOR_GUIDING_MAXIMUM   0
#define FLUID_EFFECTOR_GUIDING_MINIMUM   1
#define FLUID_EFFECTOR_GUIDING_OVERRIDE  2
#define FLUID_EFFECTOR_GUIDING_AVERAGED  3

/* collision objects (filled with smoke) */
typedef struct SmokeCollSettings {
	struct SmokeModifierData *smd; /* for fast RNA access */
	struct DerivedMesh *dm;
	float *verts_old;
	int numverts;
	float surface_distance; /* thickness of mesh surface, used in obstacle sdf */
	short type;

	/* guiding options */
	short guiding_mode;
	float vel_multi; // Multiplier for object velocity
} SmokeCollSettings;

#endif
