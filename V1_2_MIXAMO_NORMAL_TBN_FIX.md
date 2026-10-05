# V1.2 Mixamo normal-map TBN fix

The supplied `Body Block.fbx` was inspected directly.

It contains three embedded 1024x1024 PNG textures:
- `Boss_diffuse.png`
- `Boss_specular.png`
- `Boss_normal.png`

All six FBX materials intentionally reuse those atlas textures. The diffuse atlas itself is valid, so the repeated texture assignment is not the import bug.

The rendering issue was in tangent-space reconstruction: the old shader derived B as `-cross(N,T)`, assuming a fixed UV handedness. The FBX contains mirrored UV islands, so some regions received an incorrect normal basis and became unnaturally dark.

This patch reconstructs both T and B from screen-space position/UV derivatives using the UV determinant, preserving mirrored-island handedness.

No animation/skinning code is changed.
