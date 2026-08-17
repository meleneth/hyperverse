# Drone / Raider Model

`drone.glb` and `drone.obj` are a reusable low-poly interpretation of the original
`assets/sector7/sprites/robot.png` silhouette. The model points along local `+Y`, is centered near
the origin, spans about 5.6 units wingtip-to-wingtip, and uses one shared planar UV layout.

Texture variants:

- `textures/drone-friendly-albedo.png` — blue/cyan/yellow mining-drone material
- `textures/drone-hostile-albedo.png` — charcoal/crimson/orange raider material

The GLB embeds the friendly material. Engines can swap the albedo to the hostile image without
changing geometry or UVs. Scale the same mesh per entity; do not duplicate it for enemy sizes.

Rebuild the authored mesh and exports with:

```sh
blender -noaudio --background --python scripts/build-drone-model.py
```

The editable source is `drone.blend`. The generated textures were authored with OpenAI image
generation from the original drone graphic and then saved without further color alteration.
