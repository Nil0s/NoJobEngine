# NoJobEngine V1 — foundation complete

This milestone consolidates the engine into a usable Unity-style editor foundation.

## Editor/runtime already present
- Hierarchy + parenting, Inspector, Project browser, Viewport, gizmos
- Play / Pause / Stop with editor/runtime scene separation
- Scene save/load
- Native script component
- Jolt rigidbodies, colliders, triggers and raycast
- Cameras, directional/point/spot lights and editor gizmos
- OpenGL renderer, PBR, HDR, ACES, bloom, SSAO/contact shading, FXAA, shadows and sky

## Final asset/import workflow
- Assimp-backed static model import: OBJ, FBX, glTF, GLB, DAE, STL, PLY, 3DS and Blend
- Texture import/cache
- Persistent per-asset `.meta` GUIDs
- Asset registry rebuilt from `.meta` files
- Models can be instantiated from Project
- Materials `.nojobmat` can be saved, double-clicked onto the selected MeshRenderer, or dragged to its Inspector
- Prefabs can be created by dragging Hierarchy entities into Assets/Prefabs
- Prefab material/PBR texture persistence
- Project config + start-scene field

## Intentional V1 boundary
NoJobEngine V1 is a static-scene/game-engine foundation, not feature parity with the commercial Unity editor.
The following are enhancement tracks rather than blockers for V1:
- skeletal animation / animation controller
- multi-material submeshes and automatic imported-material extraction
- nested prefab overrides/variants
- C# scripting / hot reload
- Vulkan backend
- navigation, audio mixer, particles/VFX graph
- packaging/build pipeline and platform exporters
- undo/redo command stack and collaborative workflows

For glTF projects with external .bin/textures, keep the companion files beside the .gltf
inside Assets/Models, or prefer GLB for a self-contained import.
