import bpy
import json
import socket
import time

from .export_scene import build_scene_json

MAX_UDP_PAYLOAD_SIZE = 65507

_last_send_status = "Not sent yet."
_last_auto_send_time = 0.0
_last_payload = None
_timer_registered = False


def _log(message):
    print(f"[LevelEditorLiveSync] {message}")


def _get_scene_property(context, name, default):
    scene = context.scene if context is not None else bpy.context.scene
    return getattr(scene, name, default)


def send_scene_live_sync(context=None, operator=None, force=True):
    global _last_payload
    global _last_send_status

    context = context or bpy.context
    host = _get_scene_property(context, "level_editor_live_sync_host", "127.0.0.1")
    port = int(_get_scene_property(context, "level_editor_live_sync_port", 50000))

    try:
        scene_json = build_scene_json()
        json_text = json.dumps(scene_json, ensure_ascii=False, separators=(",", ":"))
        payload = json_text.encode("utf-8")
        if len(payload) > MAX_UDP_PAYLOAD_SIZE:
            _last_send_status = f"JSON too large for one UDP packet: {len(payload)} bytes"
            if operator is not None:
                operator.report({'ERROR'}, _last_send_status)
            return False
        if not force and payload == _last_payload:
            _last_send_status = f"No scene changes. skipped {len(payload)} bytes"
            return True

        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.sendto(payload, (host, port))

        _last_payload = payload
        object_count = len(scene_json.get("objects", []))
        _last_send_status = f"Sent {len(payload)} bytes to {host}:{port} root_objects={object_count}"
        if operator is not None:
            operator.report({'INFO'}, _last_send_status)
        return True
    except Exception as error:
        _last_send_status = f"Live Sync send failed: {error}"
        if operator is not None:
            operator.report({'ERROR'}, _last_send_status)
        return False


def _auto_send_timer():
    global _last_auto_send_time

    scene = bpy.context.scene
    if scene is not None and getattr(scene, "level_editor_live_sync_auto_send", False):
        interval = max(0.05, float(getattr(scene, "level_editor_live_sync_interval", 0.1)))
        now = time.perf_counter()
        if now - _last_auto_send_time >= interval:
            send_scene_live_sync(bpy.context, force=False)
            _last_auto_send_time = now
        return min(interval, 0.25)

    return 0.25


class MYADDON_OT_send_live_sync(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_send_live_sync"
    bl_label = "Send Live Sync"
    bl_description = "現在のシーン情報をUDPでゲームへ送信します"
    bl_options = {"REGISTER"}

    def execute(self, context):
        return {'FINISHED'} if send_scene_live_sync(context, self) else {'CANCELLED'}


class VIEW3D_PT_level_live_sync(bpy.types.Panel):
    bl_label = "Level Live Sync"
    bl_idname = "VIEW3D_PT_level_live_sync"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "Level Editor"

    def draw(self, context):
        layout = self.layout
        scene = context.scene
        layout.prop(scene, "level_editor_live_sync_host", text="Host")
        layout.prop(scene, "level_editor_live_sync_port", text="Port")
        layout.operator(MYADDON_OT_send_live_sync.bl_idname, icon='EXPORT')
        layout.prop(scene, "level_editor_live_sync_auto_send", text="Auto Send")
        layout.prop(scene, "level_editor_live_sync_interval", text="Interval")
        layout.label(text=_last_send_status)


def register_live_sync():
    global _timer_registered

    if not hasattr(bpy.types.Scene, "level_editor_live_sync_host"):
        bpy.types.Scene.level_editor_live_sync_host = bpy.props.StringProperty(
            name="Live Sync Host",
            default="127.0.0.1")
    if not hasattr(bpy.types.Scene, "level_editor_live_sync_port"):
        bpy.types.Scene.level_editor_live_sync_port = bpy.props.IntProperty(
            name="Live Sync Port",
            default=50000,
            min=1,
            max=65535)
    if not hasattr(bpy.types.Scene, "level_editor_live_sync_auto_send"):
        bpy.types.Scene.level_editor_live_sync_auto_send = bpy.props.BoolProperty(
            name="Auto Send Live Sync",
            default=False)
    if not hasattr(bpy.types.Scene, "level_editor_live_sync_interval"):
        bpy.types.Scene.level_editor_live_sync_interval = bpy.props.FloatProperty(
            name="Live Sync Interval",
            default=0.1,
            min=0.05,
            max=10.0)

    if not _timer_registered and not bpy.app.timers.is_registered(_auto_send_timer):
        bpy.app.timers.register(_auto_send_timer, persistent=True)
        _timer_registered = True
        _log("auto send timer registered")
    else:
        _timer_registered = True


def unregister_live_sync():
    global _timer_registered

    if _timer_registered and bpy.app.timers.is_registered(_auto_send_timer):
        bpy.app.timers.unregister(_auto_send_timer)
    _timer_registered = False

    for name in (
        "level_editor_live_sync_host",
        "level_editor_live_sync_port",
        "level_editor_live_sync_auto_send",
        "level_editor_live_sync_interval",
    ):
        if hasattr(bpy.types.Scene, name):
            delattr(bpy.types.Scene, name)
