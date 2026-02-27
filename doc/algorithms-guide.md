<!-- SPDX-License-Identifier: MIT -->
# Algorithms Guide (Beginner-Friendly)

This document explains the core algorithms used in XpressFormula in a pedagogical way.

It is written for newcomers to:

- lexical analysis (tokenizing)
- parsing and ASTs
- expression evaluation
- plotting and sampling
- contour extraction and surface meshing
- basic 3D projection in an ImGui draw-list renderer

The goal is not only to describe "what the code does", but also "why this approach was chosen".

## Big Picture

When a user types a formula, the application follows this pipeline:

1. Text input (for example `sin(x)` or `x^2 + y^2 + z^2 = 16`)
2. Tokenization (split text into tokens like numbers, identifiers, operators)
3. Parsing (build an AST: Abstract Syntax Tree)
4. Formula classification (2D curve, 3D surface, implicit equation, scalar field)
5. Evaluation by sampling points on a grid or along a line
6. Convert sampled data into drawing primitives (lines, quads, triangles)
7. Draw with ImGui `ImDrawList`

Files involved:

- Tokenizer: [`src/XpressFormula/Core/Tokenizer.h`](../src/XpressFormula/Core/Tokenizer.h)
- Parser: [`src/XpressFormula/Core/Parser.h`](../src/XpressFormula/Core/Parser.h)
- Evaluator: [`src/XpressFormula/Core/Evaluator.h`](../src/XpressFormula/Core/Evaluator.h)
- Formula classification: [`src/XpressFormula/UI/FormulaEntry.h`](../src/XpressFormula/UI/FormulaEntry.h)
- Plot rendering: [`src/XpressFormula/Plotting/PlotRenderer.h`](../src/XpressFormula/Plotting/PlotRenderer.h)

## Part 1: Expression Handling

### 1. Tokenization (Lexical Analysis)

Tokenization converts raw text into a stream of small pieces ("tokens").

Example:

`x^2 + y^2`

becomes conceptually:

- identifier `x`
- operator `^`
- number `2`
- operator `+`
- identifier `y`
- operator `^`
- number `2`

What the tokenizer recognizes:

- numbers (`12`, `3.14`, `2e-5`)
- identifiers (`x`, `y`, `z`, `sin`, `pow`)
- operators and punctuation (`+ - * / ^ ( ) ,`)
- whitespace (ignored)

Implementation notes:

- The tokenizer is a single pass over the string.
- It tracks the character position so parser errors can point to the correct place.
- It always appends an `End` token so the parser knows when input is finished.

Where to read:

- [`src/XpressFormula/Core/Tokenizer.cpp`](../src/XpressFormula/Core/Tokenizer.cpp)

### 2. Parsing (Recursive-Descent Parser)

The parser turns tokens into an AST (Abstract Syntax Tree).

Why an AST?

- It preserves operator precedence correctly.
- It makes evaluation easier and reusable.
- It lets the app inspect variables (`x`, `y`, `z`) after parsing.

The parser uses a recursive-descent design:

- `parseExpression()` handles `+` and `-`
- `parseTerm()` handles `*` and `/`
- `parsePower()` handles `^`
- `parseUnary()` handles unary `+` and `-`
- `parsePrimary()` handles numbers, variables, constants, function calls, and parentheses

This naturally encodes precedence.

### 3. Operator Precedence and Associativity

The parser uses this precedence (low to high):

1. `+`, `-`
2. `*`, `/`
3. `^`
4. unary `+`, unary `-`
5. literals, variables, function calls, parentheses

Important detail:

- `^` is parsed as right-associative.
- So `a^b^c` means `a^(b^c)` (not `(a^b)^c`).

This is mathematically common, but it can surprise users.

Example typo:

- `x^2+y^2^z^2`

This is not the same as:

- `x^2 + y^2 + z^2`

Where to read:

- [`src/XpressFormula/Core/Parser.cpp`](../src/XpressFormula/Core/Parser.cpp)

### 4. AST Node Types (What the Parser Builds)

The AST is composed of node types such as:

- Number node
- Variable node
- Unary operator node
- Binary operator node
- Function call node

Example AST for `sin(x) + 2` (conceptual):

- Binary `+`
- left: Function `sin`
- arg0: Variable `x`
- right: Number `2`

The app later evaluates this tree many times with different variable values.

