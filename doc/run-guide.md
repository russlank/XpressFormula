<!-- SPDX-License-Identifier: MIT -->
# XpressFormula Run Guide

## Prerequisites

- Windows 10/11
- Visual Studio 2022/2026 with **Desktop development with C++**
- Windows 10/11 SDK
- VS Code (optional, if using the VS Code workflow)
- VS Code extensions (recommended for VS Code workflow):
  - `ms-vscode.cpptools` (C/C++)
  - `ms-vscode.powershell` (PowerShell tasks/scripts)

See also:

- [`../README.md`](../README.md)
- [`index.md`](index.md)
- [`project-vendors.md`](project-vendors.md)

## Run from Visual Studio

1. Open [`src/XpressFormula.slnx`](../src/XpressFormula.slnx) in Visual Studio.
2. Set startup project to `XpressFormula`.
3. Select configuration:
   - `Debug | x64` (recommended)
   - `Debug | Win32` (optional legacy/compatibility build)
4. Press `F5` (Debug) or `Ctrl+F5` (Run without Debugger).

Recommended Visual Studio debugging settings:

- Keep startup project set to `XpressFormula` for app debugging.
- Use `XpressFormula.Tests` as startup project when debugging the test runner.
- Keep the working directory as the project folder (`$(ProjectDir)`) if you want behavior consistent with the repo VS Code launch config (for example `imgui.ini` location).

To run tests in Visual Studio:

1. Build the solution.
2. Run `XpressFormula.Tests` (it is a console test runner executable).

## Run from VS Code

This repository includes shared VS Code workspace configuration:

- Tasks: `.vscode/tasks.json`
- Launch configs: `.vscode/launch.json`
- Extension recommendations: `.vscode/extensions.json`
- C/C++ IntelliSense config: `.vscode/c_cpp_properties.json`
- Minimal shared workspace settings: `.vscode/settings.json`

Recommended VS Code setup/settings:

- Open the repository root folder (not only `src/`), so the shared tasks and launch configs are detected.
- Install the recommended extensions when prompted.
- Use the `cppvsdbg` debugger configuration (the included launch configs already do this).
- Keep the launch working directory as `src\\XpressFormula` (the included config already sets it) to match Visual Studio behavior.
- In VS Code, use `C/C++: Select a Configuration` and choose `x64-Debug` (recommended) or `Win32-Debug` to match the build/debug target.
- The shared `.vscode/settings.json` hides common build-output folders and reduces file-watcher/search noise; it does not set personal editor formatting/theme preferences.

Steps:

1. Open the folder in VS Code.
2. Press `Ctrl+Shift+B` and run:
   - `Build XpressFormula (x64 Debug)`
3. Press `F5` and choose:
   - `Debug XpressFormula (x64)`

Win32 alternative in VS Code (optional):

1. `Terminal -> Run Task...`
2. Select `Build XpressFormula (Win32 Debug)` (or `Run XpressFormula (Win32 Debug)`).
3. Press `F5` and choose:
   - `Debug XpressFormula (Win32)`

Run without debugger in VS Code:

1. `Terminal -> Run Task...`
2. Select `Run XpressFormula (x64 Debug)`.

Run tests in VS Code:

1. `Terminal -> Run Task...`
2. Select `Run Tests (x64 Debug)`.

Debug the test runner in VS Code:

1. Press `F5`
2. Choose `Debug XpressFormula.Tests (x64)`.

Win32 test alternatives in VS Code:

1. `Terminal -> Run Task...`
2. Select `Run Tests (Win32 Debug)`, or press `F5` and choose `Debug XpressFormula.Tests (Win32)`.

## Run from CLI (PowerShell)

From repository root:

```powershell
.\scripts\invoke-msbuild.ps1 -ProjectPath "src\XpressFormula.slnx" -Configuration Debug -Platform x64 -Targets Build
```

The VS Code build tasks call the same helper script so CLI and VS Code use the same MSBuild discovery logic.

Win32 build example:

```powershell
.\scripts\invoke-msbuild.ps1 -ProjectPath "src\XpressFormula.slnx" -Configuration Debug -Platform Win32 -Targets Build
```

Run app:

```powershell
.\src\x64\Debug\XpressFormula.exe
```

Run tests:

```powershell
.\src\x64\Debug\XpressFormula.Tests.exe
```

