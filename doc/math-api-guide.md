<!-- SPDX-License-Identifier: MIT -->
# Math API Guide (Internal Mathematics Library and Abstractions)

This document explains the application-specific math API used by XpressFormula:

- what the main math-related types and modules are
- which mathematical abstractions they represent
- how they fit together from input text to plotted pixels
- what standard C++ libraries they depend on and why
- how to extend them safely

This guide is intentionally detailed and beginner-friendly.

It complements:

- [`math-api-cheatsheet.md`](math-api-cheatsheet.md) (quick lookup and "where used" references)
- [`expression-language.md`](expression-language.md) (syntax for end users)
- [`algorithms-guide.md`](algorithms-guide.md) (algorithm concepts)
- [`imgui-implementation-guide.md`](imgui-implementation-guide.md) (UI architecture)

## 1. Scope: What We Mean by "Math API" in This Application

XpressFormula does not have a single `MathApi` class. Instead, the "math API" is a set of cooperating modules:

- `Core` namespace
  - tokenization
  - parsing
  - AST representation
  - evaluation
  - coordinate transforms
  - constants
- `UI::FormulaEntry`
  - equation normalization and render-kind classification
- `Plotting::PlotRenderer`
  - mathematical sampling and mesh/contour extraction used for rendering

Think of it as a layered internal library:

1. **Expression Math Layer** (`Core`)
2. **Formula Semantics Layer** (`FormulaEntry`)
3. **Numerical Sampling + Visualization Math Layer** (`PlotRenderer`)

## 2. Mathematical Abstractions and Their Code Equivalents

This table is the most important mental model for understanding the code.

| Mathematical concept | What it means | Code representation |
|---|---|---|
| Token | Small lexical unit in input text | `Core::Token` / `Core::TokenType` |
| Expression | A symbolic formula | `Core::ASTNode` tree |
| Scalar literal | A fixed number | `Core::NumberNode` |
| Variable | Unknown/free symbol (`x`, `y`, `z`) | `Core::VariableNode` |
| Unary operation | `-a`, `+a` | `Core::UnaryOpNode` |
| Binary operation | `a+b`, `a*b`, `a^b` | `Core::BinaryOpNode` |
| Function application | `sin(x)`, `pow(a,b)` | `Core::FunctionCallNode` |
| Evaluation context | Variable assignments | `Evaluator::Variables` (`unordered_map<string,double>`) |
| Equation | `left = right` | Internally normalized to `(left - right) = 0` |
| 2D viewport transform | World coordinates ↔ screen pixels | `Core::ViewTransform` |
| Implicit curve/surface | Zero set of a function (`F(...)=0`) | AST + contour/mesh extraction in `PlotRenderer` |

## 3. Core Expression Math API (`src/XpressFormula/Core`)

### 3.1 `Token` and `TokenType` (Lexical Units)

Files:

- [`src/XpressFormula/Core/Token.h`](../src/XpressFormula/Core/Token.h)
- [`src/XpressFormula/Core/Tokenizer.h`](../src/XpressFormula/Core/Tokenizer.h)
- [`src/XpressFormula/Core/Tokenizer.cpp`](../src/XpressFormula/Core/Tokenizer.cpp)

`TokenType` defines the language atoms recognized by the tokenizer:

- numbers
- identifiers
- operators (`+ - * / ^`)
- grouping/punctuation (`(` `)` `,`)
- `End`
- `Error`

### Code Snippet (from `Token.h`)

```cpp
enum class TokenType {
    Number,
    Identifier,
    Plus, Minus, Star, Slash, Caret,
    LeftParen, RightParen, Comma,
    End,
    Error
};
```

### Mathematical meaning

Tokens are *not* math values yet. They are syntax building blocks.

Example:

- `sin(x)+2`

is first just:

- `Identifier("sin")`
- `LeftParen`
- `Identifier("x")`
- `RightParen`
- `Plus`
- `Number("2")`

### Standard library dependencies and why

In this area you will see:

- `<string>`
  - stores token text, error messages
- `<vector>`
  - token lists
- `<cctype>`
  - character classification (`isdigit`, `isalpha`, `isspace`)

Why this design is good:

- easy to debug
- good error messages (stores positions)
- parser gets a clean token stream

## 4. AST (Abstract Syntax Tree) API

Files:

