import math

import bpy
from mathutils import Vector

from .rail_editor import ensure_rail_properties


GENERATOR_VERSION = 1
AUTO_RAIL_KEY = "level_auto_rail_generated"
META_PREFIX = "level_auto_rail_"

DEFAULTS = {
    "preset": "TestCourse",
    "rail_name": "Rail_Auto",
    "target_length": 10000.0,
    "control_point_count": 32,
    "horizontal_amount": 15.0,
    "vertical_amount": 10.0,
    "curve_count": 4,
    "start_position": "CURSOR",
    "forward_axis": "POS_Y",
    "mirror_horizontal": False,
    "mirror_vertical": False,
    "loop": False,
    "select_after_generate": True,
}

PRESET_ITEMS = (
    ("Straight", "直線", "進行方向へ直線を生成します"),
    ("RightCurve", "緩い右カーブ", "終点へ向かって滑らかに右へ曲がります"),
    ("LeftCurve", "緩い左カーブ", "終点へ向かって滑らかに左へ曲がります"),
    ("SCurve", "S字カーブ", "左右へ滑らかに繰り返すS字を生成します"),
    ("Ascend", "上昇", "終点へ向かって滑らかに上昇します"),
    ("Descend", "下降", "終点へ向かって滑らかに下降します"),
    ("TestCourse", "テストコース", "直線、左右カーブ、上昇、下降を連続生成します"),
)

START_POSITION_ITEMS = (
    ("CURSOR", "3Dカーソル", "3Dカーソルのワールド位置から生成します"),
    ("ORIGIN", "ワールド原点", "ワールド原点から生成します"),
    ("SELECTED", "選択オブジェクトの位置", "アクティブオブジェクトのワールド位置から生成します"),
)

FORWARD_AXIS_ITEMS = (
    ("POS_Y", "+Y方向", "Blenderの+Y方向へ生成します"),
    ("NEG_Y", "-Y方向", "Blenderの-Y方向へ生成します"),
    ("POS_X", "+X方向", "Blenderの+X方向へ生成します"),
    ("NEG_X", "-X方向", "Blenderの-X方向へ生成します"),
)


def _smoothstep(value):
    value = max(0.0, min(1.0, value))
    return value * value * (3.0 - 2.0 * value)


def _segment_smooth(t, start, end):
    return _smoothstep((t - start) / (end - start))


def _preset_offsets(preset, t, horizontal_amount, vertical_amount, curve_count):
    horizontal = 0.0
    vertical = 0.0
    if preset == "RightCurve":
        horizontal = horizontal_amount * _smoothstep(t)
    elif preset == "LeftCurve":
        horizontal = -horizontal_amount * _smoothstep(t)
    elif preset == "SCurve":
        envelope = math.sin(math.pi * t) ** 2
        horizontal = horizontal_amount * math.sin(
            2.0 * math.pi * curve_count * t) * envelope
    elif preset == "Ascend":
        vertical = vertical_amount * _smoothstep(t)
    elif preset == "Descend":
        vertical = -vertical_amount * _smoothstep(t)
    elif preset == "TestCourse":
        if 0.15 <= t < 0.35:
            horizontal = horizontal_amount * _segment_smooth(t, 0.15, 0.35)
        elif 0.35 <= t < 0.55:
            horizontal = horizontal_amount * (
                1.0 - 2.0 * _segment_smooth(t, 0.35, 0.55))
        elif 0.55 <= t < 0.70:
            blend = _segment_smooth(t, 0.55, 0.70)
            horizontal = -horizontal_amount * (1.0 - blend)
            vertical = vertical_amount * blend
        elif 0.70 <= t < 0.85:
            vertical = vertical_amount * (
                1.0 - _segment_smooth(t, 0.70, 0.85))
    return horizontal, vertical


def _axis_vectors(forward_axis):
    if forward_axis == "NEG_Y":
        return Vector((0.0, -1.0, 0.0)), Vector((-1.0, 0.0, 0.0))
    if forward_axis == "POS_X":
        return Vector((1.0, 0.0, 0.0)), Vector((0.0, -1.0, 0.0))
    if forward_axis == "NEG_X":
        return Vector((-1.0, 0.0, 0.0)), Vector((0.0, 1.0, 0.0))
    return Vector((0.0, 1.0, 0.0)), Vector((1.0, 0.0, 0.0))


