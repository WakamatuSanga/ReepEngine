import math
import time

import blf
import bpy
import gpu
from bpy_extras.object_utils import world_to_camera_view
from gpu_extras.batch import batch_for_shader
from mathutils import Vector


_ROLE_ITEMS = (
    ("Enemy", "Enemy", "敵 / Enemy"),
    ("Landmark", "Landmark", "目印 / Landmark"),
    ("Goal", "Goal", "ゴール / Goal"),
    ("Event", "Event", "イベント / Event"),
    ("Object", "Object", "通常Object"),
)

_POSITION_ITEMS = (
    ("TOP_RIGHT", "Top Right", "右上に表示"),
    ("BOTTOM_RIGHT", "Bottom Right", "右下に表示"),
)

_CURVE_ITEMS = (
    ("PeakAtMiddle", "Peak At Middle", "中央で盛り上がる"),
    ("Rising", "Rising", "後半へ盛り上がる"),
    ("Falling", "Falling", "後半へ落ち着く"),
    ("Flat", "Flat", "一定"),
)


class StageAnalysisCache:
    brightness = None
    perception = None
    brightness_time = 0.0
    perception_time = 0.0


class CameraPreviewCache:
    offscreen = None
    width = 0
    height = 0
    camera_name = ""
    last_update = 0.0
    last_error = ""
    used_offscreen = False

    @classmethod
    def free(cls):
        if cls.offscreen is not None:
            try:
                cls.offscreen.free()
            except Exception:
                pass
        cls.offscreen = None
        cls.width = 0
        cls.height = 0
        cls.camera_name = ""
        cls.last_update = 0.0
        cls.last_error = ""
        cls.used_offscreen = False


class DrawStageAnalysisOverlay:
    handle = None

    @staticmethod
    def draw_overlay():
        context = bpy.context
        scene = context.scene
        region = context.region
        if scene is None or region is None:
            return
        if not getattr(scene, "level_stage_analysis_enable_overlay", True):
            return

        if getattr(scene, "level_stage_analysis_enable_camera_preview", True):
            _draw_camera_preview(context)
        if getattr(scene, "level_stage_analysis_enable_parameter_panel", True):
            _draw_parameter_panel(context)

    @classmethod
    def register_handler(cls):
        if cls.handle is None:
            cls.handle = bpy.types.SpaceView3D.draw_handler_add(cls.draw_overlay, (), "WINDOW", "POST_PIXEL")

    @classmethod
    def unregister_handler(cls):
        if cls.handle is not None:
            bpy.types.SpaceView3D.draw_handler_remove(cls.handle, "WINDOW")
            cls.handle = None
        CameraPreviewCache.free()


class MYADDON_OT_analyze_stage_brightness(bpy.types.Operator):
    bl_idname = "myaddon.analyze_stage_brightness"
    bl_label = "明るさを解析 (Analyze Brightness)"
    bl_description = "Scene内のWorld/Light/Materialから軽量な明るさ目安を更新します"
    bl_options = {'REGISTER'}

    def execute(self, context):
        StageAnalysisCache.brightness = _analyze_brightness(context.scene)
        StageAnalysisCache.brightness_time = time.time()
        self.report({'INFO'}, "Stage brightness analysis updated")
        return {'FINISHED'}


class MYADDON_OT_analyze_stage_perception(bpy.types.Operator):
    bl_idname = "myaddon.analyze_stage_perception"
    bl_label = "カメラ視点を解析 (Analyze Camera View)"
    bl_description = "重要ObjectがCamera View内で見えるかを軽く確認します"
    bl_options = {'REGISTER'}

    def execute(self, context):
        StageAnalysisCache.perception = _analyze_perception(context.scene)
        StageAnalysisCache.perception_time = time.time()
        self.report({'INFO'}, "Stage perception analysis updated")
        return {'FINISHED'}


class MYADDON_OT_mark_stage_important_object(bpy.types.Operator):
    bl_idname = "myaddon.mark_stage_important_object"
    bl_label = "選択Objectを重要扱い (Mark Selected As Important)"
    bl_description = "選択中Objectを視認性ガイドの確認対象にします"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        obj = context.object
        if obj is None:
            self.report({'WARNING'}, "No active object selected")
            return {'CANCELLED'}
        obj["is_stage_important_object"] = True
        obj["stage_object_role"] = obj.get("stage_object_role", "Object")
        try:
            obj.id_properties_ui("is_stage_important_object").update(description="Stage Analysis Perception Guide target")
            obj.id_properties_ui("stage_object_role").update(description="Stage Analysis role: Enemy / Landmark / Goal / Event / Object")
        except Exception:
            pass
        self.report({'INFO'}, f"Marked important: {obj.name}")
        return {'FINISHED'}


class MYADDON_OT_setup_stage_editor_layout(bpy.types.Operator):
    bl_idname = "myaddon.setup_stage_editor_layout"
    bl_label = "ステージエディタ レイアウトを適用"
    bl_description = "左を編集ビュー、右上をカメラビュー、右下を操作可能なステージパラメーターUIにします"
    bl_options = {'REGISTER'}

    def execute(self, context):
        try:
            main_area = _find_largest_view3d_area(context.screen)
            if main_area is None:
                self.report({'WARNING'}, "3D Viewportが見つかりません。手動でAreaを3D Viewportにしてください。")
                return {'CANCELLED'}

            right_column = _find_right_column_areas(context.screen, main_area)
            if len(right_column) >= 2:
                top_area = max(right_column, key=lambda item: item.y + item.height * 0.5)
                bottom_area = min(right_column, key=lambda item: item.y + item.height * 0.5)
                _set_area_as_main_edit_view(main_area)
                if not _set_area_as_camera_preview(top_area, context.scene):
                    self.report({'WARNING'}, "Preview Cameraが見つかりません。Cameraを作成または指定してください。")
                    return {'CANCELLED'}
                _set_area_as_parameter_panel_view(bottom_area)
                context.scene.level_stage_analysis_enable_overlay = False
                self.report({'INFO'}, "既存の右側Areaを使ってステージエディタ レイアウトを設定しました")
                return {'FINISHED'}

            left_area, right_area = _split_area(context, main_area, 'VERTICAL', 0.68)
            if right_area is None:
                self.report({'WARNING'}, "Area分割に失敗しました。右上Areaで「このエリアをカメラビューにする」を使ってください。")
                return {'CANCELLED'}

            top_area, bottom_area = _split_area(context, right_area, 'HORIZONTAL', 0.55)
            if top_area is None or bottom_area is None:
                _set_area_as_main_edit_view(left_area)
                _set_area_as_camera_preview(right_area, context.scene)
                self.report({'WARNING'}, "右下Area分割に失敗しました。右上Camera Viewのみ設定しました。")
                return {'FINISHED'}

            _set_area_as_main_edit_view(left_area)
            _set_area_as_camera_preview(top_area, context.scene)
            _set_area_as_parameter_panel_view(bottom_area)
            context.scene.level_stage_analysis_enable_overlay = False
            self.report({'INFO'}, "ステージエディタ レイアウトを設定しました")
            return {'FINISHED'}
        except Exception as error:
            self.report({'WARNING'}, f"Layout setup failed: {error}")
            return {'CANCELLED'}


