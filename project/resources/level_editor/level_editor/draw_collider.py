import bpy
import gpu
import mathutils
from gpu_extras.batch import batch_for_shader


class DrawCollider:
    handle = None

    @staticmethod
    def draw_collider():
        vertices = {"pos": []}
        indices = []

        offsets = [
            (-0.5, -0.5, -0.5),
            (0.5, -0.5, -0.5),
            (-0.5, 0.5, -0.5),
            (0.5, 0.5, -0.5),
            (-0.5, -0.5, 0.5),
            (0.5, -0.5, 0.5),
            (-0.5, 0.5, 0.5),
            (0.5, 0.5, 0.5),
        ]

        for obj in bpy.context.scene.objects:
            if "collider" not in obj:
                continue

            center = mathutils.Vector((0.0, 0.0, 0.0))
            size = mathutils.Vector((2.0, 2.0, 2.0))
            if "collider_center" in obj:
                center = mathutils.Vector(obj["collider_center"])
            if "collider_size" in obj:
                size = mathutils.Vector(obj["collider_size"])

            start = len(vertices["pos"])
            for offset in offsets:
                pos = center + mathutils.Vector(offset) * size
                pos = obj.matrix_world @ pos
                vertices["pos"].append(pos)

            indices.extend([
                (start + 0, start + 1),
                (start + 1, start + 3),
                (start + 3, start + 2),
                (start + 2, start + 0),
                (start + 4, start + 5),
                (start + 5, start + 7),
                (start + 7, start + 6),
                (start + 6, start + 4),
                (start + 0, start + 4),
                (start + 1, start + 5),
                (start + 2, start + 6),
                (start + 3, start + 7),
            ])

        shader = gpu.shader.from_builtin('UNIFORM_COLOR')
        batch = batch_for_shader(shader, 'LINES', vertices, indices=indices)
        shader.bind()
        shader.uniform_float('color', (0.5, 1.0, 1.0, 1.0))
        batch.draw(shader)
