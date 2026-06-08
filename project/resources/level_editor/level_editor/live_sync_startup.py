import importlib
import os
import sys
import traceback

import bpy


LOG_PREFIX = "[LevelEditorStartup]"


def _log(message):
    print(f"{LOG_PREFIX} {message}")


def _normalize_path(path):
    return os.path.normcase(os.path.abspath(path))


def _is_child_path(path, parent):
    normalized_path = _normalize_path(path)
    normalized_parent = _normalize_path(parent)
    return normalized_path == normalized_parent or normalized_path.startswith(normalized_parent + os.sep)


def _ensure_addon_root_on_path():
    startup_script_path = os.path.abspath(__file__)
    addon_package_path = os.path.dirname(startup_script_path)
    addon_root_path = os.path.dirname(addon_package_path)

    _log(f"startup script path: {startup_script_path}")
    _log(f"addon package path: {addon_package_path}")
    _log(f"addon root path: {addon_root_path}")

    if addon_root_path not in sys.path:
        sys.path.insert(0, addon_root_path)
        _log("sys.path add result: inserted addon root at index 0")
    else:
        _log("sys.path add result: addon root already exists")

    _log(f"sys.path[0]: {sys.path[0] if sys.path else '(empty)'}")
    return addon_root_path, addon_package_path


def _clear_wrong_level_editor_modules(addon_package_path):
    module = sys.modules.get("level_editor")
    module_file = getattr(module, "__file__", "") if module is not None else ""
    if not module_file or _is_child_path(module_file, addon_package_path):
        return

    _log(f"existing level_editor module is from another path: {module_file}")
    for name in list(sys.modules.keys()):
        if name == "level_editor" or name.startswith("level_editor."):
            del sys.modules[name]
    _log("removed existing level_editor modules from sys.modules")


def _import_level_editor(addon_package_path):
    _clear_wrong_level_editor_modules(addon_package_path)
    importlib.invalidate_caches()

    try:
        module = importlib.import_module("level_editor")
        _log(f"import level_editor success: {getattr(module, '__file__', '(unknown)')}")
        return module
    except Exception as error:
        _log(f"import level_editor failed: {error}")
        _log(traceback.format_exc())
        return None


def _is_class_registered(cls):
    return hasattr(bpy.types, cls.__name__)


def _register_class_once(cls):
    if _is_class_registered(cls):
        _log(f"class already registered: {cls.__name__}")
        return True

    try:
        bpy.utils.register_class(cls)
        _log(f"class registered: {cls.__name__}")
        return True
    except RuntimeError as error:
        if "already registered" in str(error):
            _log(f"class already registered: {cls.__name__}")
            return True
        _log(f"class register failed: {cls.__name__}: {error}")
        _log(traceback.format_exc())
        return False
    except Exception as error:
        _log(f"class register failed: {cls.__name__}: {error}")
        _log(traceback.format_exc())
        return False


def _register_level_editor_package(level_editor_module):
    if level_editor_module is None:
        return False

    register_function = getattr(level_editor_module, "register", None)
    if register_function is None:
        _log("register failed: level_editor.register is missing")
        return False

    try:
        register_function()
        _log("register success: level_editor.register()")
        return True
    except Exception as error:
        _log(f"register failed: level_editor.register(): {error}")
        _log(traceback.format_exc())
        return False


def _ensure_live_sync_ui(live_sync_module):
    if live_sync_module is None:
        return False

    ok = True
    for cls_name in ("MYADDON_OT_send_live_sync", "VIEW3D_PT_level_live_sync"):
        cls = getattr(live_sync_module, cls_name, None)
        if cls is None:
            _log(f"live sync UI class missing: {cls_name}")
            ok = False
            continue
        ok = _register_class_once(cls) and ok

    return ok


def _configure_live_sync(live_sync_module):
    if live_sync_module is None:
        return False

    try:
        live_sync_module.register_live_sync()
    except Exception as error:
        _log(f"register_live_sync failed: {error}")
        _log(traceback.format_exc())
        return False

    scene = bpy.context.scene
    if scene is None:
        _log("live sync auto send failed: bpy.context.scene is None")
        return False

    scene.level_editor_live_sync_host = "127.0.0.1"
    scene.level_editor_live_sync_port = 50000
    scene.level_editor_live_sync_interval = 0.1
    scene.level_editor_live_sync_auto_send = True

    _log(f"live sync host: {scene.level_editor_live_sync_host}")
    _log(f"live sync port: {scene.level_editor_live_sync_port}")
    _log(f"live sync interval: {scene.level_editor_live_sync_interval}")
    _log(f"live sync auto send: {scene.level_editor_live_sync_auto_send}")

    try:
        send_result = live_sync_module.send_scene_live_sync(bpy.context, force=True)
        _log(f"initial live sync send result: {send_result}")
    except Exception as error:
        _log(f"initial live sync send failed: {error}")
        _log(traceback.format_exc())

    return True


def _redraw_ui():
    screen = bpy.context.screen
    if screen is None:
        return
    for area in screen.areas:
        area.tag_redraw()


def _start_live_sync():
    _log("startup begin")
    _, addon_package_path = _ensure_addon_root_on_path()

    level_editor_module = _import_level_editor(addon_package_path)
    if level_editor_module is None:
        _log("startup aborted: level_editor import failed")
        return

    register_ok = _register_level_editor_package(level_editor_module)

    try:
        live_sync_module = importlib.import_module("level_editor.live_sync")
        _log("import level_editor.live_sync success")
    except Exception as error:
        _log(f"import level_editor.live_sync failed: {error}")
        _log(traceback.format_exc())
        return

    ui_ok = _ensure_live_sync_ui(live_sync_module)
    live_sync_ok = _configure_live_sync(live_sync_module)
    _redraw_ui()

    _log(f"register success/failure: {register_ok}")
    _log(f"live sync UI success/failure: {ui_ok}")
    _log(f"live sync auto send ON/OFF: {live_sync_ok}")
    _log("startup finished")


_start_live_sync()
