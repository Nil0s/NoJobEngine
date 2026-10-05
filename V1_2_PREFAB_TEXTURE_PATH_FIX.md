# V1.2 Prefab V2 - Texture restoration fix

Prefab V4 already serializes all PBR texture slots. This patch makes texture restoration robust:
- First resolves the stored path through AssetManager (project-relative, normal case).
- Falls back to resolving it relative to the `.nojobprefab` file.
- Applies the fix to base color, normal, metallic, roughness, AO and emissive maps.
- Keeps `UseTexture` enabled only when the base texture was actually restored.

Recreate prefabs written during the broken V1.2 test so their serialized mesh/material state is clean.
