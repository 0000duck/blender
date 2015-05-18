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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/io/io_ops.c
 *  \ingroup collada
 */

#include "io_ops.h"  /* own include */

#include "io_cache_library.h"
#ifdef WITH_COLLADA
#  include "io_collada.h"
#endif

#include "WM_api.h"

void ED_operatortypes_io(void) 
{
	WM_operatortype_append(CACHELIBRARY_OT_new);
	WM_operatortype_append(CACHELIBRARY_OT_delete);
	WM_operatortype_append(CACHELIBRARY_OT_bake);
	WM_operatortype_append(CACHELIBRARY_OT_archive_info);
	WM_operatortype_append(CACHELIBRARY_OT_archive_slice);

	WM_operatortype_append(CACHELIBRARY_OT_add_modifier);
	WM_operatortype_append(CACHELIBRARY_OT_remove_modifier);

	WM_operatortype_append(CACHELIBRARY_OT_shape_key_add);
	WM_operatortype_append(CACHELIBRARY_OT_shape_key_remove);
	WM_operatortype_append(CACHELIBRARY_OT_shape_key_clear);
	WM_operatortype_append(CACHELIBRARY_OT_shape_key_retime);
	WM_operatortype_append(CACHELIBRARY_OT_shape_key_move);

#ifdef WITH_COLLADA
	/* Collada operators: */
	WM_operatortype_append(WM_OT_collada_export);
	WM_operatortype_append(WM_OT_collada_import);
#endif
}
