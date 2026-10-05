# NoJobEngine

A lightweight **C++20 game engine and Unity-inspired editor** built from
scratch as a learning and engineering project.

NoJobEngine started with a simple goal: understand what sits underneath
a modern game engine by building the important pieces myself instead of
treating the engine as a black box. The project has grown from rendering
a first triangle into a usable editor/runtime foundation with scenes,
assets, prefabs, physics, PBR rendering, skeletal animation, GPU
skinning and an evolving Unity-style component workflow.

> **Current milestone: NoJobEngine V1.3 --- In Development**\
> V1.0--V1.2 are complete and validated. Current development is focused
> on Editor UX and a more scalable component-based workflow.

**C++20 · OpenGL 4.6 · Dear ImGui · Jolt Physics · Assimp · CMake**

------------------------------------------------------------------------

## Why I built it

I have spent several years working with Unity and C#, both on
interactive applications and real-world projects. NoJobEngine is my way
of going deeper into engine architecture, graphics programming and
modern C++.

The goal is not to clone Unity feature-for-feature. Instead, the editor
intentionally follows familiar Unity-style workflows while the
underlying systems are implemented and explored from the ground up.

The project is especially focused on:

-   engine and editor architecture;
-   real-time rendering;
-   asset pipelines and serialization;
-   entity/component workflows;
-   skeletal animation and GPU skinning;
-   physics integration;
-   runtime/editor separation;
-   native C++ gameplay scripting;
-   understanding how high-level engine features are built internally.

------------------------------------------------------------------------

## Editor preview

NoJobEngine brings scene editing, asset management, rendering, prefabs,
animation, physics and runtime controls together in a Unity-inspired
editor.

### Rendering & graphics settings

The graphics validation scene is used to test PBR surfaces, emissive
rendering, multiple light types, shadows and post-processing.

### 3D model & material import

The asset pipeline imports 3D models through Assimp and can instantiate
them directly in the editor. Imported models support submeshes and
multiple material slots, while material and texture information can be
brought into the NoJobEngine asset workflow.

Supported formats include:

`FBX` · `glTF` · `GLB` · `OBJ` · `DAE` · `STL` · `PLY` · `3DS` · `Blend`

### Skeletal animation & GPU skinning

Animated FBX assets can be imported with their skeleton and animation
clips. NoJobEngine evaluates skeletal animation at runtime and performs
vertex skinning on the GPU.

The current animation workflow includes:

-   skeleton and bone hierarchy import;
-   animation clip/channel import through Assimp;
-   per-vertex bone IDs and weights;
-   GPU skeletal skinning;
-   Animator component;
-   clip selection;
-   playback speed;
-   loop/play controls;
-   Mixamo FBX animation workflow.

### PBR material showcase

The material system exposes physically based surface properties directly
in the editor. Materials support:

-   albedo;
-   normal;
-   metallic;
-   roughness;
-   ambient occlusion;
-   emissive maps;
-   opaque, alpha-clip and transparent surface modes.

### Prefab workflow

Entities and complete hierarchies can be turned into prefabs directly
from the editor.

Prefab V2 supports:

-   recursive entity hierarchies;
-   transforms;
-   meshes;
-   multiple material slots;
-   PBR material properties and textures;
-   Animator data;
-   prefab instantiation;
-   **Apply**;
-   **Revert**.

### Inspector & Add Component

V1.3 begins a more scalable Inspector workflow. Instead of exposing
scattered component creation buttons, entities can use a centralized
**Add Component** menu with search and categories.

The current menu includes components for physics, rendering, animation
and native scripting.

------------------------------------------------------------------------

## Feature set

### Editor

-   Unity-inspired editor layout using Dear ImGui
-   Scene Hierarchy and entity parenting
-   Component-based Inspector
-   Searchable **Add Component** workflow
-   Project/Asset browser
-   Scene viewport
-   Translate, rotate and scale gizmos with ImGuizmo
-   Numeric Transform editing
-   Play, Pause and Stop workflow
-   Persistent editor layout
-   Camera and light editor gizmos
-   Undo/Redo history
-   Material Slots
-   Prefab Apply/Revert