class MYADDON_OT_set_active_area_camera_preview(bpy.types.Operator):
    bl_idname = "myaddon.set_active_area_camera_preview"
    bl_label = "このエリアをカメラビューにする"
    bl_description = "現在の3D Viewport AreaをPreview CameraのCamera Viewにします"
    bl_options = {'REGISTER'}

    def execute(self, context):
        if context.area is None or context.area.type != 'VIEW_3D':
            self.report({'WARNING'}, "Camera Previewにしたい3D Viewport上で実行してください。")
            return {'CANCELLED'}
        if not _set_area_as_camera_preview(context.area, context.scene):
            self.report({'WARNING'}, "Preview Cameraが見つかりません。Cameraを作成または指定してください。")
            return {'CANCELLED'}
        context.scene.level_stage_analysis_enable_overlay = False
        self.report({'INFO'}, "Active AreaをCamera Previewにしました")
        return {'FINISHED'}


class MYADDON_OT_set_active_area_main_edit(bpy.types.Operator):
    bl_idname = "myaddon.set_active_area_main_edit"
    bl_label = "このエリアを編集ビューにする"
    bl_description = "現在の3D Viewport Areaを通常編集用Viewportに戻します"
    bl_options = {'REGISTER'}

    def execute(self, context):
        if context.area is None or context.area.type != 'VIEW_3D':
            self.report({'WARNING'}, "Main Edit Viewにしたい3D Viewport上で実行してください。")
            return {'CANCELLED'}
        _set_area_as_main_edit_view(context.area)
        self.report({'INFO'}, "Active AreaをMain Edit Viewにしました")
        return {'FINISHED'}


class MYADDON_OT_set_active_area_stage_parameters(bpy.types.Operator):
    bl_idname = "myaddon.set_active_area_stage_parameters"
    bl_label = "このエリアをステージパラメーターUIにする"
    bl_description = "現在の3D Viewport Areaを、Level Editorタブの操作用Nパネル表示にします"
    bl_options = {'REGISTER'}

    def execute(self, context):
        if context.area is None:
            self.report({'WARNING'}, "ステージパラメーターを表示したいArea上で実行してください。")
            return {'CANCELLED'}
        _set_area_as_parameter_panel_view(context.area)
        context.scene.level_stage_analysis_enable_overlay = False
        self.report({'INFO'}, "Active AreaをステージパラメーターUIにしました")
        return {'FINISHED'}

class MYADDON_OT_create_or_switch_stage_editor_workspace(bpy.types.Operator):
    bl_idname = "myaddon.create_or_switch_stage_editor_workspace"
    bl_label = "ステージエディタ ワークスペースを開く"
    bl_description = "Stage Editor Workspaceを作成または切り替え、ステージエディタ レイアウトを適用します"
    bl_options = {'REGISTER'}

    workspace_name: bpy.props.StringProperty(default="Stage Editor")

    def execute(self, context):
        if context.window is None:
            self.report({'WARNING'}, "Workspace切り替えにはBlender UI Windowが必要です。")
            return {'CANCELLED'}

        workspace = bpy.data.workspaces.get(self.workspace_name)
        if workspace is None:
            try:
                result = bpy.ops.workspace.duplicate()
                if 'FINISHED' in result:
                    workspace = context.window.workspace
                    workspace.name = self.workspace_name
            except Exception as error:
                self.report({'WARNING'}, f"Workspace作成に失敗しました: {error}")
                return {'CANCELLED'}

        try:
            context.window.workspace = workspace
        except Exception as error:
            self.report({'WARNING'}, f"Workspace切り替えに失敗しました: {error}")
            return {'CANCELLED'}

        try:
            result = bpy.ops.myaddon.setup_stage_editor_layout()
            if 'FINISHED' not in result:
                self.report({'WARNING'}, "Workspaceへ切り替えました。Layoutは手動ボタンで設定してください。")
                return {'FINISHED'}
        except Exception as error:
            self.report({'WARNING'}, f"Workspaceへ切り替えましたがLayout設定に失敗しました: {error}")
            return {'FINISHED'}

        self.report({'INFO'}, "Stage Editor Workspaceを開きました")
        return {'FINISHED'}

class MYADDON_OT_reset_stage_editor_layout(bpy.types.Operator):
    bl_idname = "myaddon.reset_stage_editor_layout"
    bl_label = "ステージエディタ レイアウトをリセット"
    bl_description = "Preview補助表示をOFFにし、現在Areaを通常編集Viewに戻します"
    bl_options = {'REGISTER'}

    def execute(self, context):
        context.scene.level_stage_analysis_enable_overlay = False
        if context.area is not None and context.area.type == 'VIEW_3D':
            _set_area_as_main_edit_view(context.area)
        CameraPreviewCache.free()
        self.report({'INFO'}, "ステージエディタ レイアウト設定をリセットしました")
        return {'FINISHED'}

