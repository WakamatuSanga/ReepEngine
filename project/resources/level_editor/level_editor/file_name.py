import bpy
from .add_filename import MYADDON_OT_add_filename


class OBJECT_PT_file_name(bpy.types.Panel):
    bl_label = "FileName"
    bl_idname = "OBJECT_PT_file_name"
    bl_space_type = "PROPERTIES"
    bl_region_type = "WINDOW"
    bl_context = "object"

    def draw(self, context):
        layout = self.layout
        if context.object is not None and "file_name" in context.object:
            layout.prop(context.object, '["file_name"]', text="FileName")
        else:
            layout.operator(MYADDON_OT_add_filename.bl_idname)
