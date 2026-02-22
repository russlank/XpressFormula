<!-- SPDX-License-Identifier: MIT -->
# XpressFormula

XpressFormula is a Windows desktop proof-of-concept for entering and plotting mathematical expressions with an ImGui-based UI.

The app supports:

- `y=f(x)` 2D curves
- `z=f(x,y)` 3D surfaces (with optional 2D heat-map view)
- `F(x,y)=0` implicit 2D equations such as `x^2+y^2=100`
- `f(x,y,z)` cross-sections at configurable `z` slices
- Plot image export to `.png`/`.bmp` and clipboard copy
- Versioned binary metadata and installer packaging (`.msi` + setup `.exe`)

## Project Goals

- Fast interactive plotting in a native C++ application
- Clear separation between parsing/evaluation logic and UI/rendering
- Simple local development workflow (Visual Studio, VS Code, CLI)
- Solid unit/integration test coverage for core math and transform logic

## Requirements

- Windows 10/11
- Visual Studio 2022/2026 with C++ workload
- Windows SDK (10.0+)

## Quick Start

Use the detailed guide:

- `doc/run-guide.md`

Common path:

1. Build `Debug|x64`
2. Run `src\x64\Debug\XpressFormula.exe`
3. Run tests with `src\x64\Debug\XpressFormula.Tests.exe`

## Repository Layout

- `src/XpressFormula` application code
- `src/XpressFormula.Tests` test runner and tests
- `src/vendor/imgui` vendored Dear ImGui
- `doc` project documentation
- `.vscode` VS Code tasks and launch configurations

## Documentation

- Documentation index: `doc/index.md`
- Run and debug guide: `doc/run-guide.md`
- Architecture overview: `doc/architecture.md`
- Expression language reference: `doc/expression-language.md`
- Testing guide: `doc/testing.md`
- Release and packaging guide: `doc/release-packaging.md`
- Vendor dependencies: `doc/project-vendors.md`

## Third-Party Dependencies

This repository vendors Dear ImGui.
See `doc/project-vendors.md` for source, license, and update notes.

## Notes

- `dotnet build` is not the primary build path for this native C++ solution; use MSBuild/Visual Studio.
- In Debug builds, the app attempts to use the Direct3D debug layer and falls back when unavailable.
- Build metadata (`repo URL`, `branch`, `version`, `commit`) is shown in the application UI when provided by the build pipeline.

## License

This project is licensed under the MIT License. See `LICENSE`.
