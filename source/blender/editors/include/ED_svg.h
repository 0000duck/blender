/*
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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup editors
 */

#ifndef __ED_SVG_H__
#define __ED_SVG_H__

struct Text;
struct LANPR_RenderBuffer;
struct LANPR_LineLayer;
struct bGPdata;
struct bGPDlayer;

bool ED_svg_data_from_lanpr_chain(struct Text *ta,
                                  struct LANPR_RenderBuffer *rb,
                                  struct LANPR_LineLayer *ll);
bool ED_svg_data_from_gpencil(struct bGPdata *gpd,
                              struct Text *ta,
                              struct bGPDlayer *layer,
                              int frame);

#endif /* __ED_SVG_H__ */
