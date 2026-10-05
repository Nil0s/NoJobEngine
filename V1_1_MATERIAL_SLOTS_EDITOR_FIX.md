# V1.1 Material Slots Editor Fix

MeshRenderer material slots are now actual editable slots.

- Click Element 0, Element 1, ... to choose the active material.
- Surface mode, Alpha Cutoff, PBR values and texture maps edit the selected slot.
- Every Element accepts `.nojobmat` drag/drop independently.
- The Material Slot field also accepts a material for the currently selected Element.
- Texture drag/drop applies to the selected Element.
- Saving a material produces `<Entity>_ElementN.nojobmat`.
- Slot 0 remains synchronized with the legacy `MaterialAsset` field.

For foliage: select the leaf Element, choose `Alpha Clip`, and start with
Alpha Cutoff 0.5. This edits the leaf material instead of the bark material.
