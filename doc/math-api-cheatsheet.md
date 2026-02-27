<!-- SPDX-License-Identifier: MIT -->
# Math API Cheatsheet (Quick Reference + Where It Is Used)

This is a practical companion to [`math-api-guide.md`](math-api-guide.md).

Use this page when you want a fast answer to:

- "What type/function should I use?"
- "What mathematical concept does it represent?"
- "Where in the app is it used?"

## 1. Core Math Pipeline (One-Page Mental Model)

```text
Formula text
  -> Tokenizer (text -> tokens)
  -> Parser (tokens -> AST)
  -> FormulaEntry (AST + equation normalization + render kind classification)
  -> Evaluator (AST + variables -> double samples)
  -> PlotRenderer (samples -> lines/contours/triangles)
  -> ImGui DrawList (pixels)
```

Primary files:

- [`src/XpressFormula/Core/Tokenizer.cpp`](../src/XpressFormula/Core/Tokenizer.cpp)
- [`src/XpressFormula/Core/Parser.cpp`](../src/XpressFormula/Core/Parser.cpp)
- [`src/XpressFormula/UI/FormulaEntry.h`](../src/XpressFormula/UI/FormulaEntry.h)
- [`src/XpressFormula/Core/Evaluator.cpp`](../src/XpressFormula/Core/Evaluator.cpp)
- [`src/XpressFormula/Plotting/PlotRenderer.cpp`](../src/XpressFormula/Plotting/PlotRenderer.cpp)

## 2. Core Types Cheatsheet (`Core`)

### `Core::TokenType`

- Represents: lexical categories (number, identifier, operator, punctuation)
- Defined in: [`src/XpressFormula/Core/Token.h`](../src/XpressFormula/Core/Token.h)
- Used by:
  - `Tokenizer` to emit tokens
  - `Parser` to drive grammar decisions
  - error messages via `tokenTypeName(...)`

Common values:

- `Number`
- `Identifier`
- `Plus`, `Minus`, `Star`, `Slash`, `Caret`
- `LeftParen`, `RightParen`, `Comma`
- `End`, `Error`

### `Core::Token`

- Represents: one token with original text and source position
- Defined in: [`src/XpressFormula/Core/Token.h`](../src/XpressFormula/Core/Token.h)
- Fields:
  - `type`
  - `value`
  - `position`
- Used by:
  - `Parser` for syntax decisions and error reporting

### `Core::ASTNode` / `Core::ASTNodePtr`

- Represents: parsed symbolic expression tree
- Defined in: [`src/XpressFormula/Core/ASTNode.h`](../src/XpressFormula/Core/ASTNode.h)
- Mathematical abstraction: expression syntax tree
- Used by:
  - `Parser` (produces AST)
  - `Evaluator` (interprets AST)
  - `FormulaEntry` (stores AST and equation-side ASTs)
  - `PlotRenderer` (evaluates AST repeatedly while sampling)

### `Core::NumberNode`

- Represents: numeric literal (`3.14`, `2`, `1e-5`)
- Defined in: [`src/XpressFormula/Core/ASTNode.h`](../src/XpressFormula/Core/ASTNode.h)
- Used by:
  - created by `Parser::parsePrimary()`
  - interpreted by `Evaluator::evaluate(...)`

### `Core::VariableNode`

- Represents: symbolic variable (`x`, `y`, `z`)
- Defined in: [`src/XpressFormula/Core/ASTNode.h`](../src/XpressFormula/Core/ASTNode.h)
- Used by:
  - parser variable detection
  - evaluator variable lookup
  - formula classification (`FormulaEntry`)

### `Core::BinaryOpNode`

- Represents: binary operation (`+`, `-`, `*`, `/`, `^`)
- Defined in: [`src/XpressFormula/Core/ASTNode.h`](../src/XpressFormula/Core/ASTNode.h)
- Mathematical abstraction: `op(left, right)`
- Used by:
  - parser precedence tree
  - equation normalization (`left - right`)
  - evaluator recursion

### `Core::UnaryOpNode`

- Represents: unary sign operators (`-a`, `+a`)
- Defined in: [`src/XpressFormula/Core/ASTNode.h`](../src/XpressFormula/Core/ASTNode.h)
- Used by:
  - parser unary rule
  - evaluator recursion

### `Core::FunctionCallNode`

- Represents: function application (`sin(x)`, `pow(a,b)`)
- Defined in: [`src/XpressFormula/Core/ASTNode.h`](../src/XpressFormula/Core/ASTNode.h)
- Used by:
  - parser function-call rule
  - evaluator function dispatch

### `Core::Parser::Result`

- Represents: parse attempt output
- Defined in: [`src/XpressFormula/Core/Parser.h`](../src/XpressFormula/Core/Parser.h)
- Includes:
  - `ast`
  - `error`
  - `variables`
