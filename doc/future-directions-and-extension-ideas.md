<!-- SPDX-License-Identifier: MIT -->
# Future Directions and Extension Ideas

XpressFormula is currently a native Windows application for entering, evaluating, visualizing, and exporting mathematical expressions in two and three dimensions.

Its present capabilities include:

- explicit 2D curves such as `y = f(x)`;
- explicit 3D surfaces such as `z = f(x, y)`;
- implicit 2D equations such as `F(x, y) = 0`;
- scalar-field cross-sections for expressions using `x`, `y`, and `z`;
- implicit 3D surfaces such as `F(x, y, z) = 0`;
- signed-distance helpers, smooth implicit composition, and procedural noise;
- versioned `.xfplot` project files;
- image export, metadata sidecars, and release packaging;
- a modular architecture separating expression processing, model state, plotting, infrastructure, UI, and application workflows.

This document collects possible directions in which XpressFormula could grow.

It is intentionally an **ideas catalogue**, not a committed roadmap. Items are exploratory, may change substantially, and are not promises of implementation or release dates.

---

## Guiding Principles

Future development should preserve the qualities established by the current architecture:

1. **Formula-first interaction**  
   Mathematical and procedural definitions should remain the primary way users create content.

2. **Immediate visual feedback**  
   Changes should be reflected interactively wherever practical.

3. **Clear architectural ownership**  
   Expression, model, plotting, infrastructure, UI, and platform responsibilities should remain separated.

4. **Deterministic and testable behavior**  
   Core evaluation, sampling, geometry, persistence, and export paths should remain reproducible and independently testable.

5. **Graceful handling of invalid input**  
   Invalid formulas, excessive input, numerical singularities, and incomplete projects should produce useful diagnostics rather than crashes or silent corruption.

6. **Backward-compatible project evolution**  
   New project capabilities should use explicit schema evolution and preserve older `.xfplot` files where practical.

7. **Useful before complex**  
   Small features that improve exploration and understanding should generally be preferred over large features with unclear user value.

---

# Opportunity Areas

## 1. Named Parameters and Interactive Controls

A natural next step is allowing formulas to use named user parameters in addition to the plotting variables `x`, `y`, and `z`.

Example:

```text
amplitude = 2
frequency = 0.5
y = amplitude * sin(frequency * x)
```

The application could generate controls automatically for these parameters.

Possible capabilities:

- sliders with configurable minimum, maximum, and step;
- direct numeric entry;
- boolean and enumerated parameters;
- parameter groups;
- saved parameter presets;
- reset-to-default actions;
- linking one parameter to several formulas;
- parameter animation;
- playback speed, looping, and direction controls.

This would turn static projects into interactive mathematical experiments.

### Architectural considerations

- Keep `x`, `y`, and `z` as optimized built-in evaluation slots.
- Bind user parameters to indexed slots during compilation.
- Store parameter definitions and values in the document model.
- Ensure parameter-only changes invalidate the appropriate plotting caches without recompiling unrelated formulas.
- Define project-schema migration before persisting parameters.

---

## 2. Analysis and Measurement Tools

XpressFormula could help users analyze plotted content rather than only display it.

Possible tools include:

- cursor-based coordinate and value inspection;
- formula evaluation at a selected point;
- roots and zero crossings;
- intersections between curves;
- local minima and maxima;
- numerical derivatives;
- tangent and normal lines;
- definite integrals and shaded areas;
- curve arc length;
- estimated surface area;
- estimated enclosed volume;
- distance and angle measurements;
- scalar-field statistics over a selected region.

A practical first set could be:

1. cursor probe;
2. root finder;
3. curve intersection finder;
4. derivative and tangent display;
5. definite integral over a selected interval.

These features could initially use numerical methods without requiring a symbolic algebra system.

### Architectural considerations

- Keep numerical analysis outside UI widgets.
- Return structured result objects with convergence status and diagnostics.
- Let UI components select ranges and display results without owning algorithms.
- Add deterministic tests for convergence, discontinuities, repeated roots, and invalid domains.

---

## 3. Parametric Curves and Surfaces

The current rendering modes focus on explicit functions and implicit equations. Parametric geometry would broaden the range of representable objects considerably.

### Parametric 2D curves

```text
x(t) = cos(t)
y(t) = sin(t)
t = 0 .. 2*pi
```

Possible uses:

- circles and ellipses;
- spirals;
- Lissajous figures;
- cycloids;
- paths that fail the vertical-line test;
- educational demonstrations of parameterization.

### Parametric 3D curves

```text
x(t) = cos(t)
y(t) = sin(t)
z(t) = 0.1*t
```

