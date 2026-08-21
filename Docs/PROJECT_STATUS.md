# Project Status

## Current Milestone

Milestone 11: Vertical Slice Closeout: implementation started with basic native HUD status/objective overlay, pause menu, manual save/load, and autosave foundation; inventory/build/options menus and hands-on closeout pending.

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
- Native free-build placement component added for The Forest-style house schema preview, rotate, confirm, and cancel controls.
- Basic house placement now starts from player build mode instead of relying on a fixed map schema marker.
- Basic house building definition and `Building.BasicHouse` / `Settlement.Stage.Claimed` tags added.
- Native construction component added for one active builder job, half-day time progress, and completed building state.
- Builder profession tag, builder data asset, and builder NPC bootstrap path added.
- Native HUD feedback added for construction start and completion.
- Milestone 4 builder construction-time foundation compiles.
- Content bootstrap succeeds with zero warnings and regenerates the builder NPC/data asset.
- Native versioned settlement SaveGame added for time, milestones, resources, unlocks, settlement claim, pending schemas, construction jobs, completed buildings, and future NPC profession tags.
- Native settlement persistence component added with snapshot restore plus manual F5 save / F9 load bindings.
- Milestone 5 persistence foundation compiles.
- Native hunter animal intel component added for one daily deer lead, lead expiry on day change, and persistence restore.
- Native animal lead marker actor added as a lightweight temporary world marker.
- Hunter NPC interaction now reports daily animal signs after quest handoff priority.
- Native HUD feedback added for animal lead reported/expired.
- Milestone 6 hunter daily animal intel foundation compiles.
- Native plant definition asset class added for plant tags, effect tags, movement multiplier, and duration.
- Native plant effect component added for learned plant knowledge, temporary player effects, time-based expiry, and persistence restore.
- Native plant actor added as a no-tick interactable that applies known plant effects and consumes itself.
- Herbalist profession tag, herbalist data asset, and herbalist NPC bootstrap path added.
- Player movement now applies active plant speed multipliers across standing, crouch, and prone speeds.
- Native HUD feedback added for plant knowledge learned, plant effect applied, and plant effect expired.
- Milestone 7 herbalist plant knowledge foundation compiles.
- Soldier profession tag, soldier data asset, and soldier NPC bootstrap path added.
- Soldier NPC interaction now grants one basic bow and a basic arrow bundle if the player does not already have a bow.
- Milestone 8 soldier weapon-supply foundation compiles.
- Native NPC service component added for shared role service query, prompt, execution, and time-refresh signaling.
- Builder, hunter, herbalist, and soldier services now expose consistent service tags and options.
- NPC actors now delegate role-service prompts and interactions to the shared service component while preserving quest handoff priority rules.
- Milestone 9 NPC role framework foundation compiles.
- Native exploration marker actor added for cheap no-tick village, forest, trail, and animal-area landmarks.
- One Morning Hunt map now has a readable village-to-forest route with trail markers, a forest edge marker, an animal ridge marker, and a village core marker.
- Milestone 10 exploration foundation compiles.
- Native HUD status overlay added for vertical-slice testing: time/day, carried resources, active animal lead, active plant effect, and construction progress.
- Native HUD objective guidance added for the early loop: hunter hunt/turn-in, settlement claim, house placement, builder start, construction sleep, and post-house exploration.
- Native pause menu foundation added: `Esc` cancels build placement first, otherwise pauses/resumes and shows Resume, Save, Load, Options, and Exit commands.
- Manual pause-menu save/load uses the default save slot; event-driven autosave uses a separate autosave slot.
- Autosave now runs on milestone completion, time advance/sleep, construction start, and construction completion, with suppression during manual load restore.
- Native One Morning Hunt loop smoke test added for accept, hunt, harvest, turn-in, milestone, and unlock flow.
- VS Code tasks added for building, automation tests, content bootstrap, and opening the One Morning Hunt map.
- Headless One Morning Hunt map launch smoke pass succeeds and loads `HSGameMode`.
- Hands-on editor playtest passed with the visible `Morning hunt completed` confirmation.
- Hands-on temporary shelter playtest passed and Milestone 2 was confirmed complete.
- Hands-on movement playtest passed for jump, crouch, and prone.
- Milestone 3 settlement claim and house schema foundation compiles.
- Content bootstrap succeeds with zero warnings and regenerates the basic house schema marker.
- All 74 `HimalayanSettlement` automation tests pass.

## In Progress

- Hands-on closeout for Milestone 6 hunter intel, Milestone 7 herbalist plant knowledge, Milestone 8 soldier weapon supplies, Milestone 9 NPC service framework, Milestone 10 exploration readability/performance, and Milestone 11 HUD/objective/menu readability.

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

1. Run hands-on hunter intel playtest: ask hunter for animal signs, confirm marker appears, sleep into next day, and confirm a fresh report is available.
2. Run hands-on herbalist playtest: talk to Herbalist, use Speed Herb, confirm movement is faster, sleep/advance time, and confirm the effect expires.
3. Run hands-on soldier playtest: talk to Soldier, confirm bow and arrows are granted once, and confirm repeat interaction does not duplicate supplies.
4. Run hands-on free-build playtest: claim settlement, press `B`, rotate with `Q/R`, place house with left mouse or `E`, ask Builder to start construction, then sleep until the 1-day build completes.
5. Run hands-on service-framework sanity pass: confirm Builder/Hunter/Herbalist/Soldier prompts still appear only when their service is available.
6. Run hands-on exploration pass: walk settlement to village core, follow trail markers to forest edge, find animal ridge, and confirm the route is readable without explanation.
7. Run hands-on closeout for Milestones 3-5 if not already confirmed in-editor.
8. Record numeric FPS using the low-end profile task before forest density increases.
9. Run hands-on HUD pass: confirm status/objective lines appear clearly without blocking the center interaction prompt.
10. Run hands-on pause/save pass: press `Esc`, test Resume/Save/Load/Options, confirm build placement `Esc` still cancels placement before pausing, and confirm autosave files are produced after sleep/construction/progress events.
11. Implement the first inventory screen.
12. Replace direct `B` house placement with a build-mode schema menu.
