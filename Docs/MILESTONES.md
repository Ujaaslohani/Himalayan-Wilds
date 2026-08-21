# Milestones

This roadmap follows the updated direction: temporary camps are useful for short survival beats, but long-term progression comes from houses, builder-led schema construction, time passing, and specialist NPC roles.

Each milestone must stay C++-first, end with automation coverage for core rules, and close only after a hands-on editor pass.

## Milestone 1: One Morning Hunt

Status: Complete.

Player experience: The player learns the first loop: talk to a hunter, hunt one deer, harvest it, return resources, unlock early settlement possibility, and rest to end the morning.

Complete when:

- Player can accept `Quest.OneMorningHunt`.
- Player can hunt and harvest the deer.
- Quest turn-in consumes required resources.
- Player receives the first settlement unlock.
- Rest endpoint marks `Milestone.OneMorningHunt.DayEnded`.
- Automation and hands-on playtest pass.

## Milestone 2: Temporary Shelter And Sleep

Status: Complete.

Player experience: The player can use a very basic temporary shelter marker. It exists to make sleep possible, like a short survival shelter, not to become the center of long-term progression.

Scope:

- One temporary shelter/camp marker.
- Sleep interaction unlocked by shelter.
- Sleep flips time state: day to night or night to day.
- Temporary shelter expires after use.
- Time state is owned by a reusable native world-time system so builders and daily NPC services can use it later.
- No camp upgrade tree.
- No permanent camp economy.
- No personal log-hauling/build-the-camp loop.

Complete when:

- Shelter enables sleep.
- Sleep changes the current time phase.
- Time phase is visible enough for testing.
- Shelter does not behave like a permanent house.
- Expired shelter cannot be used for another sleep.
- Automation and manual playtest pass.

## Milestone 3: Settlement Claim And House Schema

Status: Implementation complete; hands-on closeout pending.

Player experience: The player chooses a permanent settlement spot and places a house schema/ghost, similar in spirit to schema-based building games. The player is choosing intent, not personally constructing the house by hand.

Scope:

- Native schema placement actor/component.
- Native settlement claim marker.
- One permanent schema: basic house.
- Schema validates placement and records pending construction.
- No instant building completion.
- No tree chopping/log hauling loop.
- No builder job timer yet; that starts in Milestone 4.

Complete when:

- Player can place a house schema in a valid area.
- Invalid placement is blocked.
- Placed schema is recorded as pending construction.
- Automation covers valid/invalid schema placement.

## Milestone 4: Builder NPC And Construction Time

Status: Implementation complete; hands-on closeout pending.

Player experience: The builder NPC makes construction real. The player chooses what to build; the builder takes time to finish it.

Scope:

- Builder role definition.
- Construction job state: pending, active, complete.
- Time-cost rules:
  - Minor upgrade: `0.25` day.
  - Mid-tier upgrade: `0.5` day.
  - House or major upgrade: `1.0` day.
- Construction progresses when time advances through sleep/day changes.
- One builder, one active job at first.

Complete when:

- Builder can start the house schema.
- Sleeping/time advancement progresses construction.
- House completes after the configured duration.
- Tests cover job start, blocked start, progress, and completion.

## Milestone 5: Persistent Settlement State

Status: In progress.

Player experience: The house, time state, recruited NPCs, construction progress, and completed milestones persist across sessions.

Scope:

- Native SaveGame data for time phase, progress, resources, NPC roles, schemas, and completed buildings.
- Simple manual save/load or milestone-bound autosave.
- Versioned save structure.
- Conservative invalid-save handling.

Complete when:

- House/construction/time state can be saved and loaded.
- Broken or missing save data fails gracefully.
- Automation covers serialization of tags, time, construction jobs, and settlement records.

## Milestone 6: Hunter Role: Daily Animal Intel

Player experience: A recruited hunter becomes useful every day by reporting nearby animal activity. The player gets one daily lead, such as "deer spotted near the ridge," and a temporary marker appears.

