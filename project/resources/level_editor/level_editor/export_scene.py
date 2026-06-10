import bpy
import json
import math
import os
import mathutils
from bpy_extras.io_utils import ExportHelper


def write_and_print(file, text):
    print(text)
    file.write(text)


def parse_scene_recursive(file, object, level):
    indent = "    " * level
    write_and_print(file, indent + object.type + " - " + object.name + "\n")

    trans, rot_quat, scale = object.matrix_local.decompose()
    rot = rot_quat.to_euler()
    rot.x = math.degrees(rot.x)
    rot.y = math.degrees(rot.y)
    rot.z = math.degrees(rot.z)

    write_and_print(file, indent + "Trans(%f,%f,%f)\n" % (trans.x, trans.y, trans.z))
    write_and_print(file, indent + "Rot(%f,%f,%f)\n" % (rot.x, rot.y, rot.z))
    write_and_print(file, indent + "Scale(%f,%f,%f)\n" % (scale.x, scale.y, scale.z))

    if "file_name" in object:
        write_and_print(file, indent + "N %s\n" % object["file_name"])

    if "collider" in object:
        write_and_print(file, indent + "C %s\n" % object["collider"])
        if "collider_center" in object:
            cc = object["collider_center"]
            write_and_print(file, indent + "CC %f %f %f\n" % (cc[0], cc[1], cc[2]))
        if "collider_size" in object:
            cs = object["collider_size"]
            write_and_print(file, indent + "CS %f %f %f\n" % (cs[0], cs[1], cs[2]))

    write_and_print(file, indent + "END\n")

    for child in object.children:
        parse_scene_recursive(file, child, level + 1)


class WM_OT_level_export(bpy.types.Operator):
    bl_idname = "wm.level_export"
    bl_label = "Level Export"
    bl_description = "レベルデータをJSON形式で書き出します"

    def execute(self, context):
        print("レベルエクスポートを実行します")
        data_list = []
        for obj in bpy.context.scene.objects:
            data = {
                "name": obj.name,
                "type": obj.type,
                "location": {
                    "x": obj.location.x,
                    "y": obj.location.y,
                    "z": obj.location.z,
                },
            }
            data_list.append(data)

        save_path = "C:/Users/K024G/Desktop/level_data/level_data.json"
        save_dir = os.path.dirname(save_path)
        if save_dir and not os.path.exists(save_dir):
            os.makedirs(save_dir, exist_ok=True)
        with open(save_path, 'w', encoding='utf-8') as f:
            json.dump(data_list, f, indent=4)

        self.report({'INFO'}, f"保存完了: {save_path}")
        return {'FINISHED'}


def _to_string(value, default=""):
    if value is None:
        return default
    return str(value)


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
        "targetObjectId": _to_string(
            action.get("targetObjectId", action.get("target_object_id", ""))),
        "targetObjectName": _to_string(
            action.get("targetObjectName", action.get("target_object_name", ""))),
        "actionType": _to_string(
            action.get("actionType", action.get("action_type", ""))),
        "actionDescription": _to_string(
            action.get("actionDescription", action.get("action_description", ""))),
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
                print(f"[LevelEditor] event_object_actions JSON parse failed: {error}")

        actions = []
        for item in text.split(";"):
            parts = [part.strip() for part in item.split("|")]
            if not parts or not parts[0]:
                continue
            action = {
                "targetObjectName": parts[0],
                "actionType": parts[1] if len(parts) > 1 else "",
                "actionDescription": parts[2] if len(parts) > 2 else "",
            }
            actions.append(action)
        return actions

    if isinstance(value, list):
        return [
            action for action in (_normalize_action(item) for item in value)
            if action is not None
        ]

    return []


def _custom_string(object, key, default=""):
    if key not in object:
        return default
    return _to_string(object[key], default)


def _custom_float(object, key, default=0.0):
    if key not in object:
        return default
    try:
        return float(object[key])
    except (TypeError, ValueError):
        return default


