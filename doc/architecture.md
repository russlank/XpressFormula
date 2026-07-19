<!-- SPDX-License-Identifier: MIT -->
# Architecture

## High-Level Design

XpressFormula is moving toward explicit production modules around the existing source layout:

- `Expression`: expression tokenization, parsing, AST queries, compilation/classification, and evaluation
- `Model`: durable formula/view models and pure scene analysis
- `Plotting`: draw routines for grid/axes/curves/heat maps/implicit contours and 3D surfaces (explicit + implicit)
- `UI`/`App`: ImGui panels, application orchestration, and current platform/rendering side effects

The architecture modernization now adds build-enforced production library boundaries around the existing source layout. See [`architecture-dependencies.md`](architecture-dependencies.md) for the current project graph, forbidden dependencies, and migration exceptions.

## Production Project Boundaries

Current production projects:

- `XpressFormula.Expression`: expression tokenization, parsing, shared AST queries, formula compilation/classification, evaluation, function metadata, examples, and version parsing helpers.
- `XpressFormula.Model`: stable formula identity/state headers, scene-summary analysis, persistent view state, transient viewport geometry, and `ViewTransform`.
- `XpressFormula.Plotting`: plotting renderer implementation.
- `XpressFormula.Infrastructure`: temporary boundary for header-only persistence/export infrastructure until JSON/file/project sources are extracted.
- `XpressFormula.UI`: ImGui panels, components, UiKit, and Dear ImGui core sources.
- `XpressFormula.App`: current application orchestration and Win32/DX11 ImGui backend sources.
- `XpressFormula`: executable host containing `main.cpp`, resources, and project references.
- `XpressFormula.Tests`: tests linked against the production libraries.

Intended dependency direction:

```text
Expression -> standard library
Model -> Expression when needed
Plotting -> Model + Expression
Infrastructure -> Model + Expression when needed
UI -> Model + Plotting + Expression + Infrastructure + ImGui
App -> UI + Infrastructure + Plotting + Model + Expression + platform APIs
Executable -> App and production libraries
Tests -> production libraries
```

This first boundary step references existing files from new static-library projects. File moves are deferred until later modernization phases establish stable models and pure helpers.

## Module Breakdown

- [`src/XpressFormula/Core/Tokenizer.h`](../src/XpressFormula/Core/Tokenizer.h) and [`src/XpressFormula/Core/Tokenizer.cpp`](../src/XpressFormula/Core/Tokenizer.cpp)
  - Converts expression text into token stream.
- [`src/XpressFormula/Core/Parser.h`](../src/XpressFormula/Core/Parser.h) and [`src/XpressFormula/Core/Parser.cpp`](../src/XpressFormula/Core/Parser.cpp)
  - Recursive-descent parser producing an AST.
- [`src/XpressFormula/Expression/AstQueries.h`](../src/XpressFormula/Expression/AstQueries.h) and [`src/XpressFormula/Expression/AstQueries.cpp`](../src/XpressFormula/Expression/AstQueries.cpp)
  - Shared pure AST traversal queries, including variable collection and variable search.
- [`src/XpressFormula/Expression/FormulaCompiler.h`](../src/XpressFormula/Expression/FormulaCompiler.h) and [`src/XpressFormula/Expression/FormulaCompiler.cpp`](../src/XpressFormula/Expression/FormulaCompiler.cpp)
  - Formula trimming, expression/equation parsing, equation normalization, unsupported-variable validation, and formula-kind classification.
- [`src/XpressFormula/Model/Formula.h`](../src/XpressFormula/Model/Formula.h)
  - Stable formula identity and string-backed domain formula state.
- [`src/XpressFormula/Model/SceneSummary.h`](../src/XpressFormula/Model/SceneSummary.h) and [`src/XpressFormula/Model/SceneSummary.cpp`](../src/XpressFormula/Model/SceneSummary.cpp)
  - Pure visible-scene capability analysis for render-mode planning, toolbar state, idle redraw scheduling, and plot rendering.
- [`src/XpressFormula/Model/ViewState.h`](../src/XpressFormula/Model/ViewState.h)
  - Persistent center/scale view state and transient viewport geometry types.
- [`src/XpressFormula/Core/Evaluator.h`](../src/XpressFormula/Core/Evaluator.h) and [`src/XpressFormula/Core/Evaluator.cpp`](../src/XpressFormula/Core/Evaluator.cpp)
  - Evaluates AST values for provided variables.
- [`src/XpressFormula/Core/ViewTransform.h`](../src/XpressFormula/Core/ViewTransform.h) and [`src/XpressFormula/Core/ViewTransform.cpp`](../src/XpressFormula/Core/ViewTransform.cpp)
  - Handles world-to-screen mapping, zoom, pan, and grid spacing from explicit `ViewState` plus `Viewport`.
- [`src/XpressFormula/Core/UpdateVersionUtils.h`](../src/XpressFormula/Core/UpdateVersionUtils.h)
  - Small header-only utilities for semantic-version parsing/comparison and extracting GitHub release fields from API JSON.
