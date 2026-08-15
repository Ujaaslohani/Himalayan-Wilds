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

## Interaction And Quest Handoff

Interactable actors expose prompts and use actions through the native `IHSInteractable` contract. Runtime C++ systems should call the `HSInteraction` helper functions so native actors use direct C++ dispatch, while non-native Blueprint implementers remain a fallback path.

`UHSInteractionSensorComponent` owns focused interactable detection and caches the current prompt on a short timer. HUD code should read the cached prompt instead of tracing every frame.

The first quest handoff is intentionally small: `UHSQuestHandoffComponent` references one `UHSQuestDefinition`, tracks whether it is available or accepted, and does not tick. It does not yet own quest completion, inventory turn-in, rewards, or persistence.

## HUD

`AHSHUD` is the first native HUD layer. It draws a minimal interaction prompt from the player interaction sensor cache. It is intentionally not a broader inventory, dialogue, quest log, or survival HUD.

## Animal Runtime

`AHSAnimalActor` is the first native runtime animal shell. It references a `UHSAnimalDefinition`, tracks health and dead state, and exposes one-shot harvesting of configured drop stacks. It does not yet own wildlife AI, tracking clues, animation, inventory insertion, or projectile handling.

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
