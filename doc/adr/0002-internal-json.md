<!-- SPDX-License-Identifier: MIT -->
# ADR 0002: Internal JSON Infrastructure

## Status

Accepted.

## Context

Project files, recent projects, export metadata, and update checks all need JSON handling. The application already has a constrained JSON surface and should avoid adding a third-party dependency solely for this small, stable need.

## Decision

Keep shared JSON parsing and writing in `XpressFormula.Infrastructure` under `Infrastructure/Serialization`.

Domain model code must not depend on JSON parser or writer types. Persistence uses DTOs in `Infrastructure/Persistence`, then maps validated data into `Model::Document`.

## Consequences

- `.xfplot` and export metadata behavior stays deterministic and testable.
- JSON schema validation is owned by infrastructure, not UI or model objects.
- Model code remains serialization-format agnostic.
- If the JSON surface grows substantially, replacing the internal parser can be considered behind the same infrastructure boundary.