- [`src/XpressFormula/Core/ASTNode.h`](../src/XpressFormula/Core/ASTNode.h)

The AST is the canonical internal representation of parsed math expressions.

### Node family overview

`ASTNode` is the base type and each derived node represents a mathematical construct:

- `NumberNode`: scalar constant
- `VariableNode`: symbolic variable
- `UnaryOpNode`: unary operator application
- `BinaryOpNode`: binary operator application
- `FunctionCallNode`: function application to arguments

### Code Snippet (from `ASTNode.h`)

```cpp
class BinaryOpNode : public ASTNode {
public:
    BinaryOperator op;
    ASTNodePtr     left;
    ASTNodePtr     right;
    BinaryOpNode(BinaryOperator o, ASTNodePtr l, ASTNodePtr r)
        : op(o), left(std::move(l)), right(std::move(r)) {}
    NodeType type() const override { return NodeType::BinaryOp; }
};
```

### Mathematical abstraction mapping

This node corresponds to the abstract form:

- `BinaryOp(op, left, right)`

For example:

- `x^2 + y^2`

becomes conceptually:

- `Add(Power(x,2), Power(y,2))`

### Standard library dependencies and why

- `<memory>`
  - AST nodes use `std::shared_ptr` (`ASTNodePtr`) for tree ownership
- `<vector>`
  - function argument lists
- `<string>`
  - variable and function names

Why `shared_ptr` here?

- AST nodes are easy to pass around and store
- formula entries can hold ASTs without manual lifetime management
- developer ergonomics matters more than micro-optimizing ownership in this project

## 5. Parser API (Recursive-Descent)

Files:

- [`src/XpressFormula/Core/Parser.h`](../src/XpressFormula/Core/Parser.h)
- [`src/XpressFormula/Core/Parser.cpp`](../src/XpressFormula/Core/Parser.cpp)

### Public API

`Parser::parse(const std::string&)` returns a `Parser::Result`:

- `ast`
- `error`
- `variables`

This is a strong API design because it returns both:

- the parsed structure
- semantic metadata (which variables were used)

### Code Snippet (from `Parser.h`)

```cpp
struct Result {
    ASTNodePtr            ast;
    std::string           error;
    std::set<std::string> variables;
    bool success() const { return ast != nullptr && error.empty(); }
};
```

### Grammar implemented (from parser structure)

The parser implements precedence through function layering:

```cpp
// expression := term (('+' | '-') term)*
// term       := power (('*' | '/') power)*
// power      := unary ('^' power)?    // right-associative
// unary      := ('-' | '+')? primary
```

### Why recursive-descent is a good fit here

- easy to read and maintain
- easy to attach good error messages
- supports function calls and nested expressions naturally
- enough power for the app’s expression language without parser generators

### Standard library dependencies and why

- `<vector>`
  - token storage
- `<string>`
  - error text and names
- `<set>`
  - variable collection, built-in names, constants lookup

## 6. Evaluator API (Numerical Evaluation)

Files:

- [`src/XpressFormula/Core/Evaluator.h`](../src/XpressFormula/Core/Evaluator.h)
- [`src/XpressFormula/Core/Evaluator.cpp`](../src/XpressFormula/Core/Evaluator.cpp)

### Public API

```cpp
using Variables = std::unordered_map<std::string, double>;
static double evaluate(const ASTNodePtr& node, const Variables& vars);
```

The evaluator is a numerical interpreter for ASTs.

### Mathematical abstraction

You can think of `Evaluator::evaluate(ast, vars)` as:

- a function from `(expression tree, variable assignments)` to a real number

with the important caveat:

- invalid/undefined cases return `NaN`

### Code Snippet (from `Evaluator.cpp`)

```cpp
case NodeType::BinaryOp: {
    auto* bin = static_cast<BinaryOpNode*>(node.get());
    double l = evaluate(bin->left,  vars);
    double r = evaluate(bin->right, vars);
    switch (bin->op) {
        case BinaryOperator::Add:      return l + r;
        case BinaryOperator::Subtract: return l - r;
        case BinaryOperator::Multiply: return l * r;
        case BinaryOperator::Divide:   return (r == 0.0) ? NaN : l / r;
        case BinaryOperator::Power:    return std::pow(l, r);
    }
}
```

### Why `NaN` is used (important design choice)

