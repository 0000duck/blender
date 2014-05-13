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
 * Datatypes for internal use in the Depsgraph
 * 
 * All of these datatypes are only really used within the "core" depsgraph.
 * In particular, node types declared here form the structure of operations
 * in the graph.
 */

#ifndef __DEPSGRAPH_TYPES_H__
#define __DEPSGRAPH_TYPES_H__

#include "depsgraph_util_string.h"

struct ChannelDriver;
struct ModifierData;

/* Evaluation Operation for atomic operation 
 * < context: (ComponentEvalContext) context containing data necessary for performing this operation
 *            Results can generally be written to the context directly...
 * < (item): (ComponentDepsNode/PointerRNA) the specific entity involved, where applicable
 */
// XXX: move this to another header that can be exposed?
typedef void (*DepsEvalOperationCb)(void *context, void *item);

/* Metatype of Nodes - The general "level" in the graph structure the node serves */
typedef enum eDepsNode_Class {
	DEPSNODE_CLASS_GENERIC         = 0,        /* Types generally unassociated with user-visible entities, but needed for graph functioning */
	
	DEPSNODE_CLASS_COMPONENT       = 1,        /* [Outer Node] An "aspect" of evaluating/updating an ID-Block, requiring certain types of evaluation behaviours */    
	DEPSNODE_CLASS_OPERATION       = 2,        /* [Inner Node] A glorified function-pointer/callback for scheduling up evaluation operations for components, subject to relationship requirements */
} eDepsNode_Class;

/* Types of Nodes */
typedef enum eDepsNode_Type {
	DEPSNODE_TYPE_UNDEFINED        = -1,       /* fallback type for invalid return value */
	
	/* Generic Types */
	DEPSNODE_TYPE_ROOT             = 0,        /* "Current Scene" - basically whatever kicks off the evaluation process */
	DEPSNODE_TYPE_TIMESOURCE       = 1,        /* Time-Source */
	
	DEPSNODE_TYPE_ID_REF           = 2,        /* ID-Block reference - used as landmarks/collection point for components, but not usually part of main graph */
	DEPSNODE_TYPE_SUBGRAPH         = 3,        /* Isolated sub-graph - used for keeping instanced data separate from instances using them */
	
	/* Outer Types */
	DEPSNODE_TYPE_PARAMETERS       = 10,       /* Parameters Component - Default when nothing else fits (i.e. just SDNA property setting) */
	DEPSNODE_TYPE_PROXY            = 11,       /* Generic "Proxy-Inherit" Component */   // XXX: Also for instancing of subgraphs?
	DEPSNODE_TYPE_ANIMATION        = 12,       /* Animation Component */                 // XXX: merge in with parameters?
	DEPSNODE_TYPE_TRANSFORM        = 13,       /* Transform Component (Parenting/Constraints) */
	DEPSNODE_TYPE_GEOMETRY         = 14,       /* Geometry Component (DerivedMesh/Displist) */
	DEPSNODE_TYPE_SEQUENCER        = 15,       /* Sequencer Component (Scene Only) */
	
	/* Evaluation-Related Outer Types (with Subdata) */
	DEPSNODE_TYPE_EVAL_POSE        = 20,       /* Pose Component - Owner/Container of Bones Eval */
	DEPSNODE_TYPE_BONE             = 21,       /* Bone Component - Child/Subcomponent of Pose */
	
	DEPSNODE_TYPE_EVAL_PARTICLES   = 22,       /* Particle Systems Component */
	
	
	/* Inner Types */
	DEPSNODE_TYPE_OP_PARAMETER     = 100,      /* Parameter Evaluation Operation */
	DEPSNODE_TYPE_OP_PROXY         = 101,      /* Proxy Evaluation Operation */
	DEPSNODE_TYPE_OP_ANIMATION     = 102,      /* Animation Evaluation Operation */
	DEPSNODE_TYPE_OP_TRANSFORM     = 103,      /* Transform Evaluation Operation (incl. constraints, parenting, anim-to-matrix) */
	DEPSNODE_TYPE_OP_GEOMETRY      = 104,      /* Geometry Evaluation Operation (incl. modifiers) */
	DEPSNODE_TYPE_OP_SEQUENCER     = 105,      /* Sequencer Evaluation Operation */
	
	DEPSNODE_TYPE_OP_UPDATE        = 110,      /* Property Update Evaluation Operation [Parameter] */
	DEPSNODE_TYPE_OP_DRIVER        = 112,      /* Driver Evaluation Operation [Parameter] */
	
	DEPSNODE_TYPE_OP_POSE          = 115,      /* Pose Evaluation (incl. setup/cleanup IK trees, IK Solvers) [Pose] */
	DEPSNODE_TYPE_OP_BONE          = 116,      /* Bone Evaluation [Bone] */
	
	DEPSNODE_TYPE_OP_PARTICLE      = 120,      /* Particles Evaluation [Particle] */
	DEPSNODE_TYPE_OP_RIGIDBODY     = 121,      /* Rigidbody Sim (Step) Evaluation */
} eDepsNode_Type;

