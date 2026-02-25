# SPDX-License-Identifier: MIT
<#
.SYNOPSIS
Builds XpressFormula release packages (portable EXE copy, MSI, and setup bundle EXE).

.DESCRIPTION
By default this script behaves exactly like the original packaging flow (no code signing).
Optional Authenticode signing can be enabled later by passing -SignArtifacts and the
certificate/signing parameters. The signing order is intentional:

1) Sign app EXE
2) Build MSI
3) Sign MSI
4) Build bundle EXE (consumes MSI)
5) Sign bundle EXE

If the MSI is signed only after the bundle is built, the bundle still contains the
unsigned MSI payload.

.EXAMPLE
.\packaging\build-packages.ps1 -AppExePath .\build\bin\XpressFormula.exe -Version 1.2.3

.EXAMPLE
.\packaging\build-packages.ps1 `
  -AppExePath .\build\bin\XpressFormula.exe `
  -Version 1.2.3 `
  -SignArtifacts `
  -CodeSignPfxPath C:\secure\codesign.pfx `
  -CodeSignPfxPassword "<secret>"
#>
param(
    [Parameter(Mandatory = $true)]
    [string]$AppExePath,

    [Parameter(Mandatory = $true)]
    [string]$Version,

    [string]$OutputDir = "artifacts\release",

    [string]$RequiredWixVersion = "6.0.2",

    [string]$WixBalExtensionId = "WixToolset.Bal.wixext",

    # Signing is optional and disabled unless the caller explicitly passes -SignArtifacts.
    # This keeps local builds and CI working before a signing certificate is available.
    [switch]$SignArtifacts,

    # Optional explicit SignTool path. Useful in CI or on machines with multiple SDKs.
    [string]$SignToolPath,

    # PFX path/password are only required when -SignArtifacts is set.
    [string]$CodeSignPfxPath,

    [string]$CodeSignPfxPassword,

    # Timestamping preserves signature validity after certificate expiry/replacement.
    [string]$TimestampUrl = "http://timestamp.digicert.com"
)

$ErrorActionPreference = "Stop"

function Resolve-SignToolPath {
    param(
        [string]$PreferredPath
    )

    if ($PreferredPath) {
        $resolvedPreferredPath = (Resolve-Path $PreferredPath).Path
        if (-not (Test-Path $resolvedPreferredPath)) {
            throw "SignTool not found at '$PreferredPath'."
        }
        return $resolvedPreferredPath
    }

    $signToolCommand = Get-Command signtool.exe -ErrorAction SilentlyContinue
    if ($signToolCommand) {
        return $signToolCommand.Source
    }

    # Fallback for hosted runners / local machines where signtool is installed with Windows SDK
    # but not added to PATH.
    $candidate = Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\bin" `
        -Recurse -Filter signtool.exe -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match '\\x64\\signtool\.exe$' } |
        Sort-Object FullName -Descending |
        Select-Object -First 1
    if ($candidate) {
        return $candidate.FullName
    }

    throw "signtool.exe not found. Install the Windows SDK or pass -SignToolPath."
}

