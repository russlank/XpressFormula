<!-- SPDX-License-Identifier: MIT -->
# Release and Packaging Guide

This guide explains how to version, package, and publish XpressFormula releases.

If you are publishing public Windows releases, also read the Windows signing guide:

- [`code-signing.md`](code-signing.md)

## Version Source of Truth

Version values are defined in:

- [`src/XpressFormula/Version.h`](../src/XpressFormula/Version.h)

Key macros:

- `XF_VERSION_MAJOR`
- `XF_VERSION_MINOR`
- `XF_VERSION_PATCH`
- `XF_VERSION_BUILD`

The GitHub release tag must match `v<major>.<minor>.<patch>`.

## Build Outputs

Packaging produces three files:

1. Portable executable: `XpressFormula-<version>-x64.exe`
2. MSI installer: `XpressFormula-<version>-x64.msi`
3. Setup bootstrapper: `XpressFormula-<version>-x64-setup.exe`

For best Defender/SmartScreen results, sign all three artifacts. See [`code-signing.md`](code-signing.md) for the required signing order and CI snippets.

## CI Toolchain Pins

Workflow file:

- [`.github/workflows/release-packaging.yml`](../.github/workflows/release-packaging.yml)

The release job pins key versions with environment variables:

- `DOTNET_VERSION` (currently `9.0.x`)
- `PYTHON_VERSION` (currently `3.12`)
- `WIX_VERSION` (currently `6.0.2`)
- `MSVC_PLATFORM_TOOLSET` (currently `v145`)
- `BUILD_CONFIGURATION` (currently `Release`)
- `BUILD_PLATFORM` (currently `x64`)

`MSVC_PLATFORM_TOOLSET` is explicitly passed to MSBuild using `/p:PlatformToolset=...`.
This prevents runner mismatch errors like `MSB8020` when a project file was saved with an unavailable toolset.
The release build also injects `XF_BUILD_REPO_URL`, `XF_BUILD_BRANCH`, `XF_BUILD_COMMIT`,
and `XF_BUILD_VERSION` into the binary so metadata appears in the running application.
These are passed through MSBuild properties: `XfBuildRepoUrl`, `XfBuildBranch`,
`XfBuildCommit`, and `XfBuildVersion`.

The workflow currently uses `windows-2025-vs2026` to get MSBuild 18.x/VS 2026 toolchain on GitHub-hosted runners.

Release packaging runs the architecture boundary check and the Release test suite before package creation. A failed boundary check, build, or test run blocks artifact upload and release publication.

## Local Packaging (Windows)

Prerequisites:

- Visual Studio 2026 or Build Tools 2026 with the C++ workload and toolset `v145` installed
- WiX Toolset v6 CLI (`wix`)
- WiX Burn extension (`WixToolset.Bal.wixext`) matching your WiX v6 version

The repository, CI, and local release simulation default to `v145`. Older Visual Studio installations can be used only by explicitly retargeting local builds to an installed toolset such as `v143`.

Install WiX CLI and extension:

```powershell
$wixVersion = "6.0.2"
dotnet tool uninstall --global wix
dotnet tool install --global wix --version $wixVersion
wix extension add --global WixToolset.Bal.wixext/$wixVersion
wix --version
```

[`packaging/build-packages.ps1`](../packaging/build-packages.ps1) expects the BAL extension to already be installed and prints the exact install command if it is missing.
For WiX 6, the script also auto-detects and uses the extension DLL path when the extension cache reports the package as damaged.

Build app binary:

```powershell
$solutionDir = (Resolve-Path .\src).Path + '\'
msbuild src\XpressFormula\XpressFormula.vcxproj /t:Build /m `
  /p:Configuration=Release `
  /p:Platform=x64 `
  /p:PlatformToolset=v145 `
  /p:SolutionDir="$solutionDir" `
  /p:IntDir="$PWD\build\obj\" `
  /p:OutDir="$PWD\build\bin\"
```

Build and run release tests before packaging:

```powershell
msbuild src\XpressFormula.Tests\XpressFormula.Tests.vcxproj /t:Build /m `
  /p:Configuration=Release `
  /p:Platform=x64 `
  /p:PlatformToolset=v145 `
  /p:IntDir="$PWD\build\test-obj\" `
  /p:OutDir="$PWD\build\test-bin\"

