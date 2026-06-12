import json

import blf
import bpy
import gpu
from bpy_extras import view3d_utils
from gpu_extras.batch import batch_for_shader
from mathutils import Vector


def _to_bool(value, default=False):
    if value is None:
        return default
    if isinstance(value, bool):
        return value
    if isinstance(value, (int, float)):
        return value != 0
    if isinstance(value, str):
        text = value.strip().lower()
        if text in ("true", "1", "yes", "on"):
            return True
        if text in ("false", "0", "no", "off"):
            return False
    return default


def _to_string(value, default=""):
    if value is None:
        return default
    return str(value)


def _split_csv(value):
    if value is None:
        return []
    if isinstance(value, str):
        return [item.strip() for item in value.split(",") if item.strip()]
    try:
        return [str(item).strip() for item in value if str(item).strip()]
    except TypeError:
        return []


def _normalize_action(action):
    if not isinstance(action, dict):
        return None
    result = {
        "targetObjectId": _to_string(action.get("targetObjectId", action.get("target_object_id", ""))),
        "targetObjectName": _to_string(action.get("targetObjectName", action.get("target_object_name", ""))),
        "actionType": _to_string(action.get("actionType", action.get("action_type", ""))),
        "actionDescription": _to_string(action.get("actionDescription", action.get("action_description", ""))),
    }
    if not result["targetObjectId"] and not result["targetObjectName"]:
        return None
    return result


def _parse_event_object_actions(value):
    if value is None:
        return []
    if isinstance(value, str):
        text = value.strip()
        if not text:
            return []
        if text.startswith("[") or text.startswith("{"):
            try:
                loaded = json.loads(text)
                if isinstance(loaded, dict):
                    loaded = [loaded]
                if isinstance(loaded, list):
                    return [
                        action for action in (_normalize_action(item) for item in loaded)
                        if action is not None
                    ]
            except json.JSONDecodeError as error:
                print(f"[LevelEditor] Event link action JSON parse failed: {error}")

        actions = []
        for item in text.split(";"):
            parts = [part.strip() for part in item.split("|")]
            if not parts or not parts[0]:
                continue
            actions.append({
                "targetObjectName": parts[0],
                "actionType": parts[1] if len(parts) > 1 else "",
                "actionDescription": parts[2] if len(parts) > 2 else "",
            })
        return actions
    if isinstance(value, list):
        return [
            action for action in (_normalize_action(item) for item in value)
            if action is not None
        ]
    return []


def _is_event_flag(obj):
    return _to_bool(obj.get("is_event_flag"), False) or "event_flag_id" in obj


def _event_flag_id(obj):
    return _to_string(obj.get("event_flag_id", obj.get("object_id", obj.name)))


def _object_position(obj):
    return obj.matrix_world.translation.copy()


def _display_name(obj):
    return _to_string(obj.get("event_display_name", obj.get("editor_label", obj.name)))


def _description(obj):
    return _to_string(obj.get("event_description", obj.get("editor_description", "")))


def _trigger_type(obj):
    return _to_string(obj.get("event_trigger_type", "PlayerEnter"))


def _scene_settings():
    scene = bpy.context.scene
    return {
        "show_flag_links": getattr(scene, "level_event_links_show_flag_links", True),
        "show_object_links": getattr(scene, "level_event_links_show_object_links", True),
        "show_names": getattr(scene, "level_event_links_show_names", True),
        "show_descriptions": getattr(scene, "level_event_links_show_descriptions", True),
        "show_action_descriptions": getattr(scene, "level_event_links_show_action_descriptions", True),
        "show_missing": getattr(scene, "level_event_links_show_missing", True),
        "line_thickness": getattr(scene, "level_event_links_line_thickness", 2.0),
        "text_size": getattr(scene, "level_event_links_text_size", 14),
    }


def _collect_maps():
    flag_map = {}
    object_map = {}
    event_flags = []
    for obj in bpy.context.scene.objects:
        object_map[obj.name] = obj
        if "object_id" in obj:
            object_map[_to_string(obj["object_id"])] = obj
        if _is_event_flag(obj):
            flag_id = _event_flag_id(obj)
            if flag_id:
                flag_map[flag_id] = obj
            event_flags.append(obj)
    return event_flags, flag_map, object_map


