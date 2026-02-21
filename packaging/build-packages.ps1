param(
    [Parameter(Mandatory = $true)]
    [string]$AppExePath,

    [Parameter(Mandatory = $true)]
    [string]$Version,

    [string]$OutputDir = "artifacts\release",

    [string]$RequiredWixVersion = "4.0.5"
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
    throw "WiX CLI was not found in PATH. Install WiX 4.x: dotnet tool uninstall --global wix ; dotnet tool install --global wix --version $RequiredWixVersion ; wix extension add --global WixToolset.Bal.wixext/$RequiredWixVersion"
}

$wixVersionText = (& wix --version).Trim()
$wixMajor = $null
if ($wixVersionText -match '^(\d+)\.') {
    $wixMajor = [int]$Matches[1]
}

if ($wixMajor -ne 4) {
    throw "Unsupported WiX version '$wixVersionText'. This repository currently requires WiX 4.x. Run: dotnet tool uninstall --global wix ; dotnet tool install --global wix --version $RequiredWixVersion ; wix extension add --global WixToolset.Bal.wixext/$RequiredWixVersion"
}

& wix build $productWxs `
    -arch x64 `
    -dAppExePath="$resolvedExe" `
    -dAppIconPath="$iconPath" `
    -dProductVersion="$Version" `
    -out $msiPath

if ($LASTEXITCODE -ne 0 -or -not (Test-Path $msiPath)) {
    throw "Failed to build MSI package '$msiPath'."
}

& wix build $bundleWxs `
    -arch x64 `
    -ext WixToolset.Bal.wixext `
    -dMsiPath="$msiPath" `
    -dAppIconPath="$iconPath" `
    -dProductVersion="$Version" `
    -out $bundlePath

if ($LASTEXITCODE -ne 0 -or -not (Test-Path $bundlePath)) {
    throw "Failed to build setup bundle '$bundlePath'."
}

Write-Host "Created packages:"
Write-Host "  Portable EXE: $portableExePath"
Write-Host "  MSI: $msiPath"
Write-Host "  Setup EXE: $bundlePath"
