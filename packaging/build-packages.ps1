param(
    [Parameter(Mandatory = $true)]
    [string]$AppExePath,

    [Parameter(Mandatory = $true)]
    [string]$Version,

    [string]$OutputDir = "artifacts\release",

    [string]$RequiredWixVersion = "6.0.2",

    [string]$WixBalExtensionId = "WixToolset.Bal.wixext"
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$resolvedExe = (Resolve-Path $AppExePath).Path
$resolvedOutput = Join-Path $repoRoot $OutputDir
New-Item -ItemType Directory -Force -Path $resolvedOutput | Out-Null

$iconPath = Join-Path $repoRoot "src\XpressFormula\Resources\XpressFormula.ico"
if (-not (Test-Path $iconPath)) {
    throw "Icon file not found at $iconPath"
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

Write-Host "Created packages:"
Write-Host "  Portable EXE: $portableExePath"
Write-Host "  MSI: $msiPath"
Write-Host "  Setup EXE: $bundlePath"