class VIEW3D_PT_stage_analysis_overlay(bpy.types.Panel):
    bl_label = "ステージエディタ / 解析 (Stage Editor)"
    bl_idname = "VIEW3D_PT_stage_analysis_overlay"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "Level Editor"

    def draw(self, context):
        layout = self.layout
        scene = context.scene
        obj = context.object

        layout_box = layout.box()
        layout_box.label(text="ステージエディタ レイアウト")
        layout_box.operator(MYADDON_OT_create_or_switch_stage_editor_workspace.bl_idname, text="ステージエディタ ワークスペースを開く")
        layout_box.operator(MYADDON_OT_setup_stage_editor_layout.bl_idname, text="ステージエディタ レイアウトを適用")
        layout_box.operator(MYADDON_OT_set_active_area_camera_preview.bl_idname, text="このエリアをカメラビューにする")
        layout_box.operator(MYADDON_OT_set_active_area_main_edit.bl_idname, text="このエリアを編集ビューにする")
        layout_box.operator(MYADDON_OT_set_active_area_stage_parameters.bl_idname, text="このエリアをパラメーターUIにする")
        layout_box.operator(MYADDON_OT_reset_stage_editor_layout.bl_idname, text="レイアウトをリセット")
        layout_box.label(text="右上はCamera View、右下は操作可能なNパネルUIにします")

        preview = layout.box()
        preview.label(text="カメラプレビュー")
        preview.prop_search(scene, "level_stage_analysis_preview_camera_name", scene, "objects", text="プレビューCamera")
        preview.prop(scene, "level_stage_analysis_use_scene_camera", text="Scene Cameraを使う")
        preview.prop(scene, "level_stage_analysis_hide_grid_in_preview", text="PreviewのGridを隠す")
        preview.prop(scene, "level_stage_analysis_hide_gizmo_in_preview", text="PreviewのGizmoを隠す")
        camera = _resolve_preview_camera(scene)
        preview.label(text=f"現在のCamera: {camera.name if camera else 'No Camera'}")
        preview.operator(MYADDON_OT_set_active_area_camera_preview.bl_idname, text="このエリアをCamera Viewにする")

        parameters = layout.box()
        parameters.label(text="ステージパラメーター")
        parameters.prop(scene, "level_stage_analysis_show_brightness", text="明るさガイドを表示")
        parameters.prop(scene, "level_stage_analysis_show_composition", text="構図ガイドを表示")
        parameters.prop(scene, "level_stage_analysis_show_perception", text="視認性ガイドを表示")
        parameters.prop(scene, "level_stage_analysis_show_selected_object_info", text="選択オブジェクト情報を表示")
        parameters.prop(scene, "level_stage_analysis_show_visual_bars", text="図解バーを表示")
        parameters.operator(MYADDON_OT_analyze_stage_brightness.bl_idname, text="明るさを解析")
        parameters.operator(MYADDON_OT_analyze_stage_perception.bl_idname, text="カメラ視点を解析")
        parameters.operator(MYADDON_OT_mark_stage_important_object.bl_idname, text="選択Objectを重要扱いにする")
        parameters.label(text="右下AreaではこのLevel Editorタブを操作してください")

        brightness = layout.box()
        brightness.label(text="明るさガイド")
        brightness.prop(scene, "level_stage_analysis_auto_update_brightness", text="自動更新")
        brightness.prop(scene, "level_stage_analysis_brightness_update_interval", text="更新間隔")
        brightness.prop(scene, "level_stage_analysis_target_brightness_min", text="目標の明るさ 最小")
        brightness.prop(scene, "level_stage_analysis_target_brightness_max", text="目標の明るさ 最大")
        brightness.prop(scene, "level_stage_analysis_too_dark_threshold", text="暗すぎ判定")
        brightness.prop(scene, "level_stage_analysis_too_bright_threshold", text="明るすぎ判定")

        composition = layout.box()
        composition.label(text="構図ガイド")
        composition.prop(scene, "level_stage_analysis_stage_section_count", text="区間数")
        composition.prop(scene, "level_stage_analysis_section_1_label", text="区間1 ラベル")
        composition.prop(scene, "level_stage_analysis_section_2_label", text="区間2 ラベル")
        composition.prop(scene, "level_stage_analysis_section_3_label", text="区間3 ラベル")
        composition.prop(scene, "level_stage_analysis_width_flow", text="幅の流れ")
        composition.prop(scene, "level_stage_analysis_intensity_curve", text="盛り上がり曲線")
        composition.prop(scene, "level_stage_analysis_show_mini_graph", text="ミニグラフを表示")

        perception = layout.box()
        perception.label(text="視認性ガイド")
        perception.prop(scene, "level_stage_analysis_show_visible_important_count", text="見えている重要Object数を表示")
        perception.prop(scene, "level_stage_analysis_show_offscreen_warning", text="画面外警告を表示")
        perception.prop(scene, "level_stage_analysis_show_too_small_warning", text="小さすぎ警告を表示")
        if obj is not None:
            perception.label(text=f"選択中: {obj.name}")
            _draw_object_stage_analysis_properties(perception, obj)
        else:
            perception.label(text="Objectが選択されていません")

        summary = layout.box()
        summary.label(text="現在の解析サマリー")
        for kind, line_text in _build_parameter_panel_lines(scene, obj):
            if kind == "spacer":
                continue
            summary.label(text=line_text)

        legacy = layout.box()
        legacy.label(text="旧Viewport Overlay / Debug")
        legacy.prop(scene, "level_stage_analysis_enable_overlay", text="旧Overlayを表示")
        legacy.prop(scene, "level_stage_analysis_enable_camera_preview", text="旧小窓Camera Previewを表示")
        legacy.prop(scene, "level_stage_analysis_enable_parameter_panel", text="旧Parameter Overlayを表示")
        legacy.prop(scene, "level_stage_analysis_camera_preview_width", text="Overlay Preview 幅")
        legacy.prop(scene, "level_stage_analysis_camera_preview_height", text="Overlay Preview 高さ")
        legacy.prop(scene, "level_stage_analysis_camera_preview_alpha", text="Overlay Preview 透明度")
        legacy.prop(scene, "level_stage_analysis_parameter_panel_scale", text="Overlay Panel Scale")
        legacy.prop(scene, "level_stage_analysis_parameter_panel_alpha", text="Overlay Panel 透明度")
        if CameraPreviewCache.last_error:
            legacy.label(text=f"Overlay fallback: {CameraPreviewCache.last_error[:48]}")
def _find_largest_view3d_area(screen):
    view_areas = [area for area in screen.areas if area.type == 'VIEW_3D']
    if not view_areas:
        return None
    return max(view_areas, key=lambda area: area.width * area.height)


def _find_right_column_areas(screen, main_area):
    if screen is None or main_area is None:
        return []
    main_right = main_area.x + main_area.width
    candidates = []
    for area in screen.areas:
        if area == main_area or area.width <= 80 or area.height <= 80:
            continue
        center_x = area.x + area.width * 0.5
        if center_x >= main_right - 4:
            candidates.append(area)
    return sorted(candidates, key=lambda item: item.y + item.height * 0.5, reverse=True)

def _window_region(area):
    for region in area.regions:
        if region.type == 'WINDOW':
            return region
    return None


def _split_area(context, area, direction, factor):
    region = _window_region(area)
    if region is None:
        return area, None
    bounds = (area.x, area.y, area.width, area.height)
    try:
        with context.temp_override(window=context.window, screen=context.screen, area=area, region=region):
            result = bpy.ops.screen.area_split(direction=direction, factor=factor)
    except Exception:
        return area, None
    if 'FINISHED' not in result:
        return area, None

    x, y, width, height = bounds
    right = x + width
    top = y + height
    candidates = []
    for candidate in context.screen.areas:
        if candidate.width <= 8 or candidate.height <= 8:
            continue
        overlap_x = max(0, min(candidate.x + candidate.width, right) - max(candidate.x, x))
        overlap_y = max(0, min(candidate.y + candidate.height, top) - max(candidate.y, y))
        overlap_area = overlap_x * overlap_y
        if overlap_area > 0:
            candidates.append(candidate)
    if len(candidates) < 2:
        return area, None
    if direction == 'VERTICAL':
        return min(candidates, key=lambda item: item.x + item.width * 0.5), max(candidates, key=lambda item: item.x + item.width * 0.5)
    return max(candidates, key=lambda item: item.y + item.height * 0.5), min(candidates, key=lambda item: item.y + item.height * 0.5)
def _set_area_as_camera_preview(area, scene):
    if area is None:
        return False
    if area.type != 'VIEW_3D':
        area.type = 'VIEW_3D'
    camera = _resolve_preview_camera(scene)
    if camera is None:
        return False
    for space in area.spaces:
        if space.type != 'VIEW_3D':
            continue
        try:
            space.use_local_camera = True
            space.camera = camera
        except Exception:
            scene.camera = camera
        if space.region_3d is not None:
            space.region_3d.view_perspective = 'CAMERA'
        try:
            space.show_gizmo = not getattr(scene, "level_stage_analysis_hide_gizmo_in_preview", True)
            space.show_region_ui = False
            hide_grid = getattr(scene, "level_stage_analysis_hide_grid_in_preview", True)
            space.overlay.show_floor = not hide_grid
            space.overlay.show_axis_x = not hide_grid
            space.overlay.show_axis_y = not hide_grid
            space.overlay.show_axis_z = not hide_grid
        except Exception:
            pass
    area.tag_redraw()
    return True