- Used by:
  - `FormulaEntry::parse()` for formula/equation setup
  - formula editor live validation preview

### `Core::Evaluator::Variables`

- Type: `std::unordered_map<std::string, double>`
- Represents: variable assignment environment for evaluation
- Defined in: [`src/XpressFormula/Core/Evaluator.h`](../src/XpressFormula/Core/Evaluator.h)
- Used by:
  - all plot sampling paths (`Curve2D`, `Heatmap`, `CrossSection`, `Surface3D`, implicit contour/surface)

### `Core::Vec2`

- Represents: 2D point in screen space (float)
- Defined in: [`src/XpressFormula/Core/ViewTransform.h`](../src/XpressFormula/Core/ViewTransform.h)
- Used by:
  - world-to-screen outputs
  - 2D renderer helpers (`grid`, `axes`, `contours`, etc.)

### `Core::ViewTransform`

- Represents: world ↔ screen coordinate mapping + pan/zoom state
- Defined in:
  - [`src/XpressFormula/Core/ViewTransform.h`](../src/XpressFormula/Core/ViewTransform.h)
  - [`src/XpressFormula/Core/ViewTransform.cpp`](../src/XpressFormula/Core/ViewTransform.cpp)
- Used by:
  - `PlotPanel` (mouse interaction updates)
  - `ControlPanel` (zoom/pan/reset controls)
  - `PlotRenderer` (all draw modes)

Key fields:

- `centerX`, `centerY`
- `scaleX`, `scaleY`
- `screenWidth`, `screenHeight`
- `screenOriginX`, `screenOriginY`

Key methods:

- `worldToScreen(...)`
- `screenToWorld(...)`
- `zoomAll(...)`, `zoomX(...)`, `zoomY(...)`
- `pan(...)`, `panPixels(...)`
- `worldXMin()/Max()`, `worldYMin()/Max()`
- `gridSpacingX()/Y()`

## 3. Constants Cheatsheet

### `Core::PI`, `Core::E`, `Core::TAU`

- Defined in: [`src/XpressFormula/Core/MathConstants.h`](../src/XpressFormula/Core/MathConstants.h)
- Represents: standard math constants
- Used by:
  - parser constant resolution (`pi`, `e`, `tau`)
  - user expressions through parser constant handling

## 4. Formula Semantics Cheatsheet (`UI::FormulaEntry`)

File:

- [`src/XpressFormula/UI/FormulaEntry.h`](../src/XpressFormula/UI/FormulaEntry.h)

`FormulaEntry` is the app’s semantic layer between parsing and rendering.

### `FormulaRenderKind`

- Represents: inferred plotting mode for a formula
- Values:
  - `Curve2D`
  - `Surface3D`
  - `Implicit2D`
  - `ScalarField3D`
  - `Invalid`
- Used by:
  - `PlotPanel` to dispatch to the correct `PlotRenderer` method
  - formula editor validation (`typeLabel()`)

### `FormulaEntry::parse()`

- Represents: full parse + classify + validation step
- Used by:
  - formula list edits
  - formula editor Apply
  - formula editor live validation preview (temporary parse)

Main responsibilities:

1. trim input
2. parse plain expression or equation
3. normalize equation to `left - right`
4. collect variables
5. reject unsupported variables
6. infer render kind

### `FormulaEntry::isEquation`

- Represents: whether the user input contained `=`
- Used by:
  - classification
  - labeling (`F(x,y,z)=0` vs `f(x,y,z)`)
  - `PlotPanel` behavior (implicit surface vs cross-section for scalar fields)

### `FormulaEntry::leftAst` / `rightAst` / `ast`

- `leftAst` and `rightAst`: original equation sides
- `ast`: normalized expression used for evaluation

Used by:

- equation detection and explicit `z = ...` special case
- implicit contour/surface evaluation (`ast` as zero-form)

## 5. Plotting API Cheatsheet (`PlotRenderer`)

Files:

- [`src/XpressFormula/Plotting/PlotRenderer.h`](../src/XpressFormula/Plotting/PlotRenderer.h)
- [`src/XpressFormula/Plotting/PlotRenderer.cpp`](../src/XpressFormula/Plotting/PlotRenderer.cpp)

`PlotRenderer` consumes the math API and outputs ImGui draw primitives.

### `PlotRenderer::Surface3DOptions`

- Represents: camera + quality + styling options shared by 3D render modes
- Defined in: [`src/XpressFormula/Plotting/PlotRenderer.h`](../src/XpressFormula/Plotting/PlotRenderer.h)
- Populated in:
  - [`src/XpressFormula/UI/PlotPanel.cpp`](../src/XpressFormula/UI/PlotPanel.cpp)
