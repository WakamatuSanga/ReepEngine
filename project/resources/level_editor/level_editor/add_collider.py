import bpy
import mathutils


class MYADDON_OT_add_collider(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_add_collider"
    bl_label = "コライダー追加"
    bl_description = "オブジェクトにコライダー用のカスタムプロパティを追加します"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        if context.object is not None:
            context.object["collider"] = "BOX"
            context.object["collider_center"] = mathutils.Vector((0.0, 0.0, 0.0))
            context.object["collider_size"] = mathutils.Vector((2.0, 2.0, 2.0))
        return {'FINISHED'}
