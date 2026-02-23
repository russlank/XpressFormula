<!-- SPDX-License-Identifier: MIT -->
# ImGui Implementation Guide (How This App Builds the UI)

This guide explains how Dear ImGui is used in XpressFormula so a new contributor can:

- understand the current UI architecture
- safely modify panels and controls
- add new features without breaking state flow
- reuse the same approach in a similar application

This is intentionally practical and beginner-friendly.

## 1. What "ImGui" Means in This App

Dear ImGui is an immediate-mode UI library.

In practice, that means:

- Every frame, the app calls ImGui functions again to describe the UI.
- Widgets do not permanently exist as objects (like "button instances" you keep around).
- Application state lives in your own C++ structs/classes, not in the UI library.

XpressFormula follows that model cleanly:

- data/state is stored in `Application`, `PlotSettings`, `FormulaEntry`, `ViewTransform`
- panels render UI from that state
- panels mutate state directly (or return small action flags)

## 2. Main UI Files and Responsibilities

The UI is split into focused panels:

- [`src/XpressFormula/UI/Application.h`](../src/XpressFormula/UI/Application.h)
  - owns Win32 + D3D11 + ImGui lifecycle and main loop
- [`src/XpressFormula/UI/FormulaPanel.h`](../src/XpressFormula/UI/FormulaPanel.h)
  - formula list, add/remove/edit, editor dialog
- [`src/XpressFormula/UI/ControlPanel.h`](../src/XpressFormula/UI/ControlPanel.h)
  - global controls (view, 2D/3D mode, camera, export, performance)
- [`src/XpressFormula/UI/PlotPanel.h`](../src/XpressFormula/UI/PlotPanel.h)
  - plot canvas region, mouse interaction, renderer dispatch
- [`src/XpressFormula/UI/PlotSettings.h`](../src/XpressFormula/UI/PlotSettings.h)
  - shared settings for rendering and camera behavior

Why this split works well:

- each file stays easier to reason about
- panel code is mostly UI-only
- plotting math stays in `PlotRenderer`, not mixed with widget code

## 3. Startup and ImGui Initialization

ImGui is initialized in `Application::initialize()`.

High-level sequence:

1. Create Win32 window
2. Create D3D11 device/swap chain
3. `ImGui::CreateContext()`
4. Configure `ImGuiIO` flags
5. Apply style (`ImGui::StyleColorsDark()`)
6. Initialize platform and renderer backends:
   - `ImGui_ImplWin32_Init(...)`
   - `ImGui_ImplDX11_Init(...)`

Where to read:

- [`src/XpressFormula/UI/Application.cpp`](../src/XpressFormula/UI/Application.cpp)

## 4. Main Loop and Idle Optimization

The main loop lives in `Application::run()`.

It handles:

- Win32 messages
- resize events
- occlusion checks (minimized/hidden window)
- frame rendering
- idle optimization

### Idle Optimization (Important)

The app has an `Optimize Rendering` switch.

When enabled:

- it stops drawing frames continuously if nothing changed
- it waits for messages via `WaitMessage()`
- this reduces idle CPU/GPU usage

When disabled:

- it behaves like a continuous redraw loop (useful for debugging or comparison)

This is a good pattern for desktop tools:

- interactive when the user is active
- quiet when idle

## 5. Per-Frame ImGui Flow

`Application::renderFrame()` follows the standard ImGui frame lifecycle:

1. `ImGui_ImplDX11_NewFrame()`
2. `ImGui_ImplWin32_NewFrame()`
3. `ImGui::NewFrame()`
4. Build windows and widgets
5. `ImGui::Render()`
6. Clear D3D11 render target
7. `ImGui_ImplDX11_RenderDrawData(...)`
8. Present swap chain

This is the core frame template you can reuse in similar apps.

## 6. Layout Strategy in This App

The app uses two fixed ImGui windows that fill the OS window:

- left sidebar (`##Sidebar`)
- right plot area (`##Plot`)

This is done each frame by setting:

- `ImGui::SetNextWindowPos(...)`
- `ImGui::SetNextWindowSize(...)`

and then creating borderless windows with flags like:

- `NoTitleBar`
- `NoResize`
- `NoMove`
- `NoCollapse`

Why this pattern is useful:

