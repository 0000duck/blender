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
from bpy.types import Panel, Menu, UIList
from rna_prop_ui import PropertyPanel
from bl_ui.properties_physics_common import effector_weights_ui


class ObjectButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "object"


class OBJECT_PT_context_object(ObjectButtonsPanel, Panel):
    bl_label = ""
    bl_options = {'HIDE_HEADER'}

    def draw(self, context):
        layout = self.layout
        space = context.space_data

        if space.use_pin_id:
            layout.template_ID(space, "pin_id")
        else:
            row = layout.row()
            row.template_ID(context.scene.objects, "active")


class OBJECT_PT_transform(ObjectButtonsPanel, Panel):
    bl_label = "Transform"

    def draw(self, context):
        layout = self.layout

        ob = context.object

        row = layout.row()

        row.column().prop(ob, "location")
        if ob.rotation_mode == 'QUATERNION':
            row.column().prop(ob, "rotation_quaternion", text="Rotation")
        elif ob.rotation_mode == 'AXIS_ANGLE':
            #row.column().label(text="Rotation")
            #row.column().prop(pchan, "rotation_angle", text="Angle")
            #row.column().prop(pchan, "rotation_axis", text="Axis")
            row.column().prop(ob, "rotation_axis_angle", text="Rotation")
        else:
            row.column().prop(ob, "rotation_euler", text="Rotation")

        row.column().prop(ob, "scale")

        layout.prop(ob, "rotation_mode")


class OBJECT_PT_delta_transform(ObjectButtonsPanel, Panel):
    bl_label = "Delta Transform"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout

        ob = context.object

        row = layout.row()

        row.column().prop(ob, "delta_location")
        if ob.rotation_mode == 'QUATERNION':
            row.column().prop(ob, "delta_rotation_quaternion", text="Rotation")
        elif ob.rotation_mode == 'AXIS_ANGLE':
            #row.column().label(text="Rotation")
            #row.column().prop(pchan, "delta_rotation_angle", text="Angle")
            #row.column().prop(pchan, "delta_rotation_axis", text="Axis")
            #row.column().prop(ob, "delta_rotation_axis_angle", text="Rotation")
            row.column().label(text="Not for Axis-Angle")
        else:
            row.column().prop(ob, "delta_rotation_euler", text="Delta Rotation")

        row.column().prop(ob, "delta_scale")


class OBJECT_PT_transform_locks(ObjectButtonsPanel, Panel):
    bl_label = "Transform Locks"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout

        ob = context.object

        split = layout.split(percentage=0.1)

        col = split.column(align=True)
        col.label(text="")
        col.label(text="X:")
        col.label(text="Y:")
        col.label(text="Z:")

        split.column().prop(ob, "lock_location", text="Location")
        split.column().prop(ob, "lock_rotation", text="Rotation")
        split.column().prop(ob, "lock_scale", text="Scale")

        if ob.rotation_mode in {'QUATERNION', 'AXIS_ANGLE'}:
            row = layout.row()
            row.prop(ob, "lock_rotations_4d", text="Lock Rotation")

            sub = row.row()
            sub.active = ob.lock_rotations_4d
            sub.prop(ob, "lock_rotation_w", text="W")


class OBJECT_PT_relations(ObjectButtonsPanel, Panel):
    bl_label = "Relations"

    def draw(self, context):
        layout = self.layout

        ob = context.object

        split = layout.split()

        col = split.column()
        col.prop(ob, "layers")
        col.separator()
        col.prop(ob, "pass_index")

        col = split.column()
        col.label(text="Parent:")
        col.prop(ob, "parent", text="")

        sub = col.column()
        sub.prop(ob, "parent_type", text="")
        parent = ob.parent
        if parent and ob.parent_type == 'BONE' and parent.type == 'ARMATURE':
            sub.prop_search(ob, "parent_bone", parent.data, "bones", text="")
        sub.active = (parent is not None)


class GROUP_MT_specials(Menu):
    bl_label = "Group Specials"

    def draw(self, context):
        layout = self.layout

        layout.operator("object.group_unlink", icon='X')
        layout.operator("object.grouped_select")
        layout.operator("object.dupli_offset_from_cursor")


