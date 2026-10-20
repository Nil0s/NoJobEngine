# NoJobEngine v2.0.7 — Documentation and safe cleanup

## Generate the API reference

Install Doxygen, open a terminal at the repository root, and run:

```sh
doxygen docs/Doxyfile
```

Open `docs/generated/html/index.html`. The output directory is ignored by Git if you add `docs/generated/` to `.gitignore`.

## Scope of this patch

- Adds English `@file` and `@brief` documentation to the supplied C++ headers.
- Adds detailed API contracts to neural network, neural training, native scripting, project compilation and BT node interfaces.
- Adds a Doxygen configuration and architecture entry page.
- Normalizes trailing whitespace in touched headers.
- Does **not** remove or rename public APIs, change gameplay logic, or reorganize source files.

## Validation checklist

1. Generate Doxygen HTML and inspect warnings.
2. Rebuild Engine and Editor in Visual Studio (not possible from the supplied partial archive).
3. Compile project scripts from the Editor; confirm PongAgent works and is visible in AI Debugger.
4. Verify neural training, resume and named model saving.
5. Verify traditional and neural Behavior Trees.
6. Verify scene loading, navigation, physics and standalone build.

## Deferred cleanup

Semantic refactoring (removing unused symbols, changing ownership, splitting large source files) is intentionally deferred until a full project build and tests are available. Avoid treating a documentation-only patch as proof that the entire engine compiles.
