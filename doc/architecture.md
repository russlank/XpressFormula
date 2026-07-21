<!-- SPDX-License-Identifier: MIT -->
# Architecture

## High-Level Design

XpressFormula uses a modular monolith built from explicit production modules:

- `Expression`: expression tokenization, parsing, AST queries, compilation/classification, and evaluation
- `Model`: durable formula/view models and pure scene analysis
- `Plotting`: draw routines for grid/axes/curves/heat maps/implicit contours and 3D surfaces (explicit + implicit)
- `UI`: ImGui panels and reusable immediate-mode components
- `App`: application orchestration, controllers, export/update workflows, and Win32/DX11 backend integration

Production library boundaries are build- and CI-checked. See [`architecture-dependencies.md`](architecture-dependencies.md) for the current project graph, forbidden dependencies, and remaining exceptions.

## Production Project Boundaries

Current production projects:

- `XpressFormula.Expression`: expression tokenization, parsing, shared AST queries, formula compilation/classification, evaluation, function metadata, examples, and version parsing helpers.
- `XpressFormula.Model`: stable formula identity/state, revision-tracked document mutations, scene-summary analysis, persistent view state, transient viewport geometry, plot policy, and `ViewTransform`.
- `XpressFormula.Plotting`: pure render planning, projection, geometry/sampling/meshing helpers, mesh cache policy, and the current ImGui draw-list backend.
- `XpressFormula.Infrastructure`: JSON parsing/writing, UTF and atomic file helpers, project persistence, export settings/metadata/output workflow, image processing, and Windows platform services.
- `XpressFormula.UI`: ImGui panels, components, UiKit, and Dear ImGui core sources. Reusable UI returns explicit actions instead of owning project side effects.
- `XpressFormula.App`: application orchestration, document/project/export/update controllers, export output adapters, runtime plot state, and Win32/DX11 ImGui backend sources.
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

Some file paths still reflect the original source layout, but project ownership now carries the architecture boundary. Prefer preserving ownership and dependency direction over broad folder movement.

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
  - Stable formula identity, string-backed formula state, display state, z-slice, compiled formula, and compiler diagnostics.
- [`src/XpressFormula/Model/SceneSummary.h`](../src/XpressFormula/Model/SceneSummary.h) and [`src/XpressFormula/Model/SceneSummary.cpp`](../src/XpressFormula/Model/SceneSummary.cpp)
  - Pure visible-scene capability analysis for render-mode planning, toolbar state, idle redraw scheduling, and plot rendering.
- [`src/XpressFormula/Model/ViewState.h`](../src/XpressFormula/Model/ViewState.h)
  - Persistent center/scale view state and transient viewport geometry types.
- [`src/XpressFormula/Core/Evaluator.h`](../src/XpressFormula/Core/Evaluator.h) and [`src/XpressFormula/Core/Evaluator.cpp`](../src/XpressFormula/Core/Evaluator.cpp)
  - Evaluates AST values with fixed `x`, `y`, and `z` sample slots and registry-backed function callbacks.
- [`src/XpressFormula/Core/ViewTransform.h`](../src/XpressFormula/Core/ViewTransform.h) and [`src/XpressFormula/Core/ViewTransform.cpp`](../src/XpressFormula/Core/ViewTransform.cpp)
  - Handles world-to-screen mapping, zoom, pan, and grid spacing from explicit `ViewState` plus `Viewport`.
- [`src/XpressFormula/Core/UpdateVersionUtils.h`](../src/XpressFormula/Core/UpdateVersionUtils.h)
  - Small header-only utilities for semantic-version parsing/comparison and extracting GitHub release fields from API JSON.
- [`src/XpressFormula/UI/Application.h`](../src/XpressFormula/UI/Application.h) and [`src/XpressFormula/UI/Application.cpp`](../src/XpressFormula/UI/Application.cpp)
  - Owns Win32 window lifecycle, D3D11 resources, ImGui lifecycle, frame orchestration, controller coordination, D3D-specific export rendering, texture readback, and high-level workflow state.