### Asset pipeline

-   Asset registry
-   Persistent `.meta` files and asset GUIDs
-   Texture assets
-   Material assets (`.nojobmat`)
-   Prefab assets (`.nojobprefab`)
-   Scene assets (`.nojobscene`)
-   Project configuration (`.nojobproject`)
-   Drag & drop workflows
-   3D model importing through Assimp
-   Automatic material/texture import workflow
-   Submeshes
-   Multiple materials per mesh
-   Embedded/external FBX texture handling
-   Alpha clip and transparent materials

### Animation

-   Assimp skeleton import
-   Bone hierarchy and inverse-bind data
-   Animation clips and channels
-   Position, rotation and scale keyframes
-   Per-vertex bone IDs and weights
-   Runtime pose evaluation
-   GPU skinning
-   Animator component
-   Clip selection
-   Speed, Play and Loop controls
-   Mixamo-compatible FBX workflow
-   Animator persistence in Prefab V2

### Rendering

-   OpenGL 4.6 renderer
-   Layered renderer architecture
-   HDR framebuffer
-   Cook-Torrance PBR lighting
-   Albedo, normal, metallic, roughness, AO and emissive maps
-   Multi-material/submesh rendering
-   Directional, point and spot lights
-   Directional/spot shadow maps
-   Point-light cubemap shadows
-   Skinned mesh rendering in the PBR and shadow passes
-   Procedural sky
-   Bloom
-   Screen-space ambient/contact shading
-   FXAA
-   ACES tone mapping
-   Exposure control
-   Alpha clipping and transparency

### Scene & runtime

-   Entity-based scene system
-   Parent/child transforms
-   Scene serialization and loading
-   Editor/runtime scene separation
-   Native C++ scripting foundation
-   Prefab creation and instantiation
-   Prefab Apply/Revert
-   Camera components
-   Directional, point and spot light components

### Physics

Powered by **Jolt Physics**:

-   Static, dynamic and kinematic rigid bodies
-   Box, sphere and capsule colliders
-   Trigger volumes
-   Friction and bounciness
-   Raycasts
-   Collider visualization in the editor

------------------------------------------------------------------------

## Technology

  Area                          Technology
  ----------------------------- -------------------
  Language                      C++20
  Build system                  CMake
  Graphics                      OpenGL 4.6 / GLAD
  Window & input                GLFW
  Mathematics                   GLM
  Editor UI                     Dear ImGui
  Gizmos                        ImGuizmo
  Physics                       Jolt Physics
  Model & animation importing   Assimp
  Images                        stb_image

------------------------------------------------------------------------

## Architecture

The renderer is deliberately split into layers so high-level scene code
does not depend directly on OpenGL:

``` text
SceneRenderer
    ↓
Renderer
    ↓
RenderCommand
    ↓
RendererAPI
    ↓
OpenGLRendererAPI
```

This architecture leaves room for another rendering backend in the
future.

The project also separates the **editor scene** from the **runtime
scene**. Entering Play Mode creates a runtime copy, allowing gameplay,
animation and physics to modify the world without permanently changing
the editor version of the scene.

Assets have persistent metadata and GUIDs so the asset database is not
simply a collection of hard-coded file paths.

------------------------------------------------------------------------

## Typical workflow

``` text
Import FBX / model
        ↓
Import materials & textures
        ↓
Create entities / hierarchy
        ↓
Configure materials / Animator
        ↓
Add components
        ↓
Create prefab
        ↓
Add physics / lights / camera / scripts
        ↓
Save scene
        ↓
Play / Pause / Stop
```

The editor is designed to keep this workflow familiar to developers
coming from engines such as Unity while remaining small enough for the
underlying implementation to be understandable.

