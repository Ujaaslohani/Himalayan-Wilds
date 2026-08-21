# Gameplay Direction

## Pillars

- Grounded forest and village exploration.
- Temporary survival camps, but permanent progression through houses and settlement structures.
- Schema-based building: the player places the plan, then NPC builders perform the work over time.
- Specialist NPCs unlock practical daily services.
- Time matters: sleeping changes day/night state and advances construction or daily services.
- C++ owns gameplay rules; assets and editor content tune the world.

## Temporary Camp

The temporary camp should be useful but not central.

Player-facing purpose:

- Gives a place to sleep.
- Lets the player flip time from day to night or night to day.
- Supports short-term survival and early tutorials.
- Expires after its intended short use.

Boundaries:

- It should not become a deep upgrade tree.
- It should not last more than its intended short window.
- It should not replace the need for a permanent house/settlement.
- It should feel closer to a temporary survival shelter than a permanent base.

## House And Schema Building

Building direction is schema-based.

Player-facing flow:

1. Player chooses a schema, such as a house.
2. Player places the schema/ghost in a valid location.
3. Builder NPC starts construction.
4. Time passes through sleep/day changes.
5. Building completes after its configured duration.

Construction durations:

- Minor upgrade: `0.25` day.
- Mid-tier upgrade: `0.5` day.
- House or major upgrade: `1.0` day.

Important rule: the player should not spend most of the game cutting trees, collecting logs, and manually assembling every wall. Resource gathering can exist, but construction work belongs to the builder role.

Current implementation boundary:

- The first settlement slice records a permanent claim marker and one pending `Basic House` schema.
- Pending means the player has chosen intent; it is not a finished house.
- Construction duration, builder assignment, and completion are intentionally reserved for the builder milestone.
- The first builder slice starts with one builder, one active job, and a `1.0` day basic house duration.
- The first persistence slice stores time, progress, carried resources, unlocks, settlement claim, pending schemas, construction jobs, and completed buildings through a versioned native SaveGame.
- The first hunter intel slice gives one event-driven deer lead per in-game day and spawns a lightweight temporary marker; it does not run a broad ecology simulation.

## NPC Role Use Cases

### Builder

- Builds house schemas and upgrades.
- Owns construction time.
- May later improve construction speed or quality.

### Hunter

- Gives one daily nearby animal report.
- Example: `Deer spotted near the ridge.`
- Creates a temporary marker or lead location.
- May later improve tracking or rare animal discovery.

### Soldier

- Provides weapons, training, defense, or danger warnings.
- May unlock safer travel, settlement protection, or combat preparation.

### Herbalist

- Identifies useful plants.
- Teaches or prepares plant effects:
  - speed increase
  - scent masking
  - heightened senses
  - healing
  - stamina support
- Makes forest exploration more strategic before hunting or travel.

## Daily Loop Target

The intended early loop:

1. Wake at shelter/house.
2. Check daily NPC services.
3. Hunter reports animal activity.
4. Herbalist or soldier can help prepare.
5. Player chooses a task: hunt, gather, scout, or manage construction.
6. Player returns to settlement.
7. Builder progresses construction when time advances.
8. Player sleeps to shift day/night and start the next phase.

## Design Caution

Before adding new systems, each NPC role should answer:

- What does the player get from this role?
- How often can the service be used?
- Does it depend on day/night or sleep?
- Does it create a map/world target?
- What data assets and tags are needed?
- What automation test proves the rule?