- Originating UI state:
  - [`src/XpressFormula/UI/PlotSettings.h`](../src/XpressFormula/UI/PlotSettings.h)
  - controls in [`src/XpressFormula/UI/ControlPanel.cpp`](../src/XpressFormula/UI/ControlPanel.cpp)

Important fields:

- `azimuthDeg`, `elevationDeg`, `zScale`
- `resolution` (explicit surface)
- `implicitResolution` (implicit 3D mesh quality)
- `opacity`, `wireThickness`
- `showEnvelope`, `envelopeThickness`
- `showDimensionArrows`
- `implicitZCenter`
- `planePass` (`All`, `BelowGridPlane`, `AboveGridPlane`)
- `gridPlaneZ` (currently `0.0` for XY grid plane interleave)

### `drawCurve2D(...)`

- Mathematical abstraction: sample a scalar function along one dimension (`x`)
- Uses:
  - `Core::Evaluator`
  - `Core::ViewTransform`
  - `ImDrawList::AddLine(...)`
- Called from:
  - [`src/XpressFormula/UI/PlotPanel.cpp`](../src/XpressFormula/UI/PlotPanel.cpp)

### `drawHeatmap(...)`

- Mathematical abstraction: sample a 2D scalar field on a regular grid and color-map values
- Uses:
  - evaluator grid sampling
  - `heatColor(...)`
  - rectangle fills
- Called when:
  - `z=f(x,y)` is shown in 2D heatmap mode

### `drawCrossSection(...)`

- Mathematical abstraction: evaluate `f(x,y,z0)` over x/y for fixed `z0`
- Used for:
  - scalar-field cross-sections (`f(x,y,z)`)
  - non-equation scalar-field rendering in 2D mode

### `drawSurface3D(...)`

- Mathematical abstraction: explicit mesh for `z=f(x,y)`
- Core steps:
  - sample x/y grid
  - evaluate z
  - project vertices
  - build triangles
  - optional plane clipping pass (for `BelowGridPlane` / `AboveGridPlane`)
  - depth-sort
  - draw fill + optional wireframe

### `drawImplicitContour2D(...)`

- Mathematical abstraction: marching squares on `F(x,y)=0`
- Used for:
  - implicit 2D equations such as circles/ellipses

### `drawImplicitSurface3D(...)`

- Mathematical abstraction: sampled implicit isosurface `F(x,y,z)=0`
- Current implementation:
  - surface-nets style extraction
  - cached world-space mesh
  - optional projected-face clipping pass for grid-plane interleave
  - per-frame projection and painter sorting
- Used for:
  - implicit 3D equations (sphere, torus, etc.)

### `drawGrid3D(...)` / `drawAxes3D(...)`

- `drawGrid3D(...)` draws projected XY plane visuals in 3D mode:
  - translucent plane fill
  - major/minor interior lines
  - thicker projected frame
- `drawAxes3D(...)` draws projected XYZ axes.

In 3D mode with grid visible, `PlotPanel` uses this order:

1. formulas with `planePass=BelowGridPlane`
2. `drawGrid3D(...)`
3. formulas with `planePass=AboveGridPlane`
4. `drawAxes3D(...)` (if coordinates enabled)

## 6. Where the Math API Is Used in the UI Flow

This is the end-to-end call chain you will most often debug.

### A. User edits a formula

UI entry points:

- `FormulaPanel` inline editor
- `FormulaPanel` modal editor dialog

What happens:

1. text copied into `FormulaEntry::inputBuffer`
2. `FormulaEntry::parse()` runs
3. AST / errors / variables / render kind are updated

Files to inspect:

- [`src/XpressFormula/UI/FormulaPanel.cpp`](../src/XpressFormula/UI/FormulaPanel.cpp)
- [`src/XpressFormula/UI/FormulaEntry.h`](../src/XpressFormula/UI/FormulaEntry.h)

### B. Plot is drawn each frame

UI orchestration:

1. `Application::renderFrame()`
2. `PlotPanel::render(...)`
3. `PlotRenderer::*` selected by `FormulaRenderKind`

Files to inspect:

- [`src/XpressFormula/UI/Application.cpp`](../src/XpressFormula/UI/Application.cpp)
- [`src/XpressFormula/UI/PlotPanel.cpp`](../src/XpressFormula/UI/PlotPanel.cpp)
- [`src/XpressFormula/Plotting/PlotRenderer.cpp`](../src/XpressFormula/Plotting/PlotRenderer.cpp)

### C. View changes (pan/zoom)

Math objects involved:

- `Core::ViewTransform`

Used by:

- `ControlPanel` buttons/sliders for zoom/pan/reset
- `PlotPanel` mouse drag / wheel zoom
- all renderer world-range calculations

Files:

