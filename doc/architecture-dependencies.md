<!-- SPDX-License-Identifier: MIT -->
# Architecture Dependencies

This document records the production library boundaries introduced for the architecture modernization work. The current shape is intentionally conservative: projects reference existing source files in place, and later modernization phases may move files after the domain models stabilize.

## Current Production Projects

```text
XpressFormula.Expression
  Core tokenization, parsing, AST, shared AST queries, formula compilation/classification, evaluation, functions, examples, and version parsing helpers.

XpressFormula.Model
  Formula identity/state/compiled formula model, scene summary analysis, persistent view state, transient viewport geometry, and ViewTransform. Header-only settings models are listed here until they move to Model.

XpressFormula.Plotting
  PlotRenderer and plotting draw logic.

XpressFormula.Infrastructure
  Temporary static-library boundary for header-only persistence/export infrastructure. Later persistence work will replace the anchor with real JSON, file, and persistence sources.

XpressFormula.UI
  Formula/control/plot panels, FormulaCard/PlotToolbar components, dynamic formula editor state, formula presentation helpers, UiKit, and Dear ImGui core sources.

XpressFormula.App
  Current Application orchestration and ImGui Win32/DX11 backend sources.

XpressFormula
  Executable host: main.cpp, resources, and project references.

XpressFormula.Tests
  Test runner and tests. It links production static libraries instead of compiling production .cpp files directly.
```

## Dependency Direction

```text
Expression
Model          -> Expression, when needed
Plotting       -> Model + Expression
Infrastructure -> Model + Expression, when needed
UI             -> Model + Plotting + Expression + Infrastructure + ImGui
App            -> UI + Infrastructure + Plotting + Model + Expression + Windows/D3D backends
XpressFormula  -> App and production libraries
Tests          -> production libraries under test
```

The current code predates the final architecture, so some file paths still live under `Core` or `UI` even when their project ownership now points toward Expression, Model, or Infrastructure. Prefer changing ownership through projects first, then moving files when later modernization work changes the underlying model.

## Forbidden Dependencies

- `Expression` must not include ImGui, Win32, D3D11, UI widgets, filesystem, or project JSON types.
- `Model` must not include ImGui, Win32, D3D11, or JSON parser types.
- `Plotting` should not gain new UI workflow or platform responsibilities.
- `Infrastructure` must not mutate live application state directly.
- `UI` panels must not perform atomic file writes, WIC encoding, shell actions, clipboard operations, or HTTP calls.
- `App` may coordinate side effects during the migration, but new platform code should move behind Platform/Windows services in later modernization phases.
- Tests must not re-add production `.cpp` files directly; add a production project reference instead.

## Test Consumption Rule

`XpressFormula.Tests.vcxproj` compiles only test sources and links:

- `XpressFormula.Expression`
- `XpressFormula.Model`
- `XpressFormula.Plotting`
- `XpressFormula.Infrastructure`
- `XpressFormula.UI`

This makes tests consume the same production object code used by the executable. Header-only helpers are still compiled in test translation units until later modernization phases give them `.cpp` ownership.

## Migration Rules

- Do not mix product features with architecture-boundary changes.
- Keep `.xfplot` schema version 1 unless a dedicated schema migration is approved.
- Keep export metadata schema unchanged unless a dedicated schema migration is approved.
- Prefer characterization tests before moving behavior.
- Keep old adapters only until all call sites have moved.
- Update this document whenever a production project gains or loses responsibility.
- If a dependency rule cannot be enforced yet, document the exception and the modernization phase expected to remove it.

## Current Exceptions

- `XpressFormula.Infrastructure` contains an anchor `.cpp` because current persistence/export infrastructure is mostly header-only. Later serialization and persistence phases are expected to replace this with real JSON, atomic file, project serializer, repository, and recent-project sources.
- `XpressFormula.UI::FormulaEntry` remains only as a transitional alias for `XpressFormula.Model::Formula`; it no longer stores duplicated domain state. Formula labels and display counts are computed through UI presentation helpers.
- `XpressFormula.Plotting` still includes ImGui because rendering currently writes directly to `ImDrawList`. A later plotting foundation phase is expected to introduce geometry generation before an ImGui backend.
- `XpressFormula.App` still owns Windows API, D3D, WIC, WinHTTP, shell, clipboard, and file-dialog code. Later platform, export, document-controller, and composition phases are expected to extract these responsibilities.

## No-Feature-Change Constraint

Architecture PRs must preserve behavior unless a focused work item explicitly authorizes a correction. In particular, do not change formulas, defaults, labels, shortcuts, `.xfplot` content, export metadata, file locations, or packaging behavior as part of a boundary-only change.
