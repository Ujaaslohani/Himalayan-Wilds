# Project Status

## Current Milestone

One Morning Hunt.

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
- Concise One Morning Hunt playtest checklist added.
- Native One Morning Hunt loop smoke test added for accept, hunt, harvest, turn-in, milestone, and unlock flow.
- VS Code tasks added for building, automation tests, content bootstrap, and opening the One Morning Hunt map.
- Headless One Morning Hunt map launch smoke pass succeeds and loads `HSGameMode`.
- All 33 `HimalayanSettlement` automation tests pass.

## In Progress

- One Morning Hunt prototype architecture.
- First playable One Morning Hunt systems.

## Known Blockers

- No current blockers.

## Known Warnings

- UnrealBuildTool accepts MSVC 14.44.35228 but warns that UE 5.5 prefers compiler version 14.38.33130.

## Known Technical Debt

- Project name is provisional: `HimalayanSettlement`.
- Gameplay Tags are intentionally minimal and should be expanded only when milestone features require them.
- Data assets are prototype-authored and should stay narrow until each milestone system needs more fields.
- Input config is intentionally minimal because gameplay input mappings are created in native C++.

## Next Tasks

1. Run a hands-on editor/manual smoke pass on the playable map.
2. Decide whether first milestone should stop here or add a tiny end-of-day marker.