- [`src/XpressFormula/UI/ControlPanel.cpp`](../src/XpressFormula/UI/ControlPanel.cpp)
- [`src/XpressFormula/UI/PlotPanel.cpp`](../src/XpressFormula/UI/PlotPanel.cpp)
- [`src/XpressFormula/Core/ViewTransform.cpp`](../src/XpressFormula/Core/ViewTransform.cpp)

## 7. Quick "Which API Do I Touch?" Table

| Goal | Primary place | Usually also touch |
|---|---|---|
| Add a new math function (e.g. `clamp`) | `Evaluator.cpp` | `Parser.cpp`, `FormulaPanel.cpp` function list, docs/tests |
| Add a constant (e.g. `phi`) | `MathConstants.h` | `Parser.cpp`, docs/tests |
| Change operator behavior/precedence | `Parser.cpp` | tests, docs |
| Change invalid math handling | `Evaluator.cpp` | renderers (if `NaN` policy changes) |
| Add a new plot mode | `FormulaEntry.h` + `PlotPanel.cpp` | `PlotRenderer.*`, docs |
| Change pan/zoom/world range behavior | `ViewTransform.cpp` | `ControlPanel.cpp`, `PlotPanel.cpp` |
| Improve implicit surface meshing | `PlotRenderer.cpp` | docs, performance settings/UI |

## 8. Real Code Snippets (Short and Useful)

### Parse a user expression

```cpp
auto result = XpressFormula::Core::Parser::parse("sin(x) + 2");
if (!result.success()) {
    // result.error contains a user-facing parse error
}
```

Where used:

- formula parsing in [`src/XpressFormula/UI/FormulaEntry.h`](../src/XpressFormula/UI/FormulaEntry.h)

### Evaluate an AST at a sample point

```cpp
XpressFormula::Core::Evaluator::Variables vars;
vars["x"] = 1.0;
vars["y"] = 2.0;
double v = XpressFormula::Core::Evaluator::evaluate(ast, vars);
```

Where used:

- grid/curve/surface sampling in [`src/XpressFormula/Plotting/PlotRenderer.cpp`](../src/XpressFormula/Plotting/PlotRenderer.cpp)

### Convert world coordinate to pixel coordinate

```cpp
XpressFormula::Core::Vec2 p = vt.worldToScreen(wx, wy);
```

Where used:

- 2D rendering and contour drawing in `PlotRenderer`
- hover labels / interaction conversions via `screenToWorld(...)`

### Equation normalization (conceptual)

```cpp
ast = std::make_shared<Core::BinaryOpNode>(
    Core::BinaryOperator::Subtract, leftAst, rightAst);
```

Where used:

- [`src/XpressFormula/UI/FormulaEntry.h`](../src/XpressFormula/UI/FormulaEntry.h)

## 9. Standard Library Dependency Cheatsheet (Why Each Matters)

### `<cmath>`

Used for:

- trig (`sin`, `cos`, `atan2`)
- exponentiation (`pow`)
- roots/logs
- projection math (`cos`, `sin`, `sqrt`)

Appears in:

- `Evaluator.cpp`
- `ViewTransform.cpp`
- `PlotRenderer.cpp`

### `<unordered_map>`

Used for:

- variable lookup by name during evaluation

Appears in:

- `Evaluator.h`

### `<vector>`

Used for:

- tokens
- AST function args
- sampled grids
- mesh faces/vertices
- draw staging arrays

Appears in:

- tokenizer/parser/AST/renderer

### `<set>`

Used for:

- variable collection
- built-in function name validation
- constant name detection

Appears in:

- `Parser.*`
- `FormulaEntry.h`

### `<limits>`

Used for:

- `NaN`
- numeric sentinels for min/max bounds

Appears in:

- `Evaluator.cpp`
- `PlotRenderer.cpp`

## 10. Debugging Cheat Notes (Common Issues)

### "Formula parses but nothing renders"

Check:

1. `FormulaEntry::error`
2. `FormulaEntry::renderKind`
3. `View Range` in control panel
4. implicit `z center`
5. whether the shape is clipped by current sampling domain

### "Shape is visible but cut open"

Likely cause:

- implicit surface sampling box clips the shape

Fix:

- reset view / recenter / zoom out
- keep `z center` near the shape center

### "Rendering is heavy"

Likely cause:

- implicit 3D mesh extraction or high triangle count

Tuning points:

- lower `Implicit Surface Quality (F=0)`
- reduce/disable wireframe
- rely on implicit mesh cache for camera-only movement

## 11. Related Docs

- Deep explanation: [`math-api-guide.md`](math-api-guide.md)
- Algorithms: [`algorithms-guide.md`](algorithms-guide.md)
- ImGui UI structure: [`imgui-implementation-guide.md`](imgui-implementation-guide.md)
- Syntax reference: [`expression-language.md`](expression-language.md)

## License

This document is licensed under the MIT License. See [`../LICENSE`](../LICENSE).
