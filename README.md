# DisciplesGL Modern

Modern maintenance build of DisciplesGL for Disciples II.

## New Features

- Separate speed settings for map and battle (menu options).
- WASD map scrolling, with a menu option to toggle it.
- Window maximizes on game start.
- Autosave archive: keeps the last 15 turns as rolling saves.
- Filtered (smooth) background rendering in HD and wide modes.
- Fixed filtered transition artifacts.
- Fixed fullscreen and hotkey handling.
- Fixed saving under the Rise of the Elves expansion.

## What We Fixed

- Restored the project so it builds cleanly with Visual Studio 2022.
- Fixed include and library paths in the Visual Studio project.
- Added the missing libpng and zlib headers required by the source tree.
- Fixed the DLL base address so `C4dll-R.dll` does not collide with Disciples II runtime DLLs.
- Added `build.bat` for one-command build and copy into the game folders.
- Verified the bundled `CB63.LIB`, `SHW32.LIB`, and `hooker.lib` match the expected runtime DLL exports.
- Added runtime debug logging to help trace future crashes.
- Fixed guarded game-speed hook behavior that could crash during play.
- Kept HD mode working with full hooks enabled.
- Disabled only the unstable `trans_npc` transition-speed hook to prevent post-battle map crashes while keeping the rest of the deep hooks enabled.
- Confirmed all other deep hooks can stay enabled.

## Included Folders

- `src` - DisciplesGL source.
- `HookerLib` - matching HookerLib source and bundled hooker artifacts.
- `dist` - ready-to-use runtime files (`C4dll-R.dll`, `CB63.dll`, `SHW32.dll`, `Imgs`, `Mods`).

## Build

Run:

```bat
build.bat
```

The script builds `src\DisciplesGL\DisciplesGL.vcxproj` as Release Win32 and copies `C4dll-R.dll` to:

```text
G:\games\Disciples 2
G:\games\Disciples 2 - Rise of the Elves
```

Edit the paths at the top of `build.bat` for your install.

## Notes

- `CB63.dll` and `SHW32.dll` are game runtime dependencies and should stay in the game folder.
- Import libraries are kept under `src\lib`.
- The unstable `trans_npc` transition-speed hook is disabled to prevent post-battle map crashes; all other deep hooks remain enabled.

## Install Without Building

Copy the contents of `dist` into the Disciples II game folder. `C4dll-R.dll` is the wrapper; `Imgs` and `Mods` are optional extras.
