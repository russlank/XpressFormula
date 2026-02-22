<!-- SPDX-License-Identifier: MIT -->
# XpressFormula Run Guide

## Prerequisites

- Windows 10/11
- Visual Studio 2022/2026 with **Desktop development with C++**
- Windows 10/11 SDK

See also:

- [`../README.md`](../README.md)
- [`index.md`](index.md)
- [`project-vendors.md`](project-vendors.md)

## Run from Visual Studio

1. Open [`src/XpressFormula.slnx`](../src/XpressFormula.slnx) in Visual Studio.
2. Set startup project to `XpressFormula`.
3. Select configuration:
   - `Debug | x64` (recommended)
4. Press `F5` (Debug) or `Ctrl+F5` (Run without Debugger).

To run tests in Visual Studio:

1. Build the solution.
2. Run `XpressFormula.Tests` (it is a console test runner executable).

## Run from VS Code

If your local checkout includes VS Code configuration:

- Tasks: `.vscode/tasks.json`
- Launch configs: `.vscode/launch.json`

Steps:

1. Open the folder in VS Code.
2. Press `Ctrl+Shift+B` and run:
   - `Build XpressFormula (x64 Debug)`
3. Press `F5` and choose:
   - `Debug XpressFormula (x64)`

Run tests in VS Code:

1. `Terminal -> Run Task...`
2. Select `Run Tests (x64 Debug)`.

## Run from CLI (PowerShell)

From repository root:

```powershell
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
& "$vs\MSBuild\Current\Bin\MSBuild.exe" "src\XpressFormula.slnx" /t:Build /p:Configuration=Debug /p:Platform=x64 /m
```

Run app:

```powershell
.\src\x64\Debug\XpressFormula.exe
```

Run tests:

```powershell
.\src\x64\Debug\XpressFormula.Tests.exe
```

## Using 2D and 3D Plot Modes

After launch:

1. Add a formula in the sidebar (`+ Add Formula`).
2. Enter one of these forms:
   - `sin(x)` for a 2D curve (`y=f(x)`)
   - `x^2+y^2` or `z=sin(x)*cos(y)` for a 3D surface (`z=f(x,y)`)
   - `x^2+y^2=100` for an implicit equation contour (`F(x,y)=0`)
   - `x^2+y^2+z^2=16` for an implicit 3D surface (`F(x,y,z)=0`)
   - `(x^2+y^2+z^2+21)^2 - 100*(x^2+y^2) = 0` for a torus-like implicit 3D surface
3. In the **View Controls** section:
   - Choose **3D Surfaces / Implicit** or **2D Heatmap** for `x,y` and implicit `F(x,y,z)=0` formulas.
   - Tune azimuth, elevation, z-scale, surface density, implicit surface quality, and opacity for 3D.
   - For implicit 3D equations, keep the formula `z slice / center` near the shape center (often `0`) and make sure the visible `X/Y` range contains the shape (for example, a sphere `x^2+y^2+z^2=16` needs roughly `[-4,4]` in both `X` and `Y`).
4. Use mouse drag to pan and mouse wheel to zoom domain coordinates.

## Export Plot Image

Use the sidebar **Export** section:

1. Click **Save Plot Image...** to export the current plot area to `.png` or `.bmp`.
2. Click **Copy Plot To Clipboard** to copy the current plot area image.

## Build Metadata Display

The sidebar includes a **Build Metadata** section showing:

- repository URL
- branch
- commit
- build version

These values are embedded at compile time by CI/local pipeline scripts.

## Troubleshooting

- If the app does not open a window, rebuild `Debug|x64` and run again.
- If startup fails, the app now shows a startup error dialog to indicate initialization failure.

## License

This document is licensed under the MIT License. See [`../LICENSE`](../LICENSE).
