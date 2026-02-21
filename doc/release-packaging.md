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

## Local Packaging (Windows)

Prerequisites:

- Visual Studio C++ build tools
- WiX Toolset v4 CLI (`wix`)
- WiX Burn extension (`WixToolset.Bal.wixext`)

Install WiX CLI and extension:

```powershell
dotnet tool install --global wix
wix extension add --global WixToolset.Bal.wixext
```

Build app binary:

```powershell
$solutionDir = (Resolve-Path .\src).Path + '\'
msbuild src\XpressFormula\XpressFormula.vcxproj /t:Build /m `
  /p:Configuration=Release `
  /p:Platform=x64 `
  /p:SolutionDir="$solutionDir" `
  /p:IntDir="$PWD\build\obj\" `
  /p:OutDir="$PWD\build\bin\"
```

Create packages:

```powershell
.\packaging\build-packages.ps1 `
  -AppExePath .\build\bin\XpressFormula.exe `
  -Version 1.0.0 `
  -OutputDir artifacts\release
```

## GitHub Release Pipeline

Workflow file:

- `.github/workflows/release-packaging.yml`

Trigger modes:

- Push tag `v*.*.*` (recommended for official releases)
- Manual `workflow_dispatch`

Pipeline actions:

1. Builds `Release|x64` app binary.
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
5. Verify uploaded release assets (`.exe` and `.msi`) on GitHub.

## Packaging Sources

- WiX MSI definition: `packaging/wix/Product.wxs`
- WiX setup EXE bundle: `packaging/wix/Bundle.wxs`
- Packaging script: `packaging/build-packages.ps1`