/* Standard operation names */
/* XXX this needs to be revisited, probably operation types could be
 * combined in a concise struct with name+callback+eDepsOperation_Type
 */
extern const string deg_op_name_object_parent;
extern const string deg_op_name_object_local_transform;
extern const string deg_op_name_constraint_stack;
extern const string deg_op_name_rigidbody_world_rebuild;
extern const string deg_op_name_rigidbody_world_simulate;
extern const string deg_op_name_rigidbody_object_sync;
extern const string deg_op_name_pose_rebuild;
extern const string deg_op_name_pose_eval_init;
extern const string deg_op_name_pose_eval_flush;
extern const string deg_op_name_ik_solver;
extern const string deg_op_name_spline_ik_solver;
extern const string deg_op_name_psys_eval;
string deg_op_name_driver(const ChannelDriver *driver);
string deg_op_name_modifier(const ModifierData *md);

/* Type of operation */
typedef enum eDepsOperation_Type {
	/* Primary operation types */
	DEPSOP_TYPE_INIT    = 0, /* initialise evaluation data */
	DEPSOP_TYPE_EXEC    = 1, /* standard evaluation step */
	DEPSOP_TYPE_POST    = 2, /* cleanup evaluation data + flush results */
	
	/* Additional operation types */
	DEPSOP_TYPE_OUT     = 3, /* indicator for outputting a temporary result that other components can use */ // XXX?
	DEPSOP_TYPE_SIM     = 4, /* indicator for things like IK Solvers and Rigidbody Sim steps which modify final results of separate entities at once */
	DEPSOP_TYPE_REBUILD = 5, /* rebuild internal evaluation data - used for Rigidbody Reset and Armature Rebuild-On-Load */
} eDepsOperation_Type;

/* Types of relationships between nodes 
 *
 * This is used to provide additional hints to use when filtering
 * the graph, so that we can go without doing more extensive
 * data-level checks...
 */
typedef enum eDepsRelation_Type {
	/* reationship type unknown/irrelevant */
	DEPSREL_TYPE_STANDARD = 0,
	
	/* root -> active scene or entity (screen, image, etc.) */
	DEPSREL_TYPE_ROOT_TO_ACTIVE,
	
	/* general datablock dependency */
	DEPSREL_TYPE_DATABLOCK,
	
	/* time dependency */
	DEPSREL_TYPE_TIME,
	
	/* component depends on results of another */
	DEPSREL_TYPE_COMPONENT_ORDER,
	
	/* relationship is just used to enforce ordering of operations
	 * (e.g. "init()" callback done before "exec() and "cleanup()")
	 */
	DEPSREL_TYPE_OPERATION,
	
	/* relationship results from a property driver affecting property */
	DEPSREL_TYPE_DRIVER,
	
	/* relationship is something driver depends on */
	DEPSREL_TYPE_DRIVER_TARGET,
	
	/* relationship is used for transform stack 
	 * (e.g. parenting, user transforms, constraints)
	 */
	DEPSREL_TYPE_TRANSFORM,
	
	/* relationship is used for geometry evaluation 
	 * (e.g. metaball "motherball" or modifiers)
	 */
	DEPSREL_TYPE_GEOMETRY_EVAL,
	
	/* relationship is used to trigger a post-change validity updates */
	DEPSREL_TYPE_UPDATE,
	
	/* relationship is used to trigger editor/screen updates */
	DEPSREL_TYPE_UPDATE_UI,
} eDepsRelation_Type;

#endif // __DEPSGRAPH_TYPES_H__
