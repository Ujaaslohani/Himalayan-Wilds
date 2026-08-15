# One Morning Hunt Playtest Checklist

## Setup

- Open `L_OneMorningHunt`.
- Start play-in-editor with the native `AHSPlayerCharacter`.
- Confirm the map contains the hunter marker, deer marker, practice target marker, camp rest point marker, ground, player start, sun, and skylight.

## Expected Loop

1. Face the hunter marker.
2. Confirm the HUD shows `Accept One Morning Hunt`.
3. Press `E` to accept the quest.
4. Move to the deer marker.
5. Aim at the deer and left-click once.
6. Confirm the deer becomes harvestable and the HUD shows `Harvest Deer` when focused.
7. Press `E` to harvest the deer.
8. Return to the hunter marker.
9. Confirm the HUD shows `Return One Morning Hunt`.
10. Press `E` to complete the quest.
11. Confirm a short HUD completion message appears.
12. Move to the camp rest point marker.
13. Confirm the HUD shows `Rest until evening`.
14. Press `E` to end the morning.
15. Confirm a short HUD completion message appears.

## Current Expected State Changes

- The player receives deer meat and deer hide in native carry state after harvest.
- The hunter quest handoff enters completed state after return.
- Required deer meat and deer hide are consumed on quest turn-in.
- The player progress component marks `Quest.OneMorningHunt` complete.
- The camp rest point marks `Milestone.OneMorningHunt.DayEnded` complete.
- The player settlement unlock component grants `Building.BasicDryingRack`.

## Current Non-Goals

- No bow mesh, projectile, ammo, animation, or animal AI.
- No inventory UI, quest log UI, save/load, building placement, time simulation, or sleep/rest system.
- No historical art pass beyond native prototype markers.
