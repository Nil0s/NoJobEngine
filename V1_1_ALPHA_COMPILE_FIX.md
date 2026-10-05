# V1.1 Alpha compile fix

Fixes the two C2065 errors caused by `AI_MATKEY_GLTF_ALPHAMODE` not being
available in the Assimp headers used by this project. The importer now queries
Assimp's glTF material properties by their raw property keys:
`$mat.gltf.alphaMode` and `$mat.gltf.alphaCutoff`.

Also replaces the Editor's deprecated `strncpy` usage with a bounded `snprintf`.

The `APIENTRY` C4005 message comes from the interaction between Windows SDK and
OpenGL/GLAD headers. It is a warning, not the cause of this failed build, and no
Windows SDK header is modified by this patch.
