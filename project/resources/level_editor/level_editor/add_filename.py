import bpy


class MYADDON_OT_add_filename(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_add_filename"
    bl_label = "ファイル名を追加"
    bl_description = "オブジェクトにファイル名というカスタムプロパティを追加します"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        if context.object is not None:
            context.object["file_name"] = ""
        return {'FINISHED'}
