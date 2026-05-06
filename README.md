# Portal Mini

**Portal Mini** is a custom 10-level 2D platformer puzzle game built in C++ using the `WinBGIm` graphics library. It features advanced geometry collision, state machines, sprite mirroring, and complex mechanical interactions—including teleportation portals.

## Features & Mechanics

- **10 Unique Levels**: Ranging from simple platforming to complex timing puzzles and wind zones.
- **Portals**: Teleport instantly across the map. Link chains of portals to traverse impossible gaps.
- **Dynamic Geometry**:
  - `PT_NORMAL`: Static solid structures.
  - `PT_MOVING`: Elevators and ferries using linear interpolation for smooth movement.
  - `PT_FAKE`: Destructible platforms that disappear when stood upon.
  - `PT_ONEWAY`: Pass-through platforms.
  - `PT_WIND`: Directional forces modifying player inertia.
  - `PT_TIMED_SPIKE`: Oscillating hazards.
  - `PT_SPIKE`: Immediate lethality on contact.
- **Characters**: Play as Dokja or Soyoung. The game dynamically loads animated sprite sequences, resolving missing frames and flipping sprites mathematically for left-facing actions.
- **Double-Buffered Rendering**: Ensuring a 60 FPS flicker-free gaming experience.

## Controls

- `A` / `D` - Move Left / Right
- `SPACE` - Jump
- `ESC` - Pause / Return to Main Menu
- `ENTER` - Confirm Menu Selections

## Architecture & Code

The codebase uses a robust FSM (Finite State Machine) to manage menus, character selection, level selection, and active gameplay. The primary structures are:

- `struct Player`: Maintains spatial data, physics (velocity/inertia), hitboxes, and animation frames.
- `struct Portal`: Bounds-based trigger zones that seamlessly update player spatial coordinates.
- `struct Platform`: A unified AABB object using enum-driven flags (`PlatformType`) to differentiate between solid geometry, hazards, and elevators.

## Installation & Compilation

1. Ensure you have a C++ compiler capable of running the **BGI / WinBGIm** graphics library. (Dev-C++ or Code::Blocks are common choices).
2. Clone this repository: `git clone https://github.com/SHUKLASHRI/portal_mini.git`
3. Compile `portal_mini.cpp`.
4. Ensure the `assets/` folder (containing the character sprites) is kept in the same directory as the resulting `.exe` so the file-stream paths can resolve properly.

## Assets

This project contains custom character animation frames mapped in the `assets/` folder. The rendering pipeline contains a helper script `getCheckedSpritePath()` to handle fallback scenarios if custom animations are missing.
