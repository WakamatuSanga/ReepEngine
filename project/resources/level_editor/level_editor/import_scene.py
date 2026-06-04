import bpy
import json
import math
import mathutils
from bpy_extras.io_utils import ImportHelper


class MYADDON_OT_import_scene(bpy.types.Operator, ImportHelper):
    bl_idname = "myaddon.myaddon_ot_import_scene"
    bl_label = "シーン読込"
    bl_description = "JSON形式のシーン情報を読み込みます"

    filename_ext = ".json"
    filter_glob: bpy.props.StringProperty(
        default="*.json",
        options={'HIDDEN'}
    )

    def execute(self, context):
        try:
            self.import_json()
        except Exception as e:
            self.report({'ERROR'}, f"読み込みに失敗しました: {e}")
            return {'CANCELLED'}

        self.report({'INFO'}, "シーン情報をImportしました")
        print("シーン情報をImportしました")
        return {'FINISHED'}

    def import_json(self):
        with open(self.filepath, 'r', encoding='utf-8') as file:
            data = json.load(file)

        if not isinstance(data, dict) or "objects" not in data:
            raise ValueError("JSON形式が不正です。scene.objects を含む必要があります。")

        for obj_data in data["objects"]:
            self.create_object_recursive(obj_data, None)

    def create_object_recursive(self, obj_data, parent):
        name = obj_data.get("name", "Object")
        obj_type = obj_data.get("type", "EMPTY")

        new_obj = bpy.data.objects.new(name, None)
        if obj_type == "MESH":
            new_obj.empty_display_type = 'CUBE'
            new_obj.empty_display_size = 1.0

        bpy.context.scene.collection.objects.link(new_obj)
        if parent is not None:
            new_obj.parent = parent

        transform = obj_data.get("transform", {})
        translation = transform.get("translation", [0.0, 0.0, 0.0])
        rotation = transform.get("rotation", [0.0, 0.0, 0.0])
        scaling = transform.get("scaling", [1.0, 1.0, 1.0])

        new_obj.location = translation
        new_obj.rotation_euler = [
            math.radians(rotation[0]),
            math.radians(rotation[1]),
            math.radians(rotation[2]),
        ]
        new_obj.scale = scaling

        if "file_name" in obj_data:
            new_obj["file_name"] = obj_data["file_name"]

        if "collider" in obj_data:
            collider = obj_data.get("collider", {})
            new_obj["collider"] = collider.get("type", "BOX")
            if "center" in collider:
                new_obj["collider_center"] = mathutils.Vector(collider["center"])
            if "size" in collider:
                new_obj["collider_size"] = mathutils.Vector(collider["size"])

        for child_data in obj_data.get("children", []):
            self.create_object_recursive(child_data, new_obj)
