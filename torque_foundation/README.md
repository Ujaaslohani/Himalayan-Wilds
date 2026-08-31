# Torque3D Foundation

This folder tracks the minimal HSPFoundation Torque3D game overlay.

The live runnable copy during validation was:

`research_candidates/Torque3D/My Projects/HSPFoundation`

That checkout is intentionally ignored because it contains the full Torque3D engine, copied BaseGame runtime files, build outputs, generated binaries, logs, and local dependency state.

Tracked here:

- `HSPFoundation/source` - our C++ validation/foundation module
- `HSPFoundation/game/data/HSPFoundation` - our module scripts, mission, terrain assets, forest data, and level-owned terrain/river setup
- `HSPFoundation/BaseGame.cmake` - project-specific CMake hook

Archived old work lives in:

`old_work/unreal_prototype`

Current foundation status:

- First-person player starts reliably
- Real Torque3D `TerrainBlock` is visible and collidable
- Terrain has persistent `.ter` data with slopes/elevation
- Placeholder forest instances render through Torque's forest pipeline
- River water behavior exists through `River`
- Blue stream visibility uses a `DecalRoad` visual overlay on top of the real river
