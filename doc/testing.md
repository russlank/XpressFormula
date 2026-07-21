<!-- SPDX-License-Identifier: MIT -->
# Testing Guide

## Test Project

- Project: [`src/XpressFormula.Tests/XpressFormula.Tests.vcxproj`](../src/XpressFormula.Tests/XpressFormula.Tests.vcxproj)
- Output executable:
  - x64 Debug direct project build: `src\XpressFormula.Tests\x64\Debug\XpressFormula.Tests.exe`
  - x64 Release direct project build: `src\XpressFormula.Tests\x64\Release\XpressFormula.Tests.exe`

The project uses a lightweight in-repo test harness ([`src/XpressFormula.Tests/CppUnitTest.h`](../src/XpressFormula.Tests/CppUnitTest.h)) and does not require external MSTest headers.

The test project links the production static libraries (`XpressFormula.Expression`, `XpressFormula.Model`, `XpressFormula.Plotting`, `XpressFormula.Infrastructure`, `XpressFormula.UI`, and `XpressFormula.App`) instead of compiling production `.cpp` files directly. When adding a new production `.cpp`, add it to the owning production library and reference that library from tests as needed.

Current local closure count: `531` test cases.

## What Is Covered

- Tokenization
  - number formats, identifiers, operators, invalid character handling
- Parsing
  - precedence, associativity, function calls, constants, syntax errors
- Evaluation
  - arithmetic, function domain behavior, constants, variable substitution
- View transform
  - coordinate conversion, zoom/pan/reset, grid spacing behavior, and persistent state versus transient viewport round trips
- Scene summary / render mode policy
  - visible scene analysis for empty, invalid, hidden, 2D-only, 3D-only, scalar-field, and mixed scenes; centralized Auto/Force2D/Force3D resolution
- Model formula / mode selection
  - formula compiler diagnostics, equation parsing (`left=right`), implicit equation compilation, render-mode classification, stable formula identity, dynamic editor state, ID-targeted list actions, and revision-tracked document formula commands
- Export settings and metadata
  - infrastructure-owned export settings, size/aspect plans, export profiles, preview sizing, output workflow actions, and JSON sidecar helpers
- Project/session persistence
  - `.xfplot` numeric precision, Unicode escapes, malformed JSON rejection, schema validation, enum compatibility, safe clamping, formula warnings, and repeated save/load stability
- Document revision and dirty-state transitions
  - real persistent mutations increment revision, no-op mutations do not, save/load/new establish clean state, and transient auto-rotation does not dirty the document
- Project controller and persistence subsystem
  - New/Open/Save/Save As, current path state, recent-project integration, unsaved-change decisions, `ProjectSession` DTOs, JSON serializer/parser validation, mapper behavior, repository I/O, and atomic writes
- Architecture characterization
  - default startup formula behavior, production-library linkage, and source-boundary rules that are testable without UI automation
- UI layout plans
  - responsive plot toolbar, formula-card breakpoints, splitter clamping, and modal sizing
- Plot runtime and render planning
  - transient auto-rotation offset behavior, export-deterministic camera planning, centralized 3D option construction, and mesh/render-plan policies
- Expression runtime benchmark harness
  - opt-in timing comparison for fixed-slot expression evaluation versus the map compatibility adapter

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
.\src\XpressFormula.Tests\x64\Debug\XpressFormula.Tests.exe
```

Architecture boundary check:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\check-architecture-boundaries.ps1
```

Optional expression runtime benchmark:

```powershell
$env:XF_RUN_EXPRESSION_BENCHMARK = '1'
.\src\XpressFormula.Tests\x64\Release\XpressFormula.Tests.exe
```

CI PR validation runs the architecture boundary check, builds Debug application and test targets, runs Debug tests, builds Release application and test targets, and runs Release tests. The workflow uses explicit, non-colliding `IntDir` and `OutDir` values under `build\obj\...` and `build\bin\...`, then runs tests from those configured output directories.

Release packaging also runs the architecture boundary check and the Release test suite before package creation. First-party app, library, and test projects build at `/W4` with conformance mode and `/Zc:__cplusplus`; warnings should be fixed rather than broadly suppressed.

Win32 configurations remain available as a best-effort local compatibility path. PR validation and release gates exercise x64 Debug and Release builds.

## Interpreting Results

- Exit code `0` means all tests passed.
- Non-zero exit means at least one failure.
- Output includes `[PASS]` / `[FAIL]` lines per test and final summary.

## License

This document is licensed under the MIT License. See [`../LICENSE`](../LICENSE).
