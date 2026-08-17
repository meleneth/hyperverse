"""Build the reusable Hyperverse drone/raider mesh with a shared planar UV layout."""

from pathlib import Path

import bpy


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "assets" / "models" / "drone"
FRIENDLY_TEXTURE = OUTPUT / "textures" / "drone-friendly-albedo.png"
HOSTILE_TEXTURE = OUTPUT / "textures" / "drone-hostile-albedo.png"


def normalize_texture(path: Path) -> None:
    image = bpy.data.images.load(str(path), check_existing=False)
    if tuple(image.size) != (1024, 1024):
        image.scale(1024, 1024)
        image.filepath_raw = str(path)
        image.file_format = "PNG"
        image.save()
    bpy.data.images.remove(image)


def prism(name: str, outline: list[tuple[float, float]], bottom: float, top: float) -> bpy.types.Object:
    count = len(outline)
    vertices = [(x, y, bottom) for x, y in outline] + [(x, y, top) for x, y in outline]
    faces: list[tuple[int, ...]] = []
    faces.append(tuple(reversed(range(count))))
    faces.append(tuple(range(count, count * 2)))
    for index in range(count):
        following = (index + 1) % count
        faces.append((index, following, following + count, index + count))

    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)

    uv_layer = mesh.uv_layers.new(name="UVMap")
    for polygon in mesh.polygons:
        for loop_index in polygon.loop_indices:
            vertex = mesh.vertices[mesh.loops[loop_index].vertex_index].co
            uv_layer.data[loop_index].uv = ((vertex.x + 3.2) / 6.4, (vertex.y + 2.8) / 6.4)
    return obj


def add_material() -> bpy.types.Material:
    material = bpy.data.materials.new("DroneFriendly")
    material.use_nodes = True
    nodes = material.node_tree.nodes
    links = material.node_tree.links
    principled = nodes.get("Principled BSDF")
    image_node = nodes.new("ShaderNodeTexImage")
    image_node.name = "FactionAlbedo"
    image_node.image = bpy.data.images.load(str(FRIENDLY_TEXTURE))
    image_node.interpolation = "Linear"
    links.new(image_node.outputs["Color"], principled.inputs["Base Color"])
    principled.inputs["Metallic"].default_value = 0.48
    principled.inputs["Roughness"].default_value = 0.42
    return material


def main() -> None:
    normalize_texture(FRIENDLY_TEXTURE)
    normalize_texture(HOSTILE_TEXTURE)
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)

    pieces = [
        prism("CenterSpine", [(-0.38, -2.35), (0.38, -2.35), (0.52, 1.35), (0.0, 2.85), (-0.52, 1.35)], -0.25, 0.34),
        prism("LeftWing", [(-0.42, 0.92), (-0.82, 1.52), (-2.82, -0.35), (-2.28, -1.62), (-1.28, -1.05)], -0.20, 0.20),
        prism("RightWing", [(0.42, 0.92), (0.82, 1.52), (2.82, -0.35), (2.28, -1.62), (1.28, -1.05)], -0.20, 0.20),
        prism("LeftEngine", [(-1.54, -0.72), (-2.20, -1.18), (-2.42, -2.05), (-1.52, -1.64)], -0.30, 0.08),
        prism("RightEngine", [(1.54, -0.72), (2.20, -1.18), (2.42, -2.05), (1.52, -1.64)], -0.30, 0.08),
        prism("CenterEngine", [(-0.32, -1.72), (0.32, -1.72), (0.25, -2.55), (-0.25, -2.55)], -0.34, 0.06),
    ]

    material = add_material()
    for piece in pieces:
        piece.data.materials.append(material)
        piece.select_set(True)
        bevel = piece.modifiers.new(name="EdgeBevel", type="BEVEL")
        bevel.width = 0.08
        bevel.segments = 2

    bpy.context.view_layer.objects.active = pieces[0]
    bpy.ops.object.convert(target="MESH")
    bpy.ops.object.join()
    drone = bpy.context.active_object
    drone.name = "DroneHull"
    drone.data.name = "DroneHullMesh"
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)

    for polygon in drone.data.polygons:
        polygon.use_smooth = False

    OUTPUT.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT / "drone.blend"))
    bpy.ops.export_scene.gltf(
        filepath=str(OUTPUT / "drone.glb"),
        export_format="GLB",
        export_apply=True,
        export_yup=True,
    )
    bpy.ops.wm.obj_export(
        filepath=str(OUTPUT / "drone.obj"),
        export_materials=True,
        export_uv=True,
        export_normals=True,
        apply_modifiers=True,
        forward_axis="NEGATIVE_Z",
        up_axis="Y",
    )
    bpy.ops.wm.quit_blender()


if __name__ == "__main__":
    main()
