# V1.2 Prefab V2 compile fix

Fixes the compiler errors introduced by the Prefab V2 final patch:

- Adds the full `AnimationAsset` definition to `PrefabSerializer.cpp` via
  `Engine/Animation/Animation.h`.
  This restores access to:
  - `AnimationAsset::Load`
  - `AnimationAsset::SourcePath`
  - `AnimationAsset::Clips`
- Fixes the EditorLayer member name used by Revert:
  `m_CubeMesh` -> `m_DefaultCubeMesh`.

The APIENTRY, strncpy and shadowed-variable messages are warnings and are
unrelated to this Prefab V2 compile failure.