Invalid math occurs often in plotting:

- division by zero
- log of non-positive numbers
- sqrt of negative numbers (in real mode)

Returning `NaN` allows rendering code to simply skip invalid samples using `std::isfinite(...)`.

This keeps the plotting pipeline robust and simple.

### Built-in function dispatch

`evaluateFunction(name, args)` maps parsed function names to `<cmath>` operations.

Examples supported:

- `sin`, `cos`, `tan`
- `sqrt`, `cbrt`, `abs`
- `log`, `log2`, `log10`, `exp`
- `pow`, `min`, `max`, `mod`
- `atan2`

### Standard library dependencies and why

- `<unordered_map>`
  - fast variable lookup by name during repeated evaluation
- `<cmath>`
  - core real-valued math functions
- `<algorithm>`
  - `std::min` / `std::max`
- `<limits>`
  - canonical quiet `NaN`
- `<vector>`
  - function arguments

## 7. Math Constants API

File:

- [`src/XpressFormula/Core/MathConstants.h`](../src/XpressFormula/Core/MathConstants.h)

Defines:

- `PI`
- `E`
- `TAU`

This gives a consistent source of constants instead of relying on platform-specific macros.

### Code Snippet

```cpp
constexpr double PI  = 3.14159265358979323846;
constexpr double E   = 2.71828182845904523536;
constexpr double TAU = 2.0 * PI;
```

## 8. ViewTransform API (Mathematical Coordinate Mapping)

Files:

- [`src/XpressFormula/Core/ViewTransform.h`](../src/XpressFormula/Core/ViewTransform.h)
- [`src/XpressFormula/Core/ViewTransform.cpp`](../src/XpressFormula/Core/ViewTransform.cpp)

`ViewTransform` is one of the most important mathematical components in the app.

It converts between:

- **world coordinates** (mathematical x/y space)
- **screen coordinates** (pixels)

### Mathematical abstraction

This is an affine mapping with scaling and translation, plus an inverted Y axis for screen space.

World to screen:

- `sx = originX + width/2 + (wx - centerX) * scaleX`
- `sy = originY + height/2 - (wy - centerY) * scaleY`

The minus sign on `sy` is because screen Y increases downward.

### Code Snippet (from `ViewTransform.cpp`)

```cpp
Vec2 ViewTransform::worldToScreen(double wx, double wy) const {
    float sx = screenOriginX + screenWidth  * 0.5f
             + static_cast<float>((wx - centerX) * scaleX);
    float sy = screenOriginY + screenHeight * 0.5f
             - static_cast<float>((wy - centerY) * scaleY);
    return { sx, sy };
}
```

### Why this abstraction matters

Nearly every renderer depends on it:

- grid and axis drawing
- curve plotting
- heatmaps
- implicit contours
- mouse hover coordinates

### "Nice grid spacing" algorithm

`niceGridSpacing()` chooses grid intervals like:

- `1`
- `2`
- `5`
- `10`
- and powers of ten

This is a standard visualization technique for readable axes.

### Standard library dependencies and why

- `<cmath>`
  - `pow`, `log10`, `floor`
- `<algorithm>`
  - `std::clamp` for zoom limits

## 9. `FormulaEntry`: The Bridge Between Expression Math and Plot Semantics

File:

- [`src/XpressFormula/UI/FormulaEntry.h`](../src/XpressFormula/UI/FormulaEntry.h)

Even though it lives in `UI`, `FormulaEntry` performs important math-semantic work.

It stores:

- original input text
- parsed AST(s)
- variable set
- error state
- classification (`Curve2D`, `Surface3D`, `Implicit2D`, `ScalarField3D`)

### Equation Normalization (`left = right` -> `left - right`)

This is one of the most important abstractions in the app.

### Code Snippet (from `FormulaEntry::parse`)

```cpp
leftAst = leftResult.ast;
rightAst = rightResult.ast;
ast = std::make_shared<Core::BinaryOpNode>(
    Core::BinaryOperator::Subtract, leftAst, rightAst);
```

Mathematically this means:

- `left = right` is represented as the zero set of `left - right`

This unified representation enables:

- implicit 2D contour rendering (`F(x,y)=0`)
- implicit 3D surface rendering (`F(x,y,z)=0`)

### Render-kind classification as math semantics