.\build\test-bin\XpressFormula.Tests.exe
```

Create packages:

```powershell
$version = python scripts/get_version.py
.\packaging\build-packages.ps1 `
  -AppExePath .\build\bin\XpressFormula.exe `
  -Version $version `
  -OutputDir artifacts\release
```

## Test the Release Pipeline Locally

`windows-2025-vs2026` GitHub runner behavior cannot be reproduced with Linux-based `act` for this workflow.
Use the local Windows simulation script instead:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\test-release-pipeline-local.ps1
```

Common options:

```powershell
# Build only (skip WiX packaging)
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\test-release-pipeline-local.ps1 -SkipPackaging

# Override toolset or output directory
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\test-release-pipeline-local.ps1 `
  -PlatformToolset v145 `
  -WixVersion 6.0.2 `
  -OutputDir artifacts\release-local
```

If you also want to verify the tag check locally:

```powershell
$version = python scripts/get_version.py
$expectedTag = "v$version"
Write-Host "Expected release tag: $expectedTag"
```

## v1.6.0 Release Verification Record

Last updated: 2026-07-21

Verified branch head: `12eaf18408be9a612bb85529ce2c0d8e1b567e12`

Automated verification completed locally with Visual Studio MSBuild 18.8.2+ce25c0108 (`msbuild -version`: 18.8.2.30814):

- [x] Architecture boundary check passes:
  `powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\check-architecture-boundaries.ps1`
- [x] Debug x64 app build succeeds:
  `.\scripts\invoke-msbuild.ps1 -ProjectPath "src\XpressFormula\XpressFormula.vcxproj" -Configuration Debug -Platform x64 -Targets Build`
  Output: `src\XpressFormula\x64\Debug\XpressFormula.exe`.
- [x] Release x64 app build succeeds:
  `.\scripts\invoke-msbuild.ps1 -ProjectPath "src\XpressFormula\XpressFormula.vcxproj" -Configuration Release -Platform x64 -Targets Build`
  Output: `src\XpressFormula\x64\Release\XpressFormula.exe`.
- [x] Debug x64 test project builds:
  `.\scripts\invoke-msbuild.ps1 -ProjectPath "src\XpressFormula.Tests\XpressFormula.Tests.vcxproj" -Configuration Debug -Platform x64 -Targets Build`
  Output: `src\XpressFormula.Tests\x64\Debug\XpressFormula.Tests.exe`.
- [x] Release x64 test project builds:
  `.\scripts\invoke-msbuild.ps1 -ProjectPath "src\XpressFormula.Tests\XpressFormula.Tests.vcxproj" -Configuration Release -Platform x64 -Targets Build`
  Output: `src\XpressFormula.Tests\x64\Release\XpressFormula.Tests.exe`.
- [x] No compiler warnings in the above builds (`0 Warning(s)`, `0 Error(s)`).
- [x] Debug automated tests pass: `src\XpressFormula.Tests\x64\Debug\XpressFormula.Tests.exe` reported `531/531 tests passed`.
- [x] Release automated tests pass: `src\XpressFormula.Tests\x64\Release\XpressFormula.Tests.exe` reported `531/531 tests passed`.
- [x] Local release workflow dry run without packaging passes from a clean detached worktree at the verified head:
  `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\test-release-pipeline-local.ps1 -SkipPackaging`
- [x] PR Validation passes on the verified head:
  run `#8`, workflow run `29855326680`, result `success`.
- [x] First-party app, library, and test projects build with `/W4`, conformance mode, and `/Zc:__cplusplus`.
- [x] Existing core tests, project/session persistence tests, export settings and metadata tests, UI layout-plan tests, formula-list action tests, update-controller tests, and plotting geometry tests pass.
- [ ] Expression runtime benchmark was not rerun for Prompt 15.1 because evaluator runtime allocation behavior did not change in this closure pass.

Prompt 15.1 smoke coverage recorded by automated tests:

- [x] Formula input: malformed dot input, zero-valued scientific literals, true non-zero underflow/overflow, and wrong function arity.
- [x] Shutdown: non-blocking cancellation, late-result ignore, repeated cancellation, and controller destruction while a fake fetcher is blocked.
- [x] Projects: bounded project reads, formula-count limits, expression-length limits, serialized-size limits, rejected Save As, and old-target preservation.
- [x] Plotting: isolated exact-zero contact, exact-grid-plane mesh, constant-zero field, duplicate triangle rejection, and deterministic Surface Nets output.

