<!-- SPDX-License-Identifier: MIT -->
# Expression Language Reference

## Supported Syntax

Inputs support:

- Numbers: `42`, `3.14`, `1.5e-3`
- Variables: `x`, `y`, `z`
- Constants: `pi`, `e`, `tau`
- Operators: `+`, `-`, `*`, `/`, `^`
- Parentheses: `( ... )`
- Function calls: `sin(x)`, `atan2(y, x)`, `pow(x, 2)`
- Equations with one `=` sign: `left = right`

## Grammar

Expression parsing uses recursive descent with this structure:

- `expression := term (('+' | '-') term)*`
- `term := power (('*' | '/') power)*`
- `power := unary ('^' power)?` (right-associative)
- `unary := ('-' | '+')? primary`
- `primary := number | identifier | functionCall | '(' expression ')'`

Equation parsing is handled as:

- `formula := expression | expression '=' expression`
- Equations are internally converted to `left - right` for implicit plotting/evaluation.

## Operator Behavior

- `^` is right-associative: `2 ^ 3 ^ 2` is parsed as `2 ^ (3 ^ 2)`
- Unary operators are parsed before power in this implementation.
  - Example: `-x^2` behaves like `(-x)^2`
  - Use parentheses for clarity: `-(x^2)`

## Built-in Functions

Function names are case-sensitive. Unknown functions fail during parsing. Known
functions may parse with any number of comma-separated arguments, but evaluation
uses strict arity: too few or too many arguments return `NaN`. The one
intentional variable-arity function is `log`, which supports `log(a)` and
`log(base, value)`.

### Basic

- `sqrt(a)` - square root, `NaN` for negative input
- `cbrt(a)` - cube root
- `abs(a)` - absolute value
- `ceil(a)`, `floor(a)`, `round(a)` - integer rounding helpers
- `log(a)` - natural logarithm
- `log(base, value)` - logarithm in the given base
- `log2(a)`, `log10(a)` - base-2 and base-10 logarithms
- `exp(a)` - e raised to a power
- `pow(a, b)` - a raised to power b
- `min(a, b)`, `max(a, b)` - smaller/larger value
- `mod(a, b)` - floating-point remainder
- `sign(a)` - returns `-1`, `0`, or `1`

### Trigonometry

- `sin(a)`, `cos(a)`, `tan(a)` - angle in radians
- `asin(a)`, `acos(a)`, `atan(a)` - inverse trig functions in radians
- `atan2(y, x)` - angle of the vector `(x, y)`
- `sinh(a)`, `cosh(a)`, `tanh(a)` - hyperbolic functions

### Distance

- `hypot(a, b)` - equivalent to `sqrt(a*a + b*b)`
- `length2(x, y)` - 2D vector length
- `length3(x, y, z)` - 3D vector length
- `distance2(x1, y1, x2, y2)` - distance between two 2D points
- `distance3(x1, y1, z1, x2, y2, z2)` - distance between two 3D points

### Range / Interpolation

- `clamp(x, min, max)` - limits a value to a range; reversed bounds are swapped
- `saturate(x)` - shorthand for `clamp(x, 0, 1)`
- `mix(a, b, t)` - linear blend from a to b; `t` is not clamped
- `lerp(a, b, t)` - alias of `mix(a, b, t)`
- `inverseLerp(a, b, x)` - clamped position of x in the range a..b
- `remap(inMin, inMax, outMin, outMax, x)` - maps x from one range into another
- `step(edge, x)` - returns 0 when `x < edge`, otherwise 1
- `smoothstep(a, b, x)` - smooth clamped transition from 0 to 1
- `smootherstep(a, b, x)` - extra-smooth clamped transition

`mix` and `lerp` allow extrapolation when `t` is outside 0..1.
`smoothstep` and `smootherstep` clamp internally through `inverseLerp`.
`inverseLerp`, `smoothstep`, `smootherstep`, and `remap` return `NaN` when the
input range has equal endpoints.

### Patterns

