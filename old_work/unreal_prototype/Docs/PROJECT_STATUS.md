# Project Status

## Current Milestone

Milestone 11: Vertical Slice Closeout: implementation continues with basic native HUD status/objective overlay, sleep/time feedback, pause menu, manual save/load, autosave foundation, selected-stack inventory actions, build schema/campsite menu feedback, first options control, bow/ammo equipment rules, first-person bow placeholder, visible arrow projectiles, bow shot cooldown, visible wounded-animal reactions, wound tracking clues, and clearer prototype visuals; hands-on closeout pending.

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
- Native player health component added with explicit damage, healing, death, and reset state; no stamina, hunger, thirst, or carry-limit systems are planned per direction.
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
- Native completed-building visual actor added; house completion now spawns a visible no-tick prototype house at the placed schema location.
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
- Native player equipment component added for basic bow equip state and arrow consumption.
- Soldier weapon service now auto-equips the first granted bow while still preventing duplicate weapon bundles.
- First-person bow placeholder added as cheap native camera-attached meshes that appear only while the bow is equipped.
- Hunting now requires an equipped bow and at least one arrow, consumes one arrow on valid animal shots, and reports hit/no-target/no-ammo feedback through the HUD.
- Native visible arrow projectile actor added; hunting now looses a more readable multi-part arrow that flies forward, traces while in flight, stops on impact, and applies hunting damage when it hits an animal.
- Bow firing now has a short native cooldown to prevent instant arrow spam and make shots easier to read.
- Animal runtime state now distinguishes idle, alert, fleeing, and dead; non-lethal hits rotate and move the animal away once so wounded reactions are visible before full AI exists.
- Native wound clue actor added; non-lethal animal hits leave a short-lived no-tick marker at the hit location for early tracking readability.
- HUD status lines now show bow equip state, current arrow count, and whether the bow is ready or nocking.
- Versioned SaveGame now stores player health, bow equipped state, and emergency low-end mode state.
- Milestone 8 soldier weapon-supply foundation compiles.
- Native NPC service component added for shared role service query, prompt, execution, and time-refresh signaling.
- Builder, hunter, herbalist, and soldier services now expose consistent service tags and options.
- NPC actors now delegate role-service prompts and interactions to the shared service component while preserving quest handoff priority rules.
- NPC service component now tracks a selected available service and executes that selected service; Q/R can cycle the focused NPC service when no menu or build placement is active.
- Milestone 9 NPC role framework foundation compiles.
- Native exploration marker actor added for cheap no-tick village, forest, trail, and animal-area landmarks.
- One Morning Hunt map now has a readable village-to-forest route with trail markers, a forest edge marker, an animal ridge marker, and a village core marker.
- Milestone 10 exploration foundation compiles.
- Native HUD status overlay added for vertical-slice testing: time/day, carried resources, active animal lead, active plant effect, and construction progress.
- Native HUD objective guidance added for the early loop: hunter hunt/turn-in, settlement claim, house placement, builder start, construction sleep, and post-house exploration.
- Native HUD time-advance feedback added so sleep/day changes clearly report the new phase and refreshed services.
- Native pause menu foundation added: `Esc` cancels build placement first, otherwise pauses/resumes and shows Resume, Save, Load, Options, and Exit commands.
- Pause menu save/load now uses a selected manual save slot; the options panel cycles between three manual slots with `[9]` while autosave remains separate.
- Pause, save/load, inventory, build, and graphics option actions now surface short native menu feedback messages.
- Manual pause-menu save/load uses the default save slot; event-driven autosave uses a separate autosave slot.
- Autosave now runs on milestone completion, time advance/sleep, construction start, and construction completion, with suppression during manual load restore.
- Native inventory screen added on `I` / `Tab`, showing carried item stacks from the resource carry component.
- Native inventory selection added: `Q`/`R` changes the selected carried stack, and `[1]`, `[2]`, `[3]` inspect/drop the selected stack.
- Native resource pickup actor added; dropped inventory stacks now spawn lightweight no-tick world pickups that can be collected again.
- Native world cleanup subsystem added as the gameplay garbage collector for temporary actors such as wound clues, arrows, and dropped resources.
- Native inventory use action added on `[4]`; the carried basic bow can be equipped from inventory, while unsupported resources report clear feedback.
- Native build menu added on `B`; Basic House is selected with `[1]` before entering free placement.
- Build menu now has a highlighted selected entry; `Q`/`R` moves between Basic House and Basic Campsite, and `[1]`/`E` starts the selected entry.
- Build menu now explains blocked Basic House placement states such as missing settlement claim, pending schema, active construction, or completed house.
- Build menu now includes `[2] Basic Campsite`, which enters free placement and spawns a one-use temporary shelter for sleep/time skipping.
- Pause options can cycle a simple graphics scalability preset with `[6]`.
- Pause options can cycle mouse sensitivity with `[7]` and master volume with `[8]`; these settings are stored in settlement saves.
- Native `F10` shortcut toggles the FPS and unit performance overlays for live profiling.
- Native `F8` shortcut toggles Emergency Low-End Mode for weak hardware testing.
- Default low-end settings were tightened after hands-on FPS dropped from about 19 to about 7 during play.
- Hands-on retest with emergency low-end mode reached about 40 FPS and stayed stable.
- Exploration markers now use distinct cheap silhouettes for trail, village, forest, and animal-area landmarks.
- Completed house prototype visual now includes no-tick wall, roof, door, and chimney meshes.
- VS Code low-end benchmark smoke task added; real-renderer command path confirmed, but numeric FPS still requires manual overlay recording.
- Native One Morning Hunt loop smoke test added for accept, hunt, harvest, turn-in, milestone, and unlock flow.
- VS Code tasks added for building, automation tests, content bootstrap, and opening the One Morning Hunt map.
- Headless One Morning Hunt map launch smoke pass succeeds and loads `HSGameMode`.
- Hands-on editor playtest passed with the visible `Morning hunt completed` confirmation.
- Hands-on temporary shelter playtest passed and Milestone 2 was confirmed complete.
- Hands-on movement playtest passed for jump, crouch, and prone.
- Hands-on milestone pass reported complete by the project owner after the 40 FPS stable retest.
- Milestone 3 settlement claim and house schema foundation compiles.
- Content bootstrap succeeds with zero warnings and regenerates the basic house schema marker.
- All 89 `HimalayanSettlement` automation tests pass.