Scope:

- Hunter service state.
- One daily animal report.
- Temporary world/map marker for the reported animal area.
- Report refreshes once per day.
- No large ecology simulation yet.

Complete when:

- Hunter provides one animal lead per day.
- Lead marker appears and expires or refreshes correctly.
- Player can use the lead to find a hunt target.
- Automation covers daily report generation and refresh gating.

## Milestone 7: Herbalist Role: Plant Knowledge

Player experience: A herbalist makes the forest more interesting by identifying useful plants and effects. Plants become preparation tools for exploration and hunting.

Scope:

- Herbalist role definition.
- One or two plant definitions.
- Plant effects such as speed increase, scent masking, heightened senses, healing, or stamina.
- Temporary effect component on player.
- Minimal feedback for active effect.

Complete when:

- Herbalist can reveal or explain plant use.
- Player can consume/apply one plant effect.
- Effect changes a native gameplay value for a limited duration.
- Tests cover effect apply, expiry, and invalid use.

## Milestone 8: Soldier Role: Weapons And Safety

Player experience: A soldier gives the settlement defensive and weapon value. The player can receive weapon access, training, or threat warnings.

Scope:

- Soldier role definition.
- One weapon/service unlock.
- Basic settlement safety or threat-warning state.
- No full combat faction system yet.

Complete when:

- Soldier unlocks or provides one useful combat-related service.
- Service is gated by recruitment/settlement state.
- Tests cover service availability and unlock behavior.

## Milestone 9: NPC Role Framework

Player experience: NPCs are no longer just quest givers. Different people make different settlement systems possible.

Scope:

- Shared native NPC role/service framework.
- Role service availability rules.
- Daily or time-gated service refresh.
- Data-driven role definitions.
- Roles initially include builder, hunter, herbalist, and soldier.

Complete when:

- Role services can be queried consistently.
- Time advancement refreshes appropriate services.
- New roles can be added without rewriting core logic.
- Automation covers role availability and daily refresh behavior.

## Milestone 10: Forest And Village Exploration Slice

Player experience: The game begins to feel closer to a grounded forest/village exploration game: readable forest paths, settlement areas, animal signs, NPC services, and reasons to travel.

Scope:

- One authored forest area and one simple settlement/village area.
- Exploration markers from NPC services.
- Performance-conscious foliage and lighting.
- Navigation readability pass.
- No giant open world yet.

Complete when:

- Player can move between settlement, forest, animal lead, and resource locations.
- Forest/village traversal is readable.
- Performance is profiled on low/scalable settings.
- Manual pass confirms exploration feels coherent.

## Milestone 11: Vertical Slice Closeout

Player experience: A new player can complete an early loop without developer instruction: wake, receive NPC intel, explore, hunt/gather, assign builder, sleep, and see the settlement progress.

Scope:

- One to two in-game days.
- Temporary shelter, house schema, builder construction, hunter intel, one plant effect, and one weapon/safety service.
- Save/load.
- Basic native HUD for objectives, time, resources, and construction state.
- Replace only the most confusing prototype markers with simple readable assets.
- Performance and bug-bash pass.

Complete when:

- New player can finish the vertical slice without explanation.
- Core transactions have automation coverage.
- Hands-on playtest has no blocking issues.
- Performance target is met on the agreed low-end profile.

## Global Rules

- Temporary camp is not the long-term settlement centerpiece.
- Permanent houses and schemas carry long-term progression.
- Player places intent; builder NPCs perform construction over time.
- NPC roles must create concrete services, not just dialogue flavor.
- Time passing must matter before the world becomes large.
- Keep gameplay/system rules in native C++.
- Use Blueprint/editor content for visuals, animation, tuning, and level authoring only.
- Add Gameplay Tags and Data Asset fields only when a milestone needs them.
- Compile and run automation after each major system.
- Record manual playtest results before marking a milestone complete.
