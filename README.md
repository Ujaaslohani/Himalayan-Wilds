# Himalayan Wilds — C++ edition

A native C++17 / OpenGL 3.3 port of the game. There's no engine: everything (renderer, terrain, water, sky, foliage streaming, AI, economy, UI, audio synth) is in `src/`. The only dependencies are small libraries vendored in `third_party/`: GLFW, glad, stb, cgltf and miniaudio.

## Prebuilt
`bin/windows/HimalayanWilds.exe` (Windows x64, static, no DLLs) and `bin/linux/HimalayanWilds` (x86-64) are included. Drop your GLBs into `assets/models/` and run one. The game finds `assets/` two levels up automatically.

## Build

**Windows, Visual Studio 2022**
```
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```
The executable is at `build/Release/HimalayanWilds.exe`.

**Windows, MinGW (or cross-compiling from Linux)**
```
cmake -B build-win -DCMAKE_TOOLCHAIN_FILE=toolchain-mingw.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-win -j
```
This produces one static `HimalayanWilds.exe` with no extra DLLs.

**Linux**
```
sudo apt install cmake g++ libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl-dev
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## Folder layout to ship
```
HimalayanWilds(.exe)
config.ini
assets/
  fonts/Cinzel-Bold.ttf
  fonts/NotoSans-Bold.ttf
  models/*.glb        <- your low-poly GLBs (list below)
  music/*.mp3         <- optional; the first file loops as background music
```
The game looks for `assets/` next to the executable, then in the working directory, then one or two levels up. You can also pass `--assets <path>`. A GLB can go directly in `assets/`, in `assets/models/`, or in `assets/low poly/`.

GLB files the game loads:
- Trees: `Tree.glb`, `Tree-QVOop92WmG.glb`, `Tree-aVOxaHRPWe.glb`, `Tree-qZtx0AHhcy.glb`, `Tree-t9KbsfYdXz.glb`, `Pine.glb`, `Pine-699sFuLCN2.glb`, `Pine-79gmlLnweB.glb`, `Pine-Zt62gceKXZ.glb`, `Pine-rfnxJv0Rqa.glb`, `Twisted Tree.glb`, `Twisted Tree-7PDBpElkQr.glb`, `Twisted Tree-8oraKn9m0x.glb`, `Twisted Tree-9aWlx82xUf.glb`, `Twisted Tree-GVTsMmuzv7.glb`
- Undergrowth: `Fern.glb`, `Plant.glb`, `Plant Big.glb`, `Plant-xH5gNlQxAZ.glb`, `Bush.glb`, `Bush with Flowers.glb`, `Flower Group.glb`, `Flower Group-LqTljN6Wg2.glb`, `Flower Single.glb`, `Mushroom.glb`, `Mushroom Laetiporus.glb`
- Temple garden: `Flower Petal.glb`, `Flower Petal-LqvxG9OBOU.glb`, `Flower Petal-eVE0j49ux9.glb`, `Flower Petal-niuBUEJdvM.glb`, `Flower Petal-tzG4JcqYWs.glb`
- Rocks: `Rock Medium.glb`

If a file is missing, the game still runs: that prop is skipped and the console prints the missing name. Grass, houses, people, animals, the bow, the temple and the water are all built in code.

## Controls
| Key | Action |
|---|---|
| WASD, Shift, C, Space | Move, sprint (loud), crouch (quiet), jump |
| Left mouse (hold/release) | Draw and loose the bow |
| Right mouse | Aim |
| E | Gather / talk / harvest |
| R | Craft 10 arrows (rotates while placing) |
| Tab, J, B, M | Pouch, Journal, Build, Map |
| Q / E, 1–6 | Rotate and restyle while placing |
| Esc | Close panel, or pause |
| F11 / F3 / F2 | Fullscreen / performance overlay / screenshot |

The game autosaves to `%APPDATA%\HimalayanWilds\save.json` on Windows and `~/.local/share/HimalayanWilds/save.json` on Linux.

## Performance and memory
- The terrain is a CDLOD quadtree: one shared 16×16 patch mesh, a float height texture and geomorphing, so it costs very little vertex memory.
- Foliage streams in 16 m cells (an LRU of 800) as compact 20-byte instances, and dense layers fade out by distance.
- GLB textures are deduplicated and BC1-compressed on worker threads, then the CPU copies are freed.
- A typical frame is about 70–100 draw calls and 0.85 M triangles.
- `adaptive_quality` shrinks the grass and undergrowth radius if the frame rate stays under 50 fps, and restores it once the game runs smoothly again.
- The settings with the biggest effect are `grass_radius`, `undergrowth_radius`, `shadow_size`, `msaa`, and `vsync = false` (which uncaps the frame rate).

## Command-line options
`--windowed`, `--width N`, `--height N`, `--msaa N`, `--novsync`, `--assets DIR`, `--memlog`. Use `--selftest DIR [--shots a,b]` to run scripted camera shots and write PNGs, for regression checks.

## Where things live (for customising)
| Area | Files |
|---|---|
| Terrain shape, river spline, lakes, trails, summits | `src/world/terrain.cpp` |
| Foliage layers, density rules, LOD radii | `src/world/forest.cpp` |
| Ground, water and sky shaders | `src/render/terrain_render.cpp`, `water.cpp`, `sky.cpp`, `glsl.h` |
| Houses and their style options | `src/models/houses.cpp` |
| Animals, villagers, buildings, bow | `src/models/models.cpp`, `people.cpp`, `temple.cpp` |
| Wildlife AI, predators, raids | `src/game/wildlife.cpp` |
| Economy, jobs, trades, gear | `src/game/economy.cpp` |
| Quests, items, specialists, save/load | `src/game/app.cpp`, `engine.cpp` |
| Seasons, weather and wind | `src/game/season.cpp` |
| HUD and all menus | `src/ui/screens.cpp` |
| Sound effects, ambience, bell | `src/audio/audio.cpp` |
| Window, input, config keys | `src/platform/platform.cpp` |