def _build_points(settings, forward_span):
    forward, horizontal_axis = _axis_vectors(settings.forward_axis)
    vertical_axis = Vector((0.0, 0.0, 1.0))
    horizontal_sign = -1.0 if settings.mirror_horizontal else 1.0
    vertical_sign = -1.0 if settings.mirror_vertical else 1.0
    point_count = int(settings.control_point_count)
    points = []
    for index in range(point_count):
        t = index / float(point_count - 1)
        horizontal, vertical = _preset_offsets(
            settings.preset,
            t,
            float(settings.horizontal_amount),
            float(settings.vertical_amount),
            int(settings.curve_count))
        points.append(
            forward * (forward_span * t)
            + horizontal_axis * (horizontal * horizontal_sign)
            + vertical_axis * (vertical * vertical_sign))
    return points


def _polyline_length(points, cyclic=False):
    length = sum(
        (points[index] - points[index - 1]).length
        for index in range(1, len(points)))
    if cyclic and len(points) >= 2:
        length += (points[0] - points[-1]).length
    return length


def _generate_points(settings):
    target = float(settings.target_length)
    zero_span_points = _build_points(settings, 0.0)
    if _polyline_length(zero_span_points, settings.loop) >= target:
        points = _build_points(settings, target)
        return points, _polyline_length(points, settings.loop)

    low = 0.0
    high = target
    for _iteration in range(24):
        middle = (low + high) * 0.5
        points = _build_points(settings, middle)
        if _polyline_length(points, settings.loop) < target:
            low = middle
        else:
            high = middle

    points = _build_points(settings, (low + high) * 0.5)
    return points, _polyline_length(points, settings.loop)


def _validate_points(settings, points):
    if int(settings.control_point_count) < 4:
        return "制御点数は4以上にしてください。"
    if len(points) != int(settings.control_point_count):
        return "生成した制御点数が設定値と一致しません。"
    for point in points:
        if not all(math.isfinite(component) for component in point):
            return "制御点に無効な数値が含まれています。"
    for index in range(1, len(points)):
        if (points[index] - points[index - 1]).length <= 0.000001:
            return "連続する制御点が同じ位置になりました。設定を調整してください。"
    return ""


def _safe_id(text):
    safe = "".join(
        character if character.isalnum() or character == "_" else "_"
        for character in str(text)).strip("_")
    return f"rail_{safe}" if safe else "rail"


def _unique_name(base_name):
    base_name = base_name.strip() or DEFAULTS["rail_name"]
    if bpy.data.objects.get(base_name) is None:
        return base_name
    suffix = 1
    while bpy.data.objects.get(f"{base_name}_{suffix:03d}") is not None:
        suffix += 1
    return f"{base_name}_{suffix:03d}"


def _implicit_rail_id(obj):
    return str(obj.get("rail_id", _safe_id(obj.name)))


def _unique_rail_id(base_id, excluded_object=None):
    existing_ids = {
        _implicit_rail_id(obj)
        for obj in bpy.data.objects
        if obj.type == "CURVE" and obj != excluded_object
    }
    if base_id not in existing_ids:
        return base_id
    suffix = 1
    while f"{base_id}_{suffix:03d}" in existing_ids:
        suffix += 1
    return f"{base_id}_{suffix:03d}"


def _add_bezier_spline(curve_data, points, cyclic):
    curve_data.dimensions = "3D"
    spline = curve_data.splines.new("BEZIER")
    spline.bezier_points.add(len(points) - 1)
    for bezier_point, point in zip(spline.bezier_points, points):
        bezier_point.co = point
        bezier_point.handle_left_type = "AUTO"
        bezier_point.handle_right_type = "AUTO"
        bezier_point.radius = 1.0
        bezier_point.tilt = 0.0
    spline.use_cyclic_u = bool(cyclic)


def _replace_curve_points(obj, points, cyclic):
    curve_data = obj.data
    if curve_data.users > 1:
        curve_data = curve_data.copy()
        obj.data = curve_data
    curve_data.splines.clear()
    _add_bezier_spline(curve_data, points, cyclic)


