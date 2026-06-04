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
        json_object_root = {
            "name": "scene",
            "objects": [],
        }

        for obj in bpy.context.scene.objects:
            if obj.parent:
                continue
            self.parse_scene_recursive_json(json_object_root["objects"], obj)

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