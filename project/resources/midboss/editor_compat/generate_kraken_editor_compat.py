"""Build the lossless single-primitive Kraken Skinning Editor preview asset.

Run with Blender 4.4.x:
  blender --background --python generate_kraken_editor_compat.py
  blender --background --python generate_kraken_editor_compat.py -- --validate-only
"""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import math
import os
from pathlib import Path
import struct
import sys
from typing import Any

try:
    import bpy  # type: ignore
except ModuleNotFoundError:
    bpy = None


TOLERANCE = 1.0e-5
SHEAR_TOLERANCE = 1.0e-6
EXPECTED_ROOT = "Kraken_Tentacle_Rig_Root"
EXPECTED_JOINTS = 41
EXPECTED_VERTICES = 11_648
EXPECTED_INDICES = 52_752
COMPONENT_FORMATS = {
    5120: "b",
    5121: "B",
    5122: "h",
    5123: "H",
    5125: "I",
    5126: "f",
}
TYPE_COMPONENTS = {
    "SCALAR": 1,
    "VEC2": 2,
    "VEC3": 3,
    "VEC4": 4,
    "MAT2": 4,
    "MAT3": 9,
    "MAT4": 16,
}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def dot(left: list[float], right: list[float]) -> float:
    return sum(a * b for a, b in zip(left, right))


def cross(left: list[float], right: list[float]) -> list[float]:
    return [
        left[1] * right[2] - left[2] * right[1],
        left[2] * right[0] - left[0] * right[2],
        left[0] * right[1] - left[1] * right[0],
    ]


def rotation_matrix_to_quaternion(rotation: list[list[float]]) -> list[float]:
    trace = rotation[0][0] + rotation[1][1] + rotation[2][2]
    if trace > 0.0:
        factor = math.sqrt(trace + 1.0) * 2.0
        quaternion = [
            (rotation[2][1] - rotation[1][2]) / factor,
            (rotation[0][2] - rotation[2][0]) / factor,
            (rotation[1][0] - rotation[0][1]) / factor,
            0.25 * factor,
        ]
    elif rotation[0][0] > rotation[1][1] and rotation[0][0] > rotation[2][2]:
        factor = math.sqrt(1.0 + rotation[0][0] - rotation[1][1] - rotation[2][2]) * 2.0
        quaternion = [
            0.25 * factor,
            (rotation[0][1] + rotation[1][0]) / factor,
            (rotation[0][2] + rotation[2][0]) / factor,
            (rotation[2][1] - rotation[1][2]) / factor,
        ]
    elif rotation[1][1] > rotation[2][2]:
        factor = math.sqrt(1.0 + rotation[1][1] - rotation[0][0] - rotation[2][2]) * 2.0
        quaternion = [
            (rotation[0][1] + rotation[1][0]) / factor,
            0.25 * factor,
            (rotation[1][2] + rotation[2][1]) / factor,
            (rotation[0][2] - rotation[2][0]) / factor,
        ]
    else:
        factor = math.sqrt(1.0 + rotation[2][2] - rotation[0][0] - rotation[1][1]) * 2.0
        quaternion = [
            (rotation[0][2] + rotation[2][0]) / factor,
            (rotation[1][2] + rotation[2][1]) / factor,
            0.25 * factor,
            (rotation[1][0] - rotation[0][1]) / factor,
        ]

    length = math.sqrt(sum(component * component for component in quaternion))
    require(length > 1.0e-12 and math.isfinite(length), "Quaternion normalization failed")
    return [component / length for component in quaternion]


def quaternion_to_rotation_matrix(quaternion: list[float]) -> list[list[float]]:
    x, y, z, w = quaternion
    xx, yy, zz = x * x, y * y, z * z
    xy, xz, yz = x * y, x * z, y * z
    wx, wy, wz = w * x, w * y, w * z
    return [
        [1.0 - 2.0 * (yy + zz), 2.0 * (xy - wz), 2.0 * (xz + wy)],
        [2.0 * (xy + wz), 1.0 - 2.0 * (xx + zz), 2.0 * (yz - wx)],
        [2.0 * (xz - wy), 2.0 * (yz + wx), 1.0 - 2.0 * (xx + yy)],
    ]


def matrix_to_trs(values: list[float], node_name: str) -> tuple[list[float], list[float], list[float], float]:
    require(len(values) == 16, f"{node_name}: matrix does not have 16 elements")
    require(all(math.isfinite(value) for value in values), f"{node_name}: matrix has non-finite values")

    matrix = [[float(values[column * 4 + row]) for column in range(4)] for row in range(4)]
    require(
        max(abs(matrix[3][0]), abs(matrix[3][1]), abs(matrix[3][2]), abs(matrix[3][3] - 1.0)) <= TOLERANCE,
        f"{node_name}: matrix is not affine",
    )
    translation = [matrix[row][3] for row in range(3)]
    columns = [[matrix[row][column] for row in range(3)] for column in range(3)]
    scale = [math.sqrt(dot(column, column)) for column in columns]
    require(all(value > 1.0e-10 and math.isfinite(value) for value in scale), f"{node_name}: matrix scale is singular")
    rotation_columns = [
        [component / scale[column_index] for component in column]
        for column_index, column in enumerate(columns)
    ]

    shear = max(
        abs(dot(rotation_columns[0], rotation_columns[1])),
        abs(dot(rotation_columns[0], rotation_columns[2])),
        abs(dot(rotation_columns[1], rotation_columns[2])),
    )
    require(shear <= SHEAR_TOLERANCE, f"{node_name}: shear detected ({shear:.9g})")
    determinant = dot(rotation_columns[0], cross(rotation_columns[1], rotation_columns[2]))
    require(abs(determinant) > 1.0e-10, f"{node_name}: rotation basis is singular")
    if determinant < 0.0:
        reflected_axis = max(range(3), key=lambda index: abs(scale[index]))
        scale[reflected_axis] = -scale[reflected_axis]
        rotation_columns[reflected_axis] = [-component for component in rotation_columns[reflected_axis]]
        determinant = -determinant
    require(abs(determinant - 1.0) <= TOLERANCE, f"{node_name}: rotation determinant is {determinant:.9g}")

    rotation = [[rotation_columns[column][row] for column in range(3)] for row in range(3)]
    quaternion = rotation_matrix_to_quaternion(rotation)
    require(
        abs(sum(component * component for component in quaternion) - 1.0) <= TOLERANCE,
        f"{node_name}: quaternion is not normalized",
    )

    reconstructed_rotation = quaternion_to_rotation_matrix(quaternion)
    reconstructed = [[0.0] * 4 for _ in range(4)]
    for row in range(3):
        for column in range(3):
            reconstructed[row][column] = reconstructed_rotation[row][column] * scale[column]
        reconstructed[row][3] = translation[row]
    reconstructed[3][3] = 1.0
    reconstructed_values = [reconstructed[row][column] for column in range(4) for row in range(4)]
    max_error = max(abs(original - converted) for original, converted in zip(values, reconstructed_values))
    require(max_error <= TOLERANCE, f"{node_name}: TRS reconstruction error is {max_error:.9g}")
    return translation, quaternion, scale, max_error


def convert_node_matrices(document: dict[str, Any]) -> tuple[int, float]:
    converted_count = 0
    max_error = 0.0
    for node_index, node in enumerate(document["nodes"]):
        if "matrix" not in node:
            continue
        node_name = node.get("name", f"node_{node_index}")
        translation, rotation, scale, error = matrix_to_trs(node["matrix"], node_name)
        node["translation"] = translation
        node["rotation"] = rotation
        node["scale"] = scale
        del node["matrix"]
        converted_count += 1
        max_error = max(max_error, error)
    return converted_count, max_error


def read_accessor(document: dict[str, Any], binary: bytes, accessor_index: int) -> list[tuple[Any, ...]]:
    accessor = document["accessors"][accessor_index]
    require("sparse" not in accessor, f"Accessor {accessor_index}: sparse accessors are unsupported by this validator")
    view = document["bufferViews"][accessor["bufferView"]]
    component_format = COMPONENT_FORMATS[accessor["componentType"]]
    component_count = TYPE_COMPONENTS[accessor["type"]]
    component_size = struct.calcsize("<" + component_format)
    element_size = component_size * component_count
    stride = view.get("byteStride", element_size)
    offset = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
    require(stride >= element_size, f"Accessor {accessor_index}: invalid byte stride")
    require(
        offset + (accessor["count"] - 1) * stride + element_size <= len(binary),
        f"Accessor {accessor_index}: data exceeds the binary buffer",
    )
    unpack_format = "<" + component_format * component_count
    return [
        struct.unpack_from(unpack_format, binary, offset + element_index * stride)
        for element_index in range(accessor["count"])
    ]


def validate_hierarchy(document: dict[str, Any]) -> tuple[int, str]:
    skin = document["skins"][0]
    joints = skin["joints"]
    root_node_index = skin["skeleton"]
    require(len(joints) == EXPECTED_JOINTS, f"Expected {EXPECTED_JOINTS} joints, got {len(joints)}")
    require(joints[0] == root_node_index, "Root joint is not first in the skin joint order")
    root_name = document["nodes"][root_node_index].get("name", "")
    require(root_name == EXPECTED_ROOT, f"Unexpected root name: {root_name}")
    require(len(set(joints)) == len(joints), "Skin contains duplicate joint node indices")

    joint_set = set(joints)
    visited: set[int] = set()
    stack = [root_node_index]
    while stack:
        node_index = stack.pop()
        require(node_index not in visited, f"Cycle or duplicate hierarchy path at node {node_index}")
        visited.add(node_index)
        stack.extend(document["nodes"][node_index].get("children", []))
    require(joint_set.issubset(visited), "Not all skin joints are reachable from the skeleton root")

    parent_count = {joint: 0 for joint in joints}
    for parent_index, node in enumerate(document["nodes"]):
        for child_index in node.get("children", []):
            if child_index in parent_count and parent_index in joint_set:
                parent_count[child_index] += 1
    require(parent_count[root_node_index] == 0, "Root joint unexpectedly has a joint parent")
    require(
        all(parent_count[joint] == 1 for joint in joints[1:]),
        "A non-root joint does not have exactly one joint parent",
    )
    return len(joints), root_name

def validate_source_layout(document: dict[str, Any], binary: bytes) -> None:
    require(document.get("asset", {}).get("version") == "2.0", "Source is not glTF 2.0")
    require(len(document.get("meshes", [])) == 1, "Source must have exactly one mesh")
    primitives = document["meshes"][0]["primitives"]
    require(len(primitives) == 3, "Source must have exactly three primitives")
    require(len(document.get("materials", [])) == 3, "Source must have exactly three materials")
    require(len(document.get("textures", [])) == 1, "Source must have exactly one texture")
    require(len(document.get("images", [])) == 1, "Source must have exactly one image")
    require(len(document.get("nodes", [])) == 42, "Source must have exactly 42 nodes")
    require(len(document.get("skins", [])) == 1, "Source must have exactly one skin")
    require(len(document.get("animations", [])) == 0, "Source unexpectedly contains animations")
    require(len(document.get("buffers", [])) == 1, "Source must have exactly one buffer")
    require(document["buffers"][0]["byteLength"] == len(binary), "Source buffer byteLength mismatch")
    validate_hierarchy(document)

    first_attributes = primitives[0]["attributes"]
    require(all(primitive["attributes"] == first_attributes for primitive in primitives), "Primitive attributes differ")
    require([primitive["material"] for primitive in primitives] == [0, 1, 2], "Unexpected material assignment")
    index_accessors = [primitive["indices"] for primitive in primitives]
    require(index_accessors == [6, 7, 8], "Unexpected source index accessor layout")
    require(index_accessors == list(range(len(document["accessors"]) - 3, len(document["accessors"]))), "Index accessors are not last")
    index_views = [document["accessors"][index]["bufferView"] for index in index_accessors]
    require(index_views == [6, 7, 8], "Unexpected source index bufferView layout")
    require(index_views == list(range(len(document["bufferViews"]) - 3, len(document["bufferViews"]))), "Index views are not last")

    expected_offset = document["bufferViews"][index_views[0]].get("byteOffset", 0)
    total_count = 0
    for accessor_index, view_index in zip(index_accessors, index_views):
        accessor = document["accessors"][accessor_index]
        view = document["bufferViews"][view_index]
        require(accessor["componentType"] == 5125 and accessor["type"] == "SCALAR", "Indices are not uint32 scalars")
        require(accessor.get("byteOffset", 0) == 0 and "sparse" not in accessor, "Index accessor is not directly mergeable")
        require(view.get("byteOffset", 0) == expected_offset, "Index bufferViews are not contiguous")
        require(view["byteLength"] == accessor["count"] * 4, "Index bufferView has padding or truncation")
        expected_offset += view["byteLength"]
        total_count += accessor["count"]
    require(total_count == EXPECTED_INDICES, f"Unexpected source index count: {total_count}")
    require(expected_offset == len(binary), "Index bufferViews do not reach the end of the binary")

    texture_indices = []
    for material in document["materials"]:
        texture_indices.append(material["pbrMetallicRoughness"]["baseColorTexture"]["index"])
    require(texture_indices == [0, 0, 0], "Source materials do not share kraken_albedo texture 0")


def validate_compat(document: dict[str, Any], binary: bytes, gltf_path: Path) -> dict[str, Any]:
    require(document.get("asset", {}).get("version") == "2.0", "Output is not glTF 2.0")
    require(len(document.get("meshes", [])) == 1, "Output mesh count is not 1")
    require(len(document["meshes"][0]["primitives"]) == 1, "Output primitive count is not 1")
    require(len(document.get("materials", [])) == 1, "Output material count is not 1")
    require(document["materials"][0].get("name") == "KrakenSkin", "Output material is not KrakenSkin")
    require(len(document.get("textures", [])) == 1, "Output texture count is not 1")
    require(len(document.get("images", [])) == 1, "Output image count is not 1")
    require(len(document.get("nodes", [])) == 42, "Output node count is not 42")
    require(len(document.get("skins", [])) == 1, "Output skin count is not 1")
    require(len(document.get("animations", [])) == 0, "Output unexpectedly contains animations")
    require(len(document.get("buffers", [])) == 1, "Output buffer count is not 1")
    require(document["buffers"][0]["byteLength"] == len(binary), "Output buffer byteLength mismatch")
    joint_count, root_name = validate_hierarchy(document)

    matrix_count = sum("matrix" in node for node in document["nodes"])
    require(matrix_count == 0, f"Output still has {matrix_count} matrix nodes")
    trs_node_count = 0
    nonfinite_transform_count = 0
    for node in document["nodes"]:
        has_trs = any(key in node for key in ("translation", "rotation", "scale"))
        trs_node_count += int(has_trs)
        for key, expected_length in (("translation", 3), ("rotation", 4), ("scale", 3)):
            if key not in node:
                continue
            values = node[key]
            require(len(values) == expected_length, f"Node {node.get('name', '')}: invalid {key} length")
            nonfinite_transform_count += sum(not math.isfinite(value) for value in values)
        if "rotation" in node:
            quaternion_length = sum(value * value for value in node["rotation"])
            require(abs(quaternion_length - 1.0) <= TOLERANCE, f"Node {node.get('name', '')}: non-unit quaternion")
    require(nonfinite_transform_count == 0, "Output contains non-finite TRS values")

    primitive = document["meshes"][0]["primitives"][0]
    required_attributes = {"POSITION", "NORMAL", "TEXCOORD_0", "JOINTS_0", "WEIGHTS_0"}
    require(required_attributes.issubset(primitive["attributes"]), "Output is missing required vertex attributes")
    require(primitive["material"] == 0, "Output primitive does not use KrakenSkin material 0")
    attribute_accessors = primitive["attributes"]
    vertex_count = document["accessors"][attribute_accessors["POSITION"]]["count"]
    require(vertex_count == EXPECTED_VERTICES, f"Unexpected vertex count: {vertex_count}")
    require(
        all(document["accessors"][attribute_accessors[name]]["count"] == vertex_count for name in required_attributes),
        "Vertex attribute accessor counts differ",
    )
    index_accessor = document["accessors"][primitive["indices"]]
    require(index_accessor["count"] == EXPECTED_INDICES, f"Unexpected index count: {index_accessor['count']}")

    positions = read_accessor(document, binary, attribute_accessors["POSITION"])
    normals = read_accessor(document, binary, attribute_accessors["NORMAL"])
    texcoords = read_accessor(document, binary, attribute_accessors["TEXCOORD_0"])
    joints = read_accessor(document, binary, attribute_accessors["JOINTS_0"])
    weights = read_accessor(document, binary, attribute_accessors["WEIGHTS_0"])
    indices = read_accessor(document, binary, primitive["indices"])
    inverse_bind_accessor = document["skins"][0]["inverseBindMatrices"]
    inverse_bind_matrices = read_accessor(document, binary, inverse_bind_accessor)

    require(all(math.isfinite(value) for item in positions for value in item), "Non-finite position found")
    require(all(math.isfinite(value) for item in normals for value in item), "Non-finite normal found")
    require(all(math.isfinite(value) for item in texcoords for value in item), "Non-finite UV found")
    require(len(inverse_bind_matrices) == EXPECTED_JOINTS, "Inverse bind matrix count is not 41")
    require(all(math.isfinite(value) for item in inverse_bind_matrices for value in item), "Non-finite inverse bind matrix found")

    flat_indices = [item[0] for item in indices]
    require(flat_indices and min(flat_indices) >= 0 and max(flat_indices) < vertex_count, "Index is outside the vertex range")
    require(index_accessor.get("min") == [min(flat_indices)], "Index accessor minimum does not match the merged data")
    require(index_accessor.get("max") == [max(flat_indices)], "Index accessor maximum does not match the merged data")
    weightless_vertices = 0
    invalid_joint_indices = 0
    abnormal_weight_sums = 0
    max_influences = 0
    for joint_values, weight_values in zip(joints, weights):
        require(all(math.isfinite(value) for value in weight_values), "Non-finite skin weight found")
        positive_influences = sum(value > 1.0e-6 for value in weight_values)
        max_influences = max(max_influences, positive_influences)
        if positive_influences == 0:
            weightless_vertices += 1
        if abs(sum(weight_values) - 1.0) > 1.0e-4:
            abnormal_weight_sums += 1
        invalid_joint_indices += sum(index < 0 or index >= joint_count for index in joint_values)
    require(weightless_vertices == 0, f"Found {weightless_vertices} unweighted vertices")
    require(invalid_joint_indices == 0, f"Found {invalid_joint_indices} invalid joint indices")
    require(abnormal_weight_sums == 0, f"Found {abnormal_weight_sums} abnormal weight sums")
    require(max_influences <= 4, f"Found {max_influences} influences, engine limit is 4")

    image_path = (gltf_path.parent / document["images"][0]["uri"]).resolve()
    require(image_path.is_file(), f"Texture URI does not resolve: {image_path}")
    base_color_texture = document["materials"][0]["pbrMetallicRoughness"]["baseColorTexture"]["index"]
    require(base_color_texture == 0 and document["textures"][0]["source"] == 0, "KrakenSkin texture linkage is invalid")

    return {
        "mesh_count": 1,
        "primitive_count": 1,
        "material_count": 1,
        "texture_count": 1,
        "node_count": len(document["nodes"]),
        "joint_count": joint_count,
        "root": root_name,
        "vertex_count": vertex_count,
        "index_count": len(flat_indices),
        "matrix_node_count": matrix_count,
        "trs_node_count": trs_node_count,
        "animation_count": 0,
        "weightless_vertices": weightless_vertices,
        "invalid_joint_indices": invalid_joint_indices,
        "abnormal_weight_sums": abnormal_weight_sums,
        "max_influences": max_influences,
        "nonfinite_transforms": nonfinite_transform_count,
        "texture_path": str(image_path),
    }


def build_compat_document(source: dict[str, Any], output_bin_name: str) -> tuple[dict[str, Any], int, float]:
    document = copy.deepcopy(source)
    primitives = document["meshes"][0]["primitives"]
    index_accessors = [primitive["indices"] for primitive in primitives]
    index_views = [document["accessors"][index]["bufferView"] for index in index_accessors]
    first_accessor_index = index_accessors[0]
    first_view_index = index_views[0]
    combined_index_count = sum(document["accessors"][index]["count"] for index in index_accessors)
    combined_byte_length = sum(document["bufferViews"][index]["byteLength"] for index in index_views)
    combined_index_min = min(document["accessors"][index]["min"][0] for index in index_accessors)
    combined_index_max = max(document["accessors"][index]["max"][0] for index in index_accessors)

    merged_primitive = copy.deepcopy(primitives[0])
    merged_primitive["indices"] = first_accessor_index
    merged_primitive["material"] = 0
    document["meshes"][0]["primitives"] = [merged_primitive]
    document["meshes"][0]["name"] = "kraken_midboss_tentacles_EditorCompat"
    document["accessors"][first_accessor_index]["count"] = combined_index_count
    document["accessors"][first_accessor_index]["min"] = [combined_index_min]
    document["accessors"][first_accessor_index]["max"] = [combined_index_max]
    document["accessors"][first_accessor_index]["name"] = "Indices_EditorCompat_AllSurfaces"
    document["bufferViews"][first_view_index]["byteLength"] = combined_byte_length
    document["accessors"] = document["accessors"][: first_accessor_index + 1]
    document["bufferViews"] = document["bufferViews"][: first_view_index + 1]
    document["materials"] = [copy.deepcopy(document["materials"][0])]
    document["buffers"][0]["uri"] = output_bin_name
    document["images"][0]["uri"] = "../../textures/kraken_albedo.png"
    document.setdefault("asset", {}).setdefault("extras", {})["editorCompatibilityPreview"] = {
        "purpose": "Skinning Editor skeleton and skin-weight preview",
        "sourcePrimitiveCount": 3,
        "sourceMaterialCount": 3,
        "multiPrimitiveSupported": False,
        "multiMaterialSupported": False,
    }
    converted_count, max_error = convert_node_matrices(document)
    return document, converted_count, max_error


def parse_arguments() -> argparse.Namespace:
    blender_arguments = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    parser = argparse.ArgumentParser()
    parser.add_argument("--validate-only", action="store_true")
    return parser.parse_args(blender_arguments)


def main() -> None:
    arguments = parse_arguments()
    output_directory = Path(__file__).resolve().parent
    source_directory = output_directory.parent
    source_gltf = source_directory / "kraken_midboss_tentacles.gltf"
    source_bin = source_directory / "kraken_midboss_tentacles.bin"
    output_gltf = output_directory / "kraken_midboss_tentacles_editor_compat.gltf"
    output_bin = output_directory / "kraken_midboss_tentacles_editor_compat.bin"

    source_hashes_before = {"gltf": sha256(source_gltf), "bin": sha256(source_bin)}
    source_document = json.loads(source_gltf.read_text(encoding="utf-8"))
    source_binary = source_bin.read_bytes()
    validate_source_layout(source_document, source_binary)
    source_matrix_count = sum("matrix" in node for node in source_document["nodes"])

    if arguments.validate_only:
        require(output_gltf.is_file() and output_bin.is_file(), "Compatibility output does not exist")
        output_document = json.loads(output_gltf.read_text(encoding="utf-8"))
        output_binary = output_bin.read_bytes()
        converted_count = source_matrix_count
        max_error = output_document.get("asset", {}).get("extras", {}).get("matrixTrsMaxError", 0.0)
    else:
        output_document, converted_count, max_error = build_compat_document(source_document, output_bin.name)
        output_document.setdefault("asset", {}).setdefault("extras", {})["matrixTrsMaxError"] = max_error
        output_binary = source_binary
        validate_compat(output_document, output_binary, output_gltf)

        output_directory.mkdir(parents=True, exist_ok=True)
        temporary_gltf = output_gltf.with_suffix(".gltf.tmp")
        temporary_bin = output_bin.with_suffix(".bin.tmp")
        try:
            temporary_gltf.write_text(json.dumps(output_document, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
            temporary_bin.write_bytes(output_binary)
            os.replace(temporary_gltf, output_gltf)
            os.replace(temporary_bin, output_bin)
        finally:
            temporary_gltf.unlink(missing_ok=True)
            temporary_bin.unlink(missing_ok=True)

    output_document = json.loads(output_gltf.read_text(encoding="utf-8"))
    output_binary = output_bin.read_bytes()
    summary = validate_compat(output_document, output_binary, output_gltf)
    require(sha256(output_bin) == source_hashes_before["bin"], "Compatibility BIN is not byte-identical to the source BIN")
    source_hashes_after = {"gltf": sha256(source_gltf), "bin": sha256(source_bin)}
    require(source_hashes_after == source_hashes_before, "Source glTF or BIN changed during generation")

    summary.update(
        {
            "blender_version": ".".join(map(str, bpy.app.version)) if bpy is not None else "not running in Blender",
            "source_matrix_node_count": source_matrix_count,
            "converted_matrix_node_count": converted_count,
            "matrix_trs_max_error": max_error,
            "source_gltf_sha256": source_hashes_before["gltf"],
            "source_bin_sha256": source_hashes_before["bin"],
            "compat_bin_sha256": sha256(output_bin),
            "source_unchanged": source_hashes_after == source_hashes_before,
            "compat_bin_byte_identical": output_binary == source_binary,
            "mode": "validate-only" if arguments.validate_only else "generate",
        }
    )
    print("KRAKEN_EDITOR_COMPAT_VALIDATION")
    print(json.dumps(summary, indent=2, ensure_ascii=True))


if __name__ == "__main__":
    main()
