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
 * The Original Code is Copyright (C) Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BVM_API_H__
#define __BVM_API_H__

/** \file BVM_api.h
 *  \ingroup bvm
 */

#include "BVM_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct BVMExpression;
struct BVMFunction;
struct BVMModule;

void BVM_init(void);
void BVM_free(void);

struct BVMModule *BVM_module_create(void);
void BVM_module_free(struct BVMModule *mod);

struct BVMFunction *BVM_module_create_function(struct BVMModule *mod, const char *name);
bool BVM_module_delete_function(struct BVMModule *mod, const char *name);

/* ------------------------------------------------------------------------- */

const char *BVM_function_name(const struct BVMFunction *fun);

/* ------------------------------------------------------------------------- */

void BVM_expression_free(struct BVMExpression *expr);

/* ------------------------------------------------------------------------- */

struct BVMNodeGraph;
struct BVMNodeInstance;

struct BVMNodeInstance *BVM_nodegraph_add_node(struct BVMNodeGraph *graph, const char *type, const char *name);

/* ------------------------------------------------------------------------- */

struct BVMEvalGlobals;
struct BVMEvalContext;
struct EffectedPoint;

struct BVMEvalGlobals *BVM_globals_create(void);
void BVM_globals_free(struct BVMEvalGlobals *globals);

struct BVMEvalContext *BVM_context_create(void);
void BVM_context_free(struct BVMEvalContext *context);

void BVM_eval_forcefield(struct BVMEvalGlobals *globals, struct BVMEvalContext *context, struct BVMExpression *expr,
                         const struct EffectedPoint *point, float force[3], float impulse[3]);

/* ------------------------------------------------------------------------- */

struct bNodeTree;

struct BVMExpression *BVM_gen_forcefield_expression(struct bNodeTree *ntree);

#ifdef __cplusplus
}
#endif

#endif /* __BVM_API_H__ */
