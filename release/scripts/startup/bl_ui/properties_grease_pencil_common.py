# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>


import bpy
from bpy.types import Menu, UIList


def gpencil_stroke_placement_settings(context, layout, gpd):
    col = layout.column(align=True)

    col.label(text="Stroke Placement:")

    row = col.row(align=True)
    row.prop_enum(gpd, "draw_mode", 'VIEW')
    row.prop_enum(gpd, "draw_mode", 'CURSOR')

    if context.space_data.type == 'VIEW_3D':
        row = col.row(align=True)
        row.prop_enum(gpd, "draw_mode", 'SURFACE')
        row.prop_enum(gpd, "draw_mode", 'STROKE')

        row = col.row(align=False)
        row.active = gpd.draw_mode in ('SURFACE', 'STROKE')
        row.prop(gpd, "use_stroke_endpoints")


class GreasePencilDrawingToolsPanel():
    # subclass must set
    # bl_space_type = 'IMAGE_EDITOR'
    bl_label = "Grease Pencil"
    bl_category = "Grease Pencil"
    bl_region_type = 'TOOLS'

    @staticmethod
    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)

        col.label(text="Draw:")
        row = col.row(align=True)
        row.operator("gpencil.draw", text="Draw").mode = 'DRAW'
        row.operator("gpencil.draw", text="Erase").mode = 'ERASER'

        row = col.row(align=True)
        row.operator("gpencil.draw", text="Line").mode = 'DRAW_STRAIGHT'
        row.operator("gpencil.draw", text="Poly").mode = 'DRAW_POLY'


        row = col.row(align=True)
        row.prop(context.tool_settings, "use_grease_pencil_sessions", text="Continuous Drawing")

        gpd = context.gpencil_data
        if gpd:
            col.separator()
            gpencil_stroke_placement_settings(context, col, gpd)


        if context.space_data.type == 'VIEW_3D':
            col.separator()
            col.separator()

            col.label(text="Tools:")
            col.operator("gpencil.convert", text="Convert...")
            col.operator("view3d.ruler")


class GreasePencilStrokeEditPanel():
    # subclass must set
    # bl_space_type = 'IMAGE_EDITOR'
    bl_label = "Edit Strokes"
    bl_category = "Grease Pencil"
    bl_region_type = 'TOOLS'

    @classmethod
    def poll(cls, context):
        return (context.gpencil_data is not None)

    @staticmethod
    def draw(self, context):
        layout = self.layout

        gpd = context.gpencil_data
        edit_ok = context.editable_gpencil_strokes and gpd.use_stroke_edit_mode

        col = layout.column(align=True)
        col.prop(gpd, "use_stroke_edit_mode", text="Enable Editing", icon='EDIT', toggle=True)

        col.separator()

        col.label(text="Select:")
        subcol = col.column(align=True)
        subcol.active = edit_ok
        subcol.operator("gpencil.select_all", text="Select All")
        subcol.operator("gpencil.select_border")
        subcol.operator("gpencil.select_circle")

        col.separator()

        col.label(text="Edit:")
        subcol = col.column(align=True)
        subcol.active = edit_ok
        subcol.operator("gpencil.strokes_duplicate", text="Duplicate")
        subcol.operator("transform.mirror", text="Mirror").gpencil_strokes = True

        col.separator()

        subcol = col.column(align=True)
        subcol.active = edit_ok
        subcol.operator("transform.translate").gpencil_strokes = True   # icon='MAN_TRANS'
        subcol.operator("transform.rotate").gpencil_strokes = True      # icon='MAN_ROT'
        subcol.operator("transform.resize", text="Scale").gpencil_strokes = True      # icon='MAN_SCALE'


###############################

class GPENCIL_PIE_tool_palette(Menu):
    """A pie menu for quick access to Grease Pencil tools"""
    bl_label = "Grease Pencil Tools"

    def draw(self, context):
        layout = self.layout

        pie = layout.menu_pie()
        gpd = context.gpencil_data

        # W - Drawing Settings
        col = pie.column()
        col.operator("gpencil.draw", text="Draw", icon='GREASEPENCIL').mode = 'DRAW'
        col.operator("gpencil.draw", text="Straight Lines", icon='LINE_DATA').mode = 'DRAW_STRAIGHT'
        col.operator("gpencil.draw", text="Poly", icon='MESH_DATA').mode = 'DRAW_POLY'

        # E - Eraser
        # XXX: needs a dedicated icon...
        pie.operator("gpencil.draw", text="Eraser", icon='FORCE_CURVE').mode = 'ERASER'

        # Editing tools
        if gpd:
            if gpd.use_stroke_edit_mode and context.editable_gpencil_strokes:
                # S - Select
                col = pie.column()
                col.operator("gpencil.select_all", text="Select All", icon='PARTICLE_POINT')
                col.operator("gpencil.select_border", text="Border Select", icon='BORDER_RECT')
                col.operator("gpencil.select_circle", text="Circle Select", icon='META_EMPTY')
                #col.operator("gpencil.select", text="Stroke Under Mouse").entire_strokes = True

                # N - Move
                pie.operator("transform.translate", icon='MAN_TRANS').gpencil_strokes = True

                # NW - Rotate
                pie.operator("transform.rotate", icon='MAN_ROT').gpencil_strokes = True

                # NE - Scale
                pie.operator("transform.resize", text="Scale", icon='MAN_SCALE').gpencil_strokes = True

                # SW - Copy
                pie.operator("gpencil.strokes_duplicate", icon='PARTICLE_PATH')

                # SE - Exit Edit Mode
                pie.prop(gpd, "use_stroke_edit_mode", text="Exit Edit Mode", icon='EDIT')
            else:
                # Toggle Edit Mode
                pie.prop(gpd, "use_stroke_edit_mode", text="Enable Stroke Editing", icon='EDIT')


