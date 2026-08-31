# ADR-002: Data Assets And Gameplay Tags

## Status

Accepted.

## Decision

Use native `UPrimaryDataAsset` classes for gameplay definitions and Gameplay Tags for semantic categories, requirements, unlocks, and resource identities.

## Reason

The project needs data-driven NPCs, animals, resources, quests, buildings, and progression without hardcoding each case in C++.

## Alternatives Considered

- Hardcoded C++ definitions.
- Data Tables for every data type.
- Full custom database layer.

## Rejected Because

Hardcoded definitions do not scale. Data Tables are useful for flat records but less comfortable for references, editor UX, and asset bundles. A custom database is unnecessary for the prototype.

## Consequences

- Asset Manager can discover and audit definitions later.
- Tag taxonomy needs care to avoid becoming messy.
- Runtime systems should validate data and fail clearly when definitions are incomplete.
