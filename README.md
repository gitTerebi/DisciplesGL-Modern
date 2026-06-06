# DisciplesGL Modern

Modern maintenance build of DisciplesGL for Disciples II.

## What We Fixed

- Restored the project so it builds cleanly with Visual Studio 2022.
- Fixed include and library paths in the Visual Studio project.
- Added the missing libpng and zlib headers required by the source tree.
- Fixed the DLL base address so `C4dll-R.dll` does not collide with Disciples II runtime DLLs.
- Added `built.bat` for one-command build and copy into the game folder.
- Verified the bundled `CB63.LIB`, `SHW32.LIB`, and `hooker.lib` match the expected runtime DLL exports.
- Added runtime debug logging to help trace future crashes.
- Fixed guarded game-speed hook behavior that could crash during play.
- Kept HD mode working with full hooks enabled.
- Disabled only the unstable `trans_npc` transition hook, which caused crashes after battles when returning to the map and starting another battle.
- Confirmed all other deep hooks can stay enabled.
- Supports skipping startup intro movies with `Intro=0` in `Disciple.ini`.

## Included Folders

- `src` - DisciplesGL source.
- `HookerLib` - matching HookerLib source and bundled hooker artifacts.

## Build

Run:

```bat
built.bat
```

The script builds `src\DisciplesGL\DisciplesGL.vcxproj` as Release Win32 and copies `C4dll-R.dll` to:

```text
G:\games\Disciples 2
```

## Notes

- `CB63.dll` and `SHW32.dll` are game runtime dependencies and should stay in the game folder.
- Import libraries are kept under `src\lib`.
- The unstable `trans_npc` transition hook is skipped; all other deep hooks remain enabled.

## Release DLL

The release package contains `C4dll-R.dll`. Copy it into the Disciples II game folder.