class OBJECT_PT_groups(ObjectButtonsPanel, Panel):
    bl_label = "Groups"

    def draw(self, context):
        layout = self.layout

        obj = context.object

        row = layout.row(align=True)
        if bpy.data.groups:
            row.operator("object.group_link", text="Add to Group")
        else:
            row.operator("object.group_add", text="Add to Group")
        row.operator("object.group_add", text="", icon='ZOOMIN')

        obj_name = obj.name
        for group in bpy.data.groups:
            # XXX this is slow and stupid!, we need 2 checks, one thats fast
            # and another that we can be sure its not a name collision
            # from linked library data
            group_objects = group.objects
            if obj_name in group.objects and obj in group_objects[:]:
                col = layout.column(align=True)

                col.context_pointer_set("group", group)

                row = col.box().row()
                row.prop(group, "name", text="")
                row.operator("object.group_remove", text="", icon='X', emboss=False)
                row.menu("GROUP_MT_specials", icon='DOWNARROW_HLT', text="")

                split = col.box().split()

                col = split.column()
                col.prop(group, "layers", text="Dupli Visibility")

                col = split.column()
                col.prop(group, "dupli_offset", text="")


class OBJECT_PT_display(ObjectButtonsPanel, Panel):
    bl_label = "Display"

    def draw(self, context):
        layout = self.layout

        obj = context.object
        obj_type = obj.type
        is_geometry = (obj_type in {'MESH', 'CURVE', 'SURFACE', 'META', 'FONT'})
        is_wire = (obj_type in {'CAMERA', 'EMPTY'})
        is_empty_image = (obj_type == 'EMPTY' and obj.empty_draw_type == 'IMAGE')
        is_dupli = (obj.dupli_type != 'NONE')

        split = layout.split()

        col = split.column()
        col.prop(obj, "show_name", text="Name")
        col.prop(obj, "show_axis", text="Axis")
        # Makes no sense for cameras, armatures, etc.!
        # but these settings do apply to dupli instances
        if is_geometry or is_dupli:
            col.prop(obj, "show_wire", text="Wire")
        if obj_type == 'MESH' or is_dupli:
            col.prop(obj, "show_all_edges")

        col = split.column()
        row = col.row()
        row.prop(obj, "show_bounds", text="Bounds")
        sub = row.row()
        sub.active = obj.show_bounds
        sub.prop(obj, "draw_bounds_type", text="")

        if is_geometry:
            col.prop(obj, "show_texture_space", text="Texture Space")
        col.prop(obj, "show_x_ray", text="X-Ray")
        if obj_type == 'MESH' or is_empty_image:
            col.prop(obj, "show_transparent", text="Transparency")

        split = layout.split()

        col = split.column()
        if is_wire:
            # wire objects only use the max. draw type for duplis
            col.active = is_dupli
            col.label(text="Maximum Dupli Draw Type:")
        else:
            col.label(text="Maximum Draw Type:")
        col.prop(obj, "draw_type", text="")

        col = split.column()
        if is_geometry or is_empty_image:
            # Only useful with object having faces/materials...
            col.label(text="Object Color:")
            col.prop(obj, "color", text="")


# XXX temporary solution
bpy.types.CacheLibrary.filter_string = \
    bpy.props.StringProperty(
        name="Filter Object Name",
        description="Filter cache library objects by name",
        )

def cachelib_objects(cachelib, group):
    if not cachelib or not group:
        return []
    
    filter_string = cachelib.filter_string.lower()
    if filter_string:
        return filter(lambda ob: filter_string in ob.name.lower(), group.objects)
    else:
        return group.objects

# Yields (type, index, enabled)
def cachelib_object_items(cachelib, ob):
    filter_types = cachelib.data_types

    def items_desc():
        yield 'OBJECT', -1
        
        if (ob.type == 'MESH'):
            yield 'DERIVED_MESH', -1

        for index, psys in enumerate(ob.particle_systems):
            if psys.settings.type == 'EMITTER':
                yield 'PARTICLES', index
            if psys.settings.type == 'HAIR':
                yield 'HAIR', index
                yield 'HAIR_PATHS', index

    for datatype, index in items_desc():
        show = False
        enable = False
        # always show selected types
        if datatype in filter_types:
            show = True
            enable = True
        # special case: OBJECT type used as top level, show but disable
        elif datatype == 'OBJECT':
            show = True
            enable = False
        
        if show:
            yield datatype, index, enable

