<!-- SPDX-License-Identifier: MIT -->
# Windows Code Signing Guide

This guide explains how to enable Authenticode signing for XpressFormula release artifacts:

- `XpressFormula-<version>-x64.exe` (portable app)
- `XpressFormula-<version>-x64.msi`
- `XpressFormula-<version>-x64-setup.exe` (WiX Burn bundle)

It includes ready-to-use snippets for GitHub Actions and PowerShell, plus the repo changes required to sign in the correct order.

## Why Signing Matters

Unsigned Windows binaries are much more likely to trigger:

- Microsoft Defender download warnings
- SmartScreen "unknown publisher" prompts
- low-reputation/heuristic detections in multi-engine scanners

Signing does not guarantee zero warnings, but it is the single most important improvement for public Windows releases.

## Important Clarification (GitHub vs Certificate Ownership)

GitHub does not provide a Windows code-signing certificate automatically.

You need one publisher certificate (or a cloud signing account) owned by the project/release publisher. End users do not need their own certificate.

Typical choices:

1. `PFX certificate` (easy to wire into CI)
2. `Cloud signing` (better private-key protection)

Do not use a self-signed certificate for public releases. It will not establish trust with Defender/SmartScreen.

## What Must Be Signed (And In What Order)

Sign in this exact order:

1. Build `XpressFormula.exe`
2. Sign `XpressFormula.exe`
3. Build `.msi` from the signed EXE
4. Sign `.msi`
5. Build setup bundle `.exe` from the signed MSI
6. Sign setup bundle `.exe`

Why the order matters:

- The current packaging script builds the MSI and then the bundle in one run.
- If the MSI is signed only after the bundle is built, the bundle still contains the unsigned MSI.

## Current Repo Files Involved

- Release workflow: [`.github/workflows/release-packaging.yml`](../.github/workflows/release-packaging.yml)
- Packaging script: [`packaging/build-packages.ps1`](../packaging/build-packages.ps1)
- WiX MSI source: [`packaging/wix/Product.wxs`](../packaging/wix/Product.wxs)
- WiX bundle source: [`packaging/wix/Bundle.wxs`](../packaging/wix/Bundle.wxs)

## Option A (Recommended First): PFX Certificate in GitHub Actions

This is the fastest way to get signing working.

### Prerequisites

- A code-signing certificate from a trusted CA (exported as `.pfx`)
- PFX password
- Windows timestamp server URL (examples below)

Common timestamp URLs:

- `http://timestamp.digicert.com`
- `http://timestamp.sectigo.com`

## GitHub Secrets To Add

Add these repository or organization secrets:

- `WINDOWS_CODESIGN_PFX_BASE64` (Base64-encoded `.pfx` bytes)
- `WINDOWS_CODESIGN_PFX_PASSWORD`

Optional variables (recommended):

- `WINDOWS_CODESIGN_TIMESTAMP_URL` (for example `http://timestamp.digicert.com`)

### Create Base64 From a PFX (Local Machine)

PowerShell:

```powershell
$bytes = [System.IO.File]::ReadAllBytes("C:\secure\codesign.pfx")
[Convert]::ToBase64String($bytes) | Set-Content .\codesign-pfx-base64.txt
```

Copy the contents of `codesign-pfx-base64.txt` into the GitHub secret.

## GitHub Actions Snippets (Ready To Use)

The snippets below are intended for [`.github/workflows/release-packaging.yml`](../.github/workflows/release-packaging.yml).

### 1. Add Secrets/Tool Setup Step (Before Build/Sign Steps)

Place this after your tool setup steps (after `.NET`, Python, and WiX setup is fine):

```yaml
- name: Prepare code signing certificate
  if: ${{ secrets.WINDOWS_CODESIGN_PFX_BASE64 != '' && secrets.WINDOWS_CODESIGN_PFX_PASSWORD != '' }}
  shell: pwsh
  run: |
    $ErrorActionPreference = "Stop"

    $pfxPath = Join-Path $env:RUNNER_TEMP "xpressformula-codesign.pfx"
    [System.IO.File]::WriteAllBytes(
      $pfxPath,
      [Convert]::FromBase64String("${{ secrets.WINDOWS_CODESIGN_PFX_BASE64 }}")
    )

    $signtool = (Get-Command signtool.exe -ErrorAction SilentlyContinue)
    if (-not $signtool) {
      $candidates = Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\bin" -Recurse -Filter signtool.exe -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match "\\x64\\signtool.exe$" } |
        Sort-Object FullName -Descending
      $signtool = $candidates | Select-Object -First 1
    }
    if (-not $signtool) {
      throw "signtool.exe not found on runner."
    }

    $signtoolPath = if ($signtool.PSObject.Properties.Name -contains 'Source') { $signtool.Source } else { $signtool.FullName }

    "CODE_SIGN_PFX=$pfxPath" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
    "CODE_SIGN_PWD=${{ secrets.WINDOWS_CODESIGN_PFX_PASSWORD }}" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
    "SIGNTOOL_EXE=$signtoolPath" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
    "CODE_SIGN_TIMESTAMP_URL=${{ vars.WINDOWS_CODESIGN_TIMESTAMP_URL || 'http://timestamp.digicert.com' }}" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
```

