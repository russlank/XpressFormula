# Architecture

## High-Level Design

XpressFormula is organized into three primary layers:

- `Core`: expression tokenization, parsing, evaluation, coordinate transforms
- `UI`: ImGui panels and application orchestration
- `Plotting`: draw routines for grid/axes/curves/heat maps

## Module Breakdown

- `src/XpressFormula/Core/Tokenizer.*`
  - Converts expression text into token stream.
- `src/XpressFormula/Core/Parser.*`
  - Recursive-descent parser producing an AST.
- `src/XpressFormula/Core/Evaluator.*`
  - Evaluates AST values for provided variables.
- `src/XpressFormula/Core/ViewTransform.*`
  - Handles world-to-screen mapping, zoom, pan, and grid spacing.
- `src/XpressFormula/UI/Application.*`
  - Owns Win32 window, D3D11 resources, ImGui lifecycle, frame loop.
- `src/XpressFormula/UI/FormulaPanel.*`
  - Formula list management and per-formula controls.
- `src/XpressFormula/UI/ControlPanel.*`
  - Global camera/view controls.
- `src/XpressFormula/UI/PlotPanel.*`
  - Interactive plotting area and mouse interactions.
- `src/XpressFormula/Plotting/PlotRenderer.*`
  - Rendering primitives and formula visualizations.

## Runtime Flow

1. `main.cpp` constructs `UI::Application`.
2. `Application::initialize()` creates Win32 window, D3D11 swap chain/device, and ImGui context.
3. `Application::run()` drives the message loop and rendering frames.
4. `FormulaPanel` updates formula text and triggers parse.
5. `PlotPanel` updates `ViewTransform` from current viewport and delegates drawing to `PlotRenderer`.
6. `PlotRenderer` evaluates formulas through `Core::Evaluator` and draws based on variable dimensionality.

## Formula Rendering Modes

Formula mode is inferred from variable presence:

- `f(x)` if no `y`/`z` variable is detected
- `f(x,y)` if `y` is present and `z` is absent
- `f(x,y,z)` if `z` is present

Render mapping:

- `f(x)` -> curve line sampling across screen width
- `f(x,y)` -> heat map
- `f(x,y,z)` -> heat map cross-section at selected `z`

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