Win32 outputs (if built with `Platform=Win32`):

- App: `.\src\Debug\XpressFormula.exe`
- Tests: `.\src\Debug\XpressFormula.Tests.exe`

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
   - In **2D / 3D Formula Rendering**, choose one of:
     - **Auto**: mixed visible 2D+3D formulas render in 2D; only visible 3D-capable formulas render in 3D.
     - **Force 3D Surfaces / Implicit**: always render 3D-capable formulas as 3D surfaces/meshes.
     - **Force 2D Heatmap / Cross-Section**: render `z=f(x,y)` and implicit `F(x,y,z)=0` in 2D representations.
   - Open the **Display** accordion to toggle **Show Grid**, **Show Coordinates**, **Show Wires**, and 3D display helpers such as **Show Envelope Box**, **Show XYZ Dimension Arrows**, and **Auto Rotate**.
   - Tune azimuth, elevation, z-scale, surface density, implicit surface quality, and opacity in the **3D Camera** section.
   - In 3D mode, **Show Grid** draws a projected XY plane with translucent fill and thick frame. Surface rendering is split around `z=0` so the plane is visually interleaved between below-plane and above-plane geometry.
   - The **XYZ Dimension Arrows** gizmo is shown near the lower-left of the plot viewport only when coordinates are hidden (mutually exclusive with **Show Coordinates**).
   - Keep **Optimize Rendering** enabled for lower idle GPU usage and smoother 3D dragging/zooming (temporary interaction-time quality reduction for heavy implicit meshes).
   - For implicit 3D equations, keep the formula `z slice / center` near the shape center (often `0`) and make sure the visible `X/Y` range contains the shape (for example, a sphere `x^2+y^2+z^2=16` needs roughly `[-4,4]` in both `X` and `Y`).
4. Use mouse drag to pan and mouse wheel to zoom domain coordinates.

## Export Plot Image

Use the sidebar **Export** section:

1. Click **Open Export Dialog...**.
2. Configure export options:
   - output `Width` / `Height`
   - **Lock Aspect Ratio** (or free width/height)
   - **Color** vs **Grayscale**
   - exported **Background Color** and **Background Opacity**
   - include/exclude **Grid**, **Coordinates**, and **Wires / Wireframe**
   - include/exclude **Envelope Box (3D)**
   - use the **Preview** section to inspect export settings and click **Refresh Preview** after major changes if needed
3. Click **Copy To Clipboard** to copy the exported plot image.
4. Click **Save To File...** to save the exported plot as `.png` or `.bmp`.

Notes:

- Export uses the current formulas and current view/zoom.
- Background/grid/coordinate/wire/envelope export options are applied only to an offscreen export render pass (the on-screen plot is not used as the export source).
- Export size is used as the offscreen render size (fallback screen-capture path may resample if offscreen export fails).
- Transparent backgrounds are supported in PNG export. Some viewers may display fully transparent pixels as black because the RGB value of fully transparent pixels is not visually meaningful.

## Version Details / Build Metadata

The sidebar includes a **Version details** accordion that contains:

- repository URL
- branch
- commit
- build version

These values are embedded at compile time by CI/local pipeline scripts.

## Update Notifications

The **Version details** accordion also includes the **Updates** section, which:

- checks GitHub releases in the background after startup
- shows a notification when a newer version is available
- highlights the **Version details** accordion header in red with the latest version tag when a new release is available
- lets you manually run **Check For Updates**
- opens the releases page with **Open Releases Page**

Release source used by the app:

- `https://github.com/russlank/XpressFormula/releases`

## Troubleshooting

- If the app does not open a window, rebuild `Debug|x64` and run again.
- If startup fails, the app now shows a startup error dialog to indicate initialization failure.
- If VS Code does not show the build/debug configurations, make sure you opened the repository root folder and not a subfolder.
- If VS Code cannot start debugging, install the C/C++ extension (`ms-vscode.cpptools`) and reopen the folder.
- If IntelliSense shows missing includes/symbols in VS Code, run `C/C++: Select a Configuration` and choose the matching config (`x64-Debug` or `Win32-Debug`).
- Do not use `dotnet build` for this native C++ solution; use Visual Studio, VS Code tasks, or `scripts/invoke-msbuild.ps1`.

## License

This document is licensed under the MIT License. See [`../LICENSE`](../LICENSE).
