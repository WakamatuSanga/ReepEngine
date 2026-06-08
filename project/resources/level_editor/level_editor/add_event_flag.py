import bpy


class MYADDON_OT_add_event_flag_properties(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_add_event_flag_properties"
    bl_label = "Add Event Flag Properties"
    bl_description = "Add EventFlag custom properties to the selected object"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        obj = context.object
        if obj is None:
            self.report({'WARNING'}, "No active object selected")
            return {'CANCELLED'}

        object_id = obj.get("object_id", obj.name)
        flag_id = obj.get("event_flag_id", object_id)

        obj["object_id"] = object_id
        obj["is_event_flag"] = True
        obj["event_flag_id"] = flag_id
        obj["event_display_name"] = obj.get("event_display_name", obj.name)
        obj["event_description"] = obj.get("event_description", "")
        obj["event_trigger_type"] = obj.get("event_trigger_type", "Enter")
        obj["event_shape_type"] = obj.get("event_shape_type", "Box")
        obj["event_one_shot"] = obj.get("event_one_shot", True)
        obj["event_initially_enabled"] = obj.get("event_initially_enabled", True)
        obj["event_visible_in_editor"] = obj.get("event_visible_in_editor", True)
        obj["event_next_flag_ids"] = obj.get("event_next_flag_ids", "")
        obj["event_object_actions"] = obj.get("event_object_actions", "[]")

        self.report({'INFO'}, f"EventFlag properties added: {flag_id}")
        return {'FINISHED'}