## In Progress

- Hands-on closeout results have been reported working by the project owner; equipment/hunting polish is ready for the next manual pass.
- Current implementation batch adds native bow draw/release free-shooting, target miss support, minor house upgrade placement/construction, slope-aware build placement, second-pass NPC services, clearer build menu entries, and extra cheap exploration markers.

## Known Blockers

- No current blockers.

## Known Warnings

- UnrealBuildTool accepts MSVC 14.44.35228 but warns that UE 5.5 prefers compiler version 14.38.33130.

## Known Technical Debt

- Project name is provisional: `HimalayanSettlement`.
- Gameplay Tags are intentionally minimal and should be expanded only when milestone features require them.
- Data assets are prototype-authored and should stay narrow until each milestone system needs more fields.
- Input config is intentionally minimal because gameplay input mappings are created in native C++.
- Low-end renderer/scalability baseline, benchmark smoke launch, and Emergency Low-End Mode are configured; numeric FPS/Game/Draw/GPU readings should still be captured from the live overlay before forest density increases.

## Next Tasks

1. Run hands-on bow playtest: hold/release hunt input, confirm the visible arrow flies forward, confirm misses are visible, and confirm target hits/death are readable.
2. Run hands-on build playtest: place Basic House, complete it through Builder/time, place Minor House Upgrade, confirm it completes after 0.25 day, and place Basic Campsite for one-use sleep.
3. Run hands-on NPC services pass: Hunter refreshes an active marker, Herbalist restores health when wounded, and Soldier supplies arrows when the player owns a bow with low arrows.
4. Run hands-on placement pass: confirm houses/campsites snap to ground and reject steep or non-ground aim points.
5. Run hands-on performance pass: compare normal and Emergency Low-End Mode with `F10` overlay during hunting, building, NPC use, sleep, save/load, and exploration.
6. Run hands-on exploration pass: walk settlement to village core, builder yard, herb patch, forest edge, ridge trail, and animal ridge, and confirm the route is readable without dense foliage.
7. Keep tracking numeric FPS/Game/Draw/GPU readings from the live low-end `stat fps` and `stat unit` overlays before forest density increases.
8. Add mouse-driven UMG/Slate polish later if the native Canvas controls test well.
