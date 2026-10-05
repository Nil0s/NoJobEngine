# NoJobEngine V1.1 — Asset Pipeline + Undo/Redo

## Block 1: Model material import
When a static model is imported, NoJobEngine now asks Assimp for the material used by
the first mesh and automatically creates a reusable `.nojobmat`.

Imported properties:
- Base color
- Metallic factor
- Roughness factor
- Emissive color
- Albedo/base-color texture
- Normal texture
- Metallic texture
- Roughness texture
- Ambient-occlusion/lightmap texture
- Emissive texture

External texture files are copied into `Assets/Textures`. Compressed embedded textures
(such as embedded PNG/JPEG data in an FBX) are extracted into `Assets/Textures`.
The generated material is stored in `Assets/Materials`.

V1.1 currently assigns the first mesh/material pair to the engine's single-material
MeshRenderer. Multi-material submeshes require renderer/index-range support and remain
a dedicated renderer extension rather than being hidden behind incorrect behavior.

## Block 2: Undo / Redo
A scene-snapshot command history has been added with a 64-operation cap.

Supported in this block:
- Transform Inspector edits
- Transform gizmo edits
- Create Empty
- Create Cube
- Imported model entity creation
- Delete
- Duplicate
- Parenting
- Unparenting
- Camera creation
- Directional / Point / Spot light creation

Shortcuts:
- Ctrl+Z: Undo
- Ctrl+Y: Redo
- Ctrl+Shift+Z: Redo

An Edit menu exposes Undo/Redo and disables history operations during Play Mode.
