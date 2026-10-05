# NoJobEngine V1.3 - Inspector Add Component

- Removes the scattered Add Rigidbody/Collider/Camera/Light/Script buttons.
- Adds one centered `Add Component` button to the Inspector.
- Popup includes search and categories.
- Available components: Rigidbody, Box/Sphere/Capsule Collider, Camera,
  Directional/Point/Spot Light, Animator and Rotator Script.
- Existing components are disabled in the list.
- Keeps current engine constraints: one collider type and one camera/light type per entity.
- Adding a component participates in Undo/Redo.
- Animator can now be removed from its Inspector panel.
- Transform remains mandatory and cannot be removed.
