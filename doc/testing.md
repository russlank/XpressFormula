<!-- SPDX-License-Identifier: MIT -->
# Testing Guide

## Test Project

- Project: [`src/XpressFormula.Tests/XpressFormula.Tests.vcxproj`](../src/XpressFormula.Tests/XpressFormula.Tests.vcxproj)
- Output executable:
  - x64: `src\x64\Debug\XpressFormula.Tests.exe`
  - x86: `src\Debug\XpressFormula.Tests.exe`

The project uses a lightweight in-repo test harness ([`src/XpressFormula.Tests/CppUnitTest.h`](../src/XpressFormula.Tests/CppUnitTest.h)) and does not require external MSTest headers.

## What Is Covered

- Tokenization
  - number formats, identifiers, operators, invalid character handling
- Parsing
  - precedence, associativity, function calls, constants, syntax errors
- Evaluation
  - arithmetic, function domain behavior, constants, variable substitution
- View transform
  - coordinate conversion, zoom/pan/reset, grid spacing behavior
- Formula entry / mode selection
  - equation parsing (`left=right`), implicit equation compilation, render-mode classification

## Running Tests

### Visual Studio

1. Build solution (`Debug|x64` recommended).
2. Run `XpressFormula.Tests` as startup project, or run the test executable directly.

### VS Code

1. Open the repository root folder in VS Code (so `.vscode/tasks.json` and `.vscode/launch.json` are detected).
2. Run task `Run Tests (x64 Debug)` (it builds first, then runs the test executable).
3. To debug tests instead of just running them, press `F5` and choose `Debug XpressFormula.Tests (x64)`.
4. Optional Win32 path: use `Run Tests (Win32 Debug)` or `Debug XpressFormula.Tests (Win32)`.
5. If IntelliSense is out of sync with your target, run `C/C++: Select a Configuration` and choose `x64-Debug` or `Win32-Debug`.

### CLI (PowerShell)

```powershell
.\src\x64\Debug\XpressFormula.Tests.exe
```

## Interpreting Results

- Exit code `0` means all tests passed.
- Non-zero exit means at least one failure.
- Output includes `[PASS]` / `[FAIL]` lines per test and final summary.

## License

This document is licensed under the MIT License. See [`../LICENSE`](../LICENSE).
