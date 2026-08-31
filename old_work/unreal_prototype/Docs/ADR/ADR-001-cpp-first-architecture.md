# ADR-001: C++ First Gameplay Architecture

## Status

Accepted.

## Decision

Core gameplay systems will be implemented in native C++ with data exposed through Unreal assets.

## Reason

The project owner is an experienced C++ programmer, the game depends on maintainable systemic gameplay, and AI-assisted development benefits from explicit source structure, tests, and compile-time validation.

## Alternatives Considered

- Blueprint-heavy gameplay implementation.
- Rapid prototype with logic embedded directly in level Blueprints.

## Rejected Because

Blueprint-heavy core gameplay would make long-term debugging, testing, review, and refactoring harder for the intended development style.

## Consequences

- More up-front C++ structure is required.
- Designers can still tune data through assets.
- Gameplay rules become easier to test and review.
