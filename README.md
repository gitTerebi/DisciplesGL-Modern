# DisciplesGL Modern

A maintained build of the DisciplesGL OpenGL wrapper for Disciples II.

## Features

- **Keybinds** (Options > Keybinds): rebind any game hotkey on the map, town, army, spell and battle screens, plus the wrapper's F-key shortcuts. Each bind only works on its own screen, so one key can do different things on the map and in battle.
- **Popup key**: one key (for example Space) accepts popups and menus, backs out of full screens such as the city screen, and does nothing on the bare map.
- **Separate speeds** for the map and for battles.
- **Maximized window** on start, sized to the game's shape.
- **Autosave archive** that keeps the last 15 turns as rolling saves.
- **Smooth (filtered) background** in HD and wide modes.

## Fixes

- Leaving a menu or battle with upscaling on no longer leaves part of it on the map.
- The map redraws fully after a battle.
- Saving works under the Rise of the Elves expansion.
- Fullscreen switching and transition artifacts are fixed.
- The unstable `trans_npc` transition-speed hook is disabled to stop post-battle map crashes. All other deep hooks stay on.
- `C4dll-R.dll` uses a base address that doesn't collide with the game's runtime DLLs.

## Install without building

Copy the contents of `dist` into the Disciples II game folder. `C4dll-R.dll` is the wrapper; `Imgs` and `Mods` are optional. Keep `CB63.dll` and `SHW32.dll` in the game folder, since the game needs them.

## Build

Requires Visual Studio 2022. Run:

```bat
build.bat
```

It builds `src\DisciplesGL\DisciplesGL.vcxproj` (Release, Win32) and copies `C4dll-R.dll` to the game folders listed at the top of `build.bat`. Edit those paths for your install; the script stops without copying if any listed folder is missing.

## Repository layout

- `src`: DisciplesGL source. Import libraries are in `src\lib`.
- `HookerLib`: the matching HookerLib source and prebuilt hooker files.
- `dist`: ready-to-use runtime files.

The wrapper writes a debug log, `DisciplesGL-debug.log`, next to the game executable.
