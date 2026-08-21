# Project Status

## Current Milestone

Milestone 5: Persistent Settlement State: in progress.

## Engine Version

Unreal Engine 5.5.4.

- Install path: `C:\Program Files\Epic Games\UE_5.5`
- Changelist: `40574608`
- Compatible changelist: `37670630`
- Branch: `++UE5+Release-5.5`

## Completed Systems

- Git repository initialized.
- Minimal Unreal project descriptor added with `EngineAssociation` set to `5.5`.
- C++ module skeleton added.
- Initial Gameplay Tags config added.
- Foundational milestone Gameplay Tags are also registered natively for commandlet/runtime reliability.
- Initial Primary Data Asset classes added for items, animals, NPCs, buildings, and quests.
- Logging categories added.
- Automation-test skeleton added for definition validation.
- Architecture and ADR documentation started.
- `HimalayanSettlement` Win64 Development game target compiles with UE 5.5.4.
- `HimalayanSettlementEditor` Win64 Development target compiles with UE 5.5.4.
- All five `HimalayanSettlement.DataDefinitions` automation tests pass.
- Native first-person player foundation added.
- Native interaction interface, on-demand interaction sensor, and placeholder interaction target added.
- Native NPC actor interaction stub added for the first hunter conversation.
- Minimal One Morning Hunt data assets and test map generated through an editor-only C++ commandlet.
- Native quest handoff component added and wired to the hunter in `L_OneMorningHunt`.
- Native deer runtime actor added with health, death state, and one-shot harvest drops.
- One Morning Hunt map now contains a configured native deer actor.
- Native HUD interaction prompt feedback added through cached interaction sensor focus.
- Native player hunting component added and bound to left mouse / right trigger.
- Native resource carry component added and deer harvest now transfers drops to the player.
- Hunter quest handoff can now complete after the player carries required deer resources and records reward tags.
- Hunter quest turn-in now consumes required deer resources atomically on completion.
- Native player progress component added; completed hunter quest now marks the One Morning Hunt quest tag complete on the player.
- Native prototype visual mesh markers added for hunter, deer, and practice interaction target.
- Native settlement unlock component added; completed hunter quest now unlocks the basic drying rack tag on the player.
- Native HUD completion message added for completed player progress milestones.
- Native camp rest point added as the gated end-of-day marker for the One Morning Hunt loop.
- Native world time state added with day/night phase and half-day advancement.
- Native temporary shelter marker added; sleeping flips day/night and expires the shelter.
- Native player movement supports jump, crouch, and prone traversal controls.
- Concise One Morning Hunt playtest checklist added.
- Hands-on One Morning Hunt playtest results sheet added for milestone closeout.
- Full milestone roadmap added in `Docs/MILESTONES.md`.
- Gameplay direction documented for temporary camps, schema building, construction time, and NPC role services.
- Performance strategy documented after low-FPS editor playtest feedback.
- Low-end performance baseline preset added for renderer defaults, game user settings, no-shadow prototype lights, and VS Code profiling launch.
- Native settlement planning component added for permanent claim state and pending building schemas.
- Native settlement claim marker and basic house schema marker added for the first permanent building intent flow.
- Native HUD feedback added for settlement claim and pending schema placement.
- Basic house building definition and `Building.BasicHouse` / `Settlement.Stage.Claimed` tags added.
- Native construction component added for one active builder job, half-day time progress, and completed building state.
- Builder profession tag, builder data asset, and builder NPC bootstrap path added.
- Native HUD feedback added for construction start and completion.
- Milestone 4 builder construction-time foundation compiles.
- Content bootstrap succeeds with zero warnings and regenerates the builder NPC/data asset.
- Native versioned settlement SaveGame added for time, milestones, resources, unlocks, settlement claim, pending schemas, construction jobs, completed buildings, and future NPC profession tags.
- Native settlement persistence component added with snapshot restore plus manual F5 save / F9 load bindings.
- Milestone 5 persistence foundation compiles.
- Native One Morning Hunt loop smoke test added for accept, hunt, harvest, turn-in, milestone, and unlock flow.
- VS Code tasks added for building, automation tests, content bootstrap, and opening the One Morning Hunt map.
- Headless One Morning Hunt map launch smoke pass succeeds and loads `HSGameMode`.
- Hands-on editor playtest passed with the visible `Morning hunt completed` confirmation.
- Hands-on temporary shelter playtest passed and Milestone 2 was confirmed complete.
- Hands-on movement playtest passed for jump, crouch, and prone.
- Milestone 3 settlement claim and house schema foundation compiles.
- Content bootstrap succeeds with zero warnings and regenerates the basic house schema marker.
- All 53 `HimalayanSettlement` automation tests pass.

## In Progress

- Milestone 5 hands-on save/load playtest.

## Known Blockers

- No current blockers.

## Known Warnings

- UnrealBuildTool accepts MSVC 14.44.35228 but warns that UE 5.5 prefers compiler version 14.38.33130.

## Known Technical Debt

- Project name is provisional: `HimalayanSettlement`.
- Gameplay Tags are intentionally minimal and should be expanded only when milestone features require them.
- Data assets are prototype-authored and should stay narrow until each milestone system needs more fields.
- Input config is intentionally minimal because gameplay input mappings are created in native C++.
- Low-end renderer/scalability baseline is configured and confirmed working; numeric FPS readings should still be captured before forest density increases.

## Next Tasks

1. Run hands-on playtest: build settlement state, press F5, alter state, press F9, and confirm time/construction/resources restore.
2. Run hands-on closeout for Milestones 3 and 4 if not already confirmed in-editor.
3. Record numeric FPS using the low-end profile task before forest density increases.
