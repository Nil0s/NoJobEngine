# NoJobEngine V1.2 - Mixamo import fix

## Animator
Imported models are now inspected automatically for animation data.
If Assimp finds one or more animation clips, CreateModelEntity attaches AnimatorComponent.
The Inspector then shows the Animator panel with clip, Playing, Loop, Speed, bone count and time.

## Mixamo/FBX materials
Texture resolution is more tolerant of FBX exporter paths:
1. Exact texture reference.
2. Texture filename beside the original FBX.
3. Recursive search below the original FBX folder.
4. Matching filename already present in Assets/Textures.

The existing `.source` sidecar remains important because it lets the importer resolve textures
relative to the original FBX before it was copied into the project.

Note: visual skeletal deformation is not part of this patch yet. Animator clip detection/playback
is the validation step before GPU skinning.