def _store_generator_metadata(obj, settings, estimated_length):
    obj[AUTO_RAIL_KEY] = True
    obj[META_PREFIX + "generator_version"] = GENERATOR_VERSION
    obj[META_PREFIX + "preset"] = settings.preset
    obj[META_PREFIX + "target_length"] = float(settings.target_length)
    obj[META_PREFIX + "control_point_count"] = int(settings.control_point_count)
    obj[META_PREFIX + "horizontal_amount"] = float(settings.horizontal_amount)
    obj[META_PREFIX + "vertical_amount"] = float(settings.vertical_amount)
    obj[META_PREFIX + "curve_count"] = int(settings.curve_count)
    obj[META_PREFIX + "forward_axis"] = settings.forward_axis
    obj[META_PREFIX + "mirror_horizontal"] = bool(settings.mirror_horizontal)
    obj[META_PREFIX + "mirror_vertical"] = bool(settings.mirror_vertical)
    obj[META_PREFIX + "estimated_length"] = float(estimated_length)


def _is_auto_rail(obj):
    return bool(
        obj is not None
        and obj.type == "CURVE"
        and obj.get(AUTO_RAIL_KEY, False))


def _is_exportable_curve(obj):
    if obj is None or obj.type != "CURVE" or obj.data is None:
        return False
    return any(
        len(spline.bezier_points) >= 2 or len(spline.points) >= 2
        for spline in obj.data.splines)


def _start_world_position(context, settings):
    if settings.start_position == "ORIGIN":
        return Vector((0.0, 0.0, 0.0))
    if settings.start_position == "SELECTED":
        active_object = context.view_layer.objects.active
        if active_object is None:
            raise ValueError("開始位置に使用するアクティブオブジェクトがありません。")
        return active_object.matrix_world.translation.copy()
    return context.scene.cursor.location.copy()


def _set_error(settings, operator, message):
    settings.last_error = message
    settings.last_result = ""
    operator.report({'ERROR'}, message)
    return {'CANCELLED'}


class MYADDON_PG_auto_rail_settings(bpy.types.PropertyGroup):
    preset: bpy.props.EnumProperty(
        name="生成プリセット", description="生成するレール形状を選択します",
        items=PRESET_ITEMS, default=DEFAULTS["preset"])
    rail_name: bpy.props.StringProperty(
        name="レール名", description="新規レールのオブジェクト名と表示名です",
        default=DEFAULTS["rail_name"])
    target_length: bpy.props.FloatProperty(
        name="目標レール長", description="生成する制御点列のおおよその全長です",
        default=DEFAULTS["target_length"], min=100.0, max=100000.0)
    control_point_count: bpy.props.IntProperty(
        name="制御点数", description="生成するカーブの編集用制御点数です",
        default=DEFAULTS["control_point_count"], min=4, max=256)
    horizontal_amount: bpy.props.FloatProperty(
        name="横方向の変化量", description="進行方向に対する左右の変化量です",
        default=DEFAULTS["horizontal_amount"], min=0.0, max=1000.0)
    vertical_amount: bpy.props.FloatProperty(
        name="高さの変化量", description="BlenderのZ軸方向の変化量です",
        default=DEFAULTS["vertical_amount"], min=0.0, max=1000.0)
    curve_count: bpy.props.IntProperty(
        name="カーブ回数", description="S字カーブが左右へ変化する回数です",
        default=DEFAULTS["curve_count"], min=1, max=16)
    start_position: bpy.props.EnumProperty(
        name="開始位置", description="新規レールのオブジェクト原点を決めます",
        items=START_POSITION_ITEMS, default=DEFAULTS["start_position"])
    forward_axis: bpy.props.EnumProperty(
        name="進行方向", description="レールを伸ばすBlender軸を選択します",
        items=FORWARD_AXIS_ITEMS, default=DEFAULTS["forward_axis"])
    mirror_horizontal: bpy.props.BoolProperty(
        name="左右反転", description="左右の形状を反転します",
        default=DEFAULTS["mirror_horizontal"])
    mirror_vertical: bpy.props.BoolProperty(
        name="上下反転", description="上下の形状を反転します",
        default=DEFAULTS["mirror_vertical"])
    loop: bpy.props.BoolProperty(
        name="ループ", description="終点と始点をカーブとして接続します",
        default=DEFAULTS["loop"])
    select_after_generate: bpy.props.BoolProperty(
        name="生成後に選択", description="生成したレールをアクティブオブジェクトにします",
        default=DEFAULTS["select_after_generate"])
    last_estimated_length: bpy.props.FloatProperty(default=0.0, options={'HIDDEN'})
    last_result: bpy.props.StringProperty(default="", options={'HIDDEN'})
    last_error: bpy.props.StringProperty(default="", options={'HIDDEN'})


