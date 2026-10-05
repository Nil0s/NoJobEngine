# NoJobEngine V1.2 - Animation foundation + Prefab V2

## Animation
- Assimp skeleton import (bone hierarchy + inverse bind matrices).
- Imports every animation clip and node channel from FBX/glTF/DAE.
- AnimatorComponent with clip selection, play/loop/speed/time.
- Runtime playback clock.
- Inspector Animator panel.

This patch establishes the skeletal-animation data/runtime layer. GPU vertex skinning and
bone-palette upload are the next renderer step; animated files currently render with their
existing static mesh while the Animator advances the imported clip.

## Prefab V2
- `.nojobprefab` format V4.
- Saves and instantiates complete entity hierarchies recursively.
- Preserves local transforms.
- Preserves meshes.
- Preserves every material slot, PBR values, surface mode, alpha cutoff and texture maps.
- Prefab root receives PrefabInstanceComponent with source path.
- Inspector exposes `Apply to Prefab`.

Important: this changes Scene/Entity component layout. Delete `out/` and perform a clean build.