def _set_area_as_main_edit_view(area):
    if area is None:
        return
    if area.type != 'VIEW_3D':
        area.type = 'VIEW_3D'
    for space in area.spaces:
        if space.type != 'VIEW_3D':
            continue
        try:
            space.use_local_camera = False
        except Exception:
            pass
        if space.region_3d is not None and space.region_3d.view_perspective == 'CAMERA':
            space.region_3d.view_perspective = 'PERSP'
        try:
            space.show_gizmo = True
            space.show_region_ui = False
            space.overlay.show_floor = True
            space.overlay.show_axis_x = True
            space.overlay.show_axis_y = True
            space.overlay.show_axis_z = True
        except Exception:
            pass
    area.tag_redraw()


def _set_area_as_parameter_panel_view(area):
    if area is None:
        return
    if area.type != 'VIEW_3D':
        area.type = 'VIEW_3D'
    for space in area.spaces:
        if space.type != 'VIEW_3D':
            continue
        if space.region_3d is not None and space.region_3d.view_perspective == 'CAMERA':
            space.region_3d.view_perspective = 'PERSP'
        try:
            space.show_region_ui = True
            if hasattr(space, "show_region_toolbar"):
                space.show_region_toolbar = False
            if hasattr(space, "show_region_header"):
                space.show_region_header = True
            space.show_gizmo = False
            space.overlay.show_floor = False
            space.overlay.show_axis_x = False
            space.overlay.show_axis_y = False
            space.overlay.show_axis_z = False
        except Exception:
            pass
    area.tag_redraw()

def _draw_camera_preview(context):
    scene = context.scene
    region = context.region
    preview_width = max(240, min(960, int(getattr(scene, "level_stage_analysis_camera_preview_width", 420))))
    preview_height = max(135, min(540, int(getattr(scene, "level_stage_analysis_camera_preview_height", 236))))
    alpha = max(0.1, min(1.0, getattr(scene, "level_stage_analysis_camera_preview_alpha", 1.0)))
    position = getattr(scene, "level_stage_analysis_camera_preview_position", "TOP_RIGHT")
    title_height = 30
    panel_width = preview_width + 20
    panel_height = preview_height + title_height + 20
    x, y, top_y = _panel_rect(region, panel_width, panel_height, position, 42, 42, 92, 42)

    _draw_rect(x, y, panel_width, panel_height, (0.015, 0.018, 0.022, 0.78 * alpha))
    _draw_rect(x, y, panel_width, panel_height, (0.35, 0.70, 1.0, 0.18 * alpha), outline=True)

    camera = _resolve_preview_camera(scene)
    title = "Player Camera Preview"
    if camera is not None:
        title = f"Player Camera Preview: {camera.name}"
    _draw_text(x + 10, top_y - 20, title, _line_color("title", alpha), 13)

    image_x = x + 10
    image_y = y + 10
    if camera is None:
        _draw_preview_fallback(scene, None, image_x, image_y, preview_width, preview_height, alpha, "No Camera")
        return
    if not _draw_camera_offscreen(context, camera, image_x, image_y, preview_width, preview_height, alpha):
        reason = CameraPreviewCache.last_error or "No realtime preview available"
        _draw_preview_fallback(scene, camera, image_x, image_y, preview_width, preview_height, alpha, reason)


def _draw_parameter_panel(context):
    scene = context.scene
    region = context.region
    obj = context.object
    scale = max(0.6, min(2.0, getattr(scene, "level_stage_analysis_parameter_panel_scale", 1.15)))
    alpha = max(0.05, min(1.0, getattr(scene, "level_stage_analysis_parameter_panel_alpha", 0.85)))
    text_size = int(12 * scale)
    line_height = int(17 * scale)
    section_gap = int(7 * scale)
    width = int(350 * scale)

    lines = _build_parameter_panel_lines(scene, obj)
    line_units = sum(0.45 if kind == "spacer" else 1.0 for kind, _text in lines)
    height = int((line_units + 2) * line_height + 26 * scale)
    position = getattr(scene, "level_stage_analysis_parameter_panel_position", "BOTTOM_RIGHT")
    x, y, top_y = _panel_rect(region, width, height, position, int(36 * scale), int(36 * scale), int(92 * scale), int(44 * scale))

    _draw_rect(x, y, width, height, (0.02, 0.025, 0.03, 0.72 * alpha))
    _draw_rect(x, y, width, height, (0.3, 0.7, 1.0, 0.12 * alpha), outline=True)

    text_x = x + int(10 * scale)
    text_y = top_y - int(20 * scale)
    for kind, line_text in lines:
        if kind == "spacer":
            text_y -= section_gap
            continue
        _draw_text(text_x, text_y, line_text, _line_color(kind, alpha), text_size)
        text_y -= line_height


def _draw_camera_offscreen(context, camera, x, y, width, height, alpha):
    try:
        now = time.time()
        interval = max(0.03, getattr(context.scene, "level_stage_analysis_camera_preview_update_interval", 0.1))
        if CameraPreviewCache.offscreen is None or CameraPreviewCache.width != width or CameraPreviewCache.height != height:
            CameraPreviewCache.free()
            CameraPreviewCache.offscreen = gpu.types.GPUOffScreen(width, height)
            CameraPreviewCache.width = width
            CameraPreviewCache.height = height
        if now - CameraPreviewCache.last_update >= interval or CameraPreviewCache.camera_name != camera.name:
            depsgraph = context.evaluated_depsgraph_get()
            view_matrix = camera.matrix_world.inverted()
            projection_matrix = camera.calc_matrix_camera(depsgraph, x=width, y=height, scale_x=1.0, scale_y=1.0)
            with CameraPreviewCache.offscreen.bind():
                frame_buffer = gpu.state.active_framebuffer_get()
                frame_buffer.clear(color=(0.025, 0.028, 0.035, 1.0), depth=1.0)
                _draw_view3d_to_offscreen(CameraPreviewCache.offscreen, context, view_matrix, projection_matrix)
            CameraPreviewCache.last_update = now
            CameraPreviewCache.camera_name = camera.name
            CameraPreviewCache.last_error = ""
            CameraPreviewCache.used_offscreen = True
        texture = getattr(CameraPreviewCache.offscreen, "texture_color", None)
        if texture is None:
            texture = getattr(CameraPreviewCache.offscreen, "color_texture", None)
        if texture is None:
            raise RuntimeError("GPUOffScreen texture unavailable")
        return _draw_image_texture(texture, x, y, width, height, alpha)
    except Exception as error:
        CameraPreviewCache.last_error = str(error)
        CameraPreviewCache.used_offscreen = False
        return False