def _custom_int(object, key, default=0):
    if key not in object:
        return default
    try:
        return int(object[key])
    except (TypeError, ValueError):
        return default


def _safe_id(text, prefix):
    safe = "".join(ch if ch.isalnum() or ch == "_" else "_" for ch in str(text))
    safe = safe.strip("_")
    return f"{prefix}_{safe}" if safe else prefix


def _infer_primitive_shape(object):
    if "primitive_shape" in object:
        return _custom_string(object, "primitive_shape")
    if object.type == "EMPTY":
        return "Empty"
    if object.type != "MESH":
        return ""

    names = [object.name.lower()]
    if object.data:
        names.append(object.data.name.lower())
    joined_name = " ".join(names)
    if "plane" in joined_name:
        return "Plane"
    if "sphere" in joined_name or "ico" in joined_name:
        return "Sphere"
    if "cube" in joined_name or "box" in joined_name:
        return "Cube"
    return ""


def _append_editor_properties(json_object, object):
    if "object_id" in object:
        json_object["object_id"] = _custom_string(object, "object_id")
    if "editor_label" in object:
        json_object["editor_label"] = _custom_string(object, "editor_label")
    if "editor_description" in object:
        json_object["editor_description"] = _custom_string(object, "editor_description")
    primitive_shape = _infer_primitive_shape(object)
    if primitive_shape:
        json_object["primitive_shape"] = primitive_shape


def _vector_to_json(vector):
    return {
        "x": float(vector.x),
        "y": float(vector.y),
        "z": float(vector.z),
    }


def _edge_key(a, b):
    return (a, b) if a < b else (b, a)


def _trace_ordered_mesh_points(vertices, edges):
    if not vertices:
        return []
    if not edges:
        return vertices

    adjacency = {index: [] for index in range(len(vertices))}
    unused_edges = set()
    for edge in edges:
        a, b = int(edge[0]), int(edge[1])
        if a == b:
            continue
        adjacency.setdefault(a, []).append(b)
        adjacency.setdefault(b, []).append(a)
        unused_edges.add(_edge_key(a, b))

    ordered = []
    while unused_edges:
        component_vertices = {index for edge in unused_edges for index in edge}
        endpoints = [index for index in component_vertices if len(adjacency.get(index, [])) <= 1]
        current = min(endpoints) if endpoints else min(component_vertices)
        previous = None
        path = [current]

        while True:
            candidates = [
                index for index in adjacency.get(current, [])
                if _edge_key(current, index) in unused_edges
            ]
            if not candidates:
                break
            if previous in candidates and len(candidates) > 1:
                candidates.remove(previous)
            next_index = candidates[0]
            unused_edges.discard(_edge_key(current, next_index))
            if next_index == path[0]:
                break
            path.append(next_index)
            previous, current = current, next_index

        ordered.extend(vertices[index] for index in path)

    return ordered


def _distance_between(a, b):
    return (a - b).length


def _resample_points(points, sample_count):
    if len(points) <= 2 or len(points) <= sample_count:
        return points

    distances = [0.0]
    total_length = 0.0
    for index in range(1, len(points)):
        total_length += _distance_between(points[index - 1], points[index])
        distances.append(total_length)

    if total_length <= 0.00001:
        return [points[0], points[-1]]

    result = []
    for sample_index in range(sample_count):
        target = total_length * sample_index / max(sample_count - 1, 1)
        segment_index = 1
        while segment_index < len(distances) and distances[segment_index] < target:
            segment_index += 1
        if segment_index >= len(points):
            result.append(points[-1])
            continue
        prev_distance = distances[segment_index - 1]
        next_distance = distances[segment_index]
        ratio = 0.0 if next_distance <= prev_distance else (target - prev_distance) / (next_distance - prev_distance)
        result.append(points[segment_index - 1].lerp(points[segment_index], ratio))
    return result


