<!-- SPDX-License-Identifier: MIT -->
# Linux Portability Plan (ImGui + XpressFormula)

## Goal

Prepare XpressFormula for Linux while preserving the current architecture strengths:

- reusable `Core` math/parsing logic
- ImGui-based UI workflow
- custom plot rendering in `PlotRenderer`
- responsive interaction and export features

This document is a migration plan, not an implementation. It explains what to change, in what order, and why.

## Short Answer: Can ImGui Run on Linux?

Yes.

Dear ImGui is cross-platform. What changes is the platform/renderer backend pair.

Current app stack:

- `Win32 + Direct3D11 + Dear ImGui`

Typical Linux stacks:

- `GLFW + OpenGL3 + Dear ImGui` (recommended for this project)
- `SDL2 + OpenGL3 + Dear ImGui`
- `GLFW + Vulkan + Dear ImGui`
- `SDL2 + Vulkan + Dear ImGui`

## Recommended Linux Target (First Port)

Use:

- `GLFW` for window/input/platform layer
- `OpenGL 3.3+` for rendering backend
- Dear ImGui backends:
  - `imgui_impl_glfw`
  - `imgui_impl_opengl3`

Why this is the best first step here:

- simpler than Vulkan for a first port
- good Linux support and documentation
- keeps the app architecture close to the current ImGui integration model
- no need to rewrite `PlotPanel` / `PlotRenderer` drawing logic (still uses `ImDrawList`)

## What Is Already Portable vs Windows-Specific

Mostly portable (should require little/no logic changes):

- `src/XpressFormula/Core/*` (tokenizer, parser, evaluator, transforms)
- `src/XpressFormula/UI/FormulaEntry.h` (formula classification/parsing behavior)
- `src/XpressFormula/UI/FormulaPanel.cpp` (ImGui widgets)
- `src/XpressFormula/UI/ControlPanel.cpp` (ImGui widgets)
- `src/XpressFormula/UI/PlotPanel.cpp` (ImGui canvas interaction and setting routing)
- `src/XpressFormula/Plotting/PlotRenderer.cpp` (ImGui draw-list rendering)

Windows-specific (must be abstracted/replaced):

- `src/XpressFormula/UI/Application.cpp` / `Application.h`
  - Win32 window creation
  - D3D11 device/swapchain/render targets
  - Win32 message loop
  - shell/browser open (`ShellExecute`)
  - WinHTTP update check
  - Win32 dialogs (`GetSaveFileNameW`)
  - clipboard APIs
  - WIC image encoding path (PNG/BMP)
- `src/XpressFormula/app.rc`, `resource.h`, icon resources (Windows resource system)
- WiX packaging (`.msi` / setup `.exe`) and Windows-specific CI packaging steps

## Migration Strategy (Phased, Low-Risk)

### Phase 0: Stabilize Seams (No Linux Yet)

Goal: reduce platform coupling before porting.

Actions:

1. Extract platform-specific services behind interfaces:
   - browser open
   - update check HTTP fetch
   - save-file dialog
   - clipboard image copy
   - image encoding/saving
2. Keep current Windows implementations as the first backend.
3. Keep `Application` focused on orchestration, not OS API details.

Suggested interfaces (example names):

- `IExternalLauncher` (`openUrl`)
- `IHttpClient` or `IReleaseChecker`
- `IFileDialogService`
- `IClipboardService`
- `IImageCodecService`

Why this phase matters:

- most Linux pain comes from OS services, not ImGui itself
- easier to test and reason about platform replacements
- keeps future macOS port possible too

### Phase 1: Introduce Platform/Renderer Abstraction

Goal: separate app UI/frame logic from the backend bootstrapping.

Actions:

1. Split `Application` into:
   - backend-agnostic app controller (frames, panels, state)
   - backend host (window + graphics + backend integration)
2. Define a small host interface for:
   - current framebuffer size
   - render begin/end
   - event pumping
   - shutdown
3. Keep one Windows host first (`Win32D3D11Host`).

Suggested structure:

- `src/XpressFormula/Platform/IAppHost.h`
- `src/XpressFormula/Platform/Windows/Win32D3D11Host.*`
- later `src/XpressFormula/Platform/Linux/GlfwOpenGLHost.*`

### Phase 2: Linux Host (GLFW + OpenGL3)

Goal: get the app running on Linux using Dear ImGui Linux backends.

Actions:

1. Add GLFW and OpenGL loader dependency (`glad` or equivalent).
2. Add ImGui backends:
   - `imgui_impl_glfw.cpp`
   - `imgui_impl_opengl3.cpp`
3. Implement Linux host:
   - window creation
   - OpenGL context
   - main loop
   - ImGui backend init/shutdown
4. Reuse existing `FormulaPanel`, `ControlPanel`, `PlotPanel`, `PlotRenderer`.

Expected result:

- same UI and plotting behavior on Linux
- no D3D11 code in Linux build

### Phase 3: Replace Windows-Only Services on Linux

Goal: restore feature parity (export/save/open URL/update check).

Actions:

1. Browser open:
   - Linux implementation can use `xdg-open`
2. Update checks:
   - replace WinHTTP with portable HTTP client
   - options:
     - `libcurl` (recommended, mature)
     - simple platform command fallback (not preferred)
3. Save file dialog:
   - options:
     - native toolkit dialogs (GTK)
     - tiny cross-platform dialog lib (e.g. `portable-file-dialogs`)
     - fallback path prompt (temporary)
4. Clipboard image copy:
   - likely platform-specific and harder than Windows
   - phase it in after file export is stable
5. Image encoding:
   - replace WIC path with cross-platform encoder (e.g. `stb_image_write` or `lodepng`)
   - or keep platform-specific codec service implementations