### 2. Sign the Built App EXE (Immediately After the Build Step)

Add this right after the `Build Release Binary` step:

```yaml
- name: Sign app EXE
  if: ${{ env.CODE_SIGN_PFX != '' }}
  shell: pwsh
  run: |
    $ErrorActionPreference = "Stop"
    & "$env:SIGNTOOL_EXE" sign `
      /fd SHA256 `
      /td SHA256 `
      /tr "$env:CODE_SIGN_TIMESTAMP_URL" `
      /f "$env:CODE_SIGN_PFX" `
      /p "$env:CODE_SIGN_PWD" `
      "$env:GITHUB_WORKSPACE\build\bin\XpressFormula.exe"
```

### 3. Enable Signing During Packaging (Script Change Required)

The current packaging script builds:

1. MSI
2. Bundle EXE

To sign the MSI before it is embedded into the bundle, you must modify [`packaging/build-packages.ps1`](../packaging/build-packages.ps1) to sign artifacts between those two builds.

After patching the script (see the next section), call it like this:

```yaml
- name: Build MSI and Setup EXE Packages (Signed)
  shell: pwsh
  run: |
    .\packaging\build-packages.ps1 `
      -AppExePath "$env:GITHUB_WORKSPACE\build\bin\XpressFormula.exe" `
      -Version "${{ steps.version.outputs.app_version }}" `
      -RequiredWixVersion "$env:WIX_VERSION" `
      -OutputDir "artifacts\release" `
      -SignArtifacts `
      -SignToolPath "$env:SIGNTOOL_EXE" `
      -CodeSignPfxPath "$env:CODE_SIGN_PFX" `
      -CodeSignPfxPassword "$env:CODE_SIGN_PWD" `
      -TimestampUrl "$env:CODE_SIGN_TIMESTAMP_URL"
```

### 4. Verify Signatures (Recommended)

Add a verification step before artifact upload:

```yaml
- name: Verify Authenticode signatures
  if: ${{ env.CODE_SIGN_PFX != '' }}
  shell: pwsh
  run: |
    $ErrorActionPreference = "Stop"
    $targets = @(
      "$env:GITHUB_WORKSPACE\build\bin\XpressFormula.exe",
      "$env:GITHUB_WORKSPACE\artifacts\release\*.exe",
      "$env:GITHUB_WORKSPACE\artifacts\release\*.msi"
    )

    foreach ($pattern in $targets) {
      Get-ChildItem $pattern -ErrorAction SilentlyContinue | ForEach-Object {
        $sig = Get-AuthenticodeSignature $_.FullName
        Write-Host "$($_.Name): $($sig.Status) / $($sig.SignerCertificate.Subject)"
        if ($sig.Status -ne "Valid") {
          throw "Invalid signature on $($_.FullName): $($sig.Status)"
        }
      }
    }
```

## Required Changes To `packaging/build-packages.ps1`

This project needs packaging-time signing because the setup bundle must be built from the signed MSI.

### What To Modify

1. Add optional parameters for signing.
2. Add a helper function to run `signtool sign`.
3. Sign the MSI immediately after MSI build.
4. Build the bundle from the signed MSI.
5. Sign the bundle EXE immediately after bundle build.

### Parameter Snippet (Add To `param(...)`)

```powershell
[switch]$SignArtifacts,

[string]$SignToolPath,

[string]$CodeSignPfxPath,

[string]$CodeSignPfxPassword,

[string]$TimestampUrl = "http://timestamp.digicert.com"
```

### Helper Snippet (Add Near Top Of Script)

```powershell
function Resolve-SignToolPath {
    param([string]$PreferredPath)

    if ($PreferredPath) {
        $resolved = (Resolve-Path $PreferredPath).Path
        if (-not (Test-Path $resolved)) {
            throw "SignTool not found at $PreferredPath"
        }
        return $resolved
    }

    $cmd = Get-Command signtool.exe -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    $candidate = Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\bin" -Recurse -Filter signtool.exe -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match '\\x64\\signtool\.exe$' } |
        Sort-Object FullName -Descending |
        Select-Object -First 1
    if ($candidate) {
        return $candidate.FullName
    }

    throw "signtool.exe not found. Install Windows SDK or pass -SignToolPath."
}