def _sample_curve_control_points(object):
    points = []
    for spline in object.data.splines:
        if spline.type == "BEZIER":
            for point in spline.bezier_points:
                points.append(object.matrix_world @ point.co)
        else:
            for point in spline.points:
                points.append(object.matrix_world @ mathutils.Vector((point.co.x, point.co.y, point.co.z)))
    return points


def _sample_curve_points(object, sample_count):
    depsgraph = bpy.context.evaluated_depsgraph_get()
    evaluated_object = object.evaluated_get(depsgraph)
    mesh = None
    try:
        mesh = evaluated_object.to_mesh()
        if mesh:
            vertices = [evaluated_object.matrix_world @ vertex.co for vertex in mesh.vertices]
            edges = [(edge.vertices[0], edge.vertices[1]) for edge in mesh.edges]
            points = _trace_ordered_mesh_points(vertices, edges)
            if points:
                return _resample_points(points, sample_count)
    except RuntimeError as error:
        print(f"[LevelEditor] Curve rail sampling failed for {object.name}: {error}")
    finally:
        if mesh:
            evaluated_object.to_mesh_clear()

    return _resample_points(_sample_curve_control_points(object), sample_count)


def _is_curve_cyclic(object):
    if object.type != "CURVE" or not object.data:
        return False
    return any(getattr(spline, "use_cyclic_u", False) for spline in object.data.splines)


def _build_rail_json(object):
    sample_count = max(2, min(_custom_int(object, "rail_sample_count", 64), 128))
    points = _sample_curve_points(object, sample_count)
    if len(points) < 2:
        return None

    rail_id = _custom_string(object, "rail_id", _safe_id(object.name, "rail"))
    return {
        "rail_id": rail_id,
        "name": _custom_string(object, "rail_name", object.name),
        "rail_type": _custom_string(object, "rail_type", "Curve"),
        "loop": _to_bool(object["rail_loop"], _is_curve_cyclic(object)) if "rail_loop" in object else _is_curve_cyclic(object),
        "speed": _custom_float(object, "rail_speed", 1.0),
        "visibleInEditor": _to_bool(object["rail_visible_in_editor"], True)
        if "rail_visible_in_editor" in object else True,
        "points": [_vector_to_json(point) for point in points],
    }


def build_rails_json():
    rails = []
    for obj in bpy.context.scene.objects:
        if obj.type != "CURVE":
            continue
        rail = _build_rail_json(obj)
        if rail:
            rails.append(rail)
    return rails


def _append_event_flag_properties(json_object, object, transform_json, collider_json):
    has_event_property = "is_event_flag" in object or "event_flag_id" in object
    is_event_flag = _to_bool(object["is_event_flag"], False) if "is_event_flag" in object else False
    if not is_event_flag and not has_event_property:
        return

    object_id = _custom_string(object, "object_id", object.name)
    flag_id = _custom_string(object, "event_flag_id", object_id)
    display_name = _custom_string(object, "event_display_name", object.name)
    description = _custom_string(object, "event_description")
    trigger_type = _custom_string(object, "event_trigger_type", "PlayerEnter")
    shape_type = _custom_string(object, "event_shape_type", collider_json.get("type", "Box"))
    one_shot = _to_bool(object["event_one_shot"], True) if "event_one_shot" in object else True
    initially_enabled = (
        _to_bool(object["event_initially_enabled"], True)
        if "event_initially_enabled" in object else True
    )
    visible_in_editor = (
        _to_bool(object["event_visible_in_editor"], True)
        if "event_visible_in_editor" in object else True
    )

    json_object["object_id"] = object_id
    json_object["is_event_flag"] = True
    json_object["event_flag_id"] = flag_id
    json_object["event_flag"] = {
        "id": flag_id,
        "displayName": display_name,
        "description": description,
        "triggerType": trigger_type,
        "shapeType": shape_type,
        "position": transform_json["translation"],
        "rotation": transform_json["rotation"],
        "scale": transform_json["scaling"],
        "size": collider_json.get("size", [1.0, 1.0, 1.0]),
        "oneShot": one_shot,
        "initiallyEnabled": initially_enabled,
        "visibleInEditor": visible_in_editor,
        "nextFlagIds": _split_csv(object["event_next_flag_ids"])
        if "event_next_flag_ids" in object else [],
        "objectActions": _parse_event_object_actions(object["event_object_actions"])
        if "event_object_actions" in object else [],
    }


