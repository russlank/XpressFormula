# SPDX-License-Identifier: MIT
[CmdletBinding()]
param(
    [string]$ProjectPath = "src\XpressFormula.slnx",
    [string]$Configuration = "Debug",
    [string]$Platform = "x64",
    [string]$Targets = "Build",
    [string[]]$AdditionalMsBuildArgs = @()
)

$ErrorActionPreference = "Stop"

function Get-MSBuildExecutable {
    $msbuildCmd = Get-Command msbuild -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($msbuildCmd) {
        $path = $msbuildCmd.Source
        if (-not $path) {
            $path = $msbuildCmd.Path
        }
        if ($path -and (Test-Path $path)) {
            return $path
        }
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        throw "Could not find MSBuild in PATH and vswhere.exe was not found at '$vswhere'."
    }

    $vsInstall = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($vsInstall)) {
        throw "Could not locate a Visual Studio installation with MSBuild."
    }

    $msbuild = Join-Path $vsInstall.Trim() "MSBuild\Current\Bin\MSBuild.exe"
    if (-not (Test-Path $msbuild)) {
        throw "MSBuild executable not found at '$msbuild'."
    }

    return $msbuild
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$fullProjectPath = Join-Path $repoRoot $ProjectPath
if (-not (Test-Path $fullProjectPath)) {
    throw "Project or solution path not found: '$ProjectPath' (resolved to '$fullProjectPath')."
}

$msbuildExe = Get-MSBuildExecutable
Write-Host "Using MSBuild: $msbuildExe"
Write-Host "Building: $ProjectPath"
Write-Host "Configuration=$Configuration Platform=$Platform Targets=$Targets"

Push-Location $repoRoot
try {
    $msbuildArgs = @(
        $ProjectPath,
        "/t:$Targets",
        "/p:Configuration=$Configuration",
        "/p:Platform=$Platform",
        "/m"
    )

    if ($AdditionalMsBuildArgs -and $AdditionalMsBuildArgs.Count -gt 0) {
        $msbuildArgs += $AdditionalMsBuildArgs
    }

    & $msbuildExe @msbuildArgs
    if ($LASTEXITCODE -ne 0) {
        throw "MSBuild failed with exit code $LASTEXITCODE."
    }
}
finally {
    Pop-Location
}
