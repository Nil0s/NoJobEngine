# Multi-material Inspector crash fix

The V1.1 multi-material patch changed the memory layout of MeshRendererComponent by
adding `std::vector<std::shared_ptr<Material>> Materials`.

That is an ABI-breaking component change. If old object files remain in `out/`, code
compiled against the old layout can coexist with code compiled against the new layout,
which can make `Materials` look like invalid memory and crash inside `shared_ptr::operator bool`.

This patch also normalizes legacy MeshRenderer components in the Inspector:
- if only MaterialAsset exists, it becomes material slot 0;
- if only the material slot array exists, slot 0 becomes MaterialAsset;
- slot iteration is bounds checked.

IMPORTANT: delete the complete `out/` directory before rebuilding this patch.