def _draw_line_batch(points, color, line_thickness):
    if len(points) < 2:
        return
    shader = gpu.shader.from_builtin("UNIFORM_COLOR")
    batch = batch_for_shader(shader, "LINES", {"pos": points})
    gpu.state.blend_set("ALPHA")
    gpu.state.line_width_set(line_thickness)
    shader.bind()
    shader.uniform_float("color", color)
    batch.draw(shader)
    gpu.state.line_width_set(1.0)
    gpu.state.blend_set("NONE")


def _draw_text(region, rv3d, position, text, color, size):
    if not text:
        return
    screen = view3d_utils.location_3d_to_region_2d(region, rv3d, position)
    if screen is None:
        return
    font_id = 0
    blf.size(font_id, int(size))
    blf.color(font_id, color[0], color[1], color[2], color[3])
    blf.position(font_id, screen.x, screen.y, 0)
    blf.draw(font_id, text)


class DrawEventLinks:
    line_handle = None
    text_handle = None

    @staticmethod
    def draw_lines():
        settings = _scene_settings()
        event_flags, flag_map, object_map = _collect_maps()
        flag_lines = []
        object_lines = []
        missing_lines = []

        for flag_obj in event_flags:
            source = _object_position(flag_obj)
            if settings["show_flag_links"]:
                for target_id in _split_csv(flag_obj.get("event_next_flag_ids", "")):
                    target = flag_map.get(target_id)
                    if target:
                        flag_lines.extend([source, _object_position(target)])
                    elif settings["show_missing"]:
                        missing_lines.extend([source, source + Vector((0.0, 0.0, 1.0))])

            if settings["show_object_links"]:
                for action in _parse_event_object_actions(flag_obj.get("event_object_actions", "")):
                    target = None
                    if action.get("targetObjectId"):
                        target = object_map.get(action["targetObjectId"])
                    if not target and action.get("targetObjectName"):
                        target = object_map.get(action["targetObjectName"])
                    if target:
                        object_lines.extend([source, _object_position(target)])
                    elif settings["show_missing"]:
                        missing_lines.extend([source, source + Vector((0.0, 0.0, 1.4))])

        _draw_line_batch(flag_lines, (0.05, 0.55, 1.0, 0.85), settings["line_thickness"])
        _draw_line_batch(object_lines, (1.0, 0.18, 0.08, 0.9), settings["line_thickness"])
        _draw_line_batch(missing_lines, (1.0, 0.85, 0.05, 0.9), settings["line_thickness"])

    @staticmethod
    def draw_text():
        context = bpy.context
        region = context.region
        rv3d = context.space_data.region_3d if context.space_data else None
        if not region or not rv3d:
            return

        settings = _scene_settings()
        event_flags, flag_map, object_map = _collect_maps()
        for flag_obj in event_flags:
            base_position = _object_position(flag_obj) + Vector((0.0, 0.0, 0.35))
            lines = []
            if settings["show_names"]:
                lines.append(_display_name(flag_obj))
            if settings["show_descriptions"] and _description(flag_obj):
                lines.append(_description(flag_obj))
            if settings["show_names"]:
                lines.append(_trigger_type(flag_obj))
            for line_index, line in enumerate(lines):
                _draw_text(
                    region,
                    rv3d,
                    base_position + Vector((0.0, 0.0, line_index * 0.18)),
                    line,
                    (0.75, 0.95, 1.0, 1.0),
                    settings["text_size"])

            if settings["show_flag_links"] and settings["show_missing"]:
                for target_id in _split_csv(flag_obj.get("event_next_flag_ids", "")):
                    if target_id not in flag_map:
                        _draw_text(
                            region,
                            rv3d,
                            base_position + Vector((0.0, 0.0, 0.65)),
                            f"Missing Flag: {target_id}",
                            (1.0, 0.9, 0.05, 1.0),
                            settings["text_size"])

            if not settings["show_object_links"]:
                continue
            for action in _parse_event_object_actions(flag_obj.get("event_object_actions", "")):
                target = object_map.get(action.get("targetObjectId", ""))
                if not target:
                    target = object_map.get(action.get("targetObjectName", ""))
                if not target:
                    if settings["show_missing"]:
                        _draw_text(
                            region,
                            rv3d,
                            base_position + Vector((0.0, 0.0, 0.85)),
                            "Missing Object: " + action.get("targetObjectName", action.get("targetObjectId", "")),
                            (1.0, 0.65, 0.05, 1.0),
                            settings["text_size"])
                    continue
                if settings["show_action_descriptions"]:
                    midpoint = (_object_position(flag_obj) + _object_position(target)) * 0.5
                    text = action.get("actionDescription") or action.get("actionType") or target.name
                    _draw_text(
                        region,
                        rv3d,
                        midpoint + Vector((0.0, 0.0, 0.2)),
                        text,
                        (1.0, 0.65, 0.45, 1.0),
                        settings["text_size"])

    @classmethod
    def register_handlers(cls):
        if cls.line_handle is None:
            cls.line_handle = bpy.types.SpaceView3D.draw_handler_add(cls.draw_lines, (), "WINDOW", "POST_VIEW")
        if cls.text_handle is None:
            cls.text_handle = bpy.types.SpaceView3D.draw_handler_add(cls.draw_text, (), "WINDOW", "POST_PIXEL")

    @classmethod
    def unregister_handlers(cls):
        if cls.line_handle is not None:
            bpy.types.SpaceView3D.draw_handler_remove(cls.line_handle, "WINDOW")
            cls.line_handle = None
        if cls.text_handle is not None:
            bpy.types.SpaceView3D.draw_handler_remove(cls.text_handle, "WINDOW")
            cls.text_handle = None