- [`src/XpressFormula/UI/Application.h`](../src/XpressFormula/UI/Application.h) and [`src/XpressFormula/UI/Application.cpp`](../src/XpressFormula/UI/Application.cpp)
  - Owns Win32 window, D3D11 resources, ImGui lifecycle, frame loop, main sidebar/plot layout state, plot toolbar actions, and export workflow.
- [`src/XpressFormula/UI/UiKit`](../src/XpressFormula/UI/UiKit)
  - Thin immediate-mode UI helpers: shared metrics, pure responsive layout planners, RAII ImGui scopes, deterministic toolbar rows, property grids, splitter sizing, and modal sizing.
- [`src/XpressFormula/UI/Components`](../src/XpressFormula/UI/Components)
  - Reusable XpressFormula-specific UI components such as `PlotToolbar` and `FormulaCard`. Components may edit ordinary widget state passed by reference, but collection mutations and application commands stay with panels or `Application`.
- [`src/XpressFormula/UI/FormulaPanel.h`](../src/XpressFormula/UI/FormulaPanel.h) and [`src/XpressFormula/UI/FormulaPanel.cpp`](../src/XpressFormula/UI/FormulaPanel.cpp)
  - Formula list management, editor modal workflow, collection mutations, and action handling returned by formula-card components.
- [`src/XpressFormula/UI/FormulaEntry.h`](../src/XpressFormula/UI/FormulaEntry.h) and [`src/XpressFormula/UI/FormulaPresentation.h`](../src/XpressFormula/UI/FormulaPresentation.h)
  - UI compatibility adapter and presentation labels around the expression compiler. `FormulaEntry` keeps legacy fields available for current renderers, while stored formula text is a `std::string`.
- [`src/XpressFormula/UI/FormulaSceneAdapter.h`](../src/XpressFormula/UI/FormulaSceneAdapter.h)
  - Thin adapter from `FormulaEntry` lists to model scene-summary analysis.
- [`src/XpressFormula/UI/ControlPanel.h`](../src/XpressFormula/UI/ControlPanel.h) and [`src/XpressFormula/UI/ControlPanel.cpp`](../src/XpressFormula/UI/ControlPanel.cpp)
  - Global 2D view controls, display toggles (grid/coordinates/wires), reusable property-grid rows for 3D/heatmap controls, and export dialog launch action.
- [`src/XpressFormula/UI/PlotPanel.h`](../src/XpressFormula/UI/PlotPanel.h) and [`src/XpressFormula/UI/PlotPanel.cpp`](../src/XpressFormula/UI/PlotPanel.cpp)
  - Interactive plotting area, mouse interactions, and export-time plot render overrides (background/grid/coordinates/wires).
- [`src/XpressFormula/UI/ProjectSession.h`](../src/XpressFormula/UI/ProjectSession.h)
  - Versioned `.xfplot` persistence boundary: plain session records, JSON serialization/parsing, schema validation, and safe application of loaded values.
- [`src/XpressFormula/Version.h`](../src/XpressFormula/Version.h)
  - Centralized semantic version metadata used by window title, resources, and packaging.
- [`src/XpressFormula/Plotting/PlotRenderer.h`](../src/XpressFormula/Plotting/PlotRenderer.h) and [`src/XpressFormula/Plotting/PlotRenderer.cpp`](../src/XpressFormula/Plotting/PlotRenderer.cpp)
  - Rendering primitives and formula visualizations (2D + 3D).

## Runtime Flow

1. [`src/XpressFormula/main.cpp`](../src/XpressFormula/main.cpp) constructs `UI::Application`.
2. `Application::initialize()` creates Win32 window, D3D11 swap chain/device, and ImGui context.
3. `Application::run()` drives the message loop and rendering frames (including idle redraw optimization).
4. `FormulaPanel` updates formula text and triggers parse.
5. `Application` builds a `SceneSummary` from the edited formula list and passes it to the control panel, toolbar, and plot panel.
6. `PlotPanel` updates only the transient viewport geometry on `ViewTransform` and delegates drawing to `PlotRenderer`.
7. `PlotRenderer` evaluates formulas through `Core::Evaluator` and draws based on variable dimensionality and equation form.
8. `Application` also polls a background GitHub release check future and updates sidebar notification state when a result arrives.
9. Export requests resolve aspect/framing settings, trigger a plot-only offscreen render pass (temporary D3D11 render target and viewport) with export-specific overrides, then post-processing (pixel-format normalization, optional resize/grayscale) before file/clipboard output.
10. Project New/Open/Save/Save As workflows stay in `Application`; `.xfplot` parsing completes before active formulas, view, or plot settings are mutated.

## Project Persistence Boundary

`ProjectSession` is the versioned boundary for `.xfplot` files. `Application` still owns live state (`FormulaEntry`, `ViewTransform`, `PlotSettings`, project path, dirty flag, and recent list), while the serializer works on plain records that do not depend on ImGui widgets.

Important rules:

- Build a `ProjectSession` snapshot from application state before saving.
- Parse and validate a full project file before mutating active application state.
- Retain invalid loaded formulas where possible and report load warnings after reparsing.
- Clamp or ignore unsafe numeric values before applying them to the live view and plot settings.
- Persist only `ViewState` center/scale values; viewport origin/size is frame layout state and is not written to `.xfplot`.
- Keep unknown fields tolerated for schema version 1 so future writers can add data without breaking older builds.
- Write project files through a temporary file followed by replacement so failed writes do not leave a partial target file.
- Treat dirty state as a serialized-state comparison against the last clean snapshot.

`ProjectSession.h` currently contains both the schema records and the small JSON parser/serializer. That can be split later if the format grows, but file size alone is not a reason to refactor it.

## UI Toolkit Boundary

The UI toolkit is intentionally small and immediate-mode. `UiKit` does not own application state, retain widget objects, or replace ordinary ImGui controls. It centralizes policies that are easy to get wrong when repeated inline:

- responsive breakpoints and shared dimensions in `UiMetrics`
- pure layout decisions that can be unit tested
- row-height and cursor placement mechanics for responsive toolbars
- scope safety for ImGui push/pop and disabled blocks
- repeated table layout behavior for property controls

Domain components live one level above `UiKit`. They render reusable XpressFormula UI surfaces and return explicit action structs for one-shot commands. Panels and `Application` remain responsible for workflows, vector mutation, file/clipboard actions, export processing, and persistent state.

## Formula Rendering Modes

Formula mode is inferred from variable presence and equation shape:

- `y=f(x)` if no `y`/`z` variable is detected
- `z=f(x,y)` if `x` and `y` are present, or if equation is solved for `z`
- `F(x,y)=0` for implicit equations such as `x^2+y^2=100`
- `f(x,y,z)` scalar field if `x`, `y`, and `z` are present (cross-section in 2D mode)
- `F(x,y,z)=0` for implicit 3D equations such as `x^2+y^2+z^2=16`

Render mapping:

- `y=f(x)` -> curve line sampling across screen width
- `z=f(x,y)` -> 3D surface (or 2D heat map depending on effective mode)
- `F(x,y)=0` -> marching-squares contour rendering
- `f(x,y,z)` -> heat map cross-section at selected `z`
- `F(x,y,z)=0` -> implicit 3D surface mesh in effective 3D mode, or scalar cross-section in effective 2D mode

Effective mode policy:

- `Auto`: if both 2D and 3D scene capabilities are visible, rendering resolves to 2D.
- `Auto`: if only 3D-capable scene content is visible, rendering resolves to 3D.
- `Force3D` / `Force2D`: user override of auto behavior.

This policy is centralized as a pure `resolveXYRenderMode(preference, SceneSummary)` helper.

## Current 3D Implicit Surface Notes

- Implicit `F(x,y,z)=0` surfaces are extracted from sampled scalar-field data using a surface-nets style mesh.
- The extracted world-space mesh is cached and re-used across camera-only changes (azimuth/elevation/z-scale/style), then re-projected each frame.
- The implicit sampling domain is derived from the current visible `x/y` range and the formula `z slice / center`, so shapes can appear clipped if the view box does not fully contain them.
- 3D projection is anchored to the same world origin as the 2D grid/axes (`ViewTransform`) to avoid visual drift/"swimming" while the user pans/zooms.
- With **Optimize Rendering** enabled, `PlotPanel` temporarily reduces 3D mesh density and suppresses wireframe lines while dragging/zooming to improve responsiveness, then restores full quality when interaction stops.

## 3D Grid Plane and Render Paths

The projected 3D grid is treated as a plane at `z = 0` with:

- translucent plane fill
- major/minor grid lines drawn edge-to-edge inside a thick projected frame
- optional projected axes overlay

When **Show Grid** is enabled in effective 3D mode, `PlotPanel` uses a split render sequence:

1. draw 3D formulas clipped to the below-plane pass (`z <= 0`)
2. draw projected grid plane
3. draw 3D formulas clipped to the above-plane pass (`z >= 0`)

This split is performed in renderer space:

- explicit `z=f(x,y)` triangles are clipped against plane `z=0`
- implicit `F(x,y,z)=0` projected faces are clipped against plane `z=0`
- implicit mesh extraction/cache itself is unchanged (no extra scalar-field remesh just for grid draw order)

When **Show Grid** is disabled, 3D formulas use the single-pass path (`All`) without plane clipping.

Dimension-arrow behavior:

- 3D dimension gizmo is mutually exclusive with coordinate overlays
- shown near the lower-left of the plot viewport when enabled

## Error Handling

- Parse/tokenization failures are attached to each `FormulaEntry`.
- Evaluation domain errors produce `NaN` and are skipped/neutralized during rendering.
- Startup failures show a message box from `main.cpp` and exit with non-zero code.

## Windowing and Graphics Notes

- API stack: Win32 + Direct3D11 + Dear ImGui
- Swap chain: double buffered, vsync present
- Device creation fallback:
  - Hardware + debug layer
  - Hardware without debug layer
  - WARP + debug layer
  - WARP without debug layer

## License

This document is licensed under the MIT License. See [`../LICENSE`](../LICENSE).