class OBJECT_PT_duplication(ObjectButtonsPanel, Panel):
    bl_label = "Duplication"

    def draw(self, context):
        layout = self.layout

        ob = context.object

        layout.prop(ob, "dupli_type", expand=True)

        if ob.dupli_type == 'FRAMES':
            split = layout.split()

            col = split.column(align=True)
            col.prop(ob, "dupli_frames_start", text="Start")
            col.prop(ob, "dupli_frames_end", text="End")

            col = split.column(align=True)
            col.prop(ob, "dupli_frames_on", text="On")
            col.prop(ob, "dupli_frames_off", text="Off")

            layout.prop(ob, "use_dupli_frames_speed", text="Speed")

        elif ob.dupli_type == 'VERTS':
            layout.prop(ob, "use_dupli_vertices_rotation", text="Rotation")

        elif ob.dupli_type == 'FACES':
            row = layout.row()
            row.prop(ob, "use_dupli_faces_scale", text="Scale")
            sub = row.row()
            sub.active = ob.use_dupli_faces_scale
            sub.prop(ob, "dupli_faces_scale", text="Inherit Scale")

        elif ob.dupli_type == 'GROUP':
            layout.prop(ob, "dupli_group", text="Group")



class CACHELIB_MT_shape_key_specials(Menu):
    bl_label = "Shape Key Specials"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        #layout.operator("object.shape_key_transfer", icon='COPY_ID')  # icon is not ideal
        #layout.operator("object.join_shapes", icon='COPY_ID')  # icon is not ideal
        layout.operator("cachelibrary.shape_key_add", icon='ZOOMIN', text="New Shape From Mix").from_mix = True
        layout.operator("cachelibrary.shape_key_remove", icon='X', text="Delete All Shapes").all = True
        layout.operator("cachelibrary.shape_key_move", icon='TRIA_UP_BAR', text="Move To Top").type = 'TOP'
        layout.operator("cachelibrary.shape_key_move", icon='TRIA_DOWN_BAR', text="Move To Bottom").type = 'BOTTOM'


