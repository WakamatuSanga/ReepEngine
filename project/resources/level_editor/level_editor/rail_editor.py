import bpy


def _update_property_ui(obj, key, description):
    try:
        obj.id_properties_ui(key).update(description=description)
    except Exception:
        pass


def _to_float(value, default):
    try:
        return float(value)
    except (TypeError, ValueError):
        return default


def _to_int(value, default):
    try:
        return int(value)
    except (TypeError, ValueError):
        return default


def ensure_rail_properties(obj):
    obj["rail_id"] = obj.get("rail_id", f"rail_{obj.name}")
    obj["rail_name"] = obj.get("rail_name", obj.name)
    obj["rail_type"] = obj.get("rail_type", "Main")
    obj["rail_loop"] = bool(obj.get("rail_loop", False))
    obj["rail_reverse_direction"] = bool(obj.get("rail_reverse_direction", False))
    obj["rail_speed"] = _to_float(obj.get("rail_speed", 5.0), 5.0)
    obj["rail_visible_in_editor"] = bool(obj.get("rail_visible_in_editor", True))
    obj["rail_sample_count"] = _to_int(obj.get("rail_sample_count", 64), 64)

    _update_property_ui(obj, "rail_id", "Unique rail ID. Example: rail_MainPath")
    _update_property_ui(obj, "rail_name", "Rail display name")
    _update_property_ui(obj, "rail_type", "Rail type. Example: Main")
    _update_property_ui(obj, "rail_loop", "Connect rail end back to start")
    _update_property_ui(obj, "rail_reverse_direction", "Reverse rail start/end direction")
    _update_property_ui(obj, "rail_speed", "Default rail runtime speed")
    _update_property_ui(obj, "rail_visible_in_editor", "Show this rail in editor debug view")
    _update_property_ui(obj, "rail_sample_count", "Curve sample count for export/live sync")


def _draw_custom_property(layout, obj, key, text):
    try:
        if key in obj:
            layout.prop(obj, f'["{key}"]', text=text)
            return True
        row = layout.row()
        row.enabled = False
        row.label(text=f"{text}: (未設定)")
        return False
    except Exception as error:
        layout.label(text=f"{text}: 表示エラー")
        layout.label(text=str(error)[:96])
        print(f"[LevelEditor] draw rail custom property failed: {key}: {error}")
        return False


class MYADDON_OT_initialize_rail_info(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_initialize_rail_info"
    bl_label = "Rail情報を初期化 (Initialize Rail Info)"
    bl_description = "選択中Curve ObjectへRail用の初期プロパティを追加します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        obj = context.object
        if obj is None:
            self.report({'WARNING'}, "No active object selected")
            return {'CANCELLED'}
        if obj.type != "CURVE":
            self.report({'WARNING'}, "Selected object is not a Curve")
            return {'CANCELLED'}

        ensure_rail_properties(obj)
        self.report({'INFO'}, f"Rail info initialized: {obj.name}")
        return {'FINISHED'}


class VIEW3D_PT_level_rail_editor(bpy.types.Panel):
    bl_label = "レール編集 (Rail Editor)"
    bl_idname = "VIEW3D_PT_level_rail_editor"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "Level Editor"

    def draw(self, context):
        layout = self.layout
        obj = context.object
        if obj is None:
            layout.label(text="Objectが選択されていません")
            return

        layout.label(text=f"選択中: {obj.name}")
        if obj.type != "CURVE":
            layout.label(text="Curve Objectを選択するとRail設定を編集できます")
            layout.label(text="現在のObjectはRail対象ではありません")
            return

        layout.operator(
            MYADDON_OT_initialize_rail_info.bl_idname,
            text="Rail情報を初期化 (Initialize Rail Info)")

        box = layout.box()
        box.label(text="Rail基本情報")
        _draw_custom_property(box, obj, "rail_id", "レールID (rail_id)")
        _draw_custom_property(box, obj, "rail_name", "レール名 (rail_name)")
        _draw_custom_property(box, obj, "rail_type", "レール種別 (rail_type)")

        state_box = layout.box()
        state_box.label(text="Rail動作設定")
        _draw_custom_property(state_box, obj, "rail_loop", "ループ (rail_loop)")
        _draw_custom_property(state_box, obj, "rail_reverse_direction", "向きを反転 (rail_reverse_direction)")
        _draw_custom_property(state_box, obj, "rail_speed", "移動速度 (rail_speed)")
        _draw_custom_property(state_box, obj, "rail_visible_in_editor", "エディタで表示 (rail_visible_in_editor)")
        _draw_custom_property(state_box, obj, "rail_sample_count", "サンプル数 (rail_sample_count)")

        help_box = layout.box()
        help_box.label(text="Export / Live SyncではこのRail設定がrails JSONに出力されます")
        help_box.label(text="rail_sample_countは2〜128に制限されます")