class VIEW3D_PT_level_event_link_view(bpy.types.Panel):
    bl_label = "イベント接続表示 (Event Link View)"
    bl_idname = "VIEW3D_PT_level_event_link_view"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "Level Editor"

    def draw(self, context):
        layout = self.layout
        scene = context.scene
        layout.prop(scene, "level_event_links_show_flag_links", text="フラグ接続線を表示 (Show Flag Links)")
        layout.prop(scene, "level_event_links_show_object_links", text="オブジェクト影響線を表示 (Show Object Action Links)")
        layout.prop(scene, "level_event_links_show_names", text="イベント名を表示 (Show Event Names)")
        layout.prop(scene, "level_event_links_show_descriptions", text="イベント説明を表示 (Show Event Descriptions)")
        layout.prop(scene, "level_event_links_show_action_descriptions", text="アクション説明を表示 (Show Action Descriptions)")
        layout.prop(scene, "level_event_links_show_missing", text="Missing Linkを表示 (Show Missing Links)")
        layout.prop(scene, "level_event_links_line_thickness", text="線の太さ (Line Thickness)")
        layout.prop(scene, "level_event_links_text_size", text="文字サイズ (Text Size)")


def register_event_link_view_properties():
    if not hasattr(bpy.types.Scene, "level_event_links_show_flag_links"):
        bpy.types.Scene.level_event_links_show_flag_links = bpy.props.BoolProperty(default=True)
    if not hasattr(bpy.types.Scene, "level_event_links_show_object_links"):
        bpy.types.Scene.level_event_links_show_object_links = bpy.props.BoolProperty(default=True)
    if not hasattr(bpy.types.Scene, "level_event_links_show_names"):
        bpy.types.Scene.level_event_links_show_names = bpy.props.BoolProperty(default=True)
    if not hasattr(bpy.types.Scene, "level_event_links_show_descriptions"):
        bpy.types.Scene.level_event_links_show_descriptions = bpy.props.BoolProperty(default=True)
    if not hasattr(bpy.types.Scene, "level_event_links_show_action_descriptions"):
        bpy.types.Scene.level_event_links_show_action_descriptions = bpy.props.BoolProperty(default=True)
    if not hasattr(bpy.types.Scene, "level_event_links_show_missing"):
        bpy.types.Scene.level_event_links_show_missing = bpy.props.BoolProperty(default=True)
    if not hasattr(bpy.types.Scene, "level_event_links_line_thickness"):
        bpy.types.Scene.level_event_links_line_thickness = bpy.props.FloatProperty(default=2.0, min=1.0, max=8.0)
    if not hasattr(bpy.types.Scene, "level_event_links_text_size"):
        bpy.types.Scene.level_event_links_text_size = bpy.props.IntProperty(default=14, min=8, max=32)


def unregister_event_link_view_properties():
    for name in (
        "level_event_links_show_flag_links",
        "level_event_links_show_object_links",
        "level_event_links_show_names",
        "level_event_links_show_descriptions",
        "level_event_links_show_action_descriptions",
        "level_event_links_show_missing",
        "level_event_links_line_thickness",
        "level_event_links_text_size",
    ):
        if hasattr(bpy.types.Scene, name):
            delattr(bpy.types.Scene, name)
