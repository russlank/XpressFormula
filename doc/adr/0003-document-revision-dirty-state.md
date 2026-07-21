<!-- SPDX-License-Identifier: MIT -->
# ADR 0003: Document Revision Dirty State

## Status

Accepted.

## Context

Dirty-state tracking used to depend on serialized project snapshots. That made UI frame code sensitive to persistence formatting and made it easy for transient runtime state to affect saved-state decisions.

## Decision

`Model::Document` owns revision-based dirty state:

- persistent mutations increment `revision()`;
- successful saves and clean loads call `markSaved()`;
- a document is dirty when `revision() != savedRevision()`;
- transient viewport geometry, editor state, preview state, and runtime auto-rotation do not change the revision.

Project save/load flows map through persistence DTOs, but dirty-state checks do not serialize the project per frame.

## Consequences

- Dirty-state behavior is fast and independent of JSON formatting.
- No-op document commands can be tested directly.
- Persistence remains responsible for files and schema, while `Model::Document` remains responsible for live state.
- New persistent fields must be applied through document commands or clean replacement paths that preserve revision semantics.

