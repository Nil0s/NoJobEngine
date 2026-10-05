# NoJobEngine V1.2 - GPU Skeletal Skinning + Mixamo embedded textures

Built from the user's current source snapshot.

## Skeletal animation
- 4 bone IDs + 4 normalized weights per vertex.
- Assimp skin-weight import with `aiProcess_LimitBoneWeights`.
- Clip TRS interpolation.
- Bone palette evaluation, independent of Assimp bone ordering.
- Up to 128 bones per skinned mesh.
- GPU skinning in the PBR vertex shader.
- The shadow depth shader uses the same skinned positions.
- Static meshes keep the non-skinning path.

## Mixamo materials
- Embedded FBX textures are resolved both by `*index` and Assimp embedded filename.
- Existing external-path recovery remains active.

## Validation
Use the model already confirmed in the editor:
- clip: `mixamo.com`
- skeleton: 68 bones
- duration: 7.43 s

Press Play. The time should advance and the mesh should leave T-pose and follow the clip.

IMPORTANT: Mesh/GPU vertex layout changed. Close Visual Studio/NoJobEditor, delete `out/`,
then rebuild cleanly. Reimport the Mixamo model after rebuilding so the new mesh vertex data is used.
