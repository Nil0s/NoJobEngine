# V1.2 Mixamo / FBX material PBR fix

- Prevents legacy FBX diffuse tint from darkening an authored diffuse texture twice.
- Treats FBX/Mixamo as legacy Phong when no real metallic/roughness maps exist.
- Forces non-mapped FBX metallic to 0.
- Converts Phong shininess to PBR roughness, with a neutral fallback.
- Does not reinterpret FBX LIGHTMAP as AO, avoiding accidental darkening.
- Does not change GPU skinning or animation.