Possible uses:

- helices;
- knots;
- particle trajectories;
- space curves;
- path-based animation.

### Parametric surfaces

```text
x(u, v) = ...
y(u, v) = ...
z(u, v) = ...
```

Possible uses:

- spheres and tori;
- Möbius strips;
- ruled surfaces;
- mathematical and engineering surfaces;
- custom procedural geometry.

### Architectural considerations

- Introduce explicit formula-group types for parametric objects.
- Define parameter ranges, resolution, and periodicity.
- Reuse the current expression compiler and evaluation runtime.
- Keep geometry generation independent of the rendering backend.
- Add limits for sample counts and generated geometry.

---

## 4. Vector Fields and Dynamical Systems

Vector-valued formulas could extend XpressFormula into physics, engineering, and differential-equation visualization.

Example:

```text
vx = -y
vy = x
```

Possible visualizations:

- 2D arrow fields;
- magnitude heatmaps;
- streamlines;
- particle traces;
- 3D vector fields;
- gradient fields;
- divergence and curl visualizations.

A later stage could support ordinary differential equation systems:

```text
dx/dt = y
dy/dt = -x
```

Possible uses:

- phase portraits;
- oscillator systems;
- control-system demonstrations;
- fluid-flow visualization;
- attractors and chaotic systems;
- trajectory comparison from multiple initial conditions.

### Architectural considerations

- Represent vector fields explicitly instead of overloading scalar formulas.
- Keep numerical integrators in a pure analysis/plotting module.
- Provide selectable integration methods and tolerances.
- Bound maximum integration steps and trajectory length.
- Report divergence, non-finite states, and integration failure clearly.

---

## 5. Formula-Driven Solid Modeling

XpressFormula already supports signed-distance helpers, implicit 3D surfaces, smooth composition, and procedural noise. These capabilities could grow into a lightweight formula-driven modeling system.

Possible additions:

### More signed-distance primitives

- plane;
- capsule;
- cone;
- rounded box;
- rounded cylinder;
- ellipsoid;
- prism;
- gyroid and other periodic surfaces.

### Transform helpers

- translation;
- rotation;
- non-uniform scaling;
- mirroring;
- twisting;
- bending;
- repetition;
- radial repetition;
- symmetry helpers.

### Constructive composition

- union;
- intersection;
- subtraction;
- smooth union;
- smooth intersection;
- smooth subtraction.

Example concept:

```text
subtract(
    sdBox(x, y, z, 2, 1, 0.5),
    sdCylinderZ(x, y, z, 0.3)
) = 0
```

Possible outputs:

- STL;
- OBJ;
- PLY;
- glTF;
- sliced cross-sections;
- printable mesh diagnostics.

### Architectural considerations

Before presenting XpressFormula as a fabrication or CAD tool, the mesh pipeline would need stronger guarantees for:

- watertightness;
- manifold topology;
- consistent winding;
- duplicate vertices and faces;
- minimum feature size;
- export units and scale;
- mesh repair and validation.

This direction is distinctive and aligns closely with the existing implicit-surface engine, but it should be introduced incrementally.

---

## 6. Procedural Textures, Terrain, and Height Maps

The existing deterministic noise and fractal-noise functions provide a foundation for procedural generation.

Examples:

```text
z = fbm2(3*x, 3*y)
```

```text
smoothstep(0.3, 0.7, fbm2(5*x, 5*y))
```

Possible uses:

- terrain generation;
- grayscale height maps;
- displacement maps;
- masks;
- cloud, marble, and wood-like textures;
- density fields;
- procedural test data;
- game-development assets.

Possible export formats:

- PNG;
- 16-bit grayscale images;
- CSV matrices;
- RAW float grids;
- normal maps;
- tiled textures.

### Architectural considerations

- Separate scalar-field generation from display.
- Allow deterministic seed parameters without global mutable randomness.
- Support explicit output resolution and world bounds.
- Add checked memory limits for large grids.
- Define color-map and normalization policies separately from sampling.

---

## 7. Data Import, Comparison, and Curve Fitting

XpressFormula could combine real datasets with mathematical models.

Possible input formats:

- CSV;
- TSV;
- simple column-oriented text;
- clipboard-pasted tabular data.

Possible features:

- scatter plots;
- connected line plots;
- column selection;
- filtering and range selection;
- error bars;
- formula overlays;
- residual plots;
- basic regression;
- nonlinear parameter fitting;
- exporting fitted values and residuals.

Example use cases:

- sensor measurements versus a theoretical model;
- calibration curves;
- laboratory data;
- performance benchmarking;
- engineering measurements;
- educational statistics.