`FormulaEntry` infers "what the expression means" from variables and equation shape.

Examples:

- `sin(x)` -> one-variable function -> 2D curve
- `sin(x)*cos(y)` -> two variables -> explicit surface candidate
- `x^2 + y^2 = 100` -> implicit 2D equation
- `x^2 + y^2 + z^2 = 16` -> implicit 3D surface (`F(x,y,z)=0`)

## 10. PlotRenderer as a Consumer of the Math API

Files:

- [`src/XpressFormula/Plotting/PlotRenderer.h`](../src/XpressFormula/Plotting/PlotRenderer.h)
- [`src/XpressFormula/Plotting/PlotRenderer.cpp`](../src/XpressFormula/Plotting/PlotRenderer.cpp)

`PlotRenderer` is where the expression math is turned into images.

It depends on:

- `Core::ASTNodePtr` (the parsed expression)
- `Core::Evaluator` (numerical samples)
- `Core::ViewTransform` (world↔screen mapping)

### Mathematical techniques used by mode

- `drawCurve2D`:
  - 1D sampling over x
- `drawHeatmap`:
  - 2D grid sampling over x/y
- `drawImplicitContour2D`:
  - marching squares on a 2D sampled scalar field
- `drawSurface3D`:
  - explicit surface mesh from `z=f(x,y)`
- `drawImplicitSurface3D`:
  - surface-nets style extraction from sampled `F(x,y,z)=0`
  - cached world mesh + re-projection
  - optional split render passes around `z=0` (`All` / `BelowGridPlane` / `AboveGridPlane`)
  - plane clipping stage used only for split passes (no second mesh extraction)

### Mesh extraction vs split render passes (3D grid feature)

Recent 3D grid work separates heavy mesh calculation from lightweight display ordering:

1. **Mesh calculation**:
   - explicit `z=f(x,y)`: sample x/y and build triangles
   - implicit `F(x,y,z)=0`: sample scalar field + surface-nets extraction
2. **Projection/shading**:
   - apply azimuth/elevation/z-scale and compute depth
3. **Optional grid-plane split** (only when 3D grid is shown):
   - clip triangles/faces to `z<=0` pass
   - draw projected grid plane
   - clip triangles/faces to `z>=0` pass

Important implementation detail:

- split passes are controlled through `Surface3DOptions` (`planePass`, `gridPlaneZ`)
- implicit world-mesh cache is reused exactly as before; split rendering does not create a second cached mesh variant

### Why PlotRenderer is not in `Core`

`Core` is math infrastructure and expression semantics.

`PlotRenderer` adds:

- visualization choices
- color schemes
- ImGui draw primitives
- camera/projection style

So it is a *math consumer*, not the foundational math library.

## 11. Standard Library Dependencies by Module (and Why)

This section is a practical "why these headers exist" map for contributors.

### Tokenizer / Token / Parser / AST

- `<string>`
  - tokens, identifiers, errors, function names
- `<vector>`
  - token streams, AST function arguments
- `<memory>`
  - AST ownership (`shared_ptr`)
- `<set>`
  - variable collection and built-in name lookup
- `<cctype>`
  - character classification

### Evaluator

- `<unordered_map>`
  - variable environment lookup
- `<cmath>`
  - numerical math operations
- `<limits>`
  - `NaN`
- `<algorithm>`
  - `min`, `max`

### ViewTransform

- `<cmath>`
  - logarithmic grid spacing and powers of 10
- `<algorithm>`
  - clamping zoom

### PlotRenderer (math-heavy rendering support)

- `<vector>`
  - sampled grids, vertices, faces
- `<algorithm>`
  - sorting, clamping, min/max
- `<cmath>`
  - trig, square roots, projections, normals
- `<limits>`
  - numeric sentinels for bounds
- `<cstdio>`
  - lightweight formatting for labels

Why these are standard-library based:

- keeps the core math stack portable and easy to build
- avoids bringing in heavy symbolic math or geometry dependencies
- easier for newcomers to inspect and debug

## 12. Real Examples From This Application

### Example A: Parse and evaluate a simple expression

Input:

- `sin(x) + 2`

Conceptual use of the API:

