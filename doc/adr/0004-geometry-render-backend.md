<!-- SPDX-License-Identifier: MIT -->
# ADR 0004: Geometry Render Backend

## Status

Accepted.

## Context

Plotting has both mathematical work and immediate-mode drawing work. Sampling, meshing, projection, render planning, and quality decisions should be testable without an ImGui frame, while the current product still renders through Dear ImGui draw lists.

## Decision

Keep pure geometry, sampling, meshing, projection, quality, and render-plan code in `XpressFormula.Plotting`.

ImGui-specific drawing is allowed only in the renderer/backend layer. Pure geometry and meshing folders must remain free of ImGui includes, and this is enforced by the architecture boundary script.

## Consequences

- Plotting algorithms can be unit tested with stable data outputs.
- The current ImGui renderer remains supported without a broad rendering-framework migration.
- Future renderer backends can reuse geometry and render-plan code.
- New sampling or meshing logic should return data structures, not draw directly.

