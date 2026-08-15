# Milestones

This roadmap keeps the project C++-first and grows one playable slice at a time. Each milestone should end with a manual editor pass, automation coverage for core rules, and a short closeout note before the next milestone starts.

## Milestone 1: One Morning Hunt

Status: Complete.

Goal: Prove the core loop of talking to an NPC, hunting one deer, harvesting, turning in resources, unlocking one settlement object, and ending the morning.

Complete when:

- Player can accept `Quest.OneMorningHunt`.
- Player can hunt and harvest the deer.
- Quest turn-in consumes required resources.
- Player receives the drying rack unlock.
- Camp rest point marks `Milestone.OneMorningHunt.DayEnded`.
- Automation and hands-on playtest pass.

## Milestone 2: First Camp Craft

Goal: Turn the drying rack unlock into the first settlement construction action.

Scope:

- Native building placement component or placement actor.
- One placeable building only: `Building.BasicDryingRack`.
- Resource cost checks and atomic consumption.
- Settlement state records the placed building.
- Minimal placement preview/marker; no broad building UI.
- HUD feedback for required/current resources if needed for testing.

Complete when:

- The player can place one drying rack after the hunt unlock.
- Placement is blocked without unlock or resources.
- Placement consumes the configured cost.
- Settlement state records the placed rack.
- Automation and manual playtest pass.

## Milestone 3: Persistent Camp State

Goal: Preserve the player and camp state across editor sessions.

Scope:

- Native SaveGame data for player progress, settlement unlocks, carried resources, and placed buildings.
- Manual save/load commands or simple auto-save at milestone boundaries.
- Versioned save data structure with conservative migration hooks.
- Tests for serialization of tags, item stacks, and placed building records.

Complete when:

- Milestone 1 and Milestone 2 results can be saved and loaded.
- Invalid or missing save data fails gracefully.
- No broad menu system is required yet.

## Milestone 4: Second Day Foraging

Goal: Add a non-hunting resource loop that feeds settlement crafting.

Scope:

- One or two forage resources.
- Native gatherable actors with reusable interaction behavior.
- Resource definitions and tags.
- Simple respawn or exhausted-state rule.
- One NPC or camp objective that uses gathered resources.

Complete when:

- Player can gather resources, carry them, and spend them in a small camp objective.
- Gatherables do not tick unnecessarily.
- Automation covers gather, exhaust, and resource transfer behavior.

## Milestone 5: Simple Wildlife Behavior

Goal: Replace the static deer with basic wildlife behavior while keeping simulation cost controlled.

Scope:

- Deer idle, alert, and flee states.
- Event/timer-driven behavior, not per-frame logic unless justified.
- Basic perception radius or proximity trigger.
- Simulation LOD boundary for active vs. dormant wildlife.
- No herd ecology or large animal roster yet.

Complete when:

- Deer can react to the player and flee.
- Hunting remains playable.
- Distant/dormant animals avoid active expensive simulation.
- Automation covers state transitions where practical.

## Milestone 6: Settlement Needs Prototype

Goal: Give the camp a small reason to use resources over time.

Scope:

- One or two camp needs, such as food drying or warmth.
- Native settlement component tracks simple need values.
- Placed buildings can satisfy or modify needs.
- No complex economy, population simulation, or morale model yet.

Complete when:

- A placed drying rack affects one settlement need or production output.
- Need state is visible enough for testing.
- Save/load preserves the need state.

## Milestone 7: NPC Roles And Work Assignments

Goal: Let one NPC role affect the settlement loop.

Scope:

- Extend NPC definitions with role capabilities.
- One assignment type, such as hunter, gatherer, or camp helper.
- Native assignment state and validation.
- Offline/abstract result calculation for assigned NPC work.
- No full schedules, dialogue trees, or relationship simulation yet.

Complete when:

- One NPC can be assigned to one camp task.
- Assignment produces or modifies a resource/settlement state.
- Simulation remains abstract when the NPC is not locally active.

## Milestone 8: Expanded Camp Crafting

Goal: Add a small crafting/building chain without losing scope control.

Scope:

- Two to three additional recipes or camp objects.
- Recipe definition data asset if needed.
- Resource spending, unlock requirements, and result records.
- Minimal native crafting UI or command surface only if testing requires it.

Complete when:

- Player can craft/place multiple simple camp improvements.
- Requirements and costs are data-driven.
- Tests cover valid, blocked, and duplicate craft paths.

## Milestone 9: Local World Slice

Goal: Expand from a test arena into a small readable local environment.

Scope:

- One authored local terrain/forest/camp area.
- Navigation and interaction readability pass.
- Basic foliage/landscape setup using editor tools.
- Keep gameplay logic native; Blueprint/editor use is visual and layout only.
- Performance checks with Unreal Insights/stat commands.

Complete when:

- Milestones 1-8 work inside a more believable local world slice.
- Navigation and target readability pass manual testing.
- Performance remains acceptable for the active actor count.

## Milestone 10: Vertical Slice Closeout

Goal: Package a coherent early playable vertical slice.

Scope:

- Complete one to two in-game days of hunting, gathering, camp crafting, and settlement progression.
- Save/load supported.
- Basic native HUD for objectives, resources, and camp state.
- Replace only the most confusing prototype markers with simple visual/audio assets.
- Bug bash and performance pass.

Complete when:

- A new player can complete the vertical slice without developer instruction.
- Automated tests cover core transactions and milestone gates.
- Manual playtest notes have no blocking issues.
- The next production phase can be planned from a stable baseline.

## Global Rules

- Do not start a milestone until the previous one is closed or explicitly paused.
- Keep gameplay/system rules in native C++.
- Use Blueprint/editor content for visuals, animation, tuning, and level authoring only.
- Add Gameplay Tags and Data Asset fields only when a milestone needs them.
- Compile and run automation after each major system.
- Record manual playtest results before marking a milestone complete.
