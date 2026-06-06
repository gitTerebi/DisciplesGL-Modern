# DisciplesGL Modern

Modern maintenance build of DisciplesGL for Disciples II.

## Fixes

- Builds with Visual Studio 2022.
- Fixes include and library paths.
- Adds missing libpng/zlib headers needed by this source tree.
- Fixes DLL base address conflict with Disciples II runtime DLLs.
- Adds `built.bat` for one-command build and copy.
- Keeps HD mode working.
- Skips the unstable `trans_npc` transition hook that can crash after battle exit.
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
