# V1.1 Alpha Surfaces

Material rendering modes:
- Opaque
- Alpha Clip (cutout)
- Transparent

Imported FBX/glTF materials use Assimp opacity metadata, opacity textures and glTF
alphaMode when available. Alpha Clip uses the albedo alpha channel and configurable
Alpha Cutoff. The same alpha clipping is applied in the shadow pass so foliage does
not cast rectangular card shadows.

Transparent materials enable standard SRC_ALPHA blending and disable depth writes
while drawn. Full back-to-front transparent object sorting is a later renderer
optimization; Alpha Clip is the recommended mode for foliage, fences and cards.

Material format is bumped to NOJOB_MATERIAL 3 while the loader remains compatible
with older material files.