### 5. Constants and Built-In Functions

The parser recognizes constants like:

- `pi`
- `e`
- `tau`

It also validates function names against a built-in list (`sin`, `cos`, `pow`, `log`, ...).

Why validate during parsing?

- Better error messages for users (`Unknown function 'foo'`)
- Avoids ambiguous handling later during evaluation

## Part 2: Equation Handling (`left = right`)

XpressFormula supports both plain expressions and equations.

Examples:

- Expression: `sin(x)`
- Equation: `x^2 + y^2 = 100`

### 1. How Equations Are Represented Internally

The application parses both sides separately:

- `leftAst`
- `rightAst`

Then it builds a combined AST:

- `ast = left - right`

That means every equation becomes an implicit zero-form internally:

- `left = right` becomes `left - right = 0`

This is a powerful simplification because many algorithms naturally work with `F(...)=0`.

Examples:

- `x^2 + y^2 = 100` -> `x^2 + y^2 - 100 = 0`
- `z = sin(x)*cos(y)` -> `z - sin(x)*cos(y) = 0`

Where to read:

- [`src/XpressFormula/UI/FormulaEntry.h`](../src/XpressFormula/UI/FormulaEntry.h)

### 2. Formula Classification (What Kind of Plot to Draw)

After parsing, the app inspects variables and equation shape:

- `y=f(x)` style curve
- `z=f(x,y)` explicit 3D surface
- `F(x,y)=0` implicit 2D contour
- `f(x,y,z)` scalar field (cross-section)
- `F(x,y,z)=0` implicit 3D surface

Special case:

- If the equation is solved for `z` (for example `z = ...` or `... = z`) and the other side does not contain `z`, it is treated as an explicit `z=f(x,y)` surface.

This classification happens in `FormulaEntry::parse()`.

## Part 3: AST Evaluation

The evaluator computes a numeric result from the AST for a given set of variables.

Example:

- AST for `sin(x) + y`
- variables `{ x = 1.0, y = 2.0 }`
- result `sin(1.0) + 2.0`

### NaN as an Error Signal

The evaluator returns `NaN` for invalid cases, for example:

- missing variables
- divide by zero
- `sqrt(-1)` in real numbers
- invalid function arguments

Why `NaN`?

- Renderers can simply test `std::isfinite(...)`
- Invalid samples can be skipped without crashing
- The pipeline stays simple and robust

Where to read:

- [`src/XpressFormula/Core/Evaluator.cpp`](../src/XpressFormula/Core/Evaluator.cpp)

## Part 4: 2D and 3D Plot Rendering Algorithms

All drawing eventually goes through ImGui's `ImDrawList`, but each formula type uses a different sampling algorithm.

### 1. `y=f(x)` 2D Curve

Basic idea:

- Walk across the visible x-range (screen-aligned sampling)
- Evaluate `y = f(x)` at many x values
- Convert world `(x, y)` to screen coordinates
- Draw connected line segments

Why this works well:

- Simple and fast
- Good enough for interactive graphs

Tradeoff:

- Very sharp/high-frequency curves may need denser sampling

### 2. `z=f(x,y)` 2D Heatmap

Basic idea:

- Create a 2D grid over visible x/y range
- Evaluate `z` in each cell
- Map value to color
- Draw filled screen rectangles

This is a raster-style visualization (cell-based), not a 3D mesh.

### 3. `F(x,y)=0` Implicit 2D Contour (Marching Squares)

Used for equations like:

- `x^2 + y^2 = 100`

Basic idea:

1. Sample `F(x,y)` on a 2D grid.
2. In each grid cell, inspect the signs of the 4 corners.
3. If the sign changes across an edge, the contour crosses that edge.
4. Interpolate crossing points and draw line segments.

This is the classic marching squares technique.

Why it is good here:

- Works for circles, ellipses, many implicit curves
- No algebraic solving required

## Part 5: 3D Rendering in This App (No GPU Depth Buffer for Plot Mesh)

XpressFormula draws plot geometry using ImGui draw lists, not a custom 3D engine pipeline with depth buffering.

That means:

