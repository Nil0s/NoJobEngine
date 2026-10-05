# V1.1 Multi-Material Import

Static FBX/glTF/OBJ imports now preserve Assimp submeshes and their material indices.
Every imported material is converted into its own `.nojobmat`, referenced textures are
imported automatically, and SceneRenderer draws each submesh with the correct material
slot. MeshRenderer keeps an array of Unity-style material slots while retaining the
original slot-0 API for compatibility.

This closes the single-material limitation for static imported models.
