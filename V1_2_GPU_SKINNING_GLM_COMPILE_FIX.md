# V1.2 GPU Skinning - GLM compile fix

The real compiler error was C1189 from GLM's experimental GTX quaternion include.

Fix:
- Replaced `glm/gtx/quaternion.hpp` with stable `glm/gtc/quaternion.hpp`.
- Replaced `glm::toMat4(rot)` with `glm::mat4_cast(rot)`.

The many OpenGL/GLAD errors shown by IntelliSense are cascading parser errors and are not the root compiler failure.