Recommended rollout:

- first support `Save To File` on Linux
- then browser open and update checks
- then clipboard image copy

### Phase 4: Build System and CI

Goal: build on Linux CI and keep Windows builds intact.

Actions:

1. Introduce CMake (recommended) while preserving Visual Studio project files during transition.
2. Build targets:
   - `XpressFormula` app
   - `XpressFormula.Tests`
3. Add GitHub Actions Linux build job:
   - Ubuntu latest
   - install `xorg-dev`, `libgl1-mesa-dev`, `libglu1-mesa-dev`, `libxrandr-dev`, `libxi-dev`, `libxcursor-dev`, `libxinerama-dev`, `libxxf86vm-dev`, `libwayland-dev` (depending on backend)
4. Keep Windows packaging workflow separate from Linux build workflow.

### Phase 5: Linux Packaging (Optional After Functional Port)

Goal: distribute Linux builds in a user-friendly form.

Options:

- AppImage (good first target)
- `.deb` package (Ubuntu/Debian-focused)
- `.tar.gz` portable bundle

Do this after:

- app runs reliably
- export works
- update checks are stable or disabled gracefully on Linux

## Important Design Decisions (Project-Specific)

### 1. Keep `PlotRenderer` on `ImDrawList`

Do not rewrite plotting into OpenGL draw calls in the first Linux port.

Reason:

- current rendering logic is already backend-agnostic at the ImGui draw-list level
- backend swap changes how ImGui is rendered, not how `PlotRenderer` produces triangles/lines
- this dramatically lowers migration risk

### 2. Keep `Core` Fully Platform-Free

Avoid adding OS dependencies to:

- `Tokenizer`
- `Parser`
- `Evaluator`
- `ViewTransform`
- version parsing utilities

Reason:

- easiest to test
- easiest to reuse in other frontends (CLI, web, other desktop shells)

### 3. Decouple Update Check Transport from Update Logic

The new GitHub release notification feature currently mixes:

- GitHub API fetch
- version comparison
- UI state update

The version comparison/parser logic is already isolated in:

- `src/XpressFormula/Core/UpdateVersionUtils.h`

Next step for Linux portability:

- move the HTTP fetch behind an interface
- reuse the same version/tag parsing logic unchanged

## Proposed Folder Additions (When Porting Starts)

Example structure (incremental):

- `src/XpressFormula/Platform/`
- `src/XpressFormula/Platform/IAppHost.h`
- `src/XpressFormula/Platform/Windows/`
- `src/XpressFormula/Platform/Linux/`
- `src/XpressFormula/Services/`
- `src/XpressFormula/Services/IReleaseChecker.h`
- `src/XpressFormula/Services/IFileDialogService.h`
- `src/XpressFormula/Services/IClipboardService.h`
- `src/XpressFormula/Services/IImageCodecService.h`

This is optional at first, but strongly recommended for long-term maintainability.

## Linux Feature Parity Matrix (Suggested Tracking)

Track progress explicitly:

- [ ] App starts and renders ImGui UI
- [ ] Formula parsing/evaluation works
- [ ] 2D curves render
- [ ] 2D implicit contours render
- [ ] 3D explicit surfaces render
- [ ] 3D implicit `F(x,y,z)=0` surfaces render
- [ ] Export to file works
- [ ] Export preview works
- [ ] Clipboard copy works
- [ ] Update check works
- [ ] Open releases page works
- [ ] Tests run in Linux CI

## Performance Notes for Linux Port

The heavy cost for implicit `F(x,y,z)=0` is CPU meshing (`O(N^3)` sampling), not the D3D11 backend itself.

So on Linux:

- expect similar meshing cost at similar settings
- GPU usage patterns may differ due to OpenGL driver behavior
- the existing `Optimize Rendering` and interaction-time 3D throttling should remain useful

## Validation Plan (Recommended)

### Functional Smoke Tests

Test these formulas on Linux:

- `sin(x)`
- `x^2+y^2`
- `x^2+y^2=100`
- `x^2+y^2+z^2=16`
- `(x^2+y^2+z^2+21)^2 - 100*(x^2+y^2) = 0`

Verify:

- panning/zooming
- 3D camera controls
- origin alignment (no "swimming")
- export dialog + preview
- export output correctness

### Regression Tests

Reuse existing unit tests:

- tokenizer/parser/evaluator/view transform
- formula classification
- update version utility tests

Add CI jobs to run the test executable on Linux after the build system is ready.

## Practical First Milestone (Recommended Scope)

Keep the first Linux milestone narrow:

1. `GLFW + OpenGL3` host
2. app launches and plots formulas
3. file export works (`png`)
4. no clipboard copy yet
5. update check can be disabled if HTTP client is not ready

This gets a usable Linux prototype quickly and avoids blocking on OS integration details.

## Risks and Mitigations

- Risk: platform code remains tangled in `Application.cpp`
  - Mitigation: extract service interfaces before Linux host implementation
- Risk: trying Vulkan too early slows progress
  - Mitigation: start with `GLFW + OpenGL3`
- Risk: file dialog / clipboard APIs become the main blocker
  - Mitigation: phase feature parity; prioritize save-to-file first
- Risk: build-system migration disrupts Windows workflow
  - Mitigation: add CMake incrementally; keep current MSBuild path until Linux build is stable

## Related Docs

- [`architecture.md`](architecture.md)
- [`imgui-implementation-guide.md`](imgui-implementation-guide.md)
- [`algorithms-guide.md`](algorithms-guide.md)
- [`run-guide.md`](run-guide.md)
- [`project-vendors.md`](project-vendors.md)

## License

This document is licensed under the MIT License. See [`../LICENSE`](../LICENSE).
