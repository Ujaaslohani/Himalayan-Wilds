# Research Resources

## Available Installed Skills

Relevant to this project:

- No Unreal Engine-specific installed skill is available in this environment.

Potentially relevant later:

- `frontend-design`: useful later for HUD, dialogue, inventory, settlement/building UI, and menus.
- `ui-ux-pro-max`: useful later for UI/UX accessibility, layout, responsive behavior, and interaction quality.
- `playwright` / `agent-browser`: not relevant to Unreal C++ gameplay implementation; useful only if a web UI or browser-based workflow is introduced.

Not relevant to the current milestone:

- `imagegen`
- `pdf`
- `openai-docs`
- `plugin-creator`
- `skill-creator`
- `skill-installer`
- `find-skills`
- `vercel-react-best-practices`

## Unreal Resources Consulted

Primary source: official Epic Developer Community Unreal Engine documentation.

Relevant documentation areas:

- Enhanced Input
- Gameplay Framework
- Gameplay Tags
- Data Assets and Primary Data Assets
- Asset Manager
- AI Perception
- Behavior Trees and Blackboards
- StateTree
- Navigation System
- Environment Query System
- World Partition
- Data Layers
- HLOD
- Landscape
- Foliage
- PCG
- Animation Blueprints
- Control Rig
- SaveGame
- Automation Test Framework and C++ automation tests
- Unreal Insights and stat commands

## Current Local Engine Status

Unreal Engine 5.5.4 was found during follow-up inspection:

- Install path: `C:\Program Files\Epic Games\UE_5.5`
- Editor: `C:\Program Files\Epic Games\UE_5.5\Engine\Binaries\Win64\UnrealEditor.exe`
- UnrealBuildTool: `C:\Program Files\Epic Games\UE_5.5\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe`
- Changelist: `40574608`
- Branch: `++UE5+Release-5.5`

The project now uses `EngineAssociation` value `5.5`.

## Current Local Toolchain Status

UnrealBuildTool runs, but Win64 project-file generation is blocked because no valid Visual C++ toolchain was found.

Required by UnrealBuildTool:

- Visual Studio 2022 17.8 or later
- MSVC v143 C++ x64/x86 build tools, version 14.38.33130 or compatible
- Windows SDK selected through Visual Studio Installer
