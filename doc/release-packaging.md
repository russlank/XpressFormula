# Release and Packaging Guide

This guide explains how to version, package, and publish XpressFormula releases.

## Version Source of Truth

Version values are defined in:

- `src/XpressFormula/Version.h`

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

## CI Toolchain Pins

Workflow file:

- `.github/workflows/release-packaging.yml`

The release job pins key versions with environment variables:

- `DOTNET_VERSION` (currently `9.0.x`)
- `PYTHON_VERSION` (currently `3.12`)
- `WIX_VERSION` (currently `4.0.5`)
- `MSVC_PLATFORM_TOOLSET` (currently `v143`)
- `BUILD_CONFIGURATION` (currently `Release`)
- `BUILD_PLATFORM` (currently `x64`)

`MSVC_PLATFORM_TOOLSET` is explicitly passed to MSBuild using `/p:PlatformToolset=...`.
This prevents runner mismatch errors like `MSB8020` when a project file was saved with an unavailable toolset.

## Local Packaging (Windows)

Prerequisites:

- Visual Studio C++ build tools with toolset `v143` installed
- WiX Toolset v4 CLI (`wix`)
- WiX Burn extension (`WixToolset.Bal.wixext`) matching your WiX v4 version

Install WiX CLI and extension:

```powershell
$wixVersion = "4.0.5"
dotnet tool uninstall --global wix
dotnet tool install --global wix --version $wixVersion
wix extension add --global WixToolset.Bal.wixext/$wixVersion
wix --version
```

`packaging/build-packages.ps1` expects the BAL extension to already be installed and prints the exact install command if it is missing.

Build app binary:

```powershell
$solutionDir = (Resolve-Path .\src).Path + '\'
msbuild src\XpressFormula\XpressFormula.vcxproj /t:Build /m `
  /p:Configuration=Release `
  /p:Platform=x64 `
  /p:PlatformToolset=v143 `
  /p:SolutionDir="$solutionDir" `
  /p:IntDir="$PWD\build\obj\" `
  /p:OutDir="$PWD\build\bin\"
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

`windows-latest` GitHub runner behavior cannot be reproduced with Linux-based `act` for this workflow.
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
  -PlatformToolset v143 `
  -WixVersion 4.0.5 `
  -OutputDir artifacts\release-local
```

If you also want to verify the tag check locally:

```powershell
$version = python scripts/get_version.py
$expectedTag = "v$version"
Write-Host "Expected release tag: $expectedTag"
```

## How to Change Versions

1. Application version:
- Edit `XF_VERSION_MAJOR`, `XF_VERSION_MINOR`, and `XF_VERSION_PATCH` in `src/XpressFormula/Version.h`.
- Create a matching Git tag (`v<major>.<minor>.<patch>`).
2. CI dependency versions:
- Edit `DOTNET_VERSION`, `PYTHON_VERSION`, and `WIX_VERSION` in `.github/workflows/release-packaging.yml`.
3. MSVC toolset version used by CI:
- Edit `MSVC_PLATFORM_TOOLSET` in `.github/workflows/release-packaging.yml`.
- Ensure the selected runner image has that toolset installed, or the build will fail with `MSB8020`.
4. Local simulation defaults:
- Use parameters on `scripts/test-release-pipeline-local.ps1` (`-PlatformToolset`, `-WixVersion`, `-Configuration`, `-Platform`, `-OutputDir`).

## GitHub Release Pipeline

Trigger modes:

- Push tag `v*.*.*` (recommended for official releases)
- Manual `workflow_dispatch`

Pipeline actions:

1. Builds `Release|x64` app binary with explicit platform toolset.
2. Builds MSI and setup EXE using WiX.
3. Uploads artifacts.
4. For tag pushes, publishes assets to the GitHub release.

## Creating a New Release

1. Update version in `src/XpressFormula/Version.h`.
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

- WiX MSI definition: `packaging/wix/Product.wxs`
- WiX setup EXE bundle: `packaging/wix/Bundle.wxs`
- Packaging script: `packaging/build-packages.ps1`
- Local pipeline simulation script: `scripts/test-release-pipeline-local.ps1`
