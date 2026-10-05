# V1.2 Mixamo FBX UV fix

Root cause found after inspecting the supplied Body Block FBX and the engine's mesh importer.

The generic Assimp model loader was using `aiProcess_FlipUVs`. The supplied Mixamo FBX already has the UV orientation expected by its embedded `Boss_diffuse.png` atlas. Flipping V moved mesh regions onto unrelated parts of the atlas (and onto its black unused background), which is why parts such as trousers/face could render almost black even though the correct diffuse texture was loaded.

Fix:
- Removed `aiProcess_FlipUVs` from `Mesh::LoadModel`.
- OBJ loading is unchanged; the custom OBJ loader still performs its own V conversion.
- Animation/GPU skinning is unchanged.
- Materials/shaders are unchanged.

IMPORTANT: re-import/recreate the FBX entity after applying this patch so the Mesh is rebuilt with the corrected UV coordinates.
