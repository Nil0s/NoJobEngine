# V2.0.6 - Neural Training Agent demo (first integration slice)

## Included
- Built-in native script `NeuralTrainingAgent` (Engine/Scene/NativeScripts.h)
- `Assets/Scenes/NeuralTrainingDemo.nojobscene`
- Uses the imported `Assets/Models/testplane_100x100.obj` for NavMesh bake
- Supervised native C++ training (2 -> 8 -> 2, Tanh) from generated directional examples
- Loads/saves a named `.njn` via the existing Neural asset path system
- Drives `NavAgentComponent::Destination` using the neural prediction
- Exposes state in the existing script AI state debugger

## Run
1. Copy the archive over the existing project, retaining the other files.
2. Open `Assets/Scenes/NeuralTrainingDemo.nojobscene` in NoJobEditor.
3. Bake the NavMesh from the **imported** `testplane_100x100` mesh using your usual workflow.
4. Enter Play. `TrainOnPlay=true` trains and saves `Assets/Neural/NeuralFollowerDemo.njn`.
5. For subsequent Play sessions set `TrainOnPlay=false` in the script Inspector to load the saved model.
6. Move `NeuralTarget` in edit mode and enter Play to see a new destination.
7. Select `NeuralAgent` in AI Debugger; the script AI state reports Neural: ready/following target/target reached/model error.

## Important limitations
- This is **supervised training**, not reinforcement learning.
- Training occurs synchronously during OnCreate/Play; the editor can pause briefly. The Neural Network Editor already supports training from custom datasets separately.
- The AI Debugger shows script state; full live weights/activations per agent and Blackboard/BT binding are **not yet implemented**.
- The scene's NavMesh is not baked in the asset: bake it using the imported plane in the editor.
- The example expects 2 inputs (relative X,Z divided by 10) and 2 outputs (predicted X,Z direction). Do not assign an arbitrary shape.
- The demo scene has not been tested in Visual Studio on Windows; validate compilation, imported mesh load and bake before treating V2.0.6 as complete.
