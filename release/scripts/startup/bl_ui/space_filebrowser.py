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
from bpy.types import Header, Panel


class FILEBROWSER_HT_header(Header):
    bl_space_type = 'FILE_BROWSER'

    def draw(self, context):
        layout = self.layout

        st = context.space_data

        is_lib_browser = st.use_library_browsing

        layout.template_header()

        row = layout.row()
        row.separator()

        row = layout.row(align=True)
        row.operator("file.previous", text="", icon='BACK')
        row.operator("file.next", text="", icon='FORWARD')
        row.operator("file.parent", text="", icon='FILE_PARENT')
        row.operator("file.refresh", text="", icon='FILE_REFRESH')

        row = layout.row()
        row.separator()

        row = layout.row(align=True)
        layout.operator_context = "EXEC_DEFAULT"
        row.operator("file.directory_new", icon='NEWFOLDER')

        layout.operator_context = "INVOKE_DEFAULT"
        params = st.params

        # can be None when save/reload with a file selector open
        if params:
            layout.prop(params, "display_type", expand=True, text="")
            layout.prop(params, "sort_method", expand=True, text="")

            layout.prop(params, "show_hidden")
            layout.prop(params, "use_flat_view")
            layout.prop(params, "use_filter", text="", icon='FILTER')

            row = layout.row(align=True)
            row.active = params.use_filter

            row.prop(params, "use_filter_folder", text="")

            if params.filter_glob:
                #if st.active_operator and hasattr(st.active_operator, "filter_glob"):
                #    row.prop(params, "filter_glob", text="")
                row.label(params.filter_glob)
            else:
                row.prop(params, "use_filter_blender", text="")
                row.prop(params, "use_filter_backup", text="")
                row.prop(params, "use_filter_image", text="")
                row.prop(params, "use_filter_movie", text="")
                row.prop(params, "use_filter_script", text="")
                row.prop(params, "use_filter_font", text="")
                row.prop(params, "use_filter_sound", text="")
                row.prop(params, "use_filter_text", text="")

            if is_lib_browser:
                row.prop(params, "use_filter_blendid", text="")
                if (params.use_filter_blendid) :
                    row.separator()
                    row.prop(params, "filter_id", text="")

            row.separator()
            row.prop(params, "filter_search", text="")

        layout.template_running_jobs()



class FILEBROWSER_UL_dir(bpy.types.UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        direntry = item
        space = context.space_data
        icon = 'NONE'
        if active_propname == "system_folders_active":
            icon = 'DISK_DRIVE'
        if active_propname == "system_bookmarks_active":
            icon = 'BOOKMARKS'
        if active_propname == "bookmarks_active":
            icon = 'BOOKMARKS'
        if active_propname == "recent_folders_active":
            icon = 'FILE_FOLDER'

        #~ if (space.params.directory == direntry.path):
            #~ setattr(active_data, active_propname, index)

        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            row = layout.row(align=True)

            row.prop(direntry, "name", text="", emboss=False, icon=icon)

            if direntry.use_save:
                row.label("CaN DeLeTe")

        elif self.layout_type in {'GRID'}:
            layout.alignment = 'CENTER'
            layout.prop(direntry, "path", text="")


class FILEBROWSER_PT_system_folders(Panel):
    bl_space_type = 'FILE_BROWSER'
    bl_region_type = 'CHANNELS'
    bl_label = "System"

    def draw(self, context):
        layout = self.layout

        space = context.space_data

        if space.system_folders:
            #~ print(space.system_folders_active)
            layout.template_list("FILEBROWSER_UL_dir", "system_folders", space, "system_folders", space, "system_folders_active", rows=1, maxrows=6)


class FILEBROWSER_PT_system_bookmarks(Panel):
    bl_space_type = 'FILE_BROWSER'
    bl_region_type = 'CHANNELS'
    bl_label = "System Bookmarks"

    @classmethod
    def poll(cls, context):
        return not context.user_preferences.filepaths.hide_system_bookmarks

    def draw(self, context):
        layout = self.layout

        space = context.space_data

        if space.system_bookmarks:
            #~ print(space.system_folders_active)
            layout.template_list("FILEBROWSER_UL_dir", "system_bookmarks", space, "system_bookmarks", space, "system_bookmarks_active", rows=1, maxrows=6)


class FILEBROWSER_PT_bookmarks(Panel):
    bl_space_type = 'FILE_BROWSER'
    bl_region_type = 'CHANNELS'
    bl_label = "Bookmarks"

    def draw(self, context):
        layout = self.layout

        space = context.space_data

        if space.bookmarks:
            #~ print(space.system_folders_active)
            layout.template_list("FILEBROWSER_UL_dir", "bookmarks", space, "bookmarks", space, "bookmarks_active", rows=1, maxrows=6)


class FILEBROWSER_PT_recent_folders(Panel):
    bl_space_type = 'FILE_BROWSER'
    bl_region_type = 'CHANNELS'
    bl_label = "Recent"

    @classmethod
    def poll(cls, context):
        return not context.user_preferences.filepaths.hide_recent_locations

    def draw(self, context):
        layout = self.layout

        space = context.space_data

        if space.recent_folders:
            #~ print(space.system_folders_active)
            layout.template_list("FILEBROWSER_UL_dir", "recent_folders", space, "recent_folders", space, "recent_folders_active", rows=1, maxrows=6)


if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