- `fract(x)` - fractional part using `x - floor(x)`, so `fract(-0.25) = 0.75`
- `tri(x)` - triangle wave based on `fract`; `tri(0) = 1`
- `pulse(edge0, edge1, x)` - 1 inside an interval and 0 outside; reversed edges are swapped
- `repeat(x, period)` - centered repeat coordinate; `period = 0` returns `NaN`

### Smooth Implicit Composition

- `smin(a, b, k)` - smooth minimum for blending implicit shapes
- `smax(a, b, k)` - smooth maximum for subtracting or blending shapes

When `k <= 0`, `smin` and `smax` fall back to exact `min` and `max`.

### Signed Distance Helpers

- `sdSphere(x, y, z, r)` - signed distance to a centered sphere
- `sdBox(x, y, z, bx, by, bz)` - signed distance to a centered box
- `sdTorus(x, y, z, majorRadius, minorRadius)` - signed distance to a centered torus
- `sdCylinderX(x, y, z, r)` - signed distance to a cylinder along X
- `sdCylinderY(x, y, z, r)` - signed distance to a cylinder along Y
- `sdCylinderZ(x, y, z, r)` - signed distance to a cylinder along Z

Negative radii or half-extents return `NaN`. Zero radii/extents are allowed.

### Noise

- `noise2(x, y)` - deterministic 2D value noise, approximately in `[-1, 1]`
- `noise3(x, y, z)` - deterministic 3D value noise, approximately in `[-1, 1]`
- `fbm2(x, y)` - normalized 5-octave 2D fractal noise, approximately in `[-1, 1]`
- `fbm3(x, y, z)` - normalized 5-octave 3D fractal noise, approximately in `[-1, 1]`

Noise is deterministic and uses no global random state or time-based seeding.
Non-finite noise coordinates return `NaN`.

## Domain Rules

The evaluator returns `NaN` for invalid operations, including:

- division by zero
- `sqrt` of negative
- logarithm with invalid domain/base
- wrong number of function arguments
- `repeat(x, 0)`
- equal endpoints for `inverseLerp`, `smoothstep`, `smootherstep`, or `remap`
- negative signed-distance radii or box half-extents
- unknown variable value

`NaN` values are ignored/skipped in plotting where possible.

## Examples

- `sin(x)`
- `cos(x) * exp(-x*x/10)`
- `length2(x, y)`
- `z = sin(x) * cos(y)`
- `length2(x, y) = 10`
- `length3(x,y,z)-1=0`
- `sdTorus(x,y,z,1.2,0.25)=0`
- `smin(sdSphere(x-0.6,y,z,0.5),sdSphere(x+0.6,y,z,0.5),0.2)=0`
- `z = sin(8*length2(x,y))*smoothstep(3,0,length2(x,y))`
- `length3(x,y,z)-1+0.15*noise3(5*x,5*y,5*z)=0`

## Render Mapping

The app chooses a render mode from the parsed variables and equation form:

- `y=f(x)` for expressions using only `x` (or constants)
- `z=f(x,y)` for expressions using `x` and `y`, and equations solved for `z`
- `F(x,y)=0` contour for equations like `x^2+y^2=100`
- `f(x,y,z)` scalar-field cross-section for formulas that use `x`, `y`, and `z`
- `F(x,y,z)=0` implicit 3D surface for equations like `x^2+y^2+z^2=16`

Mode-specific behavior for 3-variable formulas:

- `f(x,y,z)` (expression) renders as a cross-section/heat map using the configured `z` slice.
- `F(x,y,z)=0` (equation) renders as an implicit 3D surface in effective **3D** mode, and as a scalar cross-section in effective **2D** mode.

2D/3D effective mode is controlled by the rendering preference:

- **Auto**: mixed visible 2D+3D content resolves to 2D; only-visible 3D content resolves to 3D.
- **Force 3D**: always use 3D rendering for 3D-capable formulas.
- **Force 2D**: always use 2D heatmap/cross-section rendering for 3D-capable formulas.

Only variables `x`, `y`, and `z` are supported for plotting.

## License

This document is licensed under the MIT License. See [`../LICENSE`](../LICENSE).
