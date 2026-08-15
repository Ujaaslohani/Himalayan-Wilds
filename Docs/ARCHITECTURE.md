# Architecture

## Direction

The project is C++ first. Core gameplay and simulation logic should live in native code, with Blueprint used for asset configuration, animation, visual setup, and editor-authored content.

Target split:

- 90-95% gameplay/system logic in C++.
- 5-10% Blueprint/editor-side setup where Unreal's workflow benefits from it.

## Scope Boundary

Current milestone: One Morning Hunt.

The player starts in a primitive settlement, talks to one hunter NPC, receives a deer-hunting task, tracks and hunts a deer, harvests resources, returns to the NPC, unlocks one object, builds it, and ends the day.

No multiplayer, large procedural world, economy simulation, complex story framework, advanced survival model, or broad animal roster should be added during this milestone.

## Source Layout

```text
Source/HimalayanSettlement/
  Core/
  Player/
  Interaction/
  Inventory/
  Resources/
  Animals/
  AI/
  Hunting/
  NPC/
  Settlement/
  Building/
  Quests/
  Progression/
  Save/
  World/
  Tests/
```

## Data-Driven Foundation

Definitions are native `UPrimaryDataAsset` classes. Runtime systems should reference definitions by asset pointer or primary asset ID rather than hardcoding gameplay data.

Initial definition types:

- `UHSItemDefinition`
- `UHSAnimalDefinition`
- `UHSNPCDefinition`
- `UHSBuildingDefinition`
- `UHSQuestDefinition`

Gameplay Tags identify categories, knowledge, resources, quest IDs, animal states, professions, and building unlocks.

## Performance Rules

- Avoid per-frame `Tick` unless the behavior genuinely requires frame-level updates.
- Prefer events, timers, perception callbacks, scheduled updates, and state transitions.
- Wildlife and NPCs must support simulation LOD before the prototype expands beyond nearby actors.
- Distant wildlife should eventually become abstract regional state rather than full actors.

## Blueprint Policy

Blueprint is allowed for:

- data-only child assets
- visual actor setup
- animation blueprints
- input mapping assets
- materials, VFX, audio
- level and foliage authoring

Blueprint should not own:

- quest completion rules
- inventory/resource transactions
- recruitment logic
- settlement unlock logic
- save/load logic
- animal or NPC core simulation rules