function Invoke-CodeSign {
    param(
        [string]$SignToolExe,
        [string]$FilePath,
        [string]$PfxPath,
        [string]$PfxPassword,
        [string]$TsUrl
    )

    if (-not (Test-Path $FilePath)) {
        throw "Cannot sign missing file: $FilePath"
    }

    & $SignToolExe sign `
        /fd SHA256 `
        /td SHA256 `
        /tr $TsUrl `
        /f $PfxPath `
        /p $PfxPassword `
        $FilePath

    if ($LASTEXITCODE -ne 0) {
        throw "signtool failed for '$FilePath'."
    }
}
```

### Validation Snippet (Add After Path Resolution)

```powershell
$signToolExe = $null
if ($SignArtifacts) {
    if (-not $CodeSignPfxPath) { throw "-CodeSignPfxPath is required when -SignArtifacts is used." }
    if (-not $CodeSignPfxPassword) { throw "-CodeSignPfxPassword is required when -SignArtifacts is used." }
    if (-not (Test-Path $CodeSignPfxPath)) { throw "PFX file not found: $CodeSignPfxPath" }

    $signToolExe = Resolve-SignToolPath -PreferredPath $SignToolPath
    Write-Host "Using SignTool: $signToolExe"
}
```

### Signing Calls (Add In Packaging Flow)

Sign the source EXE before copying or sign the copied portable EXE after copying. Signing before copying is usually simpler because the copied file remains byte-identical.

Example (after resolving `$resolvedExe`, before `Copy-Item`):

```powershell
if ($SignArtifacts) {
    Invoke-CodeSign `
        -SignToolExe $signToolExe `
        -FilePath $resolvedExe `
        -PfxPath $CodeSignPfxPath `
        -PfxPassword $CodeSignPfxPassword `
        -TsUrl $TimestampUrl
}
```

Then sign the MSI after the MSI `wix build` succeeds:

```powershell
if ($SignArtifacts) {
    Invoke-CodeSign `
        -SignToolExe $signToolExe `
        -FilePath $msiPath `
        -PfxPath $CodeSignPfxPath `
        -PfxPassword $CodeSignPfxPassword `
        -TsUrl $TimestampUrl
}
```

Then sign the setup bundle after the bundle `wix build` succeeds:

```powershell
if ($SignArtifacts) {
    Invoke-CodeSign `
        -SignToolExe $signToolExe `
        -FilePath $bundlePath `
        -PfxPath $CodeSignPfxPath `
        -PfxPassword $CodeSignPfxPassword `
        -TsUrl $TimestampUrl
}
```

## Minimal Local Signing Example (No CI)

If you want to test locally before wiring GitHub Actions:

```powershell
$pfx = "C:\secure\codesign.pfx"
$pwd = Read-Host "PFX password" -AsSecureString
$bstr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($pwd)
$plain = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($bstr)

signtool sign /fd SHA256 /td SHA256 /tr http://timestamp.digicert.com `
  /f $pfx /p $plain `
  .\build\bin\XpressFormula.exe

[Runtime.InteropServices.Marshal]::ZeroFreeBSTR($bstr)
```

## Option B: Cloud Signing (No PFX In GitHub Secrets)

Cloud signing services keep the private key outside GitHub Actions.

Typical pattern:

1. Create cloud signing account/profile with your CA/provider.
2. Configure GitHub OIDC or service principal authentication.
3. Use provider signing CLI/action in the workflow.
4. Sign the same artifacts in the same order as above (`EXE -> MSI -> bundle EXE`).

This is more secure, but setup varies by provider. The signing order in this repo does not change.

## Common Pitfalls

- Signing the MSI after the bundle is already built.
- Forgetting to timestamp signatures (`/tr` and `/td SHA256`).
- Using a self-signed certificate for public releases.
- Not verifying signatures in CI before publishing assets.
- Logging secret values by accident in debug output.

## Recommended Rollout Plan

1. Implement PFX-based signing in a private test branch.
2. Run the release workflow via `workflow_dispatch`.
3. Verify signatures on all three artifacts.
4. Upload a test release and re-check Defender/SmartScreen behavior.
5. Submit false-positive reports (if needed) after signing is in place.

## Related Docs

- [Release and packaging guide](release-packaging.md)
- [README](../README.md)

## License

This document is licensed under the MIT License. See [`../LICENSE`](../LICENSE).
