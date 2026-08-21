# Architecture

## Direction

The project is C++ first. Core gameplay and simulation logic should live in native code, with Blueprint used for asset configuration, animation, visual setup, and editor-authored content.

Target split:

- 90-95% gameplay/system logic in C++.
- 5-10% Blueprint/editor-side setup where Unreal's workflow benefits from it.

## Scope Boundary

Current milestone: early vertical-slice foundations through One Morning Hunt, temporary sleep, settlement schema, builder construction, persistence, hunter intel, herbalist plant knowledge, soldier weapon supply, bow/ammo equipment rules, first-person bow placeholder, visible arrow projectiles, basic shot rhythm, exploration markers, basic native HUD status/objective overlay, and first native inventory/build/options menu controls.

The player starts in a primitive settlement, talks to one hunter NPC, receives a deer-hunting task, tracks and hunts a deer, harvests resources, returns to the NPC, unlocks early settlement progression, places a house schema, uses specialist NPC services, and advances time through sleep.

No multiplayer, large procedural world, economy simulation, complex story framework, advanced survival model, or broad animal roster should be added during this milestone.

## Source Layout

```text
Source/HimalayanSettlement/
  Core/
  Player/
  Equipment/
  Interaction/
  Inventory/
  Resources/
  Animals/
  AI/
  Hunting/
  Herbalism/
  Exploration/
  NPC/
  Settlement/
  Building/
  Quests/
  Progression/
  Persistence/
  World/
  UI/
  Tests/
```

## Data-Driven Foundation

Definitions are native `UPrimaryDataAsset` classes. Runtime systems should reference definitions by asset pointer or primary asset ID rather than hardcoding gameplay data.

Initial definition types:

- `UHSItemDefinition`
- `UHSAnimalDefinition`
- `UHSPlantDefinition`
- `UHSNPCDefinition`
- `UHSBuildingDefinition`
- `UHSQuestDefinition`

Gameplay Tags identify categories, knowledge, resources, quest IDs, animal states, professions, and building unlocks.

## Interaction And Quest Handoff

Interactable actors expose prompts and use actions through the native `IHSInteractable` contract. Runtime C++ systems should call the `HSInteraction` helper functions so native actors use direct C++ dispatch, while non-native Blueprint implementers remain a fallback path.

`UHSInteractionSensorComponent` owns focused interactable detection and caches the current prompt on a short timer. HUD code should read the cached prompt instead of tracing every frame.

The first quest handoff is intentionally small: `UHSQuestHandoffComponent` references one `UHSQuestDefinition`, tracks whether it is available, accepted, or completed, and does not tick. Completion checks the player's carried resource stacks and records reward knowledge/unlock tags. It does not yet own resource consumption, quest-log UI, building placement, or persistence.

## HUD

`AHSHUD` is the first native HUD layer. It draws a minimal interaction prompt from the player interaction sensor cache, short feedback messages for progress, settlement planning, construction, hunter intel, plant effects, world-time advancement, save/load/options actions, and first inventory/build actions, plus a compact vertical-slice status/objective readout. Objective guidance is derived from existing native state such as quest completion, carried deer resources, settlement claim, pending schemas, construction jobs, and completed house state. It is intentionally not a broader dialogue, quest log, or survival HUD.

Pause menu presentation is currently native Canvas HUD. `Esc` cancels active build placement first; otherwise it pauses/unpauses the game and shows keyboard commands for Resume, Save, Load, Options, and Exit. This is the first functional menu layer and may later be replaced by richer UMG/Slate widgets without moving gameplay rules out of C++.

Inventory and build menu presentation also live in the native Canvas HUD for now. `I`/`Tab` shows carried item stacks from `UHSResourceCarryComponent`; `Q`/`R` moves a highlighted selected stack, `[1]` inspects it, `[2]` drops one item from it, and `[3]` drops that stack for prototype cleanup. `B` opens a schema menu where `[1]` starts the Basic House free-placement preview only when native settlement/construction state allows it, while `[2]` starts Basic Campsite placement for a temporary one-use sleep point. The pause options panel can cycle graphics, mouse sensitivity, and master-volume presets, and persistence stores those prototype option values alongside gameplay state. `F10` toggles the FPS/unit performance overlays for profiling, and `F8` toggles an emergency low-end mode for weak hardware testing. These are first-pass control surfaces, not final visual UI.

## Prototype Visual Markers

Native placeholder actors own simple non-colliding static mesh components using engine basic shapes. These markers make the current test map readable while real art, animation, and Blueprint visual setup remain out of scope for this milestone slice. Marker readability should come from cheap silhouette differences first, not expensive material or lighting work.

## Exploration Markers

`AHSExplorationMarkerActor` is a no-tick native landmark marker used for the first village/forest readability pass. Markers are gameplay-tagged as village, forest, trail, or animal area so tests and later navigation/HUD systems can query them without depending on actor labels. Trail, village, forest, and animal-area markers use different basic-mesh silhouettes so route readability improves without adding asset cost.

