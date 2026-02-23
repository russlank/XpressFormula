<!-- SPDX-License-Identifier: MIT -->
# Architecture

## High-Level Design

XpressFormula is organized into three primary layers:

- `Core`: expression tokenization, parsing, evaluation, coordinate transforms
- `UI`: ImGui panels and application orchestration
- `Plotting`: draw routines for grid/axes/curves/heat maps/implicit contours and 3D surfaces (explicit + implicit)

## Module Breakdown

- [`src/XpressFormula/Core/Tokenizer.h`](../src/XpressFormula/Core/Tokenizer.h) and [`src/XpressFormula/Core/Tokenizer.cpp`](../src/XpressFormula/Core/Tokenizer.cpp)
  - Converts expression text into token stream.
- [`src/XpressFormula/Core/Parser.h`](../src/XpressFormula/Core/Parser.h) and [`src/XpressFormula/Core/Parser.cpp`](../src/XpressFormula/Core/Parser.cpp)
  - Recursive-descent parser producing an AST.
- [`src/XpressFormula/Core/Evaluator.h`](../src/XpressFormula/Core/Evaluator.h) and [`src/XpressFormula/Core/Evaluator.cpp`](../src/XpressFormula/Core/Evaluator.cpp)
  - Evaluates AST values for provided variables.
- [`src/XpressFormula/Core/ViewTransform.h`](../src/XpressFormula/Core/ViewTransform.h) and [`src/XpressFormula/Core/ViewTransform.cpp`](../src/XpressFormula/Core/ViewTransform.cpp)
  - Handles world-to-screen mapping, zoom, pan, and grid spacing.
- [`src/XpressFormula/UI/Application.h`](../src/XpressFormula/UI/Application.h) and [`src/XpressFormula/UI/Application.cpp`](../src/XpressFormula/UI/Application.cpp)
  - Owns Win32 window, D3D11 resources, ImGui lifecycle, frame loop.
- [`src/XpressFormula/UI/FormulaPanel.h`](../src/XpressFormula/UI/FormulaPanel.h) and [`src/XpressFormula/UI/FormulaPanel.cpp`](../src/XpressFormula/UI/FormulaPanel.cpp)
  - Formula list management and per-formula controls.
- [`src/XpressFormula/UI/ControlPanel.h`](../src/XpressFormula/UI/ControlPanel.h) and [`src/XpressFormula/UI/ControlPanel.cpp`](../src/XpressFormula/UI/ControlPanel.cpp)
  - Global 2D view controls, display toggles (grid/coordinates/wires), 3D surface camera settings, and export dialog launch action.
- [`src/XpressFormula/UI/PlotPanel.h`](../src/XpressFormula/UI/PlotPanel.h) and [`src/XpressFormula/UI/PlotPanel.cpp`](../src/XpressFormula/UI/PlotPanel.cpp)
  - Interactive plotting area, mouse interactions, and export-time plot render overrides (background/grid/coordinates/wires).
- [`src/XpressFormula/Version.h`](../src/XpressFormula/Version.h)
  - Centralized semantic version metadata used by window title, resources, and packaging.
- [`src/XpressFormula/Plotting/PlotRenderer.h`](../src/XpressFormula/Plotting/PlotRenderer.h) and [`src/XpressFormula/Plotting/PlotRenderer.cpp`](../src/XpressFormula/Plotting/PlotRenderer.cpp)
  - Rendering primitives and formula visualizations (2D + 3D).

## Runtime Flow

1. [`src/XpressFormula/main.cpp`](../src/XpressFormula/main.cpp) constructs `UI::Application`.
2. `Application::initialize()` creates Win32 window, D3D11 swap chain/device, and ImGui context.
3. `Application::run()` drives the message loop and rendering frames.
4. `FormulaPanel` updates formula text and triggers parse.
5. `PlotPanel` updates `ViewTransform` from current viewport and delegates drawing to `PlotRenderer`.
6. `PlotRenderer` evaluates formulas through `Core::Evaluator` and draws based on variable dimensionality and equation form.
7. Export requests are captured after rendering and can apply export-only render overrides plus post-processing (resize/grayscale) before file/clipboard output.

## Formula Rendering Modes

Formula mode is inferred from variable presence and equation shape:

- `y=f(x)` if no `y`/`z` variable is detected
- `z=f(x,y)` if `x` and `y` are present, or if equation is solved for `z`
- `F(x,y)=0` for implicit equations such as `x^2+y^2=100`
- `f(x,y,z)` scalar field if `x`, `y`, and `z` are present (cross-section in 2D mode)
- `F(x,y,z)=0` for implicit 3D equations such as `x^2+y^2+z^2=16`

Render mapping:

- `y=f(x)` -> curve line sampling across screen width
- `z=f(x,y)` -> 3D surface (or optional 2D heat map)
- `F(x,y)=0` -> marching-squares contour rendering
- `f(x,y,z)` -> heat map cross-section at selected `z`
- `F(x,y,z)=0` -> implicit 3D surface mesh in 3D mode, or scalar cross-section in 2D heat-map mode

## Current 3D Implicit Surface Notes

- Implicit `F(x,y,z)=0` surfaces are extracted from sampled scalar-field data using a surface-nets style mesh.
- The extracted world-space mesh is cached and re-used across camera-only changes (azimuth/elevation/z-scale/style), then re-projected each frame.
- The implicit sampling domain is derived from the current visible `x/y` range and the formula `z slice / center`, so shapes can appear clipped if the view box does not fully contain them.

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