function Invoke-CodeSign {
    param(
        [Parameter(Mandatory = $true)]
        [string]$SignToolExe,

        [Parameter(Mandatory = $true)]
        [string]$FilePath,

        [Parameter(Mandatory = $true)]
        [string]$PfxPath,

        [Parameter(Mandatory = $true)]
        [string]$PfxPassword,

        [Parameter(Mandatory = $true)]
        [string]$TsUrl
    )

    if (-not (Test-Path $FilePath)) {
        throw "Cannot sign missing file '$FilePath'."
    }

    # Use RFC3161 timestamping so signatures remain valid beyond cert expiration.
    Write-Host "Signing: $FilePath"
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

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$resolvedExe = (Resolve-Path $AppExePath).Path
$resolvedOutput = Join-Path $repoRoot $OutputDir
New-Item -ItemType Directory -Force -Path $resolvedOutput | Out-Null

$iconPath = Join-Path $repoRoot "src\XpressFormula\Resources\XpressFormula.ico"
if (-not (Test-Path $iconPath)) {
    throw "Icon file not found at $iconPath"
}

$signToolExe = $null
if ($SignArtifacts) {
    # Optional path used by CI when code-signing secrets are configured.
    # Without -SignArtifacts, the script intentionally remains unsigned and unchanged.
    if (-not $CodeSignPfxPath) {
        throw "-CodeSignPfxPath is required when -SignArtifacts is used."
    }
    if (-not $CodeSignPfxPassword) {
        throw "-CodeSignPfxPassword is required when -SignArtifacts is used."
    }

    $resolvedPfxPath = (Resolve-Path $CodeSignPfxPath).Path
    if (-not (Test-Path $resolvedPfxPath)) {
        throw "PFX file not found at '$CodeSignPfxPath'."
    }

    $signToolExe = Resolve-SignToolPath -PreferredPath $SignToolPath
    Write-Host "Authenticode signing enabled."
    Write-Host "Using SignTool: $signToolExe"

    # Sign the app binary before packaging so both the portable EXE copy and the MSI payload
    # are generated from the signed file.
    Invoke-CodeSign `
        -SignToolExe $signToolExe `
        -FilePath $resolvedExe `
        -PfxPath $resolvedPfxPath `
        -PfxPassword $CodeSignPfxPassword `
        -TsUrl $TimestampUrl

    $CodeSignPfxPath = $resolvedPfxPath
}

$portableExePath = Join-Path $resolvedOutput "XpressFormula-$Version-x64.exe"
Copy-Item $resolvedExe $portableExePath -Force

$msiPath = Join-Path $resolvedOutput "XpressFormula-$Version-x64.msi"
$bundlePath = Join-Path $resolvedOutput "XpressFormula-$Version-x64-setup.exe"

$productWxs = Join-Path $PSScriptRoot "wix\Product.wxs"
$bundleWxs = Join-Path $PSScriptRoot "wix\Bundle.wxs"

$wixCommand = Get-Command wix -ErrorAction SilentlyContinue
if (-not $wixCommand) {
    throw "WiX CLI was not found in PATH. Install WiX 6.x: dotnet tool uninstall --global wix ; dotnet tool install --global wix --version $RequiredWixVersion ; wix extension add --global WixToolset.Bal.wixext/$RequiredWixVersion"
}

$wixVersionText = (& wix --version).Trim()
$wixMajor = $null
if ($wixVersionText -match '^(\d+)\.') {
    $wixMajor = [int]$Matches[1]
}

if ($wixMajor -ne 6) {
    throw "Unsupported WiX version '$wixVersionText'. This repository currently requires WiX 6.x. Run: dotnet tool uninstall --global wix ; dotnet tool install --global wix --version $RequiredWixVersion ; wix extension add --global WixToolset.Bal.wixext/$RequiredWixVersion"
}

$wixBalExtensionArg = $WixBalExtensionId
if ($wixMajor -ge 6) {
    $wixExtensionRoot = Join-Path $env:USERPROFILE ".wix\extensions\$WixBalExtensionId\$RequiredWixVersion\wixext6"
    $wixBalExtensionCandidates = @(
        (Join-Path $wixExtensionRoot "WixToolset.BootstrapperApplications.wixext.dll")
        (Join-Path $wixExtensionRoot "$WixBalExtensionId.dll")
    )

    $wixBalExtensionPath = $wixBalExtensionCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
    if ($wixBalExtensionPath) {
        $wixBalExtensionArg = $wixBalExtensionPath
        Write-Host "Using WiX BAL extension from file: $wixBalExtensionArg"
    }
}

& wix build $productWxs `
    -arch x64 `
    -d "AppExePath=$resolvedExe" `
    -d "AppIconPath=$iconPath" `
    -d "ProductVersion=$Version" `
    -out $msiPath

if ($LASTEXITCODE -ne 0 -or -not (Test-Path $msiPath)) {
    throw "Failed to build MSI package '$msiPath'."
}

if ($SignArtifacts) {
    # Sign MSI before building the bundle EXE so the bundle embeds the signed MSI.
    Invoke-CodeSign `
        -SignToolExe $signToolExe `
        -FilePath $msiPath `
        -PfxPath $CodeSignPfxPath `
        -PfxPassword $CodeSignPfxPassword `
        -TsUrl $TimestampUrl
}

& wix build $bundleWxs `
    -arch x64 `
    -ext $wixBalExtensionArg `
    -d "MsiPath=$msiPath" `
    -d "AppIconPath=$iconPath" `
    -d "ProductVersion=$Version" `
    -out $bundlePath

if ($LASTEXITCODE -ne 0 -or -not (Test-Path $bundlePath)) {
    throw "Failed to build setup bundle '$bundlePath'. Ensure WiX BAL extension is installed: wix extension add --global $WixBalExtensionId/$RequiredWixVersion"
}

if ($SignArtifacts) {
    # Final signing pass for the WiX Burn bootstrapper executable.
    Invoke-CodeSign `
        -SignToolExe $signToolExe `
        -FilePath $bundlePath `
        -PfxPath $CodeSignPfxPath `
        -PfxPassword $CodeSignPfxPassword `
        -TsUrl $TimestampUrl
}

Write-Host "Created packages:"
Write-Host "  Portable EXE: $portableExePath"
Write-Host "  MSI: $msiPath"
Write-Host "  Setup EXE: $bundlePath"