def _draw_view3d_to_offscreen(offscreen, context, view_matrix, projection_matrix):
    try:
        offscreen.draw_view3d(context.scene, context.view_layer, context.space_data, context.region, view_matrix, projection_matrix, do_color_management=True)
    except TypeError:
        offscreen.draw_view3d(context.scene, context.view_layer, context.space_data, context.region, view_matrix, projection_matrix)


def _draw_image_texture(texture, x, y, width, height, alpha):
    try:
        shader = gpu.shader.from_builtin("IMAGE")
        points = [(x, y), (x + width, y), (x + width, y + height), (x, y + height)]
        tex_coords = [(0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0)]
        batch = batch_for_shader(shader, "TRI_FAN", {"pos": points, "texCoord": tex_coords})
        gpu.state.blend_set("ALPHA")
        shader.bind()
        shader.uniform_sampler("image", texture)
        batch.draw(shader)
        gpu.state.blend_set("NONE")
        if alpha < 0.99:
            _draw_rect(x, y, width, height, (0.0, 0.0, 0.0, 1.0 - alpha))
        return True
    except Exception as error:
        CameraPreviewCache.last_error = str(error)
        gpu.state.blend_set("NONE")
        return False


def _draw_preview_fallback(scene, camera, x, y, width, height, alpha, reason):
    _draw_rect(x, y, width, height, (0.035, 0.04, 0.05, 0.95 * alpha))
    _draw_rect(x, y, width, height, (0.45, 0.70, 1.0, 0.22 * alpha), outline=True)
    _draw_text(x + 14, y + height - 28, "Player Camera Preview", _line_color("title", alpha), 13)
    if camera is None:
        _draw_text(x + 14, y + height - 52, "No Camera", _line_color("warn", alpha), 13)
        return
    _draw_text(x + 14, y + height - 52, "Realtime preview fallback", _line_color("warn", alpha), 12)
    _draw_text(x + 14, y + height - 72, f"Camera: {camera.name}", _line_color("normal", alpha), 12)
    _draw_text(x + 14, y + height - 92, reason[:52], _line_color("dim", alpha), 11)
    _draw_projected_important_points(scene, camera, x, y, width, height, alpha)


def _draw_projected_important_points(scene, camera, x, y, width, height, alpha):
    frame_height = max(24, height - 126)
    _draw_rect(x + 14, y + 18, width - 28, frame_height, (0.01, 0.012, 0.016, 0.45 * alpha), outline=True)
    for obj in scene.objects:
        if not _is_important_object(obj):
            continue
        co = world_to_camera_view(scene, camera, obj.matrix_world.translation)
        if co.z < 0.0 or co.x < 0.0 or co.x > 1.0 or co.y < 0.0 or co.y > 1.0:
            continue
        px = x + 14 + co.x * (width - 28)
        py = y + 18 + co.y * frame_height
        _draw_rect(px - 3, py - 3, 6, 6, _line_color("ok", alpha))


def _panel_rect(region, width, height, position, margin_x, margin_y, top_reserved, bottom_reserved):
    x = max(8, region.width - width - margin_x)
    if position == "BOTTOM_RIGHT":
        top_y = min(region.height - margin_y, height + margin_y + bottom_reserved)
    else:
        top_y = max(height + margin_y, region.height - margin_y - top_reserved)
    y = max(8, top_y - height)
    return x, y, top_y


def _resolve_preview_camera(scene):
    name = getattr(scene, "level_stage_analysis_preview_camera_name", "")
    if name:
        obj = scene.objects.get(name)
        if obj is not None and obj.type == "CAMERA":
            return obj
    for flag_name in ("is_player_camera", "is_game_camera", "is_camera_start"):
        for obj in scene.objects:
            if obj.type == "CAMERA" and obj.get(flag_name):
                return obj
    if getattr(scene, "level_stage_analysis_use_scene_camera", True) and scene.camera is not None:
        return scene.camera
    preferred_tokens = ("playercamera", "player_camera", "gamecamera", "game_camera", "camera")
    for token in preferred_tokens:
        for obj in scene.objects:
            if obj.type == "CAMERA" and token in obj.name.lower():
                return obj
    for obj in scene.objects:
        if obj.type == "CAMERA":
            return obj
    return None

def _draw_object_stage_analysis_properties(layout, obj):
    if "is_stage_important_object" in obj:
        layout.prop(obj, '["is_stage_important_object"]', text="重要Object")
    else:
        row = layout.row()
        row.enabled = False
        row.label(text="重要Object: (未設定)")
    if "stage_object_role" in obj:
        layout.prop(obj, '["stage_object_role"]', text="役割")
    else:
        row = layout.row()
        row.enabled = False
        row.label(text="役割: (未設定)")


def _build_overlay_lines(scene):
    return _build_parameter_panel_lines(scene, None)


def _build_parameter_panel_lines(scene, selected_object):
    _refresh_cache_if_needed(scene)
    lines = [("title", "ステージパラメーター")]
    lines.append(("normal", "------------------------------"))
    show_brightness = getattr(scene, "level_stage_analysis_show_brightness", True)
    show_composition = getattr(scene, "level_stage_analysis_show_composition", True)
    show_perception = getattr(scene, "level_stage_analysis_show_perception", True)
    if show_brightness:
        lines.extend(_brightness_lines(scene))
    if show_brightness and (show_composition or show_perception):
        lines.append(("spacer", ""))
    if show_composition:
        lines.extend(_composition_lines(scene))
    if show_composition and show_perception:
        lines.append(("spacer", ""))
    if show_perception:
        lines.extend(_perception_lines(scene))
    if getattr(scene, "level_stage_analysis_show_selected_object_info", True):
        lines.append(("spacer", ""))
        lines.extend(_selected_object_lines(selected_object))
    return lines


def _selected_object_lines(obj):
    lines = [("section", "選択オブジェクト")]
    if obj is None:
        lines.append(("dim", "なし"))
        return lines
    location = obj.location
    role = obj.get("stage_object_role", "(none)")
    important = "はい" if bool(obj.get("is_stage_important_object", False)) else "いいえ"
    lines.extend([
        ("normal", f"名前: {obj.name[:28]}"),
        ("normal", f"種類: {obj.type}"),
        ("dim", f"位置: {location.x:.1f}, {location.y:.1f}, {location.z:.1f}"),
        ("dim", f"役割: {role}  重要: {important}"),
    ])
    return lines

def _refresh_cache_if_needed(scene):
    now = time.time()
    if StageAnalysisCache.brightness is None:
        StageAnalysisCache.brightness = _analyze_brightness(scene)
        StageAnalysisCache.brightness_time = now
    if getattr(scene, "level_stage_analysis_auto_update_brightness", False):
        interval = max(0.2, getattr(scene, "level_stage_analysis_brightness_update_interval", 2.0))
        if now - StageAnalysisCache.brightness_time >= interval:
            StageAnalysisCache.brightness = _analyze_brightness(scene)
            StageAnalysisCache.brightness_time = now
    if StageAnalysisCache.perception is None or now - StageAnalysisCache.perception_time >= 1.0:
        StageAnalysisCache.perception = _analyze_perception(scene)
        StageAnalysisCache.perception_time = now


