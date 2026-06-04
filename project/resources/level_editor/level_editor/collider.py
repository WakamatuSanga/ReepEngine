import bpy
from .add_collider import MYADDON_OT_add_collider


class OBJECT_PT_collider(bpy.types.Panel):
    bl_label = "Collider"
    bl_idname = "OBJECT_PT_collider"
    bl_space_type = "PROPERTIES"
    bl_region_type = "WINDOW"
    bl_context = "object"

    def draw(self, context):
        layout = self.layout
        if context.object is not None and "collider" in context.object:
            layout.prop(context.object, '["collider"]', text="Type")
            layout.prop(context.object, '["collider_center"]', text="Center")
            layout.prop(context.object, '["collider_size"]', text="Size")
        else:
            layout.operator(MYADDON_OT_add_collider.bl_idname)
