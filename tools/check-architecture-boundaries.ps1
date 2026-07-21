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

function Get-FirstPartyProjectFiles {
    Get-ChildItem -LiteralPath (Join-Path $repoRoot 'src') -Recurse -File -Filter *.vcxproj |
        Where-Object { $_.FullName -notmatch '[\\/]vendor[\\/]' }
}

function Get-ProjectKey([System.IO.FileInfo]$ProjectFile) {
    Split-Path -Leaf (Split-Path -Parent $ProjectFile.FullName)
}

function Get-ProjectXml([System.IO.FileInfo]$ProjectFile) {
    $document = New-Object System.Xml.XmlDocument
    $document.Load($ProjectFile.FullName)
    return ,$document
}

function Get-MsBuildNamespaceManager([xml]$Xml) {
    $namespaceManager = New-Object System.Xml.XmlNamespaceManager($Xml.NameTable)
    $namespaceManager.AddNamespace('msb', 'http://schemas.microsoft.com/developer/msbuild/2003')
    return ,$namespaceManager
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

$firstPartyProjects = @(Get-FirstPartyProjectFiles)
$expectedToolset = 'v145'
$allowedProjectReferences = @{
    'XpressFormula.Expression' = @()
    'XpressFormula.Model' = @('XpressFormula.Expression')
    'XpressFormula.Plotting' = @('XpressFormula.Expression', 'XpressFormula.Model')
    'XpressFormula.Infrastructure' = @('XpressFormula.Model')
    'XpressFormula.UI' = @('XpressFormula.Expression', 'XpressFormula.Model', 'XpressFormula.Plotting')
    'XpressFormula.App' = @('XpressFormula.Expression', 'XpressFormula.Model', 'XpressFormula.Plotting', 'XpressFormula.Infrastructure', 'XpressFormula.UI')
    'XpressFormula.Tests' = @('XpressFormula.Expression', 'XpressFormula.Model', 'XpressFormula.Plotting', 'XpressFormula.Infrastructure', 'XpressFormula.App', 'XpressFormula.UI')
    'XpressFormula' = @('XpressFormula.Expression', 'XpressFormula.Model', 'XpressFormula.Plotting', 'XpressFormula.Infrastructure', 'XpressFormula.App', 'XpressFormula.UI')
}
$cppOwners = @{}

foreach ($projectFile in $firstPartyProjects) {
    $projectKey = Get-ProjectKey $projectFile
    $relativeProject = Get-RelativePath $projectFile.FullName
    $xml = Get-ProjectXml $projectFile
    $ns = Get-MsBuildNamespaceManager $xml

    foreach ($toolset in $xml.DocumentElement.SelectNodes('//msb:PlatformToolset', $ns)) {
        if ($toolset.InnerText -ne $expectedToolset) {
            $failures.Add(("{0}: PlatformToolset drift: expected {1}, found {2}" -f $relativeProject, $expectedToolset, $toolset.InnerText))
        }
    }

    foreach ($warningLevel in $xml.DocumentElement.SelectNodes('//msb:ClCompile/msb:WarningLevel', $ns)) {
        if ($warningLevel.InnerText -ne 'Level4') {
            $failures.Add(("{0}: first-party projects must build at /W4; found {1}" -f $relativeProject, $warningLevel.InnerText))
        }
    }

    foreach ($compileSettings in $xml.DocumentElement.SelectNodes('//msb:ItemDefinitionGroup/msb:ClCompile', $ns)) {
        if (-not $compileSettings.SelectSingleNode('msb:WarningLevel', $ns)) {
            continue
        }
        $additionalOptions = $compileSettings.SelectSingleNode('msb:AdditionalOptions', $ns)
        if (-not $additionalOptions -or $additionalOptions.InnerText -notmatch '/Zc:__cplusplus') {
            $failures.Add(("{0}: ClCompile settings must include /Zc:__cplusplus" -f $relativeProject))
        }
    }

    foreach ($reference in $xml.DocumentElement.SelectNodes('//msb:ProjectReference', $ns)) {
        $include = $reference.Include
        $referencePath = Join-Path (Split-Path -Parent $projectFile.FullName) $include
        $resolvedReference = Resolve-Path -LiteralPath $referencePath -ErrorAction SilentlyContinue
        if (-not $resolvedReference) {
            $failures.Add(("{0}: ProjectReference target not found: {1}" -f $relativeProject, $include))
            continue
        }

        $referenceKey = Split-Path -Leaf (Split-Path -Parent $resolvedReference.Path)
        if (-not $allowedProjectReferences.ContainsKey($projectKey) -or
            $allowedProjectReferences[$projectKey] -notcontains $referenceKey) {
            $failures.Add(("{0}: forbidden ProjectReference from {1} to {2}" -f $relativeProject, $projectKey, $referenceKey))
        }
    }

    foreach ($compile in $xml.DocumentElement.SelectNodes('//msb:ClCompile', $ns)) {
        $include = $compile.Include
        if ([string]::IsNullOrWhiteSpace($include)) {
            continue
        }
        if ($include -match '[\\/]vendor[\\/]') {
            continue
        }

        $sourcePath = Join-Path (Split-Path -Parent $projectFile.FullName) $include
        $resolvedSource = Resolve-Path -LiteralPath $sourcePath -ErrorAction SilentlyContinue
        if (-not $resolvedSource) {
            continue
        }
        if ([System.IO.Path]::GetExtension($resolvedSource.Path) -ne '.cpp') {
            continue
        }

        $normalizedSource = $resolvedSource.Path.ToLowerInvariant()
        if ($projectKey -eq 'XpressFormula.Tests' -and
            $normalizedSource.StartsWith((Join-Path $repoRoot 'src\XpressFormula\').ToLowerInvariant())) {
            $failures.Add(("{0}: tests must not compile production .cpp files directly: {1}" -f $relativeProject, $include))
        }

        if ($projectKey -ne 'XpressFormula.Tests') {
            if ($cppOwners.ContainsKey($normalizedSource) -and $cppOwners[$normalizedSource] -ne $projectKey) {
                $failures.Add(("{0}: production .cpp is also owned by {1}: {2}" -f $relativeProject, $cppOwners[$normalizedSource], (Get-RelativePath $resolvedSource.Path)))
            } else {
                $cppOwners[$normalizedSource] = $projectKey
            }
        }
    }
}

$publicDocFiles = @()
foreach ($path in @('README.md', 'CONTRIBUTING.md')) {
    $absolute = Join-Path $repoRoot $path
    if (Test-Path -LiteralPath $absolute) {
        $publicDocFiles += Get-Item -LiteralPath $absolute
    }
}
$docRoot = Join-Path $repoRoot 'doc'
if (Test-Path -LiteralPath $docRoot) {
    $publicDocFiles += Get-ChildItem -LiteralPath $docRoot -Recurse -File -Filter *.md
}
foreach ($file in $publicDocFiles) {
    $lineNumber = 0
    foreach ($line in Get-Content -LiteralPath $file.FullName) {
        ++$lineNumber
        if ($line -match '\.ai([\\/]|$|\b)') {
            $failures.Add(("{0}:{1}: public documentation must not reference private .ai planning files" -f (Get-RelativePath $file.FullName), $lineNumber))
        }
    }
}

$functionRegistryPath = Join-Path $repoRoot 'src\XpressFormula\Core\FunctionRegistry.cpp'
$expressionReferencePath = Join-Path $repoRoot 'doc\expression-language.md'
if ((Test-Path -LiteralPath $functionRegistryPath) -and
    (Test-Path -LiteralPath $expressionReferencePath)) {
    $registryText = Get-Content -LiteralPath $functionRegistryPath -Raw
    $referenceText = Get-Content -LiteralPath $expressionReferencePath -Raw
    $matches = [System.Text.RegularExpressions.Regex]::Matches(
        $registryText,
        'FunctionId::\w+,\s*"([^"]+)"')
    foreach ($match in $matches) {
        $functionName = $match.Groups[1].Value
        $needle = '`' + $functionName + '('
        if (-not $referenceText.Contains($needle)) {
            $failures.Add(("doc\expression-language.md: built-in function '{0}' is missing from the public expression reference" -f $functionName))
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
