# SPDX-License-Identifier: MIT
[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$templateRoot = Join-Path $repoRoot ".ai\template-solution"

$templateAppName = "ImGuiAppTemplate"
$templateTestName = "$templateAppName.Tests"
$templateRepoSlug = "your-org/$templateAppName"

function Reset-Directory {
    param([Parameter(Mandatory = $true)][string]$Path)

    if (-not (Test-Path $Path)) {
        New-Item -ItemType Directory -Path $Path -Force | Out-Null
        return
    }

    Get-ChildItem -Path $Path -Force | Remove-Item -Recurse -Force
}

function Ensure-ParentDirectory {
    param([Parameter(Mandatory = $true)][string]$Path)

    $parent = Split-Path -Parent $Path
    if ($parent -and -not (Test-Path $parent)) {
        New-Item -ItemType Directory -Path $parent -Force | Out-Null
    }
}

function Copy-FileRelative {
    param(
        [Parameter(Mandatory = $true)][string]$SourceRelativePath,
        [string]$DestinationRelativePath = $SourceRelativePath
    )

    $src = Join-Path $repoRoot $SourceRelativePath
    $dst = Join-Path $templateRoot $DestinationRelativePath

    if (-not (Test-Path $src)) {
        throw "Missing source file: $SourceRelativePath"
    }

    Ensure-ParentDirectory -Path $dst
    Copy-Item -LiteralPath $src -Destination $dst -Force
}

function Copy-DirectoryRelative {
    param(
        [Parameter(Mandatory = $true)][string]$SourceRelativePath,
        [string]$DestinationRelativePath = $SourceRelativePath
    )

    $src = Join-Path $repoRoot $SourceRelativePath
    $dst = Join-Path $templateRoot $DestinationRelativePath

    if (-not (Test-Path $src)) {
        throw "Missing source directory: $SourceRelativePath"
    }

    Ensure-ParentDirectory -Path $dst
    Copy-Item -LiteralPath $src -Destination $dst -Recurse -Force
}

function Rename-PathRelative {
    param(
        [Parameter(Mandatory = $true)][string]$OldRelativePath,
        [Parameter(Mandatory = $true)][string]$NewRelativePath
    )

    $oldPath = Join-Path $templateRoot $OldRelativePath
    $newPath = Join-Path $templateRoot $NewRelativePath

    if (-not (Test-Path $oldPath)) {
        return
    }

    Ensure-ParentDirectory -Path $newPath
    Move-Item -LiteralPath $oldPath -Destination $newPath -Force
}

function Write-TemplateFile {
    param(
        [Parameter(Mandatory = $true)][string]$RelativePath,
        [Parameter(Mandatory = $true)][string]$Content
    )

    $path = Join-Path $templateRoot $RelativePath
    Ensure-ParentDirectory -Path $path
    [System.IO.File]::WriteAllText($path, $Content)
}

function Get-ReplacementTargetFiles {
    $binaryExtensions = @(
        ".png", ".ico", ".ttf", ".otf", ".woff", ".woff2",
        ".exe", ".dll", ".lib", ".pdb", ".obj", ".idb",
        ".zip", ".7z", ".gz", ".pdf", ".jpg", ".jpeg", ".bmp", ".gif", ".webp"
    )

    $vendorImguiRoot = Join-Path $templateRoot "src\vendor\imgui"
    $vendorImguiExists = Test-Path $vendorImguiRoot
    if ($vendorImguiExists) {
        # Append trailing separator so "src\vendor\imguiExtra" is not falsely skipped.
        $vendorImguiRoot = $vendorImguiRoot.TrimEnd([System.IO.Path]::DirectorySeparatorChar) + [System.IO.Path]::DirectorySeparatorChar
    }

    foreach ($file in Get-ChildItem -Path $templateRoot -Recurse -File -Force) {
        if ($vendorImguiExists) {
            if ($file.FullName.StartsWith($vendorImguiRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
                continue
            }
        }

        $ext = [System.IO.Path]::GetExtension($file.Name)
        if ($ext -and ($binaryExtensions -contains $ext.ToLowerInvariant())) {
            continue
        }

        $file
    }
}

function Apply-TextReplacements {
    param(
        [Parameter(Mandatory = $true)]
        [object[]]$Replacements
    )

    foreach ($file in Get-ReplacementTargetFiles) {
        $original = [System.IO.File]::ReadAllText($file.FullName)
        $updated = $original

        foreach ($replacement in $Replacements) {
            $updated = $updated.Replace([string]$replacement.From, [string]$replacement.To)
        }

        if ($updated -ne $original) {
            [System.IO.File]::WriteAllText($file.FullName, $updated)
        }
    }
}

function Ensure-TemplateGitIgnoreOverrides {
    $gitignorePath = Join-Path $templateRoot ".gitignore"
    if (-not (Test-Path $gitignorePath)) {
        return
    }

    $text = [System.IO.File]::ReadAllText($gitignorePath)
    # Use (?:\r?\n|$) so lines at the very end of the file (no trailing newline) are also removed.
    $text = [System.Text.RegularExpressions.Regex]::Replace($text, '(?m)^!\.ai/template-solution/(?:\r?\n|$)', '')
    $text = [System.Text.RegularExpressions.Regex]::Replace($text, '(?m)^!\.ai/template-solution/\*\*(?:\r?\n|$)', '')

    if ($text -match "Template keeps \.ai developer guidance") {
        [System.IO.File]::WriteAllText($gitignorePath, $text)
        return
    }

    # Normalise trailing whitespace so the appended block always starts on its own line.
    $text = $text.TrimEnd("`r", "`n") + [System.Environment]::NewLine

    $append = @"

# Template keeps .ai developer guidance and skills under version control by default
!.ai/
!.ai/**
"@
    [System.IO.File]::WriteAllText($gitignorePath, $text + $append)
}

function Write-TemplateReadme {
    $content = @"
# $templateAppName

Starter template for a Windows C++ desktop application using ImGui, Win32, and DirectX 11.

This template was extracted from the XpressFormula solution and packaged to make new, similar
ImGui/CPP applications faster to start. It includes:

- Visual Studio solution (`src\$templateAppName.slnx`)
- Main application project (`src\$templateAppName`)
- Native test project (`src\$templateTestName`)
- Packaging (`packaging/`, WiX)
- GitHub workflows (`.github/workflows`)
- VS Code tasks/launch/config (`.vscode`)
- Developer rules/guides and reusable skills (`.ai`)
- Vendor snapshot (`src/vendor/imgui`) plus vendor refresh/cleanup scripts

## Quick Start

1. Copy this template folder to a new repository.
2. Run:

```powershell
.\scripts\initialize-template.ps1 -AppName MyApp -CompanyName "Your Company" -GitHubOwner your-org
```

3. Open `src\MyApp.slnx` in Visual Studio (or open the repo in VS Code).
4. Build with:

```powershell
.\scripts\invoke-msbuild.ps1 -ProjectPath src\MyApp.slnx -Configuration Debug -Platform x64
```

5. Run tests:

```powershell
.\scripts\run-tests.ps1 -Configuration Debug -Platform x64
```

## Notes

- The app/test code is a working sample (formula-plotting oriented) and is intended to be adapted.
- The `doc/` folder includes both reusable process/architecture docs and sample domain docs.
- Packaging, CI, and code-signing are wired for a typical Windows release flow and should be reviewed
  before publishing.
"@
    Write-TemplateFile -RelativePath "README.md" -Content $content
}

function Write-DocTemplateNotes {
    $content = @"
# Template Notes

This template keeps the original documentation set so you can reuse structure and writing style.

## Recommended first-pass cleanup for a new project

- Keep and adapt: `architecture.md`, `imgui-implementation-guide.md`, `run-guide.md`, `testing.md`,
  `release-packaging.md`, `project-vendors.md`, `code-signing.md`
- Review and likely replace/remove if not applicable:
  `expression-language.md`, `math-api-guide.md`, `math-api-cheatsheet.md`, `algorithms-guide.md`
- Update `index.md` links after removing or renaming docs

## Why the full docs are included

The goal is to preserve project-agnostic guidance completely while also providing real examples of
how a shipped ImGui/CPP app documents architecture, testing, vendors, and packaging.
"@
    Write-TemplateFile -RelativePath "doc\TEMPLATE-NOTES.md" -Content $content
}

function Write-BuildAndTestWorkflow {
    $content = @"
# SPDX-License-Identifier: MIT
name: Build and Test

on:
  pull_request:
  push:
    branches:
      - main
  workflow_dispatch:

jobs:
  build-test:
    runs-on: windows-latest

    steps:
      - name: Checkout
        uses: actions/checkout@v4

      - name: Setup MSBuild
        uses: microsoft/setup-msbuild@v2

      - name: Setup Python
        uses: actions/setup-python@v5
        with:
          python-version: "3.12"

      - name: Build (Debug x64)
        shell: pwsh
        run: .\scripts\build-ci.ps1 -Configuration Debug -Platform x64

      - name: Run Tests (Debug x64)
        shell: pwsh
        run: .\scripts\test-ci.ps1 -Configuration Debug -Platform x64
"@
    Write-TemplateFile -RelativePath ".github\workflows\build-and-test.yml" -Content $content
}

function Write-RunTestsScript {
    $content = @'
# SPDX-License-Identifier: MIT
[CmdletBinding()]
param(
    [string]$Configuration = "Debug",
    [ValidateSet("x64", "Win32")]
    [string]$Platform = "x64",
    [switch]$BuildFirst
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Push-Location $repoRoot

try {
    if ($BuildFirst) {
        & ".\scripts\invoke-msbuild.ps1" `
            -ProjectPath "src\ImGuiAppTemplate.slnx" `
            -Configuration $Configuration `
            -Platform $Platform `
            -Targets Build
        if ($LASTEXITCODE -ne 0) {
            throw "Build failed."
        }
    }

    $testExe = if ($Platform -eq "x64") {
        Join-Path $repoRoot "src\x64\$Configuration\ImGuiAppTemplate.Tests.exe"
    } else {
        Join-Path $repoRoot "src\$Configuration\ImGuiAppTemplate.Tests.exe"
    }

    if (-not (Test-Path $testExe)) {
        throw "Test executable not found: $testExe. Build first or pass -BuildFirst."
    }

    Write-Host "Running tests: $testExe"
    & $testExe
    if ($LASTEXITCODE -ne 0) {
        throw "Tests failed with exit code $LASTEXITCODE."
    }
}
finally {
    Pop-Location
}
'@
    Write-TemplateFile -RelativePath "scripts\run-tests.ps1" -Content $content
}

function Write-BuildCiScript {
    $content = @'
# SPDX-License-Identifier: MIT
[CmdletBinding()]
param(
    [string]$Configuration = "Debug",
    [ValidateSet("x64", "Win32")]
    [string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"

& "$PSScriptRoot\invoke-msbuild.ps1" `
    -ProjectPath "src\ImGuiAppTemplate.slnx" `
    -Configuration $Configuration `
    -Platform $Platform `
    -Targets Build
'@
    Write-TemplateFile -RelativePath "scripts\build-ci.ps1" -Content $content
}

function Write-TestCiScript {
    $content = @'
# SPDX-License-Identifier: MIT
[CmdletBinding()]
param(
    [string]$Configuration = "Debug",
    [ValidateSet("x64", "Win32")]
    [string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"

& "$PSScriptRoot\run-tests.ps1" `
    -Configuration $Configuration `
    -Platform $Platform
'@
    Write-TemplateFile -RelativePath "scripts\test-ci.ps1" -Content $content
}

function Write-InitializeTemplateScript {
    $content = @'
# SPDX-License-Identifier: MIT
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern("^[A-Za-z][A-Za-z0-9_]*$")]
    [string]$AppName,

    [string]$CompanyName = "Your Company",

    [string]$GitHubOwner = "your-org",

    [string]$RepositoryName,

    [switch]$SkipGuidRefresh
)

$ErrorActionPreference = "Stop"

$templateBaseName = "ImGuiAppTemplate"
$templateBaseTestName = "$templateBaseName.Tests"

if (-not $RepositoryName) {
    $RepositoryName = $AppName
}

function Ensure-ParentDirectory {
    param([Parameter(Mandatory = $true)][string]$Path)
    $parent = Split-Path -Parent $Path
    if ($parent -and -not (Test-Path $parent)) {
        New-Item -ItemType Directory -Path $parent -Force | Out-Null
    }
}

function Rename-PathIfExists {
    param(
        [Parameter(Mandatory = $true)][string]$OldPath,
        [Parameter(Mandatory = $true)][string]$NewPath
    )

    if (-not (Test-Path $OldPath)) {
        return
    }

    Ensure-ParentDirectory -Path $NewPath
    Move-Item -LiteralPath $OldPath -Destination $NewPath -Force
}

function Get-TextFiles {
    param([Parameter(Mandatory = $true)][string]$Root)

    $binaryExtensions = @(
        ".png", ".ico", ".ttf", ".otf", ".woff", ".woff2",
        ".exe", ".dll", ".lib", ".pdb", ".obj", ".idb",
        ".zip", ".7z", ".gz", ".pdf", ".jpg", ".jpeg", ".bmp", ".gif", ".webp"
    )

    $vendorImguiRoot = Join-Path $Root "src\vendor\imgui"
    $vendorImguiExists = Test-Path $vendorImguiRoot
    if ($vendorImguiExists) {
        $vendorImguiRoot = $vendorImguiRoot.TrimEnd([System.IO.Path]::DirectorySeparatorChar) + [System.IO.Path]::DirectorySeparatorChar
    }

    Get-ChildItem -Path $Root -Recurse -File -Force | Where-Object {
        $path = $_.FullName
        if ($vendorImguiExists -and $path.StartsWith($vendorImguiRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
            return $false
        }

        $ext = [System.IO.Path]::GetExtension($_.Name).ToLowerInvariant()
        return -not ($binaryExtensions -contains $ext)
    }
}

function Replace-InFiles {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][object[]]$Map
    )

    foreach ($file in Get-TextFiles -Root $Root) {
        $before = [System.IO.File]::ReadAllText($file.FullName)
        $after = $before
        foreach ($entry in $Map) {
            $after = $after.Replace([string]$entry.From, [string]$entry.To)
        }
        if ($after -ne $before) {
            [System.IO.File]::WriteAllText($file.FullName, $after)
        }
    }
}

function Replace-RegexInFile {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Pattern,
        [Parameter(Mandatory = $true)][System.Text.RegularExpressions.MatchEvaluator]$Evaluator
    )

    if (-not (Test-Path $Path)) {
        return
    }

    $text = [System.IO.File]::ReadAllText($Path)
    $updated = [System.Text.RegularExpressions.Regex]::Replace($text, $Pattern, $Evaluator)
    if ($updated -ne $text) {
        [System.IO.File]::WriteAllText($Path, $updated)
    }
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

$expectedSlnx = Join-Path $repoRoot "src\$templateBaseName.slnx"
if (-not (Test-Path $expectedSlnx)) {
    throw "Template base solution not found at '$expectedSlnx'. This script expects an uninitialized template checkout."
}

$publisherKey = ($CompanyName -replace "[^A-Za-z0-9]", "")
if ([string]::IsNullOrWhiteSpace($publisherKey)) {
    $publisherKey = "YourCompany"
}

$map = @(
    @{ From = "your-org/$templateBaseName"; To = "$GitHubOwner/$RepositoryName" },
    @{ From = "$templateBaseName.Tests"; To = "$AppName.Tests" },
    @{ From = $templateBaseName; To = $AppName },
    @{ From = $templateBaseName.ToLowerInvariant(); To = $AppName.ToLowerInvariant() },
    @{ From = $templateBaseName.ToUpperInvariant(); To = $AppName.ToUpperInvariant() },
    @{ From = "your-org"; To = $GitHubOwner },
    @{ From = "Your Name or Organization"; To = $CompanyName },
    @{ From = "YourCompany"; To = $publisherKey }
)

Replace-InFiles -Root $repoRoot -Map $map

# Rename files/folders after text replacement.
Rename-PathIfExists `
    -OldPath (Join-Path $repoRoot "src\$templateBaseName\Resources\$templateBaseName.ico") `
    -NewPath (Join-Path $repoRoot "src\$templateBaseName\Resources\$AppName.ico")
Rename-PathIfExists `
    -OldPath (Join-Path $repoRoot "src\$templateBaseName\Resources\$templateBaseName-variant1.ico") `
    -NewPath (Join-Path $repoRoot "src\$templateBaseName\Resources\$AppName-variant1.ico")
Rename-PathIfExists `
    -OldPath (Join-Path $repoRoot "src\$templateBaseName\Resources\$templateBaseName-variant2.ico") `
    -NewPath (Join-Path $repoRoot "src\$templateBaseName\Resources\$AppName-variant2.ico")
Rename-PathIfExists `
    -OldPath (Join-Path $repoRoot "src\$templateBaseName\Resources\$templateBaseName-variant3.ico") `
    -NewPath (Join-Path $repoRoot "src\$templateBaseName\Resources\$AppName-variant3.ico")
Rename-PathIfExists `
    -OldPath (Join-Path $repoRoot "src\$templateBaseName\$templateBaseName.vcxproj.filters") `
    -NewPath (Join-Path $repoRoot "src\$templateBaseName\$AppName.vcxproj.filters")
Rename-PathIfExists `
    -OldPath (Join-Path $repoRoot "src\$templateBaseName\$templateBaseName.vcxproj") `
    -NewPath (Join-Path $repoRoot "src\$templateBaseName\$AppName.vcxproj")
Rename-PathIfExists `
    -OldPath (Join-Path $repoRoot "src\$templateBaseName\$templateBaseName.vcxproj.user") `
    -NewPath (Join-Path $repoRoot "src\$templateBaseName\$AppName.vcxproj.user")
Rename-PathIfExists `
    -OldPath (Join-Path $repoRoot "src\$templateBaseTestName\$templateBaseTestName.vcxproj") `
    -NewPath (Join-Path $repoRoot "src\$templateBaseTestName\$AppName.Tests.vcxproj")
Rename-PathIfExists `
    -OldPath (Join-Path $repoRoot "src\$templateBaseTestName\$templateBaseTestName.vcxproj.user") `
    -NewPath (Join-Path $repoRoot "src\$templateBaseTestName\$AppName.Tests.vcxproj.user")
Rename-PathIfExists `
    -OldPath (Join-Path $repoRoot "src\$templateBaseName") `
    -NewPath (Join-Path $repoRoot "src\$AppName")
Rename-PathIfExists `
    -OldPath (Join-Path $repoRoot "src\$templateBaseTestName") `
    -NewPath (Join-Path $repoRoot "src\$AppName.Tests")
Rename-PathIfExists `
    -OldPath (Join-Path $repoRoot "src\$templateBaseName.slnx") `
    -NewPath (Join-Path $repoRoot "src\$AppName.slnx")

if (-not $SkipGuidRefresh) {
    $appProj = Join-Path $repoRoot "src\$AppName\$AppName.vcxproj"
    $testProj = Join-Path $repoRoot "src\$AppName.Tests\$AppName.Tests.vcxproj"
    $slnx = Join-Path $repoRoot "src\$AppName.slnx"
    $productWxs = Join-Path $repoRoot "packaging\wix\Product.wxs"
    $bundleWxs = Join-Path $repoRoot "packaging\wix\Bundle.wxs"

    $newAppProjectGuid = ([guid]::NewGuid()).ToString().ToUpperInvariant()
    $newTestProjectGuid = ([guid]::NewGuid()).ToString().ToUpperInvariant()
    $newMsiUpgradeCode = ([guid]::NewGuid()).ToString().ToUpperInvariant()
    $newBundleUpgradeCode = ([guid]::NewGuid()).ToString().ToUpperInvariant()

    Replace-RegexInFile -Path $appProj -Pattern "<ProjectGuid>\{[^}]+\}</ProjectGuid>" -Evaluator {
        param($m) "<ProjectGuid>{$newAppProjectGuid}</ProjectGuid>"
    }
    Replace-RegexInFile -Path $testProj -Pattern "<ProjectGuid>\{[^}]+\}</ProjectGuid>" -Evaluator {
        param($m) "<ProjectGuid>{$newTestProjectGuid}</ProjectGuid>"
    }

    if (Test-Path $slnx) {
        $slnxText = [System.IO.File]::ReadAllText($slnx)
        $escapedTestPath = [System.Text.RegularExpressions.Regex]::Escape("$AppName.Tests/$AppName.Tests.vcxproj")
        $escapedAppPath = [System.Text.RegularExpressions.Regex]::Escape("$AppName/$AppName.vcxproj")

        $slnxText = [System.Text.RegularExpressions.Regex]::Replace(
            $slnxText,
            "(<Project Path=""" + $escapedTestPath + """ Id="")[0-9a-fA-F-]{36}("" />)",
            [System.Text.RegularExpressions.MatchEvaluator]{
                param($m)
                return $m.Groups[1].Value + $newTestProjectGuid + $m.Groups[2].Value
            }
        )
        $slnxText = [System.Text.RegularExpressions.Regex]::Replace(
            $slnxText,
            "(<Project Path=""" + $escapedAppPath + """ Id="")[0-9a-fA-F-]{36}("" />)",
            [System.Text.RegularExpressions.MatchEvaluator]{
                param($m)
                return $m.Groups[1].Value + $newAppProjectGuid + $m.Groups[2].Value
            }
        )
        [System.IO.File]::WriteAllText($slnx, $slnxText)
    }

    Replace-RegexInFile -Path $productWxs -Pattern 'UpgradeCode="\{[^}]+\}"' -Evaluator {
        param($m) "UpgradeCode=""{$newMsiUpgradeCode}"""
    }
    Replace-RegexInFile -Path $bundleWxs -Pattern 'UpgradeCode="\{[^}]+\}"' -Evaluator {
        param($m) "UpgradeCode=""{$newBundleUpgradeCode}"""
    }
}

Write-Host "Template initialized."
Write-Host "AppName: $AppName"
Write-Host "CompanyName: $CompanyName"
Write-Host "Repository: $GitHubOwner/$RepositoryName"
Write-Host "Next: open src\$AppName.slnx and build."
'@
    Write-TemplateFile -RelativePath "scripts\initialize-template.ps1" -Content $content
}

function Write-VendorScriptsAndManifest {
    $vendorReadme = @"
# Vendor Dependencies

This folder contains vendored third-party code used by the template.

## Included vendor snapshot

- `imgui/` (Dear ImGui) for Win32 + DirectX 11 integration

## Vendor management scripts

- `scripts/vendor-refresh-imgui.ps1`: refresh/update the vendored ImGui tree from GitHub
- `scripts/vendor-cleanup-imgui.ps1`: optional cleanup/pruning after refresh

The scripts update `src/vendor/vendor-lock.json` so the pinned source/ref/commit is recorded.
"@
    Write-TemplateFile -RelativePath "src\vendor\README.md" -Content $vendorReadme

    $manifest = @"
{
  "vendors": [
    {
      "name": "imgui",
      "repo": "https://github.com/ocornut/imgui",
      "ref": "master",
      "commit": "replace-after-refresh",
      "path": "src/vendor/imgui",
      "license": "MIT",
      "notes": "Template ships with a local snapshot. Refresh and pin to a specific tag/commit before release."
    }
  ]
}
"@
    Write-TemplateFile -RelativePath "src\vendor\vendor-lock.json" -Content $manifest

    $refreshScript = @'
# SPDX-License-Identifier: MIT
[CmdletBinding()]
param(
    [string]$RepoUrl = "https://github.com/ocornut/imgui",
    [string]$Ref = "master",
    [switch]$RunCleanup,
    [string]$Destination = "src/vendor/imgui"
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$destPath = Join-Path $repoRoot $Destination
$tempRoot = Join-Path $env:TEMP ("imgui-refresh-" + [guid]::NewGuid().ToString("N"))
$clonePath = Join-Path $tempRoot "imgui"

try {
    New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null

    $git = (Get-Command git -ErrorAction Stop).Source
    Write-Host "Cloning $RepoUrl ($Ref)..."
    # Try shallow clone first (fast for branches/tags); fall back to full clone for raw commit SHAs.
    & $git clone --depth 1 --branch $Ref $RepoUrl $clonePath 2>$null
    if ($LASTEXITCODE -ne 0) {
        if (Test-Path $clonePath) { Remove-Item -Recurse -Force $clonePath }
        & $git clone $RepoUrl $clonePath
        if ($LASTEXITCODE -ne 0) {
            throw "git clone failed."
        }

        Push-Location $clonePath
        try {
            & $git checkout $Ref
            if ($LASTEXITCODE -ne 0) {
                throw "git checkout $Ref failed."
            }
        }
        finally {
            Pop-Location
        }
    }

    Push-Location $clonePath
    try {
        $commit = (& $git rev-parse HEAD).Trim()
        if (-not $commit) {
            throw "Failed to resolve commit hash."
        }
    }
    finally {
        Pop-Location
    }

    if (Test-Path $destPath) {
        Remove-Item -Recurse -Force $destPath
    }
    $destParent = Split-Path -Parent $destPath
    if ($destParent -and -not (Test-Path $destParent)) {
        New-Item -ItemType Directory -Path $destParent -Force | Out-Null
    }
    Copy-Item -LiteralPath $clonePath -Destination $destPath -Recurse -Force

    $gitDir = Join-Path $destPath ".git"
    if (Test-Path $gitDir) {
        Remove-Item -Recurse -Force $gitDir
    }

    if ($RunCleanup) {
        & "$PSScriptRoot\vendor-cleanup-imgui.ps1"
        if ($LASTEXITCODE -ne 0) {
            throw "vendor-cleanup-imgui.ps1 failed."
        }
    }

    $lockPath = Join-Path $repoRoot "src\vendor\vendor-lock.json"
    if (Test-Path $lockPath) {
        $json = Get-Content $lockPath -Raw | ConvertFrom-Json
        $imgui = $json.vendors | Where-Object { $_.name -eq "imgui" } | Select-Object -First 1
        if ($imgui) {
            $imgui.repo = $RepoUrl
            $imgui.ref = $Ref
            $imgui.commit = $commit
        }
        $json | ConvertTo-Json -Depth 8 | Set-Content -Path $lockPath
    }

    Write-Host "ImGui vendor refreshed."
    Write-Host "Path: $destPath"
    Write-Host "Commit: $commit"
}
finally {
    if (Test-Path $tempRoot) {
        Remove-Item -Recurse -Force $tempRoot
    }
}
'@
    Write-TemplateFile -RelativePath "scripts\vendor-refresh-imgui.ps1" -Content $refreshScript

    $cleanupScript = @'
# SPDX-License-Identifier: MIT
[CmdletBinding()]
param(
    [switch]$PruneExamples,
    [switch]$PruneDocs,
    [switch]$PruneMiscFonts
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$imguiRoot = Join-Path $repoRoot "src\vendor\imgui"

if (-not (Test-Path $imguiRoot)) {
    throw "ImGui vendor directory not found: $imguiRoot"
}

# Always remove upstream repo metadata if present.
$pathsToRemove = @(
    (Join-Path $imguiRoot ".git"),
    (Join-Path $imguiRoot ".github")
)

if ($PruneExamples) {
    $pathsToRemove += (Join-Path $imguiRoot "examples")
}
if ($PruneDocs) {
    $pathsToRemove += (Join-Path $imguiRoot "docs")
}
if ($PruneMiscFonts) {
    $pathsToRemove += (Join-Path $imguiRoot "misc\fonts")
}

foreach ($path in $pathsToRemove) {
    if (Test-Path $path) {
        Write-Host "Removing $path"
        Remove-Item -Recurse -Force $path
    }
}

Write-Host "ImGui vendor cleanup completed."
'@
    Write-TemplateFile -RelativePath "scripts\vendor-cleanup-imgui.ps1" -Content $cleanupScript
}

function Write-TemplateAiReadme {
    $content = @"
# AI Guidance Bundle

This `.ai` folder is copied into the template so a new repository starts with:

- `developer-guide/` (architecture, UI patterns, build/test/release, packaging, checklists)
- `skills/` (Codex skills for ImGui app foundation and WiX packaging)
- `solution-constitution.md` and workflow usage docs

Review and update project-specific rules after initializing the template.
"@
    Write-TemplateFile -RelativePath ".ai\README.md" -Content $content
}
function Remove-BuildArtifactsFromTemplate {
    # Build output directories that may exist on the local filesystem but should never
    # appear in the template.  These are normally .gitignored so they would not be in
    # a clean clone, but Copy-DirectoryRelative copies whatever is on disk.

    $projectDirs = @(
        (Join-Path $templateRoot "src\XpressFormula"),
        (Join-Path $templateRoot "src\XpressFormula.Tests")
    )

    $dirNamesToRemove = @("x64", "Debug", "Release")

    foreach ($projDir in $projectDirs) {
        if (-not (Test-Path $projDir)) { continue }

        # Remove well-known build output subdirectories.
        foreach ($dirName in $dirNamesToRemove) {
            $target = Join-Path $projDir $dirName
            if (Test-Path $target) {
                Remove-Item -LiteralPath $target -Recurse -Force
            }
        }

        # Remove the nested "XpressFormula\XpressFormula" or "XpressFormula.Tests\XpressFo.*"
        # intermediate output directories that MSBuild/VS sometimes creates.
        foreach ($child in Get-ChildItem -Path $projDir -Directory -Force -ErrorAction SilentlyContinue) {
            $name = $child.Name
            if ($name -match '^XpressFo\.' -or $name -eq 'XpressFormula') {
                $sub = Join-Path $child.FullName "x64"
                $sub2 = Join-Path $child.FullName "bin"
                if ((Test-Path $sub) -or (Test-Path $sub2)) {
                    Remove-Item -LiteralPath $child.FullName -Recurse -Force
                }
            }
        }
    }

    # Remove stray user/binary artifacts anywhere under the template source tree.
    $artifactPatterns = @("*.aps", "*.vcxproj.user", "vc*.pdb")
    foreach ($projDir in $projectDirs) {
        if (-not (Test-Path $projDir)) { continue }
        foreach ($pattern in $artifactPatterns) {
            foreach ($file in Get-ChildItem -Path $projDir -Filter $pattern -Recurse -File -Force -ErrorAction SilentlyContinue) {
                Remove-Item -LiteralPath $file.FullName -Force
            }
        }
    }
}

Write-Host "Generating template in $templateRoot"
Reset-Directory -Path $templateRoot

# Root config and repo metadata
Copy-FileRelative -SourceRelativePath ".gitignore"
Copy-FileRelative -SourceRelativePath "LICENSE"

# Editor/CI config
Copy-DirectoryRelative -SourceRelativePath ".vscode"
Copy-DirectoryRelative -SourceRelativePath ".github"

# Documentation, packaging, and scripts
Copy-DirectoryRelative -SourceRelativePath "doc"
Copy-DirectoryRelative -SourceRelativePath "packaging"
Copy-FileRelative -SourceRelativePath "scripts\invoke-msbuild.ps1"
Copy-FileRelative -SourceRelativePath "scripts\test-release-pipeline-local.ps1"
Copy-FileRelative -SourceRelativePath "scripts\get_version.py"

# Source solution/projects/vendors
Copy-DirectoryRelative -SourceRelativePath "src\vendor"
Copy-DirectoryRelative -SourceRelativePath "src\XpressFormula"
Copy-DirectoryRelative -SourceRelativePath "src\XpressFormula.Tests"
Copy-FileRelative -SourceRelativePath "src\XpressFormula.slnx"

# Remove build artifacts that are gitignored but present on disk.
Remove-BuildArtifactsFromTemplate

# AI guidance/rules/skills (exclude local environment snapshot + active todo)
Copy-DirectoryRelative -SourceRelativePath ".ai\developer-guide"
Copy-DirectoryRelative -SourceRelativePath ".ai\skills"
Copy-FileRelative -SourceRelativePath ".ai\developer-ai-agent-usage-guide.md"
Copy-FileRelative -SourceRelativePath ".ai\new-application-todo-template.md"
Copy-FileRelative -SourceRelativePath ".ai\solution-constitution.md"

# Rename project paths/files to a neutral template name.
Rename-PathRelative -OldRelativePath "src\XpressFormula\Resources\XpressFormula.ico" -NewRelativePath "src\XpressFormula\Resources\$templateAppName.ico"
Rename-PathRelative -OldRelativePath "src\XpressFormula\Resources\XpressFormula-variant1.ico" -NewRelativePath "src\XpressFormula\Resources\$templateAppName-variant1.ico"
Rename-PathRelative -OldRelativePath "src\XpressFormula\Resources\XpressFormula-variant2.ico" -NewRelativePath "src\XpressFormula\Resources\$templateAppName-variant2.ico"
Rename-PathRelative -OldRelativePath "src\XpressFormula\Resources\XpressFormula-variant3.ico" -NewRelativePath "src\XpressFormula\Resources\$templateAppName-variant3.ico"
Rename-PathRelative -OldRelativePath "src\XpressFormula\XpressFormula.vcxproj.filters" -NewRelativePath "src\XpressFormula\$templateAppName.vcxproj.filters"
Rename-PathRelative -OldRelativePath "src\XpressFormula\XpressFormula.vcxproj" -NewRelativePath "src\XpressFormula\$templateAppName.vcxproj"
Rename-PathRelative -OldRelativePath "src\XpressFormula.Tests\XpressFormula.Tests.vcxproj" -NewRelativePath "src\XpressFormula.Tests\$templateTestName.vcxproj"
Rename-PathRelative -OldRelativePath "src\XpressFormula" -NewRelativePath "src\$templateAppName"
Rename-PathRelative -OldRelativePath "src\XpressFormula.Tests" -NewRelativePath "src\$templateTestName"
Rename-PathRelative -OldRelativePath "src\XpressFormula.slnx" -NewRelativePath "src\$templateAppName.slnx"

# Text replacements (ordered longest -> shortest).
$replacements = @(
    @{ From = "https://github.com/russlank/XpressFormula"; To = "https://github.com/$templateRepoSlug" },
    @{ From = "russlank/XpressFormula"; To = $templateRepoSlug },
    @{ From = "russlank"; To = "your-org" },
    @{ From = "Russlan Kafri"; To = "Your Name or Organization" },
    @{ From = "Digixoil"; To = "YourCompany" },
    @{ From = "XpressFormula.Tests"; To = $templateTestName },
    @{ From = "XpressFormula"; To = $templateAppName },
    @{ From = "xpressformula"; To = $templateAppName.ToLowerInvariant() },
    @{ From = "XPRESSFORMULA"; To = $templateAppName.ToUpperInvariant() },
    @{ From = "XF_"; To = "APP_" },
    @{ From = "XfBuild"; To = "AppBuild" },
    @{ From = "Mathematical Plotter"; To = "ImGui Desktop App" }
)

Apply-TextReplacements -Replacements $replacements
Ensure-TemplateGitIgnoreOverrides

# Template-specific additions and overrides
Write-TemplateReadme
Write-DocTemplateNotes
Write-BuildAndTestWorkflow
Write-RunTestsScript
Write-BuildCiScript
Write-TestCiScript
Write-InitializeTemplateScript
Write-VendorScriptsAndManifest
Write-TemplateAiReadme

Write-Host "Template generation complete."
Write-Host "Output: $templateRoot"
