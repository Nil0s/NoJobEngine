# V1.3 Add Component compile fix

Fixes C2338 caused by the new Remove Animator button. The current Scene API
does not include AnimatorComponent in its compile-time removable-component
allow-list, so this patch removes that button without weakening Scene safety.

The centralized Add Component popup remains unchanged.
