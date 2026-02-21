param(
    [Parameter(Mandatory = $true)]
    [string]$AppExePath,

    [Parameter(Mandatory = $true)]
    [string]$Version,

    [string]$OutputDir = "artifacts\release"
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

& wix build $productWxs `
    -arch x64 `
    -dAppExePath="$resolvedExe" `
    -dAppIconPath="$iconPath" `
    -dProductVersion="$Version" `
    -out $msiPath

& wix build $bundleWxs `
    -arch x64 `
    -ext WixToolset.Bal.wixext `
    -dMsiPath="$msiPath" `
    -dAppIconPath="$iconPath" `
    -dProductVersion="$Version" `
    -out $bundlePath

Write-Host "Created packages:"
Write-Host "  Portable EXE: $portableExePath"
Write-Host "  MSI: $msiPath"
Write-Host "  Setup EXE: $bundlePath"