###############################

class GPENCIL_UL_layer(UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        # assert(isinstance(item, bpy.types.GPencilLayer)
        gpl = item

        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            if gpl.lock:
                layout.active = False

            split = layout.split(percentage=0.2)
            split.prop(gpl, "color", text="")
            split.prop(gpl, "info", text="", emboss=False)

            row = layout.row(align=True)
            row.prop(gpl, "lock", text="", emboss=False)
            row.prop(gpl, "hide", text="", emboss=False)
        elif self.layout_type in {'GRID'}:
            layout.alignment = 'CENTER'
            layout.label(text="", icon_value=icon)


class GreasePencilDataPanel():
    # subclass must set
    # bl_space_type = 'IMAGE_EDITOR'
    bl_label = "Grease Pencil"
    bl_region_type = 'UI'

    @staticmethod
    def draw_header(self, context):
        self.layout.prop(context.space_data, "show_grease_pencil", text="")

    @staticmethod
    def draw(self, context):
        layout = self.layout

        # owner of Grease Pencil data
        gpd_owner = context.gpencil_data_owner
        gpd = context.gpencil_data

        # Owner Selector
        # XXX: add this for 3D view too
        if context.space_data.type == 'CLIP_EDITOR':
            layout.prop(context.space_data, "grease_pencil_source", expand=True)

        # Grease Pencil data selector
        layout.template_ID(gpd_owner, "grease_pencil", new="gpencil.data_add", unlink="gpencil.data_unlink")

        # Grease Pencil data...
        if gpd is None:
            # even with no data, this operator will still work, as it makes gpd too
            layout.operator("gpencil.layer_add", text="New Layer", icon='ZOOMIN')
        else:
            self.draw_layers(context, layout, gpd)

            # only sequencer doesn't have a toolbar to show these settings in,
            # so only show this for the sequencer...
            if context.space_data.type == 'SEQUENCE_EDITOR':
                layout.separator()
                layout.separator()

                layout.prop(gpd, "use_stroke_edit_mode", text="Enable Stroke Editing", icon='EDIT', toggle=True)

                layout.separator()
                layout.separator()

                gpencil_stroke_placement_settings(context, layout, gpd)

    def draw_layers(self, context, layout, gpd):
        row = layout.row()

        col = row.column()
        col.template_list("GPENCIL_UL_layer", "", gpd, "layers", gpd.layers, "active_index", rows=5)

        col = row.column()

        sub = col.column(align=True)
        sub.operator("gpencil.layer_add", icon='ZOOMIN', text="")
        sub.operator("gpencil.layer_remove", icon='ZOOMOUT', text="")

        gpl = context.active_gpencil_layer
        if gpl:
            col.separator()

            sub = col.column(align=True)
            sub.operator("gpencil.layer_move", icon='TRIA_UP', text="").type = 'UP'
            sub.operator("gpencil.layer_move", icon='TRIA_DOWN', text="").type = 'DOWN'

        if gpl:
            self.draw_layer(layout, gpl)

    def draw_layer(self, layout, gpl):
        # layer settings
        split = layout.split(percentage=0.5)
        split.active = not gpl.lock

        # Column 1 - Appearance
        col = split.column()

        subcol = col.column(align=True)
        subcol.prop(gpl, "color", text="")
        subcol.prop(gpl, "alpha", slider=True)

        #if debug:
        #   col.prop(gpl, "show_points")

        # Column 2 - Options (& Current Frame)
        col = split.column(align=True)

        col.prop(gpl, "line_width", slider=True)
        col.prop(gpl, "show_x_ray")

        layout.separator()

        # Full-Row - Frame Locking (and Delete Frame)
        row = layout.row(align=True)
        row.active = not gpl.lock

        if gpl.active_frame:
            lock_status = "Locked" if gpl.lock_frame else "Unlocked"
            lock_label = "Frame: %d (%s)" % (gpl.active_frame.frame_number, lock_status)
        else:
            lock_label = "Lock Frame"
        row.prop(gpl, "lock_frame", text=lock_label, icon='UNLOCKED')
        row.operator("gpencil.active_frame_delete", text="", icon='X')

        layout.separator()

        # Onion skinning
        col = layout.column(align=True)
        col.active = not gpl.lock

        row = col.row()
        row.prop(gpl, "use_onion_skinning")
        row.prop(gpl, "use_ghost_custom_colors", text="", icon='COLOR')

        split = col.split(percentage = 0.5)
        split.active = gpl.use_onion_skinning

        # - Before Frames
        sub = split.column(align=True)
        row = sub.row(align=True)
        row.active = gpl.use_ghost_custom_colors
        row.prop(gpl, "before_color", text="")
        sub.prop(gpl, "ghost_before_range", text="Before")


        # - After Frames
        sub = split.column(align=True)
        row = sub.row(align=True)
        row.active = gpl.use_ghost_custom_colors
        row.prop(gpl, "after_color", text="")
        sub.prop(gpl, "ghost_after_range", text="After")
