# NoJobEngine V2.0.4 / V2.0.5 — Neural Editor and Inspector

## Launch

In the editor: **AI > Neural Network Editor**.

## Features

- Dense network topology editor (inputs, hidden layers, outputs, activation selection).
- Neural network graph with signed, weighted connections; click a layer column to inspect it.
- Layer weight and bias inspection, truncated preview for very large networks.
- Native XOR training with selectable SGD/Adam, epochs, batch size and learning rate.
- Training runs on a background worker; loss and progress update per epoch, with Stop support.
- Interactive input inference, output inspection, per-layer activations, and XOR prediction table.
- Save/load `.njn` assets with the existing native serializer.

## Important limitations / future work

- Training UI uses the built-in XOR dataset only; custom datasets are future work.
- The graph is an inspection view, not a draggable node graph.
- Layer weights are displayed but not edited directly.
- Training result replaces the current network when finished; partial training after Stop is discarded.
- This is an editor-level neural inspector, not yet bound to live Scene agents, Perception or Blackboard (V2.0.6).
- Current training callback provides per-epoch MSE and cancellation; the callback runs on the training thread.

## Files

- `Editor/Panels/NeuralEditorPanel.h/.cpp` — editor UI and background training worker.
- `Engine/AI/Neural/NeuralNetwork.h/.cpp` — optional training progress callback.
- `Editor/EditorLayer.cpp` — AI menu entry and window drawing.
- `CMakeLists.txt` — editor panel source added to `NoJobEditor`.

## Validation

`NoJobNeuralTests` passes on Linux/g++ C++20 after the callback addition:

```
XOR MSE 0.302703 -> 3.80536e-06
ALL NEURAL TESTS PASSED
```

The full Windows editor build has **not** been executed in this environment. Compile and validate with Visual Studio, then test UI and asset persistence manually.