- simple, deterministic layout
- no docking complexity
- easy to extend for desktop tooling apps

## 7. State Ownership (Most Important ImGui Rule Here)

ImGui draws widgets, but your app owns the state.

In XpressFormula:

- formulas are stored in `Application::m_formulas`
- camera/view state in `m_viewTransform`
- render settings in `m_plotSettings`
- transient export requests in booleans/action flags

Panels receive references to these objects and render widgets directly from them.

Example pattern (conceptually):

```cpp
ImGui::Checkbox("Show Envelope", &settings.showSurfaceEnvelope);
ImGui::SliderFloat("Opacity", &settings.surfaceOpacity, 0.25f, 1.0f);
```

This is the recommended mental model:

- UI is just a view/editor for your application state

## 8. Panel Communication Pattern

XpressFormula uses two patterns for panel communication:

### Pattern A: Direct mutation through references

Used for persistent state like:

- plot settings
- view transform
- formula entries

### Pattern B: Return action flags (command-like)

Used by `ControlPanel` for one-shot actions:

- open export settings dialog

`ControlPanel::render(...)` returns `ControlPanelActions`.

Why this is good:

- avoids putting file I/O inside UI panel code
- keeps rendering code separate from side effects
- easy to test and extend

## 9. FormulaPanel: Editing and Modal Dialog Pattern

`FormulaPanel` is a good example of an ImGui modal workflow.

Key ideas used:

- store selected formula index (`m_editorFormulaIndex`)
- store editor buffer (`m_editorBuffer`)
- set a flag to open popup next frame (`m_openEditorPopupNextFrame`)
- focus input with another flag (`m_focusEditorInput`)

Why "next frame" popup opening is common:

- ImGui popups are frame-driven
- opening and rendering often happens in a controlled sequence

### Live Validation in the Formula Editor

The editor does realtime validation while typing:

- parse preview text
- show valid/invalid status
- show detected plot type
- show parser error message when invalid

This is a strong UX pattern for math editors and DSL editors:

- fast feedback
- fewer trial-and-error mistakes

## 10. PlotPanel: Canvas + Mouse Interaction Pattern

The plot area is not a standard widget. It is treated as a custom canvas.

Key technique:

- `ImGui::InvisibleButton(...)` reserves a region and captures mouse input

Then the panel uses:

- `ImGui::IsItemHovered()`
- `ImGui::IsItemActive()`
- mouse delta / wheel / modifiers

to implement:

- pan (drag)
- zoom (wheel)
- axis-specific zoom (Shift/Ctrl modifiers)

This is a common and reusable ImGui pattern for:

- charts
- editors
- maps
- node canvases

### Drawing on the Plot Canvas

The panel gets the window draw list:

- `ImGui::GetWindowDrawList()`

Then it delegates rendering to `PlotRenderer`, which draws:

- background
- grid
- axes
- labels
- curves/heatmaps/triangles

This separation is important:

- `PlotPanel` handles interaction and viewport bounds
- `PlotRenderer` handles math and draw primitives

## 11. Why the Plot Uses `ImDrawList` Instead of ImGui Widgets

The plotting output is custom geometry, not forms/controls.

`ImDrawList` is used because it supports low-level primitives:

- lines
- rectangles
- circles
- triangles
- text

This lets the app draw mathematical visualizations inside an ImGui window while still using standard ImGui controls in the sidebar.

## 12. View Transform (`world <-> screen`)

`Core::ViewTransform` is the bridge between math space and pixels.

Responsibilities include:

- world-to-screen conversion
- screen-to-world conversion
- zoom / pan logic
- grid spacing logic

Why keep this out of ImGui code:

- easier math testing
- less UI coupling
- easier reuse for other frontends

## 13. Export Flow (UI Requests vs Processing)

The UI does not directly save images when a button is clicked.

Instead:

1. `ControlPanel` returns a one-shot action to open the export dialog.
2. `Application` owns and renders the export settings window (size, colors, background, include/exclude overlays).
3. When the user clicks **Save** or **Copy**, `Application` stores pending export flags + a snapshot of export settings.
4. `PlotPanel` receives temporary render overrides for export and is rendered into a temporary offscreen D3D11 render target (plot-only ImGui frame).
5. Export is processed after frame rendering (`processPendingExportActions()`), including pixel-format normalization (RGBA->BGRA, alpha handling) and post-processing (optional resize/grayscale), then file/clipboard output.