def parse_scene_recursive_json_object(data_parent, object):
    json_object = {
        "type": object.type,
        "name": object.name,
    }

    trans, rot_quat, scale = object.matrix_local.decompose()
    rot = rot_quat.to_euler()
    rot.x = math.degrees(rot.x)
    rot.y = math.degrees(rot.y)
    rot.z = math.degrees(rot.z)

    transform_json = {
        "translation": [trans.x, trans.y, trans.z],
        "rotation": [rot.x, rot.y, rot.z],
        "scaling": [scale.x, scale.y, scale.z],
    }
    json_object["transform"] = transform_json

    if "file_name" in object:
        json_object["file_name"] = object["file_name"]

    collider_json = {}
    if "collider" in object:
        collider = {"type": object["collider"]}
        if "collider_center" in object:
            collider["center"] = list(object["collider_center"])
        if "collider_size" in object:
            collider["size"] = list(object["collider_size"])
        json_object["collider"] = collider
        collider_json = collider

    _append_editor_properties(json_object, object)
    _append_event_flag_properties(json_object, object, transform_json, collider_json)

    if object.children:
        children = []
        for child in object.children:
            parse_scene_recursive_json_object(children, child)
        json_object["children"] = children

    data_parent.append(json_object)


def build_scene_json():
    json_object_root = {
        "name": "scene",
        "objects": [],
        "rails": build_rails_json(),
    }

    for obj in bpy.context.scene.objects:
        if obj.parent:
            continue
        parse_scene_recursive_json_object(json_object_root["objects"], obj)

    return json_object_root


class MYADDON_OT_export_scene(bpy.types.Operator, ExportHelper):
    bl_idname = "myaddon.myaddon_ot_export_scene"
    bl_label = "シーン出力"
    bl_description = "シーン情報をExportします"

    filename_ext = ".json"
    filter_glob: bpy.props.StringProperty(
        default="*.json",
        options={'HIDDEN'}
    )

    def export_json(self):
        json_object_root = build_scene_json()
        json_text = json.dumps(json_object_root, ensure_ascii=False, indent=4)
        with open(self.filepath, 'w', encoding='utf-8') as file:
            file.write(json_text)

    def parse_scene_recursive_json(self, data_parent, object):
        json_object = {
            "type": object.type,
            "name": object.name,
        }

        trans, rot_quat, scale = object.matrix_local.decompose()
        rot = rot_quat.to_euler()
        rot.x = math.degrees(rot.x)
        rot.y = math.degrees(rot.y)
        rot.z = math.degrees(rot.z)

        json_object["transform"] = {
            "translation": [trans.x, trans.y, trans.z],
            "rotation": [rot.x, rot.y, rot.z],
            "scaling": [scale.x, scale.y, scale.z],
        }

        if "file_name" in object:
            json_object["file_name"] = object["file_name"]

        if "collider" in object:
            collider = {"type": object["collider"]}
            if "collider_center" in object:
                collider["center"] = list(object["collider_center"])
            if "collider_size" in object:
                collider["size"] = list(object["collider_size"])
            json_object["collider"] = collider

        if object.children:
            children = []
            for child in object.children:
                self.parse_scene_recursive_json(children, child)
            json_object["children"] = children

        data_parent.append(json_object)

    def execute(self, context):
        print("シーン情報をExportします")
        save_dir = os.path.dirname(self.filepath)
        if save_dir and not os.path.exists(save_dir):
            os.makedirs(save_dir, exist_ok=True)
        self.export_json()
        self.report({'INFO'}, "シーン情報をExportしました")
        print("シーン情報をExportしました")
        return {'FINISHED'}
''