class CACHELIB_UL_shape_keys(UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        # assert(isinstance(item, bpy.types.ShapeKey))
        md = active_data
        # key = data
        key_block = item
        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            split = layout.split(0.66, False)
            split.prop(key_block, "name", text="", emboss=False, icon_value=icon)
            row = split.row(align=True)
            if key_block.mute:
                row.active = False
            if not item.id_data.use_relative:
                row.prop(key_block, "frame", text="", emboss=False)
            elif index > 0:
                row.prop(key_block, "value", text="", emboss=False)
            else:
                row.label(text="")
            row.prop(key_block, "mute", text="", emboss=False)
        elif self.layout_type == 'GRID':
            layout.alignment = 'CENTER'
            layout.label(text="", icon_value=icon)


class OBJECT_PT_cache_library(ObjectButtonsPanel, Panel):
    bl_label = "Cache"

    @classmethod
    def poll(cls, context):
        ob = context.object
        return (ob and ob.dupli_type == 'GROUP' and ob.dupli_group)

    def draw_cache_modifier(self, context, layout, cachelib, md):
        layout.context_pointer_set("cache_library", cachelib)
        layout.context_pointer_set("cache_modifier", md)

        row = layout.row(align=True)
        row.context_pointer_set("cache_modifier", md)
        row.prop(md, "name", text="")
        row.operator("cachelibrary.remove_modifier", icon='X', text="", emboss=False)

        # match enum type to our functions, avoids a lookup table.
        getattr(self, md.type)(context, layout, cachelib, md)

    def draw_cachelib(self, context, layout, ob, cachelib, objects):
        col = layout.column()
        row = col.row()
        row.label("Source:")
        row.prop(cachelib, "source_mode", text="Source", expand=True)
        row = col.row(align=True)
        row.enabled = (cachelib.source_mode == 'CACHE')
        row.prop(cachelib, "input_filepath", text="")
        props = row.operator("cachelibrary.archive_info", text="", icon='QUESTION')
        props.filepath = cachelib.input_filepath
        props.use_stdout = True
        props.use_popup = True
        props.use_clipboard = True

        layout.separator()

        layout.prop(cachelib, "display_mode", expand=True)
        row = layout.row()
        split = row.split()
        col = split.column()
        col.label("Display:")
        col.prop(cachelib, "display_motion", text="Motion")
        col.prop(cachelib, "display_children", text="Children")

        layout.separator()

        row = layout.row(align=True)
        row.enabled = (cachelib.display_mode == 'RESULT')
        row.prop(cachelib, "output_filepath", text="")
        props = row.operator("cachelibrary.archive_info", text="", icon='QUESTION')
        props.filepath = cachelib.output_filepath
        props.use_stdout = True
        props.use_popup = True
        props.use_clipboard = True

        box = layout.box()
        row = box.row()
        
        col = row.column()
        row2 = col.row()
        row2.alignment = 'LEFT'
        row2.prop(cachelib, "data_types", icon_only=True, toggle=True)
        row2.template_ID(cachelib, "filter_group")
        col = row.column()
        props = col.operator("cachelibrary.bake", text="Bake Preview", icon='RESTRICT_VIEW_OFF')
        props.eval_mode = {'PREVIEW'}
        if context.scene.use_preview_range:
            props.start_frame = context.scene.frame_preview_start
            props.end_frame = context.scene.frame_preview_end
        else:
            props.start_frame = context.scene.frame_start
            props.end_frame = context.scene.frame_end
        props = col.operator("cachelibrary.bake", text="Bake Render", icon='RESTRICT_RENDER_OFF')
        props.eval_mode = {'RENDER'}
        props.start_frame = context.scene.frame_start
        props.end_frame = context.scene.frame_end

        '''
        row = layout.row(align=True)
        row.label("Filter:")
        row.prop(cachelib, "filter_string", icon='VIEWZOOM', text="")

        first = True
        for ob in objects:
            if not any(cachelib_object_items(cachelib, ob)):
                continue

            if first:
                layout.separator()
                first = False

            for datatype, index, enable in cachelib_object_items(cachelib, ob):
                row = layout.row(align=True)
                row.alignment = 'LEFT'
                row.template_cache_library_item(cachelib, ob, datatype, index, enable)
        '''
    
        layout.operator_menu_enum("cachelibrary.add_modifier", "type")

        for md in cachelib.modifiers:
            box = layout.box()
            self.draw_cache_modifier(context, box, cachelib, md)

    def draw(self, context):
        ob = context.object

        layout = self.layout
        row = layout.row(align=True)
        row.template_ID(ob, "cache_library", new="cachelibrary.new")

        if ob.cache_library:
            cache_objects = cachelib_objects(ob.cache_library, ob.dupli_group)
            self.draw_cachelib(context, layout, ob, ob.cache_library, cache_objects)

    def HAIR_SIMULATION(self, context, layout, cachelib, md):
        params = md.parameters

        col = layout.column(align=True)
        col.prop_search(md, "object", context.blend_data, "objects", icon='OBJECT_DATA')
        sub = col.column()
        if (md.object):
            sub.prop_search(md, "hair_system", md.object, "particle_systems")
        else:
            sub.enabled = False
            sub.prop(md, "hair_system")

        layout = layout.column()
        layout.active = md.hair_system is not None

        col = layout.column()
        col.prop(params, "substeps")
        col.prop(params, "timescale")
        col.prop(params, "mass")
        col.prop(params, "drag")

        row = col.row(align=True)
        row.prop(params, "stretch_stiffness")
        row.prop(params, "stretch_damping")

        row = col.row(align=True)
        row.prop(params, "bend_stiffness")
        row.prop(params, "bend_damping")
        row.prop(params, "use_bend_stiffness_curve")
        if params.use_bend_stiffness_curve:
            sub = col.column()
            sub.template_curve_mapping(params, "bend_stiffness_curve")

        row = col.row(align=True)
        row.prop(params, "goal_stiffness")
        row.prop(params, "goal_damping")
        row = col.row(align=True)
        row.prop(params, "use_goal_stiffness_curve")
        row.prop(params, "use_goal_deflect")
        if params.use_goal_stiffness_curve:
            sub = col.column()
            sub.template_curve_mapping(params, "goal_stiffness_curve")

        layout.separator()

        effector_weights_ui(self, context, params.effector_weights, 'HAIR')

    def FORCE_FIELD(self, context, layout, cachelib, md):
        layout.prop_search(md, "object", context.blend_data, "objects", icon='OBJECT_DATA')

        layout.prop(md, "force_type", text="")

        row = layout.row(align=True)
        row.prop(md, "strength")
        row.prop(md, "falloff")

        col = layout.column(align=True)
        row = layout.row(align=True)
        row.prop(md, "min_distance")
        row.prop(md, "max_distance")
        col.prop(md, "use_double_sided")

    def HAIRCUT(self, context, layout, cachelib, md):
        col = layout.column(align=True)
        col.prop_search(md, "object", context.blend_data, "objects", icon='OBJECT_DATA')
        sub = col.column()
        if (md.object):
            sub.prop_search(md, "hair_system", md.object, "particle_systems")
        else:
            sub.enabled = False
            sub.prop(md, "hair_system")

        row = layout.row()
        row.prop_search(md, "target", context.blend_data, "objects", icon='OBJECT_DATA')
        row.prop(md, "use_internal_target", text="Internal")
        layout.prop(md, "cut_mode", toggle=True, expand=True)

        layout = layout.column()
        layout.active = md.hair_system is not None

    def SHRINK_WRAP(self, context, layout, cachelib, md):
        col = layout.column(align=True)
        col.prop_search(md, "object", context.blend_data, "objects", icon='OBJECT_DATA')
        sub = col.column()
        if (md.object):
            sub.prop_search(md, "hair_system", md.object, "particle_systems")
        else:
            sub.enabled = False
            sub.prop(md, "hair_system")

        layout.prop_search(md, "target", context.blend_data, "objects", icon='OBJECT_DATA')

        layout = layout.column()
        layout.active = md.hair_system is not None

    def STRANDS_KEY(self, context, layout, cachelib, md):
        col = layout.column(align=True)
        col.prop_search(md, "object", context.blend_data, "objects", icon='OBJECT_DATA')
        sub = col.column()
        if (md.object):
            sub.prop_search(md, "hair_system", md.object, "particle_systems")
        else:
            sub.enabled = False
            sub.prop(md, "hair_system")

        key = md.shape_keys
        kb = md.active_shape_key
        kb_index = md.active_shape_key_index

        row = layout.row()

        rows = 2
        if kb:
            rows = 4
        row.template_list("CACHELIB_UL_shape_keys", "", key, "key_blocks", md, "active_shape_key_index", rows=rows)

        col = row.column()

        sub = col.column(align=True)
        #sub.operator("object.shape_key_add", icon='ZOOMIN', text="").from_mix = False
        #sub.operator("object.shape_key_remove", icon='ZOOMOUT', text="").all = False
        sub.menu("CACHELIB_MT_shape_key_specials", icon='DOWNARROW_HLT', text="")

        col.prop(md, "use_motion_state")

        if kb:
            col.separator()

            sub = col.column(align=True)
            #sub.operator("object.shape_key_move", icon='TRIA_UP', text="").type = 'UP'
            #sub.operator("object.shape_key_move", icon='TRIA_DOWN', text="").type = 'DOWN'

            split = layout.split(percentage=0.4)
            row = split.row()
            row.prop(key, "use_relative")

            row = split.row()
            row.alignment = 'RIGHT'

            sub = row.row(align=True)
            sub.label()  # XXX, for alignment only
            subsub = sub.row(align=True)
            subsub.prop(md, "show_only_shape_key", text="")

            sub = row.row()
            #if key.use_relative:
            #    sub.operator("object.shape_key_clear", icon='X', text="")
            #else:
            #    sub.operator("object.shape_key_retime", icon='RECOVER_LAST', text="")

            if key.use_relative:
                if kb_index != 0:
                    row = layout.row()
                    row.prop(kb, "value")

                    split = layout.split()

                    col = split.column(align=True)
                    col.label(text="Range:")
                    col.prop(kb, "slider_min", text="Min")
                    col.prop(kb, "slider_max", text="Max")

                    col = split.column(align=True)
                    col.label(text="Blend:")
                    #col.prop_search(kb, "vertex_group", ob, "vertex_groups", text="")
                    col.prop_search(kb, "relative_key", key, "key_blocks", text="")

            else:
                layout.prop(kb, "interpolation")
                row = layout.column()
                row.prop(key, "eval_time")


# Simple human-readable size (based on http://stackoverflow.com/a/1094933)
def sizeof_fmt(num, suffix='B'):
    for unit in ['','K','M','G','T','P','E','Z']:
        if abs(num) < 1024.0:
            return "%3.1f%s%s" % (num, unit, suffix)
        num /= 1024.0
    return "%.1f%s%s" % (num, 'Y', suffix)

class OBJECT_PT_cache_archive_info(ObjectButtonsPanel, Panel):
    bl_label = "Cache Archive Info"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        ob = context.object
        return (ob and ob.dupli_type == 'GROUP' and ob.dupli_group and ob.cache_library)

    def draw_node_structure(self, context, layout, node, indent):
        row = layout.row()
        for i in range(indent):
            row.label(text="", icon='BLANK1')
        
        if not node.child_nodes:
            row.label(text="", icon='DOT')
        elif not node.expand:
            row.prop(node, "expand", text="", icon='DISCLOSURE_TRI_RIGHT', icon_only=True, emboss=False)
        else:
            row.prop(node, "expand", text="", icon='DISCLOSURE_TRI_DOWN', icon_only=True, emboss=False)

            for child in node.child_nodes:
                self.draw_node_structure(context, layout, child, indent + 1)


    info_columns = ['Name', 'Node', 'Samples', 'Size', 'Data', '', 'Array Size']

    def draw_node_info(self, context, layout, node, column):
        if column == 0:
            layout.prop(node, "name", text="")
        if column == 1:
            layout.prop(node, "type", text="")
        if column == 2:
            if node.type in {'SCALAR_PROPERTY', 'ARRAY_PROPERTY'}:
                layout.prop(node, "samples", text="")
            else:
                layout.label(" ")
        if column == 3:
            size = int(node.bytes_size)
            layout.label(sizeof_fmt(size) if size >= 0 else "-")
        if column == 4:
            if node.type in {'SCALAR_PROPERTY', 'ARRAY_PROPERTY'}:
                layout.prop(node, "datatype", text="")
            else:
                layout.label(" ")
        if column == 5:
            if node.type in {'SCALAR_PROPERTY', 'ARRAY_PROPERTY'}:
                layout.prop(node, "datatype_extent", text="")
            else:
                layout.label(" ")
        if column == 6:
            if node.type in {'ARRAY_PROPERTY'}:
                layout.label(node.array_size if node.array_size >= 0 else "-")
            else:
                layout.label(" ")

        if node.expand:
            for child in node.child_nodes:
                self.draw_node_info(context, layout, child, column)

    def draw(self, context):
        ob = context.object
        cachelib = ob.cache_library
        info = cachelib.archive_info

        layout = self.layout
        
        row = layout.row()
        props = row.operator("cachelibrary.archive_info", text="Input", icon='QUESTION')
        props.filepath = cachelib.input_filepath
        props.use_cache_info = True
        props = row.operator("cachelibrary.archive_info", text="Output", icon='QUESTION')
        props.filepath = cachelib.output_filepath
        props.use_cache_info = True

        if info:
            row = layout.row()
            row.enabled = bool(info.filepath)
            props = layout.operator("cachelibrary.archive_info", text="Calculate Size", icon='FILE_REFRESH')
            props.filepath = info.filepath
            props.use_cache_info = True
            props.calc_bytes_size = True

            layout.separator()

            layout.prop(info, "filepath")

            if info.root_node:
                row = layout.row()

                col = row.column()
                col.label(" ")
                self.draw_node_structure(context, col, info.root_node, 0)

                for i, column in enumerate(self.info_columns):
                    col = row.column()
                    col.label(column)
                    self.draw_node_info(context, col, info.root_node, i)


class OBJECT_PT_relations_extras(ObjectButtonsPanel, Panel):
    bl_label = "Relations Extras"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout

        ob = context.object

        split = layout.split()

        if context.scene.render.engine != 'BLENDER_GAME':
            col = split.column()
            col.label(text="Tracking Axes:")
            col.prop(ob, "track_axis", text="Axis")
            col.prop(ob, "up_axis", text="Up Axis")

        col = split.column()
        col.prop(ob, "use_slow_parent")
        row = col.row()
        row.active = ((ob.parent is not None) and (ob.use_slow_parent))
        row.prop(ob, "slow_parent_offset", text="Offset")

        layout.prop(ob, "use_extra_recalc_object")
        layout.prop(ob, "use_extra_recalc_data")


from bl_ui.properties_animviz import (
        MotionPathButtonsPanel,
        OnionSkinButtonsPanel,
        )


class OBJECT_PT_motion_paths(MotionPathButtonsPanel, Panel):
    #bl_label = "Object Motion Paths"
    bl_context = "object"

    @classmethod
    def poll(cls, context):
        return (context.object)

    def draw(self, context):
        # layout = self.layout

        ob = context.object
        avs = ob.animation_visualization
        mpath = ob.motion_path

        self.draw_settings(context, avs, mpath)


class OBJECT_PT_onion_skinning(OnionSkinButtonsPanel):  # , Panel): # inherit from panel when ready
    #bl_label = "Object Onion Skinning"
    bl_context = "object"

    @classmethod
    def poll(cls, context):
        return (context.object)

    def draw(self, context):
        ob = context.object

        self.draw_settings(context, ob.animation_visualization)


class OBJECT_PT_custom_props(ObjectButtonsPanel, PropertyPanel, Panel):
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}
    _context_path = "object"
    _property_type = bpy.types.Object

if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
