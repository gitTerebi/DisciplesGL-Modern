# Handoff: 4K rendering rewrite

The DLL is C4dll-R.dll, built from DisciplesGL-Modern for Disciples 2.

## Ground rules
- **Repo:** `C:\Users\somtam\repo\DisciplesGL-Modern`. The last commit before this brief is `471ea3c`, and the working tree was clean.
- **Scope:** v2.02b (`G:\games\Disciples 2`) and RotE v3.01b (`G:\games\Disciples 2 - Rise of the Elves`) only.
- **Build:** run `cmd /c "C:\Users\somtam\repo\DisciplesGL-Modern\build.bat"`. It builds Release, copies the DLL into all four game folders and runs a D2MT check. If a copy fails because a game is running, say so.
- **Testing:** never launch, drive or close the game. The user tests.
- **Comments:** none by default, only for a non-obvious why.
- **Git:** don't commit unless asked.

## Problem
`src/DisciplesGL/ImageReplace.cpp` swaps in 4K art by **matching**. It watches the game's 1x draws, guesses which image reached the screen, skips it, and paints the 4K version on top at the end of the frame. Each fix adds another special case, and the bugs keep coming:
- **Match fails:** the 1x image shows through.
- **Popups:** they sit under 4K art, because the stacking order between 1x draws and 4K images gets rebuilt wrong.
- **Turn order:** faces appeared in the wrong places. The strip was followed through scratch buffers by address, and those addresses get reused for other art.
- **Black areas:** they appear where the game reads the screen back. The black box at the battle roster hover spot was fixed in `OpenDrawSurface.cpp` (BltFast, around line 912) by no longer skipping the 1x blit when an image is claimed.

## Goal: PS1-emulator method (DuckStation-style upscaled video memory)
The PS1 builds its picture by copying rectangles around a flat 2D video memory, as D2 does with its blits. Emulators keep an upscaled copy of that memory on the GPU and repeat every operation there.
- **Twins:** keep a GPU texture at N× scale for every game surface: the screen, the back buffer and scratch surfaces.
- **Blits and fills:** repeat them between the twins.
- **Known images:** draw the 4K art into the twin at the source, when a known image or face is decoded or drawn.
- **Unknown 1x writes:** upload those pixels into the twin, upscaled.
- **Tile check:** each frame, compare each 1x buffer with its twin in tiles, and refresh any mismatched tile from 1x. Writes that were never hooked then fix themselves, and the worst case becomes one image at 1x, never black or wrong art.
- **Present:** show the screen's twin.
- **Look up:** how DuckStation handles CPU writes and reads of video memory. That's the same problem as D2's direct pixel writes and its save/restore behind hover areas.

## Keep
- Pack loading, WebP decoding, texture streaming and the GPU memory budget.
- Entry and name lookup, and the image store.
- The face hooks and ground hooks, and the recorded ground ops.
- The address tables in `Hooks.cpp` and `Hooks.h`.
- TextReplace.

## Replace
- Claim and Expect.
- Cover and the hole-punch plane.
- The Strip* functions.
- ClaimStore and ClaimGround matching.
- The stale checks.

## Turn order
`src/DisciplesGL/TurnOrder.cpp` is plain 1x on purpose: `faceFrame = -1` and an empty `OnStrip`. Under the new method, it should draw 4K faces and gems straight into the strip surface's twin, and the game's own copies carry them to the screen.

## Steps
The user builds and tests each step before the next one starts.
1. **1x twins:** twins that hold only 1x content. The game must look identical to plain 1x. Add F12 counters.
2. **Tile check:** the per-frame compare and refresh.
3. **4K at the source:** backgrounds and unit sprites first, then portraits, then the turn strip.
4. **Map ground:** reuse the recorded ground ops.

## Risks
- **Speed:** the game makes thousands of small blits per frame, so the twin operations need batching on the GPU. Step 1 will show quickly whether this is a problem.
- **Renderer:** the method needs the OpenGL renderer. The GDI renderer stays 1x.

## First task
1. Read these:
   - `OpenDrawSurface.cpp`: Blt, BltFast, Lock/Unlock and the fills.
   - The GL present path.
   - Every pixel-copy hook in `Hooks.cpp`: the `OnPixelCopy`/`OnPixelBlit` callers, DrawFaces, FillColor and the ground hooks.
2. Write a concrete design for step 1.
3. Wait for the user's go-ahead before changing any code.

## Known leftovers in the code
- **F12 item dump:** the "image items" dump in `ImageReplace.cpp` (DumpItems) is a temporary diagnostic.
- **MapOrder:** its F12 map dump is still in, and option A is still active. The forest ordering change didn't fix the hidden unit head.
- **`Hooks.cpp` FillColor (~3221):** the `if` has no braces, so only the first call is inside it, despite how the indentation looks. This was already in the code before this work.