class MYADDON_OT_generate_auto_rail(bpy.types.Operator):
    bl_idname = "myaddon.generate_auto_rail"
    bl_label = "新規レールを自動生成"
    bl_description = "現在の生成設定から編集可能なレールカーブを新規作成します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        settings = context.scene.level_auto_rail_settings
        if context.mode != "OBJECT":
            return _set_error(settings, self, "オブジェクトモードで実行してください。")
        try:
            start_position = _start_world_position(context, settings)
            points, estimated_length = _generate_points(settings)
        except (ValueError, TypeError) as error:
            return _set_error(settings, self, str(error))

        validation_error = _validate_points(settings, points)
        if validation_error:
            return _set_error(settings, self, validation_error)

        object_name = _unique_name(settings.rail_name)
        curve_data = None
        rail_object = None
        try:
            curve_data = bpy.data.curves.new(f"{object_name}_Curve", type="CURVE")
            curve_data.dimensions = "3D"
            curve_data.resolution_u = 12
            _add_bezier_spline(curve_data, points, settings.loop)
            rail_object = bpy.data.objects.new(object_name, curve_data)
            target_collection = context.collection or context.scene.collection
            target_collection.objects.link(rail_object)
            rail_object.location = start_position
            ensure_rail_properties(rail_object)
            rail_object["rail_id"] = _unique_rail_id(_safe_id(object_name))
            rail_object["rail_name"] = object_name
            rail_object["rail_loop"] = bool(settings.loop)
            _store_generator_metadata(rail_object, settings, estimated_length)
        except Exception as error:
            if rail_object is not None:
                bpy.data.objects.remove(rail_object, do_unlink=True)
            elif curve_data is not None:
                bpy.data.curves.remove(curve_data)
            return _set_error(settings, self, f"レールの生成に失敗しました: {error}")

        if settings.select_after_generate:
            for selected_object in context.selected_objects:
                selected_object.select_set(False)
            rail_object.select_set(True)
            context.view_layer.objects.active = rail_object

        settings.last_estimated_length = estimated_length
        settings.last_error = ""
        settings.last_result = (
            f"{object_name} を生成しました。制御点数: {len(points)} / "
            f"推定レール長: {estimated_length:.2f}")
        self.report({'INFO'}, settings.last_result)
        return {'FINISHED'}


class MYADDON_OT_regenerate_selected_auto_rail(bpy.types.Operator):
    bl_idname = "myaddon.regenerate_selected_auto_rail"
    bl_label = "選択レールを再生成"
    bl_description = "現在の設定で自動生成レールの制御点を作り直します"
    bl_options = {'REGISTER', 'UNDO'}

    def draw(self, context):
        layout = self.layout
        layout.label(text="選択中レールの制御点を再生成します。", icon='ERROR')
        layout.label(text="手動で編集した制御点は失われます。")
        layout.label(text="続行しますか？")

    def invoke(self, context, event):
        return context.window_manager.invoke_props_dialog(self, width=420)

    def execute(self, context):
        settings = context.scene.level_auto_rail_settings
        obj = context.view_layer.objects.active
        if context.mode != "OBJECT":
            return _set_error(settings, self, "オブジェクトモードで実行してください。")
        if obj is None:
            return _set_error(settings, self, "アクティブオブジェクトがありません。")
        if obj.type != "CURVE":
            return _set_error(settings, self, "選択中のオブジェクトはカーブではありません。")
        if not _is_auto_rail(obj):
            return _set_error(
                settings, self,
                "選択中のレールは自動生成レールではありません。再生成は実行できません。")

        points, estimated_length = _generate_points(settings)
        validation_error = _validate_points(settings, points)
        if validation_error:
            return _set_error(settings, self, validation_error)

        try:
            _replace_curve_points(obj, points, settings.loop)
            ensure_rail_properties(obj)
            obj["rail_loop"] = bool(settings.loop)
            _store_generator_metadata(obj, settings, estimated_length)
        except Exception as error:
            return _set_error(settings, self, f"選択レールの再生成に失敗しました: {error}")

        settings.last_estimated_length = estimated_length
        settings.last_error = ""
        settings.last_result = (
            f"{obj.name} を再生成しました。制御点数: {len(points)} / "
            f"推定レール長: {estimated_length:.2f}")
        self.report({'INFO'}, settings.last_result)
        return {'FINISHED'}


