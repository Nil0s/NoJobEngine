# NoJobEngine V1.2 - Prefab V2 finalization

Adds:
- Revert button for prefab instances.
- Revert rebuilds the complete prefab hierarchy from disk.
- Apply participates in editor Undo history.
- Revert participates in editor Undo history.
- AnimatorComponent is serialized in prefab V4:
  - source model/animation path
  - clip index
  - speed
  - playing
  - loop
- Instantiating an animated prefab reloads its AnimationAsset and resets time to 0.
- Fixes V4 procedural `CUBE` restoration.
- Existing recursive hierarchy, multi-material slots and texture maps remain supported.

Recommended validation:
1. Create a prefab from the Mixamo character.
2. Instantiate it.
3. Confirm 6 material slots/textures and Animator are present.
4. Play: animation should run.
5. Change transform/material/Animator speed on the instance.
6. Revert: values should return to the saved prefab.
7. Change a value, Apply, change it again, Revert: it should return to the applied value.