- [`src/XpressFormula/UI/UiKit`](../src/XpressFormula/UI/UiKit)
  - Thin immediate-mode UI helpers: shared metrics, pure responsive layout planners, RAII ImGui scopes, deterministic toolbar rows, property grids, splitter sizing, and modal sizing.
- [`src/XpressFormula/UI/Components`](../src/XpressFormula/UI/Components)
  - Reusable XpressFormula-specific UI components such as `PlotToolbar`, `ProjectControls`, and `FormulaCard`. Components render supplied state and return explicit actions; they do not perform file, clipboard, shell, HTTP, or formula-list ownership work.
- [`src/XpressFormula/UI/FormulaPanel.h`](../src/XpressFormula/UI/FormulaPanel.h) and [`src/XpressFormula/UI/FormulaPanel.cpp`](../src/XpressFormula/UI/FormulaPanel.cpp)
  - Formula list display and editor modal workflow. It reports document commands such as add/update/duplicate/remove/move/color/visibility/z-slice/hide-others; `Model::Document` owns the actual mutations and revision changes.
- [`src/XpressFormula/UI/FormulaEditorState.h`](../src/XpressFormula/UI/FormulaEditorState.h)
  - Temporary formula-editor state for target ID, editor text, and cached validation preview.
- [`src/XpressFormula/UI/FormulaEntry.h`](../src/XpressFormula/UI/FormulaEntry.h) and [`src/XpressFormula/UI/FormulaPresentation.h`](../src/XpressFormula/UI/FormulaPresentation.h)
  - `FormulaEntry` is a transitional alias for `Model::Formula`; presentation helpers derive user-facing labels and display counts from compiled formula kind.
- [`src/XpressFormula/UI/ControlPanel.h`](../src/XpressFormula/UI/ControlPanel.h) and [`src/XpressFormula/UI/ControlPanel.cpp`](../src/XpressFormula/UI/ControlPanel.cpp)
  - Global 2D view controls, display toggles (grid/coordinates/wires), reusable property-grid rows for 3D/heatmap controls, and export dialog launch action.
- [`src/XpressFormula/UI/PlotPanel.h`](../src/XpressFormula/UI/PlotPanel.h) and [`src/XpressFormula/UI/PlotPanel.cpp`](../src/XpressFormula/UI/PlotPanel.cpp)
  - Interactive plotting area and mouse interactions. It delegates render planning to `Plotting::buildPlotRenderPlan` and draws with effective settings, export overrides, and optional runtime camera azimuth.
- [`src/XpressFormula/Model/Document.h`](../src/XpressFormula/Model/Document.h) and [`src/XpressFormula/Model/Document.cpp`](../src/XpressFormula/Model/Document.cpp)
  - Live formulas, persistent view state, plot settings, revision and saved-revision tracking, and dirty-state calculation.
- [`src/XpressFormula/Application/ProjectController.h`](../src/XpressFormula/Application/ProjectController.h) and [`src/XpressFormula/Application/ProjectController.cpp`](../src/XpressFormula/Application/ProjectController.cpp)
  - Current project path, New/Open/Save/Save As workflow, recent-project integration, unsaved-change flow, project status, and side-effect requests.
- [`src/XpressFormula/Infrastructure/Serialization`](../src/XpressFormula/Infrastructure/Serialization)
  - Shared JSON value/parser/writer infrastructure.
- [`src/XpressFormula/Infrastructure/Persistence/ProjectSession.h`](../src/XpressFormula/Infrastructure/Persistence/ProjectSession.h)
  - Versioned `.xfplot` DTO records only.
- [`src/XpressFormula/Infrastructure/Persistence/ProjectSerializer.h`](../src/XpressFormula/Infrastructure/Persistence/ProjectSerializer.h) and [`src/XpressFormula/Infrastructure/Persistence/ProjectSerializer.cpp`](../src/XpressFormula/Infrastructure/Persistence/ProjectSerializer.cpp)
  - `ProjectSession` JSON serialization, parsing, and schema validation.
