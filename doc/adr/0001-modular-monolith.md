<!-- SPDX-License-Identifier: MIT -->
# ADR 0001: Modular Monolith

## Status

Accepted.

## Context

XpressFormula is a native Windows desktop application with a small team-sized codebase. The product needs clear ownership boundaries for expression parsing, model state, plotting, UI, platform services, persistence, export, and application orchestration without introducing a distributed architecture or new runtime framework.

## Decision

Use a modular monolith built from first-party C++ static-library projects:

- `XpressFormula.Expression`
- `XpressFormula.Model`
- `XpressFormula.Plotting`
- `XpressFormula.Infrastructure`
- `XpressFormula.UI`
- `XpressFormula.App`
- executable host `XpressFormula`

Tests link these production libraries instead of compiling production `.cpp` files directly.

## Consequences

- Boundaries are visible in Visual Studio/MSBuild and can be checked by CI.
- The application remains easy to build and debug as one desktop executable.
- Some source paths may remain transitional while project ownership carries the architectural boundary.
- New product work should respect dependency direction instead of adding shortcuts between layers.