def _brightness_lines(scene):
    data = StageAnalysisCache.brightness or _analyze_brightness(scene)
    avg = data["average"]
    min_target = getattr(scene, "level_stage_analysis_target_brightness_min", 0.35)
    max_target = getattr(scene, "level_stage_analysis_target_brightness_max", 0.75)
    too_dark = getattr(scene, "level_stage_analysis_too_dark_threshold", 0.25)
    too_bright = getattr(scene, "level_stage_analysis_too_bright_threshold", 0.85)
    if avg < too_dark:
        state = "暗すぎ"
        kind = "bad"
    elif avg > too_bright:
        state = "明るすぎ"
        kind = "warn"
    elif min_target <= avg <= max_target:
        state = "OK"
        kind = "ok"
    else:
        state = "要確認"
        kind = "warn"

    lines = [("section", "明るさ")]
    if getattr(scene, "level_stage_analysis_show_visual_bars", True):
        lines.append((kind, _brightness_gauge(avg, _scaled_char_count(scene, 22, 16, 36))))
    lines.extend([
        (kind, f"平均: {avg:.2f}  {state}"),
        ("normal", f"暗い: {data['dark_percent']:.0f}%   明るい: {data['bright_percent']:.0f}%"),
        ("dim", f"範囲: {data['min']:.2f} - {data['max']:.2f}"),
    ])
    return lines


def _composition_lines(scene):
    count = max(1, min(3, int(getattr(scene, "level_stage_analysis_stage_section_count", 3))))
    labels = [
        getattr(scene, "level_stage_analysis_section_1_label", "Intro"),
        getattr(scene, "level_stage_analysis_section_2_label", "Peak"),
        getattr(scene, "level_stage_analysis_section_3_label", "Exit"),
    ]
    result = [
        ("section", "構図"),
        ("ok", _section_diagram(count, _scaled_char_count(scene, 6, 4, 12))),
        ("normal", _section_label_line(labels, count, _scaled_char_count(scene, 6, 4, 12))),
        ("normal", f"幅の流れ: {getattr(scene, 'level_stage_analysis_width_flow', 'Wide-Narrow-Wide')}"),
    ]
    if getattr(scene, "level_stage_analysis_show_mini_graph", True):
        result.extend(_intensity_graph_lines(getattr(scene, "level_stage_analysis_intensity_curve", "PeakAtMiddle"), _scaled_char_count(scene, 16, 11, 28)))
    return result


def _perception_lines(scene):
    data = StageAnalysisCache.perception or _analyze_perception(scene)
    total = data["total"]
    visible = data["visible"]
    offscreen = data["offscreen"]
    too_small = data["too_small"]
    lines = [("section", "視認性")]

    if data.get("camera_missing"):
        lines.append(("warn", "Cameraなし"))
        if total > 0:
            lines.append(("normal", f"重要Object: {total}"))
        return lines
    if total == 0:
        lines.append(("ok", "重要Objectなし"))
        lines.append(("dim", "Objectを重要扱いにするとCamera視点で確認できます。"))
        return lines

    has_warning = offscreen > 0 or too_small > 0
    kind = "warn" if has_warning else "ok"
    bar = _ratio_bar(visible, total, _scaled_char_count(scene, 16, 12, 28)) if getattr(scene, "level_stage_analysis_show_visual_bars", True) else ""
    lines.append((kind, f"表示中: {visible} / {total} {bar}".rstrip()))
    if getattr(scene, "level_stage_analysis_show_offscreen_warning", True):
        lines.append(("warn" if offscreen else "normal", f"画面外: {offscreen}"))
    if getattr(scene, "level_stage_analysis_show_too_small_warning", True):
        lines.append(("warn" if too_small else "normal", f"小さすぎ: {too_small}"))
    lines.append((kind, f"状態: {'警告' if has_warning else '良好'}"))
    return lines


def _analyze_brightness(scene):
    samples = []
    world = scene.world
    if world is not None:
        color = getattr(world, "color", (0.05, 0.05, 0.05))
        samples.append(_luminance(color))
    for obj in scene.objects:
        if obj.type == "LIGHT":
            light = obj.data
            energy = getattr(light, "energy", 0.0)
            samples.append(max(0.0, min(1.0, math.log10(max(energy, 1.0)) / 5.0)))
        for slot in getattr(obj, "material_slots", []):
            material = slot.material
            if material is None:
                continue
            samples.append(max(0.0, min(1.0, _luminance(getattr(material, "diffuse_color", (0.5, 0.5, 0.5, 1.0))))))
    if not samples:
        samples = [0.5]
    too_dark = getattr(scene, "level_stage_analysis_too_dark_threshold", 0.25)
    too_bright = getattr(scene, "level_stage_analysis_too_bright_threshold", 0.85)
    average = sum(samples) / len(samples)
    return {
        "average": max(0.0, min(1.0, average)),
        "min": min(samples),
        "max": max(samples),
        "dark_percent": 100.0 * sum(1 for value in samples if value < too_dark) / len(samples),
        "bright_percent": 100.0 * sum(1 for value in samples if value > too_bright) / len(samples),
    }


def _analyze_perception(scene):
    important = [obj for obj in scene.objects if _is_important_object(obj)]
    camera = _resolve_preview_camera(scene) or scene.camera
    if camera is None:
        return {"total": len(important), "visible": 0, "offscreen": len(important), "too_small": 0, "camera_missing": True}
    visible = 0
    offscreen = 0
    too_small = 0
    for obj in important:
        center = obj.matrix_world.translation
        co = world_to_camera_view(scene, camera, center)
        in_view = 0.0 <= co.x <= 1.0 and 0.0 <= co.y <= 1.0 and co.z >= 0.0
        if not in_view:
            offscreen += 1
            continue
        visible += 1
        if _projected_radius(scene, camera, obj, co) < 0.025:
            too_small += 1
    return {"total": len(important), "visible": visible, "offscreen": offscreen, "too_small": too_small, "camera_missing": False}


def _is_important_object(obj):
    if bool(obj.get("is_stage_important_object", False)):
        return True
    role = str(obj.get("stage_object_role", ""))
    if role in {"Enemy", "Landmark", "Goal", "Event"}:
        return True
    name = obj.name.lower()
    return any(token in name for token in ("enemy", "goal", "landmark", "spawn", "eventflag"))


def _projected_radius(scene, camera, obj, center_co):
    try:
        radius = max(obj.dimensions.x, obj.dimensions.y, obj.dimensions.z) * 0.5
        if radius <= 0.0001:
            radius = 0.25
        offset = obj.matrix_world.translation + Vector((radius, 0.0, 0.0))
        edge_co = world_to_camera_view(scene, camera, offset)
        return abs(edge_co.x - center_co.x)
    except Exception:
        return 0.0