------------------------------------------------------------------------

## Native C++ scripting

NoJobEngine currently includes an **early native scripting foundation**.

A `NativeScriptComponent` can attach native C++ behaviour to an entity,
and the current editor exposes the example **Rotator Script** through
the Add Component workflow.

This system is intentionally still early. Planned improvements include:

-   a reusable Script base class;
-   lifecycle methods such as `OnCreate`, `OnUpdate` and `OnDestroy`;
-   script registration;
-   multiple native script types in Add Component;
-   Inspector-exposed script properties;
-   investigation of a faster iteration / hot-reload workflow.

------------------------------------------------------------------------

## Building on Windows

### Requirements

-   Windows
-   Visual Studio with the **Desktop development with C++** workload
-   CMake
-   Git
-   A GPU/driver with OpenGL 4.6 support

Clone the repository:

``` bash
git clone https://github.com/Nil0s/NoJobEngine.git
cd NoJobEngine
```

Configure and build:

``` bash
cmake -S . -B out
cmake --build out --config Debug
```

Dependencies are fetched by CMake. The first configuration can therefore
take longer than subsequent builds.

> Generated folders such as `out/`, `.vs/` and IDE-specific build files
> should not be committed.

------------------------------------------------------------------------

## Development status

### ✅ V1.0 --- Engine foundation

Core editor/runtime architecture, scenes, assets, physics,
cameras/lights, PBR rendering, shadows and post-processing.

### ✅ V1.1 --- Asset Pipeline V2 + Undo/Redo

Multi-material meshes, submeshes, improved model/material importing,
Material Slots, alpha surfaces and editor Undo/Redo.

### ✅ V1.2 --- Animation + Prefab V2

Skeletal animation, GPU skinning, Mixamo workflow, Animator component
and recursive prefabs with materials, animation data, Apply and Revert.

### 🚧 V1.3 --- Editor UX

Current development milestone.

Already implemented:

-   centralized Add Component workflow;
-   component search;
-   component categories.

Planned during this milestone:

-   component context menus;
-   Remove / Reset / Copy / Paste Component Values;
-   improved Hierarchy and Project search/workflows;
-   improved context menus;
-   more general drag & drop;
-   multi-selection;
-   additional editor shortcuts.

------------------------------------------------------------------------

## Roadmap

After the current Editor UX milestone, the planned direction is:

1.  **Native Scripting V2** --- lifecycle, registration, Inspector
    properties and improved gameplay-code workflow.
2.  **Renderer V3** --- IBL/environment maps, HDRI workflow, improved
    reflections, lighting/shadow improvements and renderer
    profiling/optimization.
3.  **Audio & VFX** --- AudioSource/AudioListener, 2D/3D audio and
    particle/VFX systems.
4.  **Standalone Build** --- package a project into a standalone
    executable with its start scene, assets and runtime configuration.

Longer-term areas of exploration include:

-   Vulkan backend;
-   NavMesh and AI;
-   terrain;
-   networking;
-   asset cooking;
-   profiling tools;
-   animation state machines, transitions and blend trees;
-   more advanced scripting/hot reload.

------------------------------------------------------------------------

## What this project demonstrates

NoJobEngine is both a technical project and a record of my progression
beyond using an existing engine API.

It covers problems across:

-   graphics programming;
-   editor tooling;
-   ECS/component workflows;
-   serialization;
-   asset management;
-   skeletal animation;
-   GPU skinning;
-   physics integration;
-   native scripting;
-   engine architecture;
-   editor/runtime design.

The development approach is intentionally iterative: **build a system,
expose it through the editor, validate it end-to-end, and then improve
it.**

------------------------------------------------------------------------

## Author

**Francisco Ramón Asensi Domingo**

-   GitHub: `Nil0s`
-   Portfolio: `nil0s.github.io`

------------------------------------------------------------------------

## License

This repository currently does not declare a project license.
Third-party dependencies retain their respective licenses.