Manual verification still required before tagging:

- [ ] Interactive formula editor workflows: Add/Cancel, Add/Apply, Edit/Cancel, and live error display in the running UI.
- [ ] Interactive shutdown workflow: start a manual update check in the running app, close immediately, and confirm no visible delay or crash.
- [ ] Plotting workflows: 2D curves, discontinuities, implicit contours, heatmaps, scalar-field cross-sections, 3D explicit surfaces, implicit surfaces, multiple implicit surfaces, camera presets, auto-rotation dirty-state behavior, grid-plane interleave, axis triad, coordinates, and wire/envelope thickness minimums and maximums.
- [ ] Project workflows: New, Open valid `.xfplot`, Save, Save As, Recent reopen, dirty marker set/clear, Save/Discard/Cancel before New/Open/Close, unsupported schema error, malformed file error, invalid formula warning, Unicode formula round trip, multiple save/load cycles retaining values.
- [ ] Export workflows: every export profile, transparent PNG, grayscale export, metadata sidecar output, metadata JSON opens in a parser/editor, preview/final export parity, save/copy/open/reveal/copy-path, offscreen fallback behavior if reproducible.
- [ ] Responsive UI: wide/medium/compact/extra-compact toolbar, minimum/default/maximum sidebar width, narrow/short windows, formula cards at narrow widths, and 100%, 125%, 150%, and 200% Windows scaling where available.

## How to Change Versions

1. Application version:
- Edit `XF_VERSION_MAJOR`, `XF_VERSION_MINOR`, and `XF_VERSION_PATCH` in [`src/XpressFormula/Version.h`](../src/XpressFormula/Version.h).
- Create a matching Git tag (`v<major>.<minor>.<patch>`).
2. CI dependency versions:
- Edit `DOTNET_VERSION`, `PYTHON_VERSION`, and `WIX_VERSION` in [`.github/workflows/release-packaging.yml`](../.github/workflows/release-packaging.yml).
3. MSVC toolset version used by CI:
- Edit `MSVC_PLATFORM_TOOLSET` in [`.github/workflows/release-packaging.yml`](../.github/workflows/release-packaging.yml).
- Ensure the selected runner image has that toolset installed, or the build will fail with `MSB8020`.
4. Local simulation defaults:
- Use parameters on [`scripts/test-release-pipeline-local.ps1`](../scripts/test-release-pipeline-local.ps1) (`-PlatformToolset`, `-WixVersion`, `-Configuration`, `-Platform`, `-OutputDir`).

## GitHub Release Pipeline

Trigger modes:

- Push tag `v*.*.*` (recommended for official releases)
- Manual `workflow_dispatch`

Pipeline actions:

1. Builds `Release|x64` app binary with explicit platform toolset.
2. (Recommended) Signs the app EXE.
3. Builds MSI and setup EXE using WiX.
4. (Recommended) Signs the MSI before building/publishing the bundle, and signs the final setup EXE.
5. Verifies signatures (recommended).
6. Uploads artifacts.
7. For tag pushes, publishes assets to the GitHub release.

## Creating a New Release

1. Update version in [`src/XpressFormula/Version.h`](../src/XpressFormula/Version.h).
2. Commit changes:

```powershell
git add .
git commit -m "Release v1.2.3"
```

3. Create and push tag:

```powershell
git tag v1.2.3
git push origin main --tags
```

4. Wait for the `Release Packaging` workflow to complete.
5. Verify uploaded release assets (`.exe`, `.msi`, and setup `.exe`) on GitHub.

## Packaging Sources

- WiX MSI definition: [`packaging/wix/Product.wxs`](../packaging/wix/Product.wxs)
- WiX setup EXE bundle: [`packaging/wix/Bundle.wxs`](../packaging/wix/Bundle.wxs)
- Packaging script: [`packaging/build-packages.ps1`](../packaging/build-packages.ps1)
- Local pipeline simulation script: [`scripts/test-release-pipeline-local.ps1`](../scripts/test-release-pipeline-local.ps1)

## License

This document is licensed under the MIT License. See [`../LICENSE`](../LICENSE).