This avoids mixing:

- UI widget code
- OS dialogs
- file encoding
- clipboard operations

It also ensures the plot image is captured from a valid rendered frame while keeping OS dialogs, clipboard APIs, and pixel processing out of panel widget code.

## 14. Practical ImGui Patterns Used in This Repo

### A. Child windows for grouped scrolling content

Used in the formula editor for:

- supported functions list
- example patterns list
- live validation area

Why useful:

- constrains height
- keeps dialog compact
- improves layout control

### B. Stable IDs with `PushID`

Used when rendering repeated rows (examples/formulas).

Why necessary:

- avoids ID collisions between identical labels
- prevents unpredictable widget behavior

### C. Hidden window names (`##Name`)

Used for top-level windows like `##Sidebar` and `##Plot`.

Meaning:

- text after `##` is internal ID only
- keeps UI title hidden while preserving stable identity

## 15. How to Add a New Control Safely (Recipe)

Example: adding a new rendering toggle.

1. Add a field to `PlotSettings`.
2. Add the widget in `ControlPanel::render(...)`.
3. Pass the setting into `PlotRenderer` options in `PlotPanel`.
4. Use the option in the renderer.
5. Keep side effects out of the panel unless absolutely necessary.

Why this sequence works:

- clear ownership
- minimal coupling
- easy to review

## 16. How to Add a New Panel (Recipe)

If you want a new panel (for example "Statistics" or "Scene Layers"):

1. Create `UI/NewPanel.h/.cpp`
2. Add a panel instance to `Application`
3. Call `newPanel.render(...)` from `renderFrame()`
4. Pass only the state it needs
5. Return an action struct if it needs one-shot commands

Tip:

- Start with a dumb panel (display-only)
- Add interactions after the layout is stable

## 17. Common Mistakes Newcomers Make (and How This App Avoids Them)

### Mistake 1: Storing too much UI state inside ImGui widgets

Better:

- store state in app structs (`PlotSettings`, `FormulaEntry`, etc.)

### Mistake 2: Mixing rendering math with widget layout code

Better:

- keep `PlotRenderer` separate from `PlotPanel`

### Mistake 3: Doing heavy work directly in button callbacks

Better:

- return action flags and process later (used by export flow)

### Mistake 4: Not using stable IDs in repeated lists

Better:

- `PushID(index)` around repeated controls

## 18. How to Build a Similar ImGui UI in a New App

You can reuse this architecture almost directly:

1. `Application` owns platform/renderer/ImGui lifecycle
2. `Settings` structs hold persistent UI-editable state
3. Panels render from references to state
4. Plot/canvas panel uses `InvisibleButton` + `ImDrawList`
5. Heavy operations are separated from UI and optionally cached

This scales well from:

- graphing tools
- visualization tools
- engineering utilities
- internal desktop dashboards

## 19. Suggested Reading Order in This Codebase

1. [`src/XpressFormula/UI/Application.h`](../src/XpressFormula/UI/Application.h)
2. [`src/XpressFormula/UI/Application.cpp`](../src/XpressFormula/UI/Application.cpp)
3. [`src/XpressFormula/UI/PlotSettings.h`](../src/XpressFormula/UI/PlotSettings.h)
4. [`src/XpressFormula/UI/FormulaPanel.h`](../src/XpressFormula/UI/FormulaPanel.h)
5. [`src/XpressFormula/UI/ControlPanel.h`](../src/XpressFormula/UI/ControlPanel.h)
6. [`src/XpressFormula/UI/PlotPanel.h`](../src/XpressFormula/UI/PlotPanel.h)
7. [`src/XpressFormula/Plotting/PlotRenderer.h`](../src/XpressFormula/Plotting/PlotRenderer.h)

## Related Docs

- Algorithms guide: [`algorithms-guide.md`](algorithms-guide.md)
- Architecture overview: [`architecture.md`](architecture.md)
- Run guide: [`run-guide.md`](run-guide.md)

## License

This document is licensed under the MIT License. See [`../LICENSE`](../LICENSE).