def _luminance(color):
    try:
        return float(color[0]) * 0.2126 + float(color[1]) * 0.7152 + float(color[2]) * 0.0722
    except Exception:
        return 0.5


def _brightness_gauge(value, width):
    clamped = max(0.0, min(1.0, value))
    marker = int(round(clamped * (width - 1)))
    chars = ["-"] * width
    for tick in (0, width // 4, width // 2, (width * 3) // 4, width - 1):
        chars[tick] = "|"
    chars[marker] = "*"
    return "Dark " + "".join(chars) + " Bright"


def _ratio_bar(value, total, width):
    if total <= 0:
        return "[--------------]"
    ratio = max(0.0, min(1.0, float(value) / float(total)))
    filled = int(round(ratio * width))
    return "[" + "#" * filled + "-" * (width - filled) + "]"


def _section_diagram(count, dash_count):
    if count <= 1:
        return "[1]"
    dashes = "-" * max(2, dash_count)
    if count == 2:
        return f"[1] {dashes} [2]"
    return f"[1] {dashes} [2] {dashes} [3]"


def _section_label_line(labels, count, dash_count):
    clipped = [(labels[index] if index < len(labels) else "")[:7] for index in range(count)]
    gap = max(8, dash_count + 5)
    if count <= 1:
        return clipped[0]
    if count == 2:
        return f"{clipped[0]:<7}{clipped[1]:>{gap}}"
    return f"{clipped[0]:<7}{clipped[1]:^{gap}}{clipped[2]:>7}"


def _intensity_graph_lines(curve, width):
    width = max(7, width)
    half = max(2, width // 2)
    if curve == "Rising":
        return [("dim", "Intensity"), ("ok", "_" * half + "/" + "^" * max(2, width - half - 1))]
    if curve == "Falling":
        return [("dim", "Intensity"), ("ok", "^" * half + "\\" + "_" * max(2, width - half - 1))]
    if curve == "Flat":
        return [("dim", "Intensity"), ("ok", "_" * width)]
    left = max(2, (width - 2) // 2)
    right = max(2, width - left - 2)
    return [("dim", "Intensity"), ("ok", "_" * left + "/\\" + "_" * right)]


def _overlay_scale(scene):
    return max(0.6, min(2.0, getattr(scene, "level_stage_analysis_parameter_panel_scale", 1.15)))


def _scaled_char_count(scene, base, minimum, maximum):
    return max(minimum, min(maximum, int(round(base * _overlay_scale(scene)))))


def _line_color(kind, alpha):
    colors = {
        "title": (0.90, 0.96, 1.0, alpha),
        "section": (0.55, 0.85, 1.0, alpha),
        "ok": (0.70, 1.0, 0.95, alpha),
        "warn": (1.0, 0.82, 0.25, alpha),
        "bad": (1.0, 0.32, 0.25, alpha),
        "normal": (0.86, 0.90, 0.94, alpha),
        "dim": (0.58, 0.66, 0.72, alpha),
    }
    return colors.get(kind, colors["normal"])


def _draw_text(x, y, text, color, size):
    font_id = 0
    blf.size(font_id, size)
    blf.color(font_id, color[0], color[1], color[2], color[3])
    blf.position(font_id, x, y, 0)
    blf.draw(font_id, text)


def _draw_rect(x, y, width, height, color, outline=False):
    try:
        shader = gpu.shader.from_builtin("2D_UNIFORM_COLOR")
    except Exception:
        shader = gpu.shader.from_builtin("UNIFORM_COLOR")
    if outline:
        points = [(x, y), (x + width, y), (x + width, y + height), (x, y + height), (x, y)]
        batch = batch_for_shader(shader, "LINE_STRIP", {"pos": points})
    else:
        points = [(x, y), (x + width, y), (x + width, y + height), (x, y + height)]
        batch = batch_for_shader(shader, "TRI_FAN", {"pos": points})
    gpu.state.blend_set("ALPHA")
    shader.bind()
    shader.uniform_float("color", color)
    batch.draw(shader)
    gpu.state.blend_set("NONE")


def register_stage_analysis_properties():
    scene_type = bpy.types.Scene
    if not hasattr(scene_type, "level_stage_analysis_enable_overlay"):
        scene_type.level_stage_analysis_enable_overlay = bpy.props.BoolProperty(default=False)

    if not hasattr(scene_type, "level_stage_analysis_enable_camera_preview"):
        scene_type.level_stage_analysis_enable_camera_preview = bpy.props.BoolProperty(default=False)
    if not hasattr(scene_type, "level_stage_analysis_camera_preview_position"):
        scene_type.level_stage_analysis_camera_preview_position = bpy.props.EnumProperty(items=_POSITION_ITEMS, default="TOP_RIGHT")
    if not hasattr(scene_type, "level_stage_analysis_camera_preview_width"):
        scene_type.level_stage_analysis_camera_preview_width = bpy.props.IntProperty(default=420, min=240, max=960)
    if not hasattr(scene_type, "level_stage_analysis_camera_preview_height"):
        scene_type.level_stage_analysis_camera_preview_height = bpy.props.IntProperty(default=236, min=135, max=540)
    if not hasattr(scene_type, "level_stage_analysis_camera_preview_alpha"):
        scene_type.level_stage_analysis_camera_preview_alpha = bpy.props.FloatProperty(default=1.0, min=0.1, max=1.0)
    if not hasattr(scene_type, "level_stage_analysis_camera_preview_update_interval"):
        scene_type.level_stage_analysis_camera_preview_update_interval = bpy.props.FloatProperty(default=0.1, min=0.03, max=2.0)
    if not hasattr(scene_type, "level_stage_analysis_preview_camera_name"):
        scene_type.level_stage_analysis_preview_camera_name = bpy.props.StringProperty(default="")
    if not hasattr(scene_type, "level_stage_analysis_use_scene_camera"):
        scene_type.level_stage_analysis_use_scene_camera = bpy.props.BoolProperty(default=True)
    if not hasattr(scene_type, "level_stage_analysis_hide_grid_in_preview"):
        scene_type.level_stage_analysis_hide_grid_in_preview = bpy.props.BoolProperty(default=True)
    if not hasattr(scene_type, "level_stage_analysis_hide_gizmo_in_preview"):
        scene_type.level_stage_analysis_hide_gizmo_in_preview = bpy.props.BoolProperty(default=True)

    if not hasattr(scene_type, "level_stage_analysis_enable_parameter_panel"):
        scene_type.level_stage_analysis_enable_parameter_panel = bpy.props.BoolProperty(default=False)
    if not hasattr(scene_type, "level_stage_analysis_parameter_panel_position"):
        scene_type.level_stage_analysis_parameter_panel_position = bpy.props.EnumProperty(items=_POSITION_ITEMS, default="BOTTOM_RIGHT")
    if not hasattr(scene_type, "level_stage_analysis_parameter_panel_scale"):
        scene_type.level_stage_analysis_parameter_panel_scale = bpy.props.FloatProperty(default=1.15, min=0.6, max=2.0)
    if not hasattr(scene_type, "level_stage_analysis_parameter_panel_alpha"):
        scene_type.level_stage_analysis_parameter_panel_alpha = bpy.props.FloatProperty(default=0.85, min=0.05, max=1.0)
    if not hasattr(scene_type, "level_stage_analysis_show_selected_object_info"):
        scene_type.level_stage_analysis_show_selected_object_info = bpy.props.BoolProperty(default=True)

    if not hasattr(scene_type, "level_stage_analysis_overlay_position"):
        scene_type.level_stage_analysis_overlay_position = bpy.props.EnumProperty(items=_POSITION_ITEMS, default="TOP_RIGHT")
    if not hasattr(scene_type, "level_stage_analysis_overlay_scale"):
        scene_type.level_stage_analysis_overlay_scale = bpy.props.FloatProperty(default=1.5, min=0.6, max=2.0)
    if not hasattr(scene_type, "level_stage_analysis_overlay_alpha"):
        scene_type.level_stage_analysis_overlay_alpha = bpy.props.FloatProperty(default=0.85, min=0.05, max=1.0)

    if not hasattr(scene_type, "level_stage_analysis_show_visual_bars"):
        scene_type.level_stage_analysis_show_visual_bars = bpy.props.BoolProperty(default=True)

    if not hasattr(scene_type, "level_stage_analysis_show_brightness"):
        scene_type.level_stage_analysis_show_brightness = bpy.props.BoolProperty(default=True)
    if not hasattr(scene_type, "level_stage_analysis_auto_update_brightness"):
        scene_type.level_stage_analysis_auto_update_brightness = bpy.props.BoolProperty(default=False)
    if not hasattr(scene_type, "level_stage_analysis_brightness_update_interval"):
        scene_type.level_stage_analysis_brightness_update_interval = bpy.props.FloatProperty(default=2.0, min=0.2, max=30.0)
    if not hasattr(scene_type, "level_stage_analysis_target_brightness_min"):
        scene_type.level_stage_analysis_target_brightness_min = bpy.props.FloatProperty(default=0.35, min=0.0, max=1.0)
    if not hasattr(scene_type, "level_stage_analysis_target_brightness_max"):
        scene_type.level_stage_analysis_target_brightness_max = bpy.props.FloatProperty(default=0.75, min=0.0, max=1.0)
    if not hasattr(scene_type, "level_stage_analysis_too_dark_threshold"):
        scene_type.level_stage_analysis_too_dark_threshold = bpy.props.FloatProperty(default=0.25, min=0.0, max=1.0)
    if not hasattr(scene_type, "level_stage_analysis_too_bright_threshold"):
        scene_type.level_stage_analysis_too_bright_threshold = bpy.props.FloatProperty(default=0.85, min=0.0, max=1.0)

    if not hasattr(scene_type, "level_stage_analysis_show_composition"):
        scene_type.level_stage_analysis_show_composition = bpy.props.BoolProperty(default=True)
    if not hasattr(scene_type, "level_stage_analysis_stage_section_count"):
        scene_type.level_stage_analysis_stage_section_count = bpy.props.IntProperty(default=3, min=1, max=3)
    if not hasattr(scene_type, "level_stage_analysis_section_1_label"):
        scene_type.level_stage_analysis_section_1_label = bpy.props.StringProperty(default="Intro")
    if not hasattr(scene_type, "level_stage_analysis_section_2_label"):
        scene_type.level_stage_analysis_section_2_label = bpy.props.StringProperty(default="Peak")
    if not hasattr(scene_type, "level_stage_analysis_section_3_label"):
        scene_type.level_stage_analysis_section_3_label = bpy.props.StringProperty(default="Exit")
    if not hasattr(scene_type, "level_stage_analysis_width_flow"):
        scene_type.level_stage_analysis_width_flow = bpy.props.StringProperty(default="Wide-Narrow-Wide")
    if not hasattr(scene_type, "level_stage_analysis_intensity_curve"):
        scene_type.level_stage_analysis_intensity_curve = bpy.props.EnumProperty(items=_CURVE_ITEMS, default="PeakAtMiddle")
    if not hasattr(scene_type, "level_stage_analysis_show_mini_graph"):
        scene_type.level_stage_analysis_show_mini_graph = bpy.props.BoolProperty(default=True)

    if not hasattr(scene_type, "level_stage_analysis_show_perception"):
        scene_type.level_stage_analysis_show_perception = bpy.props.BoolProperty(default=True)
    if not hasattr(scene_type, "level_stage_analysis_show_visible_important_count"):
        scene_type.level_stage_analysis_show_visible_important_count = bpy.props.BoolProperty(default=True)
    if not hasattr(scene_type, "level_stage_analysis_show_offscreen_warning"):
        scene_type.level_stage_analysis_show_offscreen_warning = bpy.props.BoolProperty(default=True)
    if not hasattr(scene_type, "level_stage_analysis_show_too_small_warning"):
        scene_type.level_stage_analysis_show_too_small_warning = bpy.props.BoolProperty(default=True)


def unregister_stage_analysis_properties():
    for name in (
        "level_stage_analysis_enable_overlay",
        "level_stage_analysis_enable_camera_preview",
        "level_stage_analysis_camera_preview_position",
        "level_stage_analysis_camera_preview_width",
        "level_stage_analysis_camera_preview_height",
        "level_stage_analysis_camera_preview_alpha",
        "level_stage_analysis_camera_preview_update_interval",
        "level_stage_analysis_preview_camera_name",
        "level_stage_analysis_use_scene_camera",
        "level_stage_analysis_hide_grid_in_preview",
        "level_stage_analysis_hide_gizmo_in_preview",
        "level_stage_analysis_enable_parameter_panel",
        "level_stage_analysis_parameter_panel_position",
        "level_stage_analysis_parameter_panel_scale",
        "level_stage_analysis_parameter_panel_alpha",
        "level_stage_analysis_show_selected_object_info",
        "level_stage_analysis_overlay_position",
        "level_stage_analysis_overlay_scale",
        "level_stage_analysis_overlay_alpha",
        "level_stage_analysis_show_visual_bars",
        "level_stage_analysis_show_brightness",
        "level_stage_analysis_auto_update_brightness",
        "level_stage_analysis_brightness_update_interval",
        "level_stage_analysis_target_brightness_min",
        "level_stage_analysis_target_brightness_max",
        "level_stage_analysis_too_dark_threshold",
        "level_stage_analysis_too_bright_threshold",
        "level_stage_analysis_show_composition",
        "level_stage_analysis_stage_section_count",
        "level_stage_analysis_section_1_label",
        "level_stage_analysis_section_2_label",
        "level_stage_analysis_section_3_label",
        "level_stage_analysis_width_flow",
        "level_stage_analysis_intensity_curve",
        "level_stage_analysis_show_mini_graph",
        "level_stage_analysis_show_perception",
        "level_stage_analysis_show_visible_important_count",
        "level_stage_analysis_show_offscreen_warning",
        "level_stage_analysis_show_too_small_warning",
    ):
        if hasattr(bpy.types.Scene, name):
            delattr(bpy.types.Scene, name)
