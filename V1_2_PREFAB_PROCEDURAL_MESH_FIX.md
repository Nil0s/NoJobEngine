# V1.2 Prefab V2 - Procedural Mesh Fix

Fixes Prefab V2 hierarchy instances being recreated without visible cube meshes.

Cause:
- AssetManager::GetMeshPath() correctly returns an empty path for procedural meshes.
- Prefab V2 serialized that empty path as "no mesh".
- SceneSerializer already handled this case using the CUBE sentinel, but PrefabSerializer V4 did not.

Fix:
- PrefabSerializer V4 now writes `MESH "CUBE"` for procedural cube meshes.
- Instantiate already understands the CUBE sentinel and restores the supplied default cube mesh.

Existing prefabs created with the broken V4 serializer should be recreated once after applying this patch,
because their files already contain empty mesh paths.