- The app computes a custom 3D projection in CPU code
- Triangles are depth-sorted (painter's algorithm)
- Then triangles are drawn back-to-front

This is simpler to integrate with ImGui, but it has tradeoffs:

- Sorting cost grows with triangle count
- Some overlapping geometry can still show painter-sort artifacts
- Performance depends strongly on triangle count

### 1. `z=f(x,y)` Explicit 3D Surface

Basic idea:

1. Sample a regular x/y grid.
2. Evaluate `z`.
3. Project each point to screen (custom camera: azimuth/elevation/z-scale).
4. Build two triangles per grid cell.
5. Depth-sort triangles.
6. Draw filled triangles and optional wireframe.

This produces a familiar mesh surface.

#### Grid-Plane Interleaving Path (`Show Grid` in 3D)

Recent rendering changes treat the projected 3D grid as a real visual plane at `z=0`.
To keep draw order intuitive, explicit surface rendering supports three plane-pass modes:

- `All` (single pass, no clipping)
- `BelowGridPlane` (`z <= 0`)
- `AboveGridPlane` (`z >= 0`)

When 3D grid is visible, the draw sequence is:

1. draw explicit triangles clipped to `BelowGridPlane`
2. draw projected grid plane (translucent fill + frame + interior lines)
3. draw explicit triangles clipped to `AboveGridPlane`

Triangle clipping uses a polygon clip step against plane `z=0` (Sutherland-Hodgman style),
then triangulates the clipped polygon fan.

Cost model for this path:

- mesh sampling/extraction cost is unchanged
- extra cost is linear in triangle count for clipping + second draw pass

### 2. `F(x,y,z)=0` Implicit 3D Surface (Current Approach)

Examples:

- Sphere: `x^2 + y^2 + z^2 = 16`
- Torus: `(x^2+y^2+z^2+21)^2 - 100*(x^2+y^2) = 0`

#### The Core Problem

For implicit surfaces, we do not have a direct `z = ...` formula.

Instead, we only know:

- points on the surface satisfy `F(x,y,z)=0`

So we must discover the surface numerically by sampling a 3D volume.

#### Current Algorithm: Surface Nets Style Extraction

The implementation uses a surface-nets style workflow (with cached world-space mesh).

High-level steps:

1. Define a 3D sampling grid over the current visible x/y range and derived z range.
2. Evaluate `F(x,y,z)` at grid vertices.
3. For each voxel cell that contains a sign change, estimate one representative surface vertex by averaging edge crossings.
4. Stitch neighboring cell vertices into quads around sign-changing grid edges.
5. Split each quad into two triangles.
6. Cache the resulting world-space mesh.
7. On later frames (camera changes), reuse the cached mesh and only re-project + redraw.

Why this is better than the earlier point cloud:

- Produces a true surface mesh (triangles)
- More even vertex distribution than tetrahedra triangulation for smooth shapes
- Fewer triangles than tetrahedra-based extraction at similar resolution

#### Why Shapes Can Look "Cut Open"

This is a very common confusion for implicit plotting.

The surface is sampled only inside a finite 3D box:

- x/y range comes from the current view
- z range is derived from x/y span around `z center`

If the shape extends outside that box, the mesh is clipped.

Example:

- `x^2 + y^2 + z^2 = 16` is a sphere of radius `4`
- If visible x/y range is not roughly centered around `[-4, 4]`, the sampled box may cut the sphere

This is not a formula error; it is domain clipping.

#### Mesh Cache (Performance Optimization)

Implicit meshing is expensive because it samples a 3D grid (`O(N^3)` work).

To improve interaction performance, the app caches the extracted implicit mesh using a key based on:

- AST identity
- implicit resolution
- visible x/y domain
- implicit z center and derived z range

What does not invalidate the mesh cache:

- camera azimuth/elevation
- z scale (visual scaling only)
- opacity / wireframe / envelope toggles

This means camera rotation can remain interactive without rebuilding the 3D scalar field each frame.

#### Grid-Plane Interleaving for Implicit Meshes

The same `All / BelowGridPlane / AboveGridPlane` path is applied for implicit rendering,
but importantly it happens **after** mesh extraction:

- world-space mesh cache is reused as before
- camera projection is computed as before
- projected faces are clipped against `z=0` only for split passes

This avoids introducing a second extraction cache or remeshing just to support grid-plane ordering.

#### Why We Do Not Build Two Mesh Variants

A possible approach is maintaining two persistent meshes ("with grid split" and "without split").
Current implementation intentionally does **not** do that because:

- extraction cost dominates and is already cached in world space
- split behavior is display-state dependent (grid visible + 3D mode), not math-state dependent
- clipping projected/world faces per frame is cheaper than duplicating mesh storage and invalidation paths

In short: one extracted mesh, multiple lightweight render passes.

## Part 6: Projection and Drawing (How 3D Becomes 2D)

The renderer uses a lightweight camera model:

- yaw-like rotation (`azimuth`)
- elevation tilt
- z scaling

Each 3D point is transformed into:

- projected x/y (screen position)
- depth (used for sorting)

Then triangles are:

1. converted to screen coordinates
2. sorted by depth
3. drawn with fill color and optional wireframe

Important alignment detail (recent fix):

- 3D projected geometry is anchored to the same world origin (`0,0`) used by the 2D grid/axes.
- The renderer does **not** auto-center the mesh to the viewport every frame.
- This avoids the visual "swimming" effect where the 3D shape drifts relative to the 2D coordinates while panning/dragging.

3D grid plane details:

- The XY plane is projected with the same camera model used by surface triangles.
- Plane visuals include: low-opacity fill, thick projected frame, interior major/minor lines.
- Interior lines are generated to span frame edge-to-edge while keeping frame edges as separate thicker lines.

Shading:

- A simple directional light is used
- Triangle normal + light direction produce a shade factor
- Color also blends with a z-based gradient for readability

## Part 7: Why Performance Changes So Much Between Modes

The costs are very different:

- 2D curve: cheap
- 2D contour (marching squares): moderate
- explicit 3D `z=f(x,y)`: moderate to heavy
- implicit 3D `F(x,y,z)=0`: heavy (3D sampling + extraction + triangle rendering)

Main cost drivers for implicit 3D:

- `Implicit Surface Quality (F=0)` (3D grid size)
- how much of the domain is visible
- wireframe enabled (extra line drawing)
- number of triangles after meshing

Additional path-dependent cost:

- when 3D grid is visible, 3D surfaces use split passes around `z=0`
- this adds clipping + an extra draw pass for above/below-plane geometry
- implicit extraction cost (`O(N^3)`) is still governed by mesh cache invalidation, not by grid-plane interleaving

Interaction optimization currently used (when **Optimize Rendering** is enabled):

- while dragging/zooming in 3D, the app temporarily lowers 3D mesh density
- implicit `F(x,y,z)=0` quality is reduced during active interaction
- wireframe is temporarily suppressed during interaction
- full quality returns when interaction stops

This improves responsiveness because panning changes the sampled x/y domain, which can invalidate the implicit mesh cache and force a rebuild.

## Part 8: Practical Mental Model for Debugging

When a plot looks wrong, ask these questions in order:

1. Did parsing succeed?
2. Is the formula classified into the mode I expect?
3. Is the view domain large enough / centered correctly?
4. Is the formula clipped by the implicit sampling box?
5. Is mesh quality too low?
6. Is wireframe making the result look noisier than it is?

Examples:

- "No torus visible" -> usually view domain or `z center`, not parser failure
- "Sphere is open" -> sampling box clipping
- "Surface looks faceted" -> quality/resolution, expected for mesh extraction

## Part 9: If You Want to Extend the Algorithms

Good next algorithm upgrades (in increasing complexity):

1. Separate wireframe toggle for implicit surfaces
2. Adaptive quality while dragging / rotating
3. Backface culling before drawing
4. Smooth vertex normals for implicit meshes
5. Marching cubes with edge-vertex reuse
6. Adaptive/octree sampling for sparse large domains

## Glossary (Quick Definitions)

- Token: a small parsed unit from text (`+`, `x`, `3.14`)
- Lexer/Tokenizer: converts text to tokens
- Parser: converts tokens to a syntax tree (AST)
- AST: tree representation of an expression
- Implicit function: shape defined by `F(...)=0`
- Sampling: evaluating a function on many points
- Marching squares: 2D contour extraction from a sampled grid
- Surface nets: mesh extraction using one vertex per active voxel cell
- Painter's algorithm: draw far geometry first, near geometry later

## Related Reading in This Repo

- Architecture overview: [`architecture.md`](architecture.md)
- Expression syntax reference: [`expression-language.md`](expression-language.md)
- ImGui implementation guide: [`imgui-implementation-guide.md`](imgui-implementation-guide.md)

## License

This document is licensed under the MIT License. See [`../LICENSE`](../LICENSE).
