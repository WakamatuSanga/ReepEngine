import bpy
import re


def _make_default_flag_id(obj):
    safe_name = re.sub(r"\W+", "_", obj.name).strip("_")
    if not safe_name:
        safe_name = "Object"
    return f"flag_{safe_name}"


def _set_default(obj, key, value):
    if key not in obj:
        obj[key] = value


def _update_property_ui(obj, key, description):
    try:
        obj.id_properties_ui(key).update(description=description)
    except Exception:
        pass


def ensure_event_flag_properties(obj):
    object_id = obj.get("object_id", obj.name)
    flag_id = obj.get("event_flag_id", _make_default_flag_id(obj))

    obj["object_id"] = object_id
    obj["is_event_flag"] = True
    obj["event_flag_id"] = flag_id
    _set_default(obj, "event_display_name", obj.name)
    _set_default(obj, "event_description", "")
    _set_default(obj, "event_trigger_type", "PlayerEnter")
    _set_default(obj, "event_shape_type", "Box")
    _set_default(obj, "event_visible_in_editor", True)
    _set_default(obj, "event_initially_enabled", True)
    _set_default(obj, "event_one_shot", True)
    _set_default(obj, "event_next_flag_ids", "")
    _set_default(obj, "event_object_actions", "")

    _update_property_ui(obj, "event_flag_id", "Unique EventFlag ID. Example: flag_Cube")
    _update_property_ui(obj, "event_display_name", "Display name shown in editor debug UI")
    _update_property_ui(obj, "event_description", "Editor description for this EventFlag")
    _update_property_ui(obj, "event_trigger_type", "Trigger type. Example: PlayerEnter")
    _update_property_ui(obj, "event_shape_type", "Event shape: Box or Sphere")
    _update_property_ui(obj, "event_next_flag_ids", "Comma-separated next EventFlag IDs")
    _update_property_ui(obj, "event_object_actions", "Actions: target|action|description; target2|action|description")
    return flag_id


class MYADDON_OT_add_event_flag_properties(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_add_event_flag_properties"
    bl_label = "イベントフラグ化 (Add Event Flag Properties)"
    bl_description = "選択中ObjectへEventFlag用の初期プロパティを追加します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        obj = context.object
        if obj is None:
            self.report({'WARNING'}, "No active object selected")
            return {'CANCELLED'}

        flag_id = ensure_event_flag_properties(obj)
        self.report({'INFO'}, f"EventFlag properties added: {flag_id}")
        return {'FINISHED'}


class MYADDON_OT_set_event_flag_shape(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_set_event_flag_shape"
    bl_label = "Set Event Flag Shape"
    bl_description = "EventFlagの形状をBoxまたはSphereに設定します"
    bl_options = {'REGISTER', 'UNDO'}

    shape_type: bpy.props.StringProperty(default="Box")

    def execute(self, context):
        obj = context.object
        if obj is None:
            self.report({'WARNING'}, "No active object selected")
            return {'CANCELLED'}

        ensure_event_flag_properties(obj)
        obj["event_shape_type"] = self.shape_type
        return {'FINISHED'}


def _draw_custom_property(layout, obj, key, text):
    if key in obj:
        layout.prop(obj, f'["{key}"]', text=text)


class VIEW3D_PT_level_event_flag_editor(bpy.types.Panel):
    bl_label = "イベントフラグ編集 (Event Flag Editor)"
    bl_idname = "VIEW3D_PT_level_event_flag_editor"
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
        layout.operator(MYADDON_OT_add_event_flag_properties.bl_idname, text="イベントフラグ化 (Add Event Flag Properties)", icon='BOOKMARKS')

        if not bool(obj.get("is_event_flag", False)):
            layout.label(text="このObjectはEventFlagではありません")
            return

        box = layout.box()
        box.label(text="基本情報")
        _draw_custom_property(box, obj, "event_flag_id", "イベントフラグID (event_flag_id)")
        _draw_custom_property(box, obj, "event_display_name", "表示名 (event_display_name)")
        _draw_custom_property(box, obj, "event_description", "説明 (event_description)")
        _draw_custom_property(box, obj, "event_trigger_type", "トリガー種別 (event_trigger_type)")

        shape_box = layout.box()
        shape_box.label(text="形状 (event_shape_type)")
        shape_row = shape_box.row(align=True)
        op = shape_row.operator(MYADDON_OT_set_event_flag_shape.bl_idname, text="Box")
        op.shape_type = "Box"
        op = shape_row.operator(MYADDON_OT_set_event_flag_shape.bl_idname, text="Sphere")
        op.shape_type = "Sphere"
        _draw_custom_property(shape_box, obj, "event_shape_type", "現在の形状")

        state_box = layout.box()
        state_box.label(text="状態")
        _draw_custom_property(state_box, obj, "event_visible_in_editor", "エディタで表示 (event_visible_in_editor)")
        _draw_custom_property(state_box, obj, "event_initially_enabled", "初期有効 (event_initially_enabled)")
        _draw_custom_property(state_box, obj, "event_one_shot", "一回だけ発動 (event_one_shot)")

        link_box = layout.box()
        link_box.label(text="接続")
        _draw_custom_property(link_box, obj, "event_next_flag_ids", "次に有効化するフラグID一覧")
        _draw_custom_property(link_box, obj, "event_object_actions", "影響を与えるオブジェクト")
