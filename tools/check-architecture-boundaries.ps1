# SPDX-License-Identifier: MIT
# Lightweight source boundary checks for XpressFormula.

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$failures = New-Object System.Collections.Generic.List[string]

function Get-RelativePath([string]$Path) {
    return $Path.Substring($repoRoot.Length).TrimStart([char[]]@('\', '/'))
}

function Get-SourceFiles([string[]]$Roots) {
    $files = New-Object System.Collections.Generic.List[System.IO.FileInfo]
    foreach ($root in $Roots) {
        $absoluteRoot = Join-Path $repoRoot $root
        if (-not (Test-Path -LiteralPath $absoluteRoot)) {
            continue
        }
        Get-ChildItem -LiteralPath $absoluteRoot -Recurse -File -Include *.h, *.cpp |
            ForEach-Object { $files.Add($_) }
    }
    return $files
}

function Test-RuleSet(
    [string]$RuleName,
    [string[]]$Roots,
    [hashtable[]]$Patterns,
    [string[]]$ExcludeRelative = @()
) {
    $excludeSet = @{}
    foreach ($exclude in $ExcludeRelative) {
        $excludeSet[$exclude.ToLowerInvariant()] = $true
    }

    foreach ($file in Get-SourceFiles $Roots) {
        $relative = Get-RelativePath $file.FullName
        $normalized = $relative.Replace('/', '\').ToLowerInvariant()
        if ($excludeSet.ContainsKey($normalized)) {
            continue
        }

        $lineNumber = 0
        foreach ($line in Get-Content -LiteralPath $file.FullName) {
            ++$lineNumber
            foreach ($pattern in $Patterns) {
                if ($line -match $pattern.Regex) {
                    $failures.Add(("{0}:{1}: {2}: {3}" -f $relative, $lineNumber, $RuleName, $pattern.Message))
                }
            }
        }
    }
}

$includeUiPattern = '#\s*include\s*[<"].*(\.\.[\\/])?UI[\\/]'

Test-RuleSet `
    -RuleName 'Infrastructure must not include UI' `
    -Roots @('src\XpressFormula\Infrastructure') `
    -Patterns @(
        @{ Regex = $includeUiPattern; Message = 'infrastructure source includes a UI header' }
    )

Test-RuleSet `
    -RuleName 'Expression and core runtime must stay pure' `
    -Roots @('src\XpressFormula\Core', 'src\XpressFormula\Expression') `
    -Patterns @(
        @{ Regex = $includeUiPattern; Message = 'expression source includes a UI header' },
        @{ Regex = '#\s*include\s*[<"].*Platform[\\/]Windows'; Message = 'expression source includes Windows platform services' },
        @{ Regex = '#\s*include\s*[<"]Windows\.h'; Message = 'expression source includes Windows.h' },
        @{ Regex = '#\s*include\s*[<"]imgui\.h'; Message = 'expression source includes ImGui' }
    )

Test-RuleSet `
    -RuleName 'Model must stay pure' `
    -Roots @('src\XpressFormula\Model') `
    -Patterns @(
        @{ Regex = $includeUiPattern; Message = 'model source includes a UI header' },
        @{ Regex = '#\s*include\s*[<"].*Platform[\\/]Windows'; Message = 'model source includes Windows platform services' },
        @{ Regex = '#\s*include\s*[<"]Windows\.h'; Message = 'model source includes Windows.h' },
        @{ Regex = '#\s*include\s*[<"]imgui\.h'; Message = 'model source includes ImGui' },
        @{ Regex = '#\s*include\s*[<"].*Infrastructure[\\/]Serialization'; Message = 'model source includes JSON infrastructure' },
        @{ Regex = '#\s*include\s*[<"].*Infrastructure[\\/]Persistence[\\/]ProjectRepository'; Message = 'model source includes the project repository' },
        @{ Regex = '\bProjectRepository\b'; Message = 'model source references the project repository' },
        @{ Regex = '\bJson(Parser|Writer|Value)?\b'; Message = 'model source references JSON infrastructure names' }
    )

Test-RuleSet `
    -RuleName 'Geometry and meshing must not depend on ImGui' `
    -Roots @('src\XpressFormula\Plotting\Geometry', 'src\XpressFormula\Plotting\Meshing') `
    -Patterns @(
        @{ Regex = '#\s*include\s*[<"]imgui\.h'; Message = 'pure plotting geometry/meshing includes ImGui' }
    )

Test-RuleSet `
    -RuleName 'Reusable UI must not implement platform internals' `
    -Roots @('src\XpressFormula\UI') `
    -ExcludeRelative @('src\xpressformula\ui\application.cpp', 'src\xpressformula\ui\application.h') `
    -Patterns @(
        @{ Regex = '#\s*include\s*[<"].*Platform[\\/]Windows'; Message = 'UI component source includes platform services directly' },
        @{ Regex = '#\s*include\s*[<"]filesystem[>"]'; Message = 'UI component source includes filesystem' },
        @{ Regex = '\bstd::filesystem\b'; Message = 'UI component source uses filesystem' },
        @{ Regex = '\b(WinHttpClient|WicImageEncoder|ClipboardService|FileDialogService|ShellService)\b'; Message = 'UI component source references a platform service implementation' },
        @{ Regex = '\b(copyDibImageBgra|copyUtf16Text|writeTextAtomically)\b'; Message = 'UI component source performs platform or file output work' }
    )

$testsProject = Join-Path $repoRoot 'src\XpressFormula.Tests\XpressFormula.Tests.vcxproj'
if (Test-Path -LiteralPath $testsProject) {
    $lineNumber = 0
    foreach ($line in Get-Content -LiteralPath $testsProject) {
        ++$lineNumber
        if ($line -match '<ClCompile\s+Include="..\\XpressFormula\\.*\.cpp"') {
            $relative = Get-RelativePath $testsProject
            $failures.Add(("{0}:{1}: Tests must not compile production .cpp files directly: {2}" -f $relative, $lineNumber, $line.Trim()))
        }
    }
}

if ($failures.Count -gt 0) {
    Write-Host 'Architecture boundary check failed:'
    foreach ($failure in $failures) {
        Write-Host "  $failure"
    }
    exit 1
}

Write-Host 'Architecture boundary check passed.'