## Animal Runtime

`AHSAnimalActor` is the first native runtime animal shell. It references a `UHSAnimalDefinition`, tracks health plus idle/alert/fleeing/dead state, and exposes one-shot harvesting of configured drop stacks. Non-lethal damage performs one cheap no-tick reaction by rotating and stepping the animal away from the attacker, and spawns a short-lived no-tick `AHSWoundClueActor` at the hit location. Alert, fleeing, and wound clues are early hooks for later tracking and AI; they do not yet drive pathfinding, animation, or ecology simulation.

## Hunting

`UHSHuntingComponent` is the first player hunting action. It performs an on-demand camera visibility trace, requires `UHSPlayerEquipmentComponent` to have a bow equipped and at least one basic arrow, blocks instant arrow spam with a short shot cooldown, consumes one arrow only when a valid live animal target is found, spawns a visible `AHSArrowProjectileActor`, and broadcasts lightweight HUD feedback. `AHSArrowProjectileActor` ticks only while flying, uses cheap native mesh parts for shaft/head/fletching, sweeps along its path with a visibility trace, stops on impact, and applies prototype hunting damage when it hits a live animal. It is not yet animation, weapon-skill, miss-spread, or advanced ballistics.

## Equipment

`UHSPlayerEquipmentComponent` is the first narrow equipment state. The bow equip flag lives on the player, while bow and arrow quantities remain authoritative in `UHSResourceCarryComponent`. Soldier service auto-equips the first granted bow, `G` toggles bow equip, and save/load persists the equip flag after carried resources restore.

The first-person bow visual is a native placeholder on `AHSPlayerCharacter`, built from no-collision static mesh components attached to the camera. It follows `UHSPlayerEquipmentComponent` state through an equip-change delegate and is deliberately not a final weapon model or animation system.

## Resource Carry State

`UHSResourceCarryComponent` is the first lightweight resource state on the player. It stores harvested `FHSItemStack` values merged by gameplay tag so quest return logic and the first inventory screen can query carried resources. Player menu code keeps a selected row clamped against the current carried stacks and supports selected-row inspect/drop actions. Dropped stacks spawn a lightweight no-tick `AHSResourcePickupActor` that can be interacted with to merge the stack back into carry state, but this is still not a full container, weight, equipment, crafting, or durable world-item system.

## Player Progress

`UHSPlayerProgressComponent` is the first lightweight milestone signal on the player. It records completed milestone tags once and broadcasts a completion delegate. The hunter quest handoff marks the quest tag complete when returned resources satisfy the handoff. It is not yet save/load, achievements, a quest journal, or broad progression logic.

## Settlement Unlock State

`UHSSettlementUnlockComponent` is the first lightweight settlement unlock state on the player. Quest reward unlock tags are granted once and can be queried by later building/placeable systems. It is not yet building placement, construction, resource spending, UI, settlement storage, or persistence.

## Specialist Role Services

NPC services are coordinated by `UHSNPCServiceComponent`, a no-tick native component on NPC actors. It exposes service options by gameplay tag, selects prompts by priority, executes the requested service, and broadcasts refreshes when world time advances. Builder starts pending schema construction, Hunter reports one daily animal lead, Herbalist teaches one known plant, and Soldier grants basic weapon supplies through this shared query/execution path.

## Build Placement

`UHSBuildPlacementComponent` owns the player-facing free-build placement flow. It starts disabled, ticks only while a placement preview is active, and drives a lightweight no-tick preview actor. Basic House placement validates against `UHSSettlementPlanComponent` and records a pending schema when confirmed. Basic Campsite placement skips permanent schema state and spawns a one-use `AHSTemporaryShelterActor` so the player can sleep/advance time without turning camps into long-term settlement infrastructure. Builder construction still consumes only pending building schemas and takes the configured time to complete them.

`AHSCompletedBuildingActor` is the first no-tick completed-building visual. `UHSConstructionComponent` spawns it when a construction job completes and rebuilds it from completed construction jobs during save restore. The prototype house uses basic wall, roof, door, and chimney meshes for readability, but it is not final house art or collision.

## Herbalism

`UHSPlantEffectComponent` stores known plants and one active temporary effect on the player. `AHSPlantActor` is a no-tick interactable that applies a known plant definition and consumes itself. Plant effects expire from `UHSWorldTimeComponent` events, not per-frame checks.

## Persistence

`UHSSettlementSaveGame` is versioned and stores time, milestones, carried resources, bow equip state, prototype option presets, emergency low-end mode state, settlement unlocks, schema state, construction jobs, hunter intel, and herbalist plant state. `UHSSettlementPersistenceComponent` snapshots and restores from native components and rejects unsupported save versions. Manual pause-menu saves use the default slot, while event-driven autosaves use a separate autosave slot so deliberate saves are not overwritten by sleep, milestone, or construction events.

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
