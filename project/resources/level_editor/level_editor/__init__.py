bl_info = {
    "name": "レベルエディタ",
    "author": "Taro Kamata",
    "version": (1, 0),
    "blender": (3, 3, 1),
    "location": "",
    "description": "レベルエディタ",
    "warning": "",
    "wiki_url": "",
    "tracker_url": "",
    "category": "Object",
}

import bpy

from .create_ico_sphere import MYADDON_OT_create_ico_sphere
from .add_collider import MYADDON_OT_add_collider
from .add_filename import MYADDON_OT_add_filename
from .export_scene import WM_OT_level_export, MYADDON_OT_export_scene
from .import_scene import MYADDON_OT_import_scene
from .file_name import OBJECT_PT_file_name
from .collider import OBJECT_PT_collider
from .my_menu import TOPBAR_MT_my_menu
from .draw_collider import DrawCollider

classes = (
    MYADDON_OT_create_ico_sphere,
    WM_OT_level_export,
    MYADDON_OT_add_filename,
    MYADDON_OT_add_collider,
    MYADDON_OT_export_scene,
    MYADDON_OT_import_scene,
    OBJECT_PT_file_name,
    OBJECT_PT_collider,
    TOPBAR_MT_my_menu,
)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    bpy.types.TOPBAR_MT_editor_menus.append(TOPBAR_MT_my_menu.submenu)
    DrawCollider.handle = bpy.types.SpaceView3D.draw_handler_add(
        DrawCollider.draw_collider, (), 'WINDOW', 'POST_VIEW'
    )


def unregister():
    bpy.types.TOPBAR_MT_editor_menus.remove(TOPBAR_MT_my_menu.submenu)
    if DrawCollider.handle is not None:
        bpy.types.SpaceView3D.draw_handler_remove(DrawCollider.handle, 'WINDOW')
        DrawCollider.handle = None
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
