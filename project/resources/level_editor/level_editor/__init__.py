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
from .add_event_flag import (
    MYADDON_PG_event_object_action,
    MYADDON_OT_add_event_flag_properties,
    MYADDON_OT_initialize_object_editor_info,
    MYADDON_OT_set_event_flag_shape,
    MYADDON_OT_add_event_object_action,
    MYADDON_OT_remove_event_object_action,
    MYADDON_OT_set_event_object_action_type,
    MYADDON_OT_load_event_object_actions_from_raw,
    VIEW3D_PT_level_event_flag_editor,
    register_event_flag_editor_properties,
    unregister_event_flag_editor_properties,
)
from .add_filename import MYADDON_OT_add_filename
from .export_scene import WM_OT_level_export, MYADDON_OT_export_scene
from .import_scene import MYADDON_OT_import_scene
from .file_name import OBJECT_PT_file_name
from .collider import OBJECT_PT_collider
from .my_menu import TOPBAR_MT_my_menu
from .draw_collider import DrawCollider
from .draw_event_links import (
    DrawEventLinks,
    VIEW3D_PT_level_event_link_view,
    register_event_link_view_properties,
    unregister_event_link_view_properties,
)
from .live_sync import (
    MYADDON_OT_send_live_sync,
    VIEW3D_PT_level_live_sync,
    register_live_sync,
    unregister_live_sync,
)
from .rail_editor import (
    MYADDON_OT_initialize_rail_info,
    VIEW3D_PT_level_rail_editor,
)

classes = (
    MYADDON_OT_create_ico_sphere,
    WM_OT_level_export,
    MYADDON_OT_add_filename,
    MYADDON_OT_add_collider,
    MYADDON_PG_event_object_action,
    MYADDON_OT_add_event_flag_properties,
    MYADDON_OT_initialize_object_editor_info,
    MYADDON_OT_set_event_flag_shape,
    MYADDON_OT_add_event_object_action,
    MYADDON_OT_remove_event_object_action,
    MYADDON_OT_set_event_object_action_type,
    MYADDON_OT_load_event_object_actions_from_raw,
    MYADDON_OT_export_scene,
    MYADDON_OT_import_scene,
    OBJECT_PT_file_name,
    OBJECT_PT_collider,
    MYADDON_OT_send_live_sync,
    MYADDON_OT_initialize_rail_info,
    VIEW3D_PT_level_live_sync,
    VIEW3D_PT_level_event_flag_editor,
    VIEW3D_PT_level_event_link_view,
    VIEW3D_PT_level_rail_editor,
    TOPBAR_MT_my_menu,
)


def _is_class_registered(cls):
    return hasattr(bpy.types, cls.__name__)


def _register_class_once(cls):
    if _is_class_registered(cls):
        print(f"[LevelEditorAddon] class already registered: {cls.__name__}")
        return

    try:
        bpy.utils.register_class(cls)
        print(f"[LevelEditorAddon] class registered: {cls.__name__}")
    except RuntimeError as error:
        if "already registered" in str(error):
            print(f"[LevelEditorAddon] class already registered: {cls.__name__}")
            return
        print(f"[LevelEditorAddon] class register failed: {cls.__name__}: {error}")
        raise


def _unregister_class_if_registered(cls):
    if not _is_class_registered(cls):
        return

    try:
        bpy.utils.unregister_class(cls)
        print(f"[LevelEditorAddon] class unregistered: {cls.__name__}")
    except RuntimeError as error:
        print(f"[LevelEditorAddon] class unregister skipped: {cls.__name__}: {error}")


def register():
    print("[LevelEditorAddon] register start")
    register_live_sync()
    register_event_link_view_properties()
    for cls in classes:
        _register_class_once(cls)
    register_event_flag_editor_properties()

    try:
        bpy.types.TOPBAR_MT_editor_menus.append(TOPBAR_MT_my_menu.submenu)
        print("[LevelEditorAddon] topbar menu appended")
    except Exception as error:
        print(f"[LevelEditorAddon] topbar menu append failed: {error}")

    if DrawCollider.handle is None:
        try:
            DrawCollider.handle = bpy.types.SpaceView3D.draw_handler_add(
                DrawCollider.draw_collider, (), 'WINDOW', 'POST_VIEW'
            )
            print("[LevelEditorAddon] collider draw handler registered")
        except Exception as error:
            print(f"[LevelEditorAddon] collider draw handler register failed: {error}")

    try:
        DrawEventLinks.register_handlers()
        print("[LevelEditorAddon] event link draw handlers registered")
    except Exception as error:
        print(f"[LevelEditorAddon] event link draw handler register failed: {error}")

    print("[LevelEditorAddon] register finished")


def unregister():
    print("[LevelEditorAddon] unregister start")
    try:
        bpy.types.TOPBAR_MT_editor_menus.remove(TOPBAR_MT_my_menu.submenu)
        print("[LevelEditorAddon] topbar menu removed")
    except Exception as error:
        print(f"[LevelEditorAddon] topbar menu remove skipped: {error}")

    if DrawCollider.handle is not None:
        try:
            bpy.types.SpaceView3D.draw_handler_remove(DrawCollider.handle, 'WINDOW')
            print("[LevelEditorAddon] collider draw handler removed")
        except Exception as error:
            print(f"[LevelEditorAddon] collider draw handler remove skipped: {error}")
        finally:
            DrawCollider.handle = None

    try:
        DrawEventLinks.unregister_handlers()
        print("[LevelEditorAddon] event link draw handlers removed")
    except Exception as error:
        print(f"[LevelEditorAddon] event link draw handler remove skipped: {error}")

    unregister_event_flag_editor_properties()
    for cls in reversed(classes):
        _unregister_class_if_registered(cls)
    unregister_event_link_view_properties()
    unregister_live_sync()
    print("[LevelEditorAddon] unregister finished")
