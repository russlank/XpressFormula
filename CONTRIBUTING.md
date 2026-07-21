<!-- SPDX-License-Identifier: MIT -->
# Contributing

Thanks for improving XpressFormula. Keep changes focused and verify them locally before opening a pull request.

## Quality Gate

Run these checks from the repository root for architecture or release-facing changes:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\check-architecture-boundaries.ps1

msbuild src\XpressFormula\XpressFormula.vcxproj /t:Build /m `
  /p:Configuration=Debug /p:Platform=x64 `
  /p:SolutionDir="$PWD\src\"

msbuild src\XpressFormula.Tests\XpressFormula.Tests.vcxproj /t:Build /m `
  /p:Configuration=Debug /p:Platform=x64
.\src\XpressFormula.Tests\x64\Debug\XpressFormula.Tests.exe

msbuild src\XpressFormula\XpressFormula.vcxproj /t:Build /m `
  /p:Configuration=Release /p:Platform=x64 `
  /p:SolutionDir="$PWD\src\"

msbuild src\XpressFormula.Tests\XpressFormula.Tests.vcxproj /t:Build /m `
  /p:Configuration=Release /p:Platform=x64
.\src\XpressFormula.Tests\x64\Release\XpressFormula.Tests.exe
```

First-party app, library, and test projects build with MSVC `/W4`, `/permissive-`, and `/Zc:__cplusplus`. Fix first-party warnings instead of suppressing them broadly.

The repository default toolset is `v145` and CI/release workflows run on the Visual Studio 2026/MSBuild 18 toolchain. Local builds may override `PlatformToolset` only when intentionally retargeting to an installed toolset for compatibility testing.

## Architecture Rules

- Respect the dependency direction documented in [`doc/architecture-dependencies.md`](doc/architecture-dependencies.md).
- Keep `Expression` and `Model` free of ImGui, Win32, D3D, JSON parser/writer, and project repository dependencies.
- Keep plotting geometry and meshing free of ImGui.
- Keep reusable UI components free of file, clipboard, shell, HTTP, and image-encoding side effects.
- Tests should link production libraries rather than compiling production `.cpp` files directly.

## Documentation Rules

Public docs live in `doc/` and should describe user-facing behavior, architecture decisions, and supported development workflows. Keep private planning, prompt logs, and modernization tracking out of public documentation.

## Release Checks

The release workflow builds the Release executable, builds and runs the Release tests, and only then creates packages. For a local dry run, use:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\test-release-pipeline-local.ps1
```