### Architectural considerations

- Introduce dataset objects in the document model.
- Keep imported data separate from expression definitions.
- Store source references and optionally embed datasets.
- Make fitting deterministic and report convergence quality.
- Avoid hiding model assumptions behind one-click fitting.

---

## 8. Complex-Number Visualization

A dedicated complex-expression mode could support:

```text
z = x + i*y
f(z) = ...
```

Possible visualizations:

- domain coloring;
- magnitude and phase;
- poles and zeros;
- conformal grids;
- Mandelbrot and Julia sets;
- Newton fractals;
- Riemann-surface approximations.

This would require a complex-valued evaluation path rather than forcing complex behavior into the existing scalar runtime.

### Architectural considerations

- Add a distinct complex numeric type and evaluator.
- Keep scalar and complex formula classification explicit.
- Define supported operators and functions carefully.
- Add color-map policies for phase and magnitude.
- Bound iterative fractal evaluation and escape tests.

---

## 9. Animation and Presentation Mode

Projects could become animated demonstrations rather than only static scenes.

Possible capabilities:

- animate named parameters;
- animate camera position and orientation;
- animate formula visibility;
- keyframes and interpolation;
- timeline scrubbing;
- loop and playback controls;
- annotations and callouts;
- scene sequencing;
- export to image sequence, GIF, or video.

Possible uses:

- classroom demonstrations;
- technical presentations;
- documentation;
- tutorial videos;
- mathematical storytelling;
- social-media visualizations.

### Architectural considerations

- Treat time as transient playback state unless explicitly persisted.
- Store animation tracks separately from formula definitions.
- Keep deterministic frame evaluation.
- Ensure exported animation uses a fixed time step rather than UI frame timing.
- Avoid coupling timeline logic to ImGui widgets.

---

## 10. Formula Worksheets and Project Notes

A lighter notebook-style model could make projects more explanatory and reusable.

Possible document elements:

- formula;
- scalar definition;
- parameter;
- text note;
- Markdown section;
- result value;
- table;
- plot group;
- image or annotation.

Example:

```text
a = 3
b = 4
hypotenuse = sqrt(a^2 + b^2)
```

Displayed result:

```text
hypotenuse = 5
```

This could begin with simple formula grouping and project notes rather than a full notebook environment.

### Architectural considerations

- Avoid turning every UI widget into a persisted document object.
- Define a small, stable set of worksheet item types.
- Preserve formula identity and references across reordering.
- Keep rendered notes separate from expression evaluation.
- Introduce schema evolution before persisting new item kinds.

---

## 11. Headless Command-Line Rendering

The modular architecture makes a non-GUI frontend plausible.

Possible commands:

```powershell
xpressformula render project.xfplot --output graph.png
```

```powershell
xpressformula plot "sin(x)" --x-min -10 --x-max 10 --output sin.png
```

```powershell
xpressformula mesh "sdSphere(x,y,z,1)=0" --output sphere.obj
```

Potential uses:

- batch rendering;
- CI-generated images;
- documentation pipelines;
- regression snapshots;
- automated reports;
- scripting;
- reproducible asset generation.

### Architectural considerations

- Keep the CLI as another composition root over existing production libraries.
- Avoid duplicating project loading, plotting, and export logic.
- Provide machine-readable diagnostics and exit codes.
- Support deterministic rendering options.
- Add integration tests for command parsing and outputs.

---

## 12. Reusable SDK and Language Bindings

Some XpressFormula components may eventually be useful outside the desktop application.

Potential distribution forms:

- stable C++ API;
- C ABI;
- Windows DLL;
- Python bindings;
- .NET bindings;
- package-manager distribution.

Possible public capabilities:

- compile and validate a formula;
- inspect variables and formula kind;
- evaluate at selected coordinates;
- sample scalar fields;
- generate contours;
- generate surfaces;
- render or export an image.

Example conceptual API:

```cpp
auto formula = xf::compile("sin(x)");
auto image = xf::render2D(formula, options);
```

### Architectural considerations

A supported SDK would require:

- explicit API stability rules;
- clear ownership and lifetime semantics;
- versioned ABI where relevant;
- reduced exposure of internal implementation types;
- dedicated SDK tests and examples;
- careful separation of public and private headers.

---

## 13. Example Gallery and Project Sharing

A curated formula and project gallery could help users discover capabilities.

Gallery entries could contain:

- preview image;
- formula or project file;
- explanation;
- parameter suggestions;
- author and license;
- one-click loading.

Possible categories:

- classic curves;
- implicit geometry;
- signed-distance models;
- procedural terrains;
- fractals;
- educational demonstrations;
- community examples.

Possible sharing mechanisms:

- `.xfplot` files;
- GitHub-hosted example repository;
- GitHub Gists;
- compact encoded share links;
- read-only browser viewer.

### Architectural considerations

- Treat external content as untrusted input.
- Validate limits before loading.
- Preserve author and license metadata.
- Avoid network requirements for core application use.
- Keep gallery integration optional.

---

## 14. Cross-Platform and Browser-Based Viewing

The current application is Windows-native, but the architecture could support additional frontends over time.

Possible targets:

- Linux desktop;
- macOS;
- WebAssembly;
- browser-based `.xfplot` viewer;
- portable headless renderer.

A useful incremental path could be:

1. isolate remaining Win32 and Direct3D assumptions;
2. add a portable window/render backend;
3. bring up a Linux desktop build;
4. create a read-only browser viewer;
5. evaluate whether full browser editing is justified.

### Architectural considerations

- Keep platform services behind narrow interfaces.
- Avoid weakening the Windows experience merely to claim portability.
- Define rendering-backend abstractions only where a second backend exists.
- Ensure project files remain platform-independent.
- Track font, clipboard, filesystem, dialog, and GPU differences explicitly.

---

# Suggested Prioritization

The following ordering balances user value, architectural fit, and implementation risk.

## Near-Term Candidates

These ideas extend current behavior without redefining the product:

1. cursor probe and point evaluation;
2. roots and curve intersections;
3. formula groups and project notes;
4. named parameters with sliders;
5. parameter animation;
6. improved example gallery;
7. headless image export from `.xfplot` projects.

## Medium-Term Candidates

These require new document and plotting concepts:

1. parametric 2D curves;
2. parametric 3D curves;
3. parametric surfaces;
4. dataset import and formula overlays;
5. numerical derivatives and integrals;
6. procedural height-map export;
7. vector fields and streamlines.

## Long-Term Explorations

These are potentially distinctive but require substantial design and validation:

1. formula-driven solid modeling;
2. watertight mesh and fabrication export;
3. differential-equation systems;
4. complex-number visualization;
5. worksheet/notebook documents;
6. reusable SDK and language bindings;
7. cross-platform editing;
8. browser-based interactive projects.

---

# Possible Product Positioning

A useful long-term identity for XpressFormula could be:

> **An interactive formula-driven visualization and procedural modeling workbench.**

This positioning is broader than a graphing calculator while remaining focused on the project's strongest qualities:

- formulas as the source of truth;
- immediate native visualization;
- explicit and implicit geometry;
- procedural fields;
- deterministic export;
- modular, reusable foundations.

It also leaves room for educational, analytical, creative, and engineering use cases without requiring XpressFormula to become a full computer algebra system, general-purpose CAD package, or scientific notebook all at once.

---

# Evaluation Criteria for New Features

Before implementing a major extension, it is useful to ask:

1. Does this feature strengthen formula-driven exploration?
2. Can it be implemented without violating existing architectural boundaries?
3. Is its behavior deterministic and testable?
4. Can invalid or excessive input be handled safely?
5. Does it fit the existing project model, or does it require an explicit schema change?
6. Can it be delivered in a useful minimal form?
7. Does it duplicate a mature external tool without adding a distinctive XpressFormula advantage?
8. Can users understand the result and any numerical limitations?
9. Can the feature work without requiring a network connection?
10. Is the long-term maintenance cost proportionate to its user value?

---

# Contributions and Discussion

Ideas, design sketches, and focused prototypes are welcome.

For substantial features, a proposal should ideally describe:

- the user problem;
- example formulas or workflows;
- the smallest useful implementation;
- affected architectural modules;
- project-schema implications;
- numerical and performance risks;
- required tests;
- documentation needs;
- compatibility considerations.

Large changes should be divided into independently reviewable stages whenever practical.

---

## Related Docs

- [`index.md`](index.md)
- [`architecture.md`](architecture.md)
- [`architecture-dependencies.md`](architecture-dependencies.md)
- [`algorithms-guide.md`](algorithms-guide.md)
- [`math-api-guide.md`](math-api-guide.md)
- [`imgui-implementation-guide.md`](imgui-implementation-guide.md)
- [`linux-portability-plan.md`](linux-portability-plan.md)
- [`expression-language.md`](expression-language.md)

---

# Status

This document is informational and exploratory.

It does not define release commitments, priorities, schedules, or guarantees. Future development will depend on user interest, maintainability, implementation complexity, and available contributor time.
