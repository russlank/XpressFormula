# Testing Guide

## Test Project

- Project: `src/XpressFormula.Tests/XpressFormula.Tests.vcxproj`
- Output executable:
  - x64: `src\x64\Debug\XpressFormula.Tests.exe`
  - x86: `src\Debug\XpressFormula.Tests.exe`

The project uses a lightweight in-repo test harness (`CppUnitTest.h`) and does not require external MSTest headers.

## What Is Covered

- Tokenization
  - number formats, identifiers, operators, invalid character handling
- Parsing
  - precedence, associativity, function calls, constants, syntax errors
- Evaluation
  - arithmetic, function domain behavior, constants, variable substitution
- View transform
  - coordinate conversion, zoom/pan/reset, grid spacing behavior

## Running Tests

### Visual Studio

1. Build solution (`Debug|x64` recommended).
2. Run `XpressFormula.Tests` as startup project, or run the test executable directly.

### VS Code

1. Run task `Build XpressFormula (x64 Debug)`.
2. Run task `Run Tests (x64 Debug)`.

### CLI (PowerShell)

```powershell
.\src\x64\Debug\XpressFormula.Tests.exe
```

## Interpreting Results

- Exit code `0` means all tests passed.
- Non-zero exit means at least one failure.
- Output includes `[PASS]` / `[FAIL]` lines per test and final summary.
