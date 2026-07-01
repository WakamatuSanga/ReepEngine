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
    MYADDON_OT_set_event_object_action_post_effect_type,
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
from .stage_analysis_overlay import (
    DrawStageAnalysisOverlay,
    MYADDON_OT_analyze_stage_brightness,
    MYADDON_OT_analyze_stage_perception,
    MYADDON_OT_mark_stage_important_object,
    MYADDON_OT_create_or_switch_stage_editor_workspace,
    MYADDON_OT_setup_stage_editor_layout,
    MYADDON_OT_set_active_area_camera_preview,
    MYADDON_OT_set_active_area_main_edit,
    MYADDON_OT_set_active_area_stage_parameters,
    MYADDON_OT_reset_stage_editor_layout,
    VIEW3D_PT_stage_analysis_overlay,
    register_stage_analysis_properties,
    unregister_stage_analysis_properties,
)
_topbar_menu_appended = False

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
    MYADDON_OT_set_event_object_action_post_effect_type,
    MYADDON_OT_load_event_object_actions_from_raw,
    MYADDON_OT_export_scene,
    MYADDON_OT_import_scene,
    OBJECT_PT_file_name,
    OBJECT_PT_collider,
    MYADDON_OT_send_live_sync,
    MYADDON_OT_initialize_rail_info,
    MYADDON_OT_analyze_stage_brightness,
    MYADDON_OT_analyze_stage_perception,
    MYADDON_OT_mark_stage_important_object,
    MYADDON_OT_create_or_switch_stage_editor_workspace,
    MYADDON_OT_setup_stage_editor_layout,
    MYADDON_OT_set_active_area_camera_preview,
    MYADDON_OT_set_active_area_main_edit,
    MYADDON_OT_set_active_area_stage_parameters,
    MYADDON_OT_reset_stage_editor_layout,
    VIEW3D_PT_level_live_sync,
    VIEW3D_PT_level_event_flag_editor,
    VIEW3D_PT_level_event_link_view,
    VIEW3D_PT_level_rail_editor,
    VIEW3D_PT_stage_analysis_overlay,
    TOPBAR_MT_my_menu,
)


def _is_class_registered(cls):
    return bool(getattr(cls, "is_registered", False)) or hasattr(bpy.types, cls.__name__)


def _register_class_once(cls):
    if _is_class_registered(cls):
        print(f"[LevelEditorAddon] class already registered: {cls.__name__}")
        return

    try:
        bpy.utils.register_class(cls)
        print(f"[LevelEditorAddon] class registered: {cls.__name__}")
    except (RuntimeError, ValueError) as error:
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
    except (RuntimeError, ValueError) as error:
        print(f"[LevelEditorAddon] class unregister skipped: {cls.__name__}: {error}")


def register():
    global _topbar_menu_appended
    print("[LevelEditorAddon] register start")
    register_live_sync()
    register_event_link_view_properties()
    register_stage_analysis_properties()
    for cls in classes:
        _register_class_once(cls)
    register_event_flag_editor_properties()

    if not _topbar_menu_appended:
        try:
            bpy.types.TOPBAR_MT_editor_menus.append(TOPBAR_MT_my_menu.submenu)
            _topbar_menu_appended = True
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
    try:
        DrawStageAnalysisOverlay.register_handler()
        print("[LevelEditorAddon] stage analysis overlay draw handler registered")
    except Exception as error:
        print(f"[LevelEditorAddon] stage analysis overlay draw handler register failed: {error}")
    print("[LevelEditorAddon] register finished")


def unregister():
    global _topbar_menu_appended
    print("[LevelEditorAddon] unregister start")
    if _topbar_menu_appended:
        try:
            bpy.types.TOPBAR_MT_editor_menus.remove(TOPBAR_MT_my_menu.submenu)
            print("[LevelEditorAddon] topbar menu removed")
        except Exception as error:
            print(f"[LevelEditorAddon] topbar menu remove skipped: {error}")
        finally:
            _topbar_menu_appended = False

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
    try:
        DrawStageAnalysisOverlay.unregister_handler()
        print("[LevelEditorAddon] stage analysis overlay draw handler removed")
    except Exception as error:
        print(f"[LevelEditorAddon] stage analysis overlay draw handler remove skipped: {error}")
    unregister_event_flag_editor_properties()
    for cls in reversed(classes):
        _unregister_class_if_registered(cls)
    unregister_event_link_view_properties()
    unregister_stage_analysis_properties()
    unregister_live_sync()
    print("[LevelEditorAddon] unregister finished")
