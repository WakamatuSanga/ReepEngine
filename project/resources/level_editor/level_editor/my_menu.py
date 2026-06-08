import bpy
from .create_ico_sphere import MYADDON_OT_create_ico_sphere
from .export_scene import WM_OT_level_export, MYADDON_OT_export_scene
from .import_scene import MYADDON_OT_import_scene
from .add_filename import MYADDON_OT_add_filename
from .add_collider import MYADDON_OT_add_collider
from .add_event_flag import MYADDON_OT_add_event_flag_properties
from .live_sync import MYADDON_OT_send_live_sync


class TOPBAR_MT_my_menu(bpy.types.Menu):
    bl_idname = "TOPBAR_MT_my_menu"
    bl_label = "MyMenu"
    bl_description = "拡張メニュー by Unknown"

    def draw(self, context):
        layout = self.layout
        layout.operator(MYADDON_OT_create_ico_sphere.bl_idname, text="ICO球を作成", icon='MESH_UVSPHERE')
        layout.operator(WM_OT_level_export.bl_idname, text="Level Export", icon='EXPORT')
        layout.operator(MYADDON_OT_export_scene.bl_idname, icon='EXPORT')
        layout.operator(MYADDON_OT_import_scene.bl_idname, icon='IMPORT')
        layout.operator(MYADDON_OT_send_live_sync.bl_idname, icon='EXPORT')
        layout.operator(MYADDON_OT_add_filename.bl_idname, icon='FILE')
        layout.operator(MYADDON_OT_add_collider.bl_idname, icon='MESH_CUBE')
        layout.operator(MYADDON_OT_add_event_flag_properties.bl_idname, icon='BOOKMARKS')
        layout.separator()
        layout.operator("wm.url_open_preset", text="Manual", icon='HELP')

    def submenu(self, context):
        self.layout.menu(TOPBAR_MT_my_menu.bl_idname)