- [`src/XpressFormula/Infrastructure/Persistence/ProjectMapper.h`](../src/XpressFormula/Infrastructure/Persistence/ProjectMapper.h) and [`src/XpressFormula/Infrastructure/Persistence/ProjectMapper.cpp`](../src/XpressFormula/Infrastructure/Persistence/ProjectMapper.cpp)
  - Safe mapping between persistence DTOs and `Model::Document` state.
- [`src/XpressFormula/Infrastructure/Persistence/ProjectRepository.h`](../src/XpressFormula/Infrastructure/Persistence/ProjectRepository.h) and [`src/XpressFormula/Infrastructure/Persistence/ProjectRepository.cpp`](../src/XpressFormula/Infrastructure/Persistence/ProjectRepository.cpp)
  - Project file reading and atomic project writing.
- [`src/XpressFormula/Infrastructure/Persistence/RecentProjectsStore.h`](../src/XpressFormula/Infrastructure/Persistence/RecentProjectsStore.h) and [`src/XpressFormula/Infrastructure/Persistence/RecentProjectsStore.cpp`](../src/XpressFormula/Infrastructure/Persistence/RecentProjectsStore.cpp)
  - Recent-project storage, deduplication, and cleanup.
- [`src/XpressFormula/Version.h`](../src/XpressFormula/Version.h)
  - Centralized semantic version metadata used by window title, resources, and packaging.
- [`src/XpressFormula/Plotting/PlotRenderer.h`](../src/XpressFormula/Plotting/PlotRenderer.h) and [`src/XpressFormula/Plotting/PlotRenderer.cpp`](../src/XpressFormula/Plotting/PlotRenderer.cpp)
  - Rendering primitives and formula visualizations (2D + 3D), with shared construction of 3D surface options from model plot policy.

## Runtime Flow

1. [`src/XpressFormula/main.cpp`](../src/XpressFormula/main.cpp) constructs `UI::Application`.
2. `Application::initialize()` creates Win32 window, D3D11 swap chain/device, and ImGui context.
3. `Application::run()` drives the message loop and rendering frames (including idle redraw optimization).
4. `FormulaPanel` reports formula commands, and `Model::Document` applies only real changes, incrementing revision/dirty state only when persistent state changes.
5. `Application` calls `Model::analyzeScene()` after document revision changes and passes the resulting `SceneSummary` to the control panel, toolbar, and plot panel.
6. `PlotPanel` updates only the transient viewport geometry on `ViewTransform` and delegates render planning plus drawing to `Plotting`.
7. `Plotting` evaluates formulas through `Core::Evaluator`, plans passes/effective settings, and draws based on variable dimensionality and equation form.
8. `Application` also polls a background GitHub release check future and updates sidebar notification state when a result arrives.
9. Auto-rotation is tracked as transient runtime azimuth offset, not by editing saved plot settings. New/open, camera preset/reset/manual camera edit, and disabling auto-rotate reset the runtime offset.
10. Export requests resolve aspect/framing settings, trigger a plot-only offscreen render pass (temporary D3D11 render target and viewport) with export-specific overrides, then post-processing (pixel-format normalization, optional resize/grayscale) before file/clipboard output. Exports use the persisted base camera azimuth unless a caller intentionally supplies a runtime camera value.
11. Project New/Open/Save/Save As workflows stay in application controllers; `.xfplot` parsing completes before active formulas, view, or plot settings are mutated.

## Project Persistence Boundary

`Model::Document` owns live formulas, persistent view state, plot settings, revision, saved revision, and dirty-state calculation. Dirty state is revision-based: the document is clean when `revision() == savedRevision()`. Persistent mutations increment revision only when they change state, successful saves call `markSaved()`, and New/Open replace the document with a clean loaded state. Transient viewport geometry and runtime auto-rotation do not dirty the document.

`Application::ProjectController` owns the current project path, New/Open/Save/Save As workflow, recent-project integration, unsaved-change flow, project status, and side-effect requests. It coordinates persistence services but does not own JSON parsing, schema validation, or dirty-state comparison.

