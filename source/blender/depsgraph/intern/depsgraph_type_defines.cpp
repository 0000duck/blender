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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 *
 * Original Author: Joshua Leung
 * Contributor(s): None Yet
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * Defines and code for core node types
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_utildefines.h"

#include "DNA_action_types.h"
#include "DNA_anim_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_action.h"
#include "BKE_animsys.h"
#include "BKE_armature.h"
#include "BKE_object.h"

#include "DEG_depsgraph.h"

#include "RNA_access.h"
#include "RNA_types.h"
} /* extern "C" */

#include "depsgraph_types.h"
#include "depsgraph_intern.h"
#include "depsnode.h"
#include "depsnode_component.h"
#include "depsnode_operation.h"

#include "depsgraph_util_map.h"

#include "stubs.h" // XXX: THIS MUST BE REMOVED WHEN THE DEPSGRAPH REFACTOR IS DONE

void BKE_animsys_eval_driver(ID *id, FCurve *fcurve) {}

void BKE_object_constraints_evaluate(Object *ob) {}
void BKE_pose_constraints_evaluate(Object *ob, bPoseChannel *pchan) {}

void BKE_pose_iktree_evaluate(Object *ob, bPoseChannel *rootchan) {}
void BKE_pose_splineik_evaluate(Object *ob, bPoseChannel *rootchan) {}
void BKE_pose_eval_bone(Object *ob, bPoseChannel *pchan) {}

void BKE_pose_rebuild_op(Object *ob, bPose *pose) {}
void BKE_pose_eval_init(Object *ob, bPose *pose) {}
void BKE_pose_eval_flush(Object *ob, bPose *pose) {}

void BKE_particle_system_eval(Object *ob, ParticleSystem *psys) {}

void BKE_rigidbody_rebuild_sim(Scene *scene) {}
void BKE_rigidbody_eval_simulation(Scene *scene) {}
void BKE_rigidbody_object_sync_transforms(Scene *scene, Object *ob) {}

void BKE_object_eval_local_transform(Scene *scene, Object *ob, int a, int b, int c) {
	printf("BKE_object_eval_local_transform on %s (%d, %d, %d)\n", ob->id.name, a, b, c);
	BKE_object_where_is_calc_ex(scene, NULL, ob, NULL);
}
void BKE_object_eval_parent(Object *ob) {}
void BKE_object_eval_modifier(Object *ob, ModifierData *md) {}

void BKE_mesh_eval_geometry(Mesh *mesh) {}
void BKE_mball_eval_geometry(MetaBall *mball) {}
void BKE_curve_eval_geometry(Curve *curve) {}
void BKE_curve_eval_path(Curve *curve) {}
void BKE_lattice_eval_geometry(Lattice *latt) {}

const string deg_op_name_object_parent = "BKE_object_eval_parent";
const string deg_op_name_object_local_transform = "BKE_object_eval_local_transform";
const string deg_op_name_constraint_stack = "Constraint Stack";
const string deg_op_name_rigidbody_world_rebuild = "Rigidbody World Rebuild";
const string deg_op_name_rigidbody_world_simulate = "Rigidbody World Do Simulation";
const string deg_op_name_rigidbody_object_sync = "RigidBodyObject Sync";
const string deg_op_name_pose_rebuild = "Rebuild Pose";
const string deg_op_name_pose_eval_init = "Init Pose Eval";
const string deg_op_name_pose_eval_flush = "Flush Pose Eval";
const string deg_op_name_ik_solver = "IK Solver";
const string deg_op_name_spline_ik_solver = "Spline IK Solver";
const string deg_op_name_psys_eval = "PSys Eval";
string deg_op_name_driver(const ChannelDriver *driver)
{
	return string_format("Driver @ %p", driver);
}
string deg_op_name_modifier(const ModifierData *md)
{
	return string_format("Modifier %s", md->name);
}

/* ******************************************************** */
/* External API */

/* Global type registry */

/* NOTE: For now, this is a hashtable not array, since the core node types
 * currently do not have contiguous ID values. Using a hash here gives us
 * more flexibility, albeit using more memory and also sacrificing a little
 * speed. Later on, when things stabilise we may turn this back to an array
 * since there are only just a few node types that an array would cope fine...
 */
static GHash *_depsnode_typeinfo_registry = NULL;

/* Registration ------------------------------------------- */

/* Register node type */
void DEG_register_node_typeinfo(DepsNodeFactory *factory)
{
	BLI_assert(factory != NULL);
	BLI_ghash_insert(_depsnode_typeinfo_registry, SET_INT_IN_POINTER(factory->type()), factory);
}

/* Register all node types */
void DEG_register_node_types(void)
{
	/* initialise registry */
	_depsnode_typeinfo_registry = BLI_ghash_int_new("Depsgraph Node Type Registry");
	
	/* register node types */
	DEG_register_base_depsnodes();
	DEG_register_component_depsnodes();
	DEG_register_operation_depsnodes();
}

/* Free registry on exit */
void DEG_free_node_types(void)
{
	BLI_ghash_free(_depsnode_typeinfo_registry, NULL, NULL);
}

/* Getters ------------------------------------------------- */

/* Get typeinfo for specified type */
DepsNodeFactory *DEG_get_node_factory(const eDepsNode_Type type)
{
	/* look up type - at worst, it doesn't exist in table yet, and we fail */
	return (DepsNodeFactory *)BLI_ghash_lookup(_depsnode_typeinfo_registry, SET_INT_IN_POINTER(type));
}

/* Get typeinfo for provided node */
DepsNodeFactory *DEG_node_get_factory(const DepsNode *node)
{
	if (!node)
		return NULL;
	
	return DEG_get_node_factory(node->type);
}

/* ******************************************************** */