class MYADDON_OT_reset_auto_rail_settings(bpy.types.Operator):
    bl_idname = "myaddon.reset_auto_rail_settings"
    bl_label = "レール生成設定をリセット"
    bl_description = "レール自動生成の設定を初期値へ戻します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        settings = context.scene.level_auto_rail_settings
        for property_name, value in DEFAULTS.items():
            setattr(settings, property_name, value)
        settings.last_estimated_length = 0.0
        settings.last_error = ""
        settings.last_result = "レール自動生成設定を初期値へ戻しました。"
        self.report({'INFO'}, settings.last_result)
        return {'FINISHED'}


class VIEW3D_PT_auto_rail_generator(bpy.types.Panel):
    bl_label = "レール自動生成"
    bl_idname = "VIEW3D_PT_auto_rail_generator"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "Level Editor"

    def draw(self, context):
        layout = self.layout
        settings = context.scene.level_auto_rail_settings
        obj = context.view_layer.objects.active

        shape_box = layout.box()
        shape_box.label(text="生成設定")
        shape_box.prop(settings, "preset")
        shape_box.prop(settings, "rail_name")
        shape_box.prop(settings, "target_length")
        shape_box.prop(settings, "control_point_count")
        shape_box.prop(settings, "horizontal_amount")
        shape_box.prop(settings, "vertical_amount")
        shape_box.prop(settings, "curve_count")

        position_box = layout.box()
        position_box.label(text="開始位置と方向")
        position_box.prop(settings, "start_position")
        position_box.prop(settings, "forward_axis")
        position_box.prop(settings, "mirror_horizontal")
        position_box.prop(settings, "mirror_vertical")
        position_box.prop(settings, "loop")
        position_box.prop(settings, "select_after_generate")

        action_box = layout.box()
        action_box.label(text="生成操作")
        action_box.operator(MYADDON_OT_generate_auto_rail.bl_idname, icon='CURVE_DATA')
        action_box.operator(
            MYADDON_OT_regenerate_selected_auto_rail.bl_idname,
            icon='FILE_REFRESH')
        action_box.operator(MYADDON_OT_reset_auto_rail_settings.bl_idname, icon='LOOP_BACK')

        diagnostics_box = layout.box()
        diagnostics_box.label(text="生成結果")
        if obj is None:
            diagnostics_box.label(text="オブジェクトが選択されていません")
        elif _is_exportable_curve(obj):
            diagnostics_box.label(text="選択オブジェクトはレールとして出力可能です")
            if _is_auto_rail(obj):
                diagnostics_box.label(text="選択オブジェクトは自動生成レールです")
        else:
            diagnostics_box.label(text="選択オブジェクトはレールとして出力できません")
        diagnostics_box.label(text=f"生成予定の制御点数: {settings.control_point_count}")
        try:
            _points, planned_length = _generate_points(settings)
            diagnostics_box.label(text=f"生成予定の推定レール長: {planned_length:.2f}")
            error_rate = abs(
                planned_length - settings.target_length) / settings.target_length * 100.0
            diagnostics_box.label(text=f"目標長との差: {error_rate:.3f}%")
        except Exception:
            diagnostics_box.label(text="推定レール長を計算できません")
        if settings.last_result:
            diagnostics_box.label(text=f"最後の生成結果: {settings.last_result}")
        if settings.last_error:
            diagnostics_box.label(text=f"最後のエラー: {settings.last_error}", icon='ERROR')


def register_auto_rail_properties():
    if not hasattr(bpy.types.Scene, "level_auto_rail_settings"):
        bpy.types.Scene.level_auto_rail_settings = bpy.props.PointerProperty(
            type=MYADDON_PG_auto_rail_settings)


def unregister_auto_rail_properties():
    if hasattr(bpy.types.Scene, "level_auto_rail_settings"):
        del bpy.types.Scene.level_auto_rail_settings

