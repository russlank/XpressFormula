param(
    [string]$Configuration = "Release",
    [string]$Platform = "x64",
    [string]$PlatformToolset = "v143",
    [string]$WixVersion = "6.0.2",
    [string]$OutputDir = "artifacts\release",
    [switch]$SkipPackaging
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Push-Location $repoRoot

try {
    $version = python scripts/get_version.py
    if (-not $version) {
        throw "Failed to read version from scripts/get_version.py."
    }

    $solutionDir = Join-Path $repoRoot "src\"
    $intDir = Join-Path $repoRoot "build\obj\"
    $outDir = Join-Path $repoRoot "build\bin\"

    $msbuild = (Get-Command msbuild -ErrorAction SilentlyContinue | Select-Object -First 1).Source
    if (-not $msbuild) {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
        if (-not (Test-Path $vswhere)) {
            throw "Could not find msbuild in PATH and vswhere.exe was not found."
        }

        $vsInstall = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
        if (-not $vsInstall) {
            throw "Could not locate a Visual Studio installation with MSBuild."
        }

        $msbuild = Join-Path $vsInstall "MSBuild\Current\Bin\MSBuild.exe"
        if (-not (Test-Path $msbuild)) {
            throw "MSBuild executable not found at '$msbuild'."
        }
    }

    Write-Host "Using MSBuild: $msbuild"
    Write-Host "Version: $version"
    Write-Host "Configuration=$Configuration Platform=$Platform PlatformToolset=$PlatformToolset"

    & $msbuild "src\XpressFormula\XpressFormula.vcxproj" /t:Build /m `
        /p:Configuration=$Configuration `
        /p:Platform=$Platform `
        /p:PlatformToolset=$PlatformToolset `
        /p:SolutionDir="$solutionDir" `
        /p:IntDir="$intDir" `
        /p:OutDir="$outDir"

    if ($LASTEXITCODE -ne 0) {
        throw "MSBuild failed with exit code $LASTEXITCODE."
    }

    if ($SkipPackaging) {
        Write-Host "SkipPackaging was set. Build stage completed successfully."
        return
    }

    $wixCommand = Get-Command wix -ErrorAction SilentlyContinue
    if (-not $wixCommand) {
        throw "WiX CLI was not found in PATH. Install WiX 6.x: dotnet tool uninstall --global wix ; dotnet tool install --global wix --version $WixVersion ; wix extension add --global WixToolset.Bal.wixext/$WixVersion"
    }

    $wixInstalledVersion = (& wix --version).Trim()
    $wixMajor = $null
    if ($wixInstalledVersion -match '^(\d+)\.') {
        $wixMajor = [int]$Matches[1]
    }
    if ($wixMajor -ne 6) {
        throw "Unsupported WiX version '$wixInstalledVersion'. Install WiX 6.x: dotnet tool uninstall --global wix ; dotnet tool install --global wix --version $WixVersion ; wix extension add --global WixToolset.Bal.wixext/$WixVersion"
    }

    Write-Host "Using WiX: $wixInstalledVersion"

    & ".\packaging\build-packages.ps1" `
        -AppExePath "$repoRoot\build\bin\XpressFormula.exe" `
        -Version $version `
        -RequiredWixVersion $WixVersion `
        -OutputDir $OutputDir

    if ($LASTEXITCODE -ne 0) {
        throw "Packaging failed with exit code $LASTEXITCODE."
    }

    Write-Host "Local release pipeline simulation completed successfully."
    Write-Host "Artifacts: $(Join-Path $repoRoot $OutputDir)"
}
finally {
    Pop-Location
}
