# Performance Strategy

The target is a forest and village exploration game that can scale down for low-end PCs. The game can be inspired by dense survival/exploration worlds, but the implementation must stay disciplined: readable forests, limited active simulation, conservative rendering defaults, and profiling before visual ambition.

## Current Issue

The first editor playtest had very low FPS. Treat this as an early production constraint.

Immediate notes:

- Editor viewport performance is not the same as packaged-game performance.
- UE5 defaults can be expensive for low-end PCs.
- Prototype maps should still be profiled early, because forest/village scenes can become costly quickly.

## Rendering Direction

- Build for scalable settings first.
- Avoid assuming Lumen, Virtual Shadow Maps, Nanite-heavy assets, or dense dynamic shadows are always acceptable.
- Prefer baked/static lighting or cheaper dynamic lighting where the gameplay allows it.
- Use simple readable art before cinematic fidelity.
- Keep foliage density, shadow distance, and post-processing under control.

Epic's documentation describes Lumen as UE5's default dynamic global illumination/reflection system and notes that lower scalability levels reduce its cost, with Low disabling Lumen features. Epic also documents Virtual Shadow Maps as a high-resolution shadowing system intended for film-quality assets and large dynamically lit worlds, which means it must be treated carefully for low-end targets.

## Simulation Direction

- No broad per-frame ticking for NPCs, wildlife, plants, buildings, or settlement systems.
- Use events, timers, daily refreshes, and abstract state.
- Distant animals should be regional reports/markers before they become active actors.
- NPC work should advance through time deltas, not full always-on behavior.
- Construction is a job record until the player is close enough to need visuals.

## Forest/Village Performance Rules

- Start with small authored spaces.
- Add foliage in measured layers.
- Use LODs and cull distances from the beginning.
- Prefer fewer high-value interactable objects over many active actors.
- Profile before and after adding forest density.
- Track performance in editor and, later, packaged builds separately.

## Near-Term Profiling Checklist

- Record FPS in editor with the current prototype map.
- Test viewport scalability settings at Low, Medium, and High.
- Check whether lighting, shadows, or editor overhead are the biggest visible cost.
- Add a packaged Development build performance check once packaging is introduced.
- Keep a performance note after each visual/world milestone.

## Target Philosophy

The goal is not to copy any specific game technically. The goal is to achieve a similar feeling of forest/village exploration with a much smaller, more controlled simulation:

- Fewer active systems.
- Strong role-based daily loops.
- Smart abstraction for distant activity.
- Scalable visuals.
- Measured expansion.
