# Git Strategy

## Commit Style

Use small, reviewable commits with a conventional prefix:

```text
docs: record first milestone architecture
feat: add native interaction component
feat: add deer definition data
test: add quest requirement validation
fix: prevent duplicate animal harvest
perf: reduce distant wildlife update frequency
```

## Branching

For solo development, keep the strategy simple:

- `main`: stable, buildable project state.
- short-lived feature branches when a change is risky or touches multiple systems.

Avoid long-running branches during the prototype.

## Commit Boundaries

Prefer one commit per coherent system slice:

- project foundation
- player/input foundation
- interaction foundation
- inventory/resource logic
- deer AI prototype
- bow/projectile prototype
- quest turn-in prototype
- building unlock prototype

Do not combine unrelated systems into large commits.

## Generated Files

Do not commit:

- `Binaries/`
- `Intermediate/`
- `Saved/`
- `DerivedDataCache/`
- generated solution/user files

Do commit:

- `Config/`
- `Docs/`
- `Source/`
- `.uproject`
- intentionally authored content assets once they exist