```cpp
auto parsed = XpressFormula::Core::Parser::parse("sin(x) + 2");
if (parsed.success()) {
    XpressFormula::Core::Evaluator::Variables vars;
    vars["x"] = 1.0;
    double y = XpressFormula::Core::Evaluator::evaluate(parsed.ast, vars);
}
```

What this represents mathematically:

- the function `f(x) = sin(x) + 2`
- evaluated at `x = 1`

### Example B: Equation to implicit function conversion

Input:

- `x^2 + y^2 = 100`

Internal representation:

- `F(x,y) = x^2 + y^2 - 100`

Why this is useful:

- contour extraction only needs to know where `F(x,y)` changes sign and crosses zero

### Example C: World-space math to screen-space pixels

`ViewTransform` converts plot coordinates to screen pixels and back.

This allows:

- drawing axes in the correct place
- displaying hover coordinates
- zooming toward the mouse cursor

### Example D: Implicit 3D sphere and torus

Sphere:

- `x^2 + y^2 + z^2 = 16`

Torus:

- `(x^2+y^2+z^2+21)^2 - 100*(x^2+y^2) = 0`

In both cases, the math API path is:

1. parse to AST
2. normalize equation to zero-form (already zero-form in these examples)
3. classify as `F(x,y,z)=0`
4. sample scalar field
5. extract surface mesh
6. project and render
7. if 3D grid is visible: split render around `z=0` and interleave plane draw

## 13. Important Numerical and Design Limitations

This is not a symbolic math system (CAS).

What it does well:

- parse and numerically evaluate expressions
- sample and visualize them interactively

What it does not do:

- algebraic simplification
- symbolic differentiation/integration
- exact arithmetic

### Floating-point realities

Everything is evaluated in `double`.

So expect:

- rounding error
- numerical instability near singularities
- domain problems returning `NaN`

This is normal and expected in interactive plotting tools.

## 14. How to Extend the Math API Safely

### Add a new built-in function

1. Add the function name to `Parser::s_builtinFunctions`
2. Implement it in `Evaluator::evaluateFunction`
3. Add examples/docs/tests
4. Add it to Formula Editor "Supported functions" list if user-facing

### Add a new constant

1. Add constant value in `MathConstants.h`
2. Add constant name to `Parser::s_constants`
3. Resolve it in `Parser::parsePrimary`
4. Document it in `expression-language.md`

### Add a new render interpretation

1. Extend `FormulaRenderKind`
2. Update `FormulaEntry::parse()` classification
3. Add render branch in `PlotPanel`
4. Add renderer implementation in `PlotRenderer`

## 15. Suggested Reading Path for New Contributors

If you want to understand the math stack end-to-end:

1. [`src/XpressFormula/Core/Token.h`](../src/XpressFormula/Core/Token.h)
2. [`src/XpressFormula/Core/Tokenizer.cpp`](../src/XpressFormula/Core/Tokenizer.cpp)
3. [`src/XpressFormula/Core/ASTNode.h`](../src/XpressFormula/Core/ASTNode.h)
4. [`src/XpressFormula/Core/Parser.cpp`](../src/XpressFormula/Core/Parser.cpp)
5. [`src/XpressFormula/Core/Evaluator.cpp`](../src/XpressFormula/Core/Evaluator.cpp)
6. [`src/XpressFormula/Core/ViewTransform.cpp`](../src/XpressFormula/Core/ViewTransform.cpp)
7. [`src/XpressFormula/UI/FormulaEntry.h`](../src/XpressFormula/UI/FormulaEntry.h)
8. [`src/XpressFormula/Plotting/PlotRenderer.cpp`](../src/XpressFormula/Plotting/PlotRenderer.cpp)

## 16. Glossary

- **AST**: Abstract Syntax Tree, a structured representation of an expression
- **Evaluation context**: variable bindings used during evaluation
- **Implicit function**: function used via its zero set `F(...)=0`
- **Scalar field**: a function assigning one scalar value to each point in space
- **Affine transform**: linear transform plus translation (used in coordinate mapping)
- **NaN**: "Not a Number", used here to mark invalid numeric results

## Related Documentation

- [`algorithms-guide.md`](algorithms-guide.md)
- [`expression-language.md`](expression-language.md)
- [`architecture.md`](architecture.md)
- [`imgui-implementation-guide.md`](imgui-implementation-guide.md)

## License

This document is licensed under the MIT License. See [`../LICENSE`](../LICENSE).
