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

Single argument:

- `sin`, `cos`, `tan`
- `asin`, `acos`, `atan`
- `sinh`, `cosh`, `tanh`
- `sqrt`, `cbrt`, `abs`
- `ceil`, `floor`, `round`
- `log`, `log2`, `log10`, `exp`
- `sign`

Two arguments:

- `atan2(a, b)`
- `pow(a, b)`
- `min(a, b)`, `max(a, b)`
- `mod(a, b)`
- `log(base, value)` (base-log form)

## Domain Rules

The evaluator returns `NaN` for invalid operations, including:

- division by zero
- `sqrt` of negative
- logarithm with invalid domain/base
- unknown variable value

`NaN` values are ignored/skipped in plotting where possible.

## Examples

- `sin(x)`
- `cos(x) * exp(-x*x/10)`
- `x^2 + y^2`
- `z = sin(x) * cos(y)`
- `x^2 + y^2 = 100`
- `sin(x) * cos(y)`
- `x^2 + y^2 + z^2 - 4`

## Render Mapping

The app chooses a render mode from the parsed variables and equation form:

- `y=f(x)` for expressions using only `x` (or constants)
- `z=f(x,y)` for expressions using `x` and `y`, and equations solved for `z`
- `F(x,y)=0` contour for equations like `x^2+y^2=100`
- `f(x,y,z)` scalar-field cross-section for formulas that use `x`, `y`, and `z`

Only variables `x`, `y`, and `z` are supported for plotting.