The persistence implementation is split by responsibility:

- `Infrastructure/Serialization`: shared JSON value/parser/writer.
- `Infrastructure/Persistence/ProjectSession.h`: versioned persistence DTOs only.
- `Infrastructure/Persistence/ProjectSerializer.*`: DTO serialization, parsing, and schema validation.
- `Infrastructure/Persistence/ProjectMapper.*`: `ProjectSession` <-> `Model::Document` mapping.
- `Infrastructure/Persistence/ProjectRepository.*`: project file reading and atomic project writing.
- `Infrastructure/Persistence/RecentProjectsStore.*`: recent-project storage, deduplication, and cleanup.

Important rules:

- Build a `ProjectSession` DTO from `Model::Document` state before saving.
- Parse and validate a full project file before mutating active application state.
- Retain invalid loaded formulas where possible and report load warnings after reparsing.
- Persist formula expression, color, visibility, and z-slice only; runtime IDs, compiled ASTs, diagnostics, and editor state are rebuilt in memory.
- Persist the base 3D camera azimuth/elevation/z-scale; auto-rotation's per-frame offset is runtime-only and is never written to `.xfplot`.
- Clamp or ignore unsafe numeric values before applying them to the live view and plot settings.
- Persist only `ViewState` center/scale values; viewport origin/size is frame layout state and is not written to `.xfplot`.
- Keep unknown fields tolerated for schema version 1 so future writers can add data without breaking older builds.
- Write project files through a temporary file followed by replacement so failed writes do not leave a partial target file.
- Treat dirty state as a revision comparison against the last saved revision.

## UI Toolkit Boundary

The UI toolkit is intentionally small and immediate-mode. `UiKit` does not own application state, retain widget objects, or replace ordinary ImGui controls. It centralizes policies that are easy to get wrong when repeated inline:

- responsive breakpoints and shared dimensions in `UiMetrics`
- pure layout decisions that can be unit tested
- row-height and cursor placement mechanics for responsive toolbars
- scope safety for ImGui push/pop and disabled blocks
- repeated table layout behavior for property controls

Domain components live one level above `UiKit`. They render reusable XpressFormula UI surfaces and return explicit action structs for one-shot commands. Panels and `Application` remain responsible for workflows, while persistent formula/view/plot mutations route through `Model::Document`. File, clipboard, shell, HTTP, and image-encoding work stays outside reusable UI components.

## Architecture Boundary Check

Run the lightweight boundary check from the repository root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\check-architecture-boundaries.ps1
```

The script checks that infrastructure does not include UI, Expression and Model stay free of UI/Win32/ImGui/JSON dependencies, pure plotting geometry/meshing stays free of ImGui, reusable UI does not implement platform internals, project references follow the documented dependency direction, first-party projects use the repository toolset/warning policy, production `.cpp` files have one owning project, tests do not compile production `.cpp` files directly, public docs do not reference private planning files, and built-in functions remain covered by the public expression reference.

## Quality Gate

First-party app, library, and test projects build with MSVC warning level `/W4`, conformance mode (`/permissive-`), and `/Zc:__cplusplus`. The repository default toolset is `v145`. First-party warnings should be fixed at source. Vendor warnings are isolated through project configuration when needed.

Pull-request validation runs the architecture boundary check, Debug x64 app/test builds and tests, and Release x64 app/test builds and tests. Release packaging also runs the boundary check and Release test suite before creating packages.

## Architecture Decisions

Durable architecture decisions are recorded in [`doc/adr`](adr/README.md):

- [0001: Modular Monolith](adr/0001-modular-monolith.md)
- [0002: Internal JSON](adr/0002-internal-json.md)
- [0003: Document Revision Dirty State](adr/0003-document-revision-dirty-state.md)
- [0004: Geometry Render Backend](adr/0004-geometry-render-backend.md)

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

- Parse/tokenization failures are exposed through each `Model::Formula` compiled diagnostic list.
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
