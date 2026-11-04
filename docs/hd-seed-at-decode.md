# HD twins: seed at decode

This page describes the `seed` mode of the twin renderer. It replaces the old way of deciding which 4K art a copy is: expectations from the render call, identified regions, and pixel hashes in `twin-index.bin`. In seed mode, 4K art is written into a twin only at the moment named 1x art is decoded into game memory. That is the DuckStation-style "replacement at upload". From then on, every copy, pixel op, cache, drag image and popup is a plain twin replay, so the 4K travels with the pixels and nothing downstream identifies anything.

`docs/hd-twin-architecture.md` describes the twin machinery that both modes share.

## Switch

`disciple.ini`, section `[Twin]`:

| Key | Values | Default |
|---|---|---|
| `Mode` | `seed`, `match` | `seed` |
| `SeedWait` | ms the game thread may wait at one decode for the pack worker to decode its art (0 to 2000). 0: it never waits, the seed is deferred and lands when the art arrives (see Streaming) | 0 |
| `SeedLoadWait` | ms a decode may wait while the game is loading, that is when no present came for more than 100 ms (0 to 2000) | 100 |
| `WriteTrap` | `1` keeps the pages of every offscreen surface read-only until the game's first write, to name the code that writes it (see First-write trap); `0` off | 1 |
| `TrapCopy` | `1` replays a trapped `rep movs` from one twin into another as a twin copy when the 1x rows are equal (see Map view) | 1 |
| `LoadSeed` | `1` seeds an image the game converts from indices outside `StoreDraw` when its size equals an image the store just loaded by name (see Map view); `0` only logs the match | 0 |
| `GpuCheck` | `1` runs the GPU check every 5 s and takes the screenshots 15 s after the first seed, as before; `0` only on F12 | 0 |

The environment variable `DGL_TWIN_MODE=match` overrides the ini. The first log line names the mode and where it came from: `twin mode seed (Twin.Mode=seed, seed wait 0 ms)`.

`match` is the old path and is unchanged, so the two modes can be compared. It is due for removal in the next round.

## Seed points

All seeds go through `ImageReplace.cpp:AddRange`. That is the one place where named 1x art is known to sit at a known address, pitch and size. It calls `SeedStore`, which calls `Twin::SeedArt`.

| Source | Where it enters game memory | Seed |
|---|---|---|
| Store image (`StoreLoad` / `StoreDraw`) | `ImageReplace.cpp:StoreDraw`, after the game decoded the file into `desc` (data, rect, pitch) | `SeedStore`: when the file is a packer sheet, a sheet seed (below). Otherwise one piece, the pack frame the file name resolves to (`StoreFrame`), over the whole image |
| Packer sheet behind `Build`/`Render` images | Loaded through the image store too: the sheet PNG is a file of the `.ff`, decoded by `StoreDraw`. Evidence: `_CUDEFAUL` is a `Build` image with one 24x54 part, and its store decode resolved to the same pack frame | `ImageReplace.cpp:SeedSheet` builds the sheet from the archive's `-INDEX.OPT`, `-IMAGES.OPT` and `-ANIMS.OPT` (`LoadSheets`) |
| Face | `ImageReplace.cpp:FaceLoad` decodes it, but the stride is unknown there; `OnFaceCopy` registers it at its first `DrawFaces` with the real pitch. That happens before the hook's twin calls | `SeedStore`, kind `face`, one piece |
| Ground tile and border textures (`TileFactory` / `BorderFactory`) | Not seeded | Nothing replays from them; see Ground |

`StoreDraw` now reseeds on every call in seed mode. The match path returned early when the same image was drawn into the same memory again, but a repeat decode through a hooked routine marks the memory dirty, and that drops the earlier seed. A repeat decode over the image's own range keeps the range and its twin (`AddRange`, `rangeW`/`rangeH`). If no hook saw a write since the last seed there, the seed is skipped as still intact (`repeat decodes still intact`).

`Twin::SeedArt` (`Twin.cpp`):

1. It finds the twin that holds the memory: a surface twin, or a memory twin made from the store range by `Adopt`. A memory twin whose geometry no longer fits is replaced (`DropOverlapping`).
2. It makes a seed record (twin, rect, pieces).
3. It waits up to `SeedWait` ms for the pack worker to decode the frames (`ImageReplace::ArtState`).
4. It emits the seed (`SeedEmit`):
   1. drain the mirror syncs, then cover and flush the dirty decode;
   2. upload the 1x with the seed key encoding (`Upload(..., seed = TRUE)`);
   3. record a `CmdSeed` with the ready pieces;
   4. `AddArt` the rect, so the GPU check treats it as 4K.

The render thread (`ReplaySeed`) uploads a decoded but not yet uploaded frame on the spot (`ImageReplace::ArtUpload`).

**Deferred seeds.** Pieces whose art isn't decoded in time stay pending. `SeedPoll` runs at every present and emits them once they are ready. It does so only for the pieces still intact. A piece is dropped by any write the twins see in its rect, and the record when no piece is left or the write covers it: `MarkDirty`, fills, copies or pixel ops into it, `Repair` from the tile check, `GroundArt`, `OnWriteAll`, or the release or eviction of the twin. The record itself is the seeded flag. Its generation is "no write seen since the decode".

After a rebuild (GL release, queue overflow), Ctrl+F11 or Ctrl+F10, every intact record is queued again, because its 1x provably hasn't changed. `SeedPoll` spreads these redraws over frames (at most 24 MB of list arena per frame), so a rebuild can't cascade into queue overflows. A record pending for more than 3600 frames expires.

## Transfers: from the decode buffer into the surface

### What the first seed run showed

The 14:58 test (commit `03ccdc6`) seeded 120 sheet decodes, but about 97% of copied pixels were still 1x. Every top 1x-only copy source was a DirectDraw surface with no store name (232x222, 350x397, 448x448, many 128x128). Only 2 seed records and 2 memory twins were left.

The code and the logs that survived show why:

- **`desc` is the game's `BlitObject`.** It has data at +4, the clip rect at +8 and the pitch at +0x18, the same layout as `Hooks.h:BlitObject`. In every logged run the store decodes into one reused buffer, `base=7E7A0040 pitch 576` (the 13:47 log, `twin memory twin … base=7E7A0040`). That buffer is not a DirectDraw surface: none of the 137 surfaces is 576 wide.
- **The decode buffer is written by code no twin hook sees.** Its memory twin was never marked written between decodes (`overwritten after seeding 0`, `dropped 0`). Each new decode replaces the range (`AddRange` → `DropRange` → `OnMemoryFree`), and that destroys the memory twin and its seed without counting it. That is why `image memory dropped=124` but `twin seed records 2`.
- **Nothing ever copied out of it through a hooked op.** No `twin trace` line in any log has the decode buffer's memory twin as a source (`TraceEnd` logs every early copy or pixel op that touches a memory twin), and `addr fallback hits` is 0, so there is no `Blt` from a surface that wraps it.
- **The surface is created first.** It has the decoded size, with the width rounded up to even: 123x128 is decoded into a 124x128 surface, 43x97 into 44x97. In the 13:47 log `twin create 5 24x54 buffer … data 24CE69E0` comes just before the decode's memory twin at `7E7A0040`.
- **That surface's twin gets plain 1x.** Its create-time full dirty rect is uploaded lazily at the first `BltFast` out of it, after the game's CPU wrote the pixels. That upload is the only thing the twin ever sees.

So the game creates a surface for the texture, decodes the art into its scratch `BlitObject` (`StoreDraw`), and then writes it into the surface itself, by CPU. The exact routine is not known yet, because no hook sees it, but `Lock`/`Unlock` on the surface do (`OpenDrawSurface::Lock`; `Unlock` is now overridden). The next run's arrival lines say which path it was (see Logs).

### What the second run showed (15:32, commit `3a4c499`)

Most of the map and sprites were still 1x: `twin coverage 5s` had 53M px from sources carrying 4K against 1021M px from 1x-only twins. Two findings:

- **Every transfer failed its guard** (`transfer done ... 0`, `guard failed 406 surfaces`). The likely target was there: `surface created before the decode: decode _CUDEFAUL.PNG 24x54 -> twin 5 24x54 at 0,0 ... px 0,0 holds FF5F6563, decode 000000B5`. The 43 failures "via unlock" all named twin 4, the 1152x1152 ground atlas (lock caller `006699AF<0066DFCD`, locked millions of times), which is plainly not a target.
- **The 1x-only surfaces are written with no lock the twins see.** Every `twin arrival top 1x-only` entry (the 640x600 and 640x576 map buffers, 274x397, 320x281, 218x218 sprites, the 448x448 and 512x512 pages) reads `up 1/<full size> ... lk 0/0`: the create-time lazy upload and no lock. The game writes through a pointer it kept, so the twin only sees the 1x at the first `BltFast`.

### The snapshot was read in the wrong format

`StoreDraw`'s `desc` is a game `BlitObject` (`Hooks.h`): format byte at +0 (`isTrueColor`), data at +4, clip rect at +8, pitch field at +0x18. The game uses that pitch field two ways (see `DrawMinimapObjects`, `FillColor`): for a true-colour object the row stride is the field / 2 pixels of 32 bits (the 16-bit byte pitch reused with 32-bit pixels, because `bpp32Hooked` makes every 16 bpp surface 32 bpp in `StateBuffer` and `OpenDrawSurface::CreateBuffer`), for an 8-bit object it is the field in bytes. `StoreDrawn` ignored the format byte and always read 32 bits at field / 2.

The logged values fit 8-bit palette indices read as DWORDs, not 32-bit pixels and not 565:

| Decode | Surface at 0,0 | Snapshot DWORD | As 565 (`ConvertToRGB`) | As four indices |
|---|---|---|---|---|
| `DLG_WAIT_HOURGLASS` | `FFFF00FF` (key) | `00000000` | black | index 0, the transparent entry |
| `_CUDEFAUL` | `FF5F6563` (grey) | `000000B5` | `(0,20,173)` blue | index B5 then three 0 |
| `WATERBKGND1` | `FF484A1C` | `CA997264` | `(115,77,33)` | four different indices of a water texture |

So the snapshot is suspected (not yet confirmed) to be the 8-bit decode before the palette conversion. That also explains the even-width surfaces: the game converts the indices into its own 32-bit surface, likely through `Pixel_Blit_Indexed_to_565` (`Hooks.cpp`, the first entry of the replaced pixel function table, which takes the source indices, pitch and palette) or an unhooked loop. The next run's `twin seed decode #n` lines print the format byte, pitch field and first bytes of each decode, which settles it.

### The guard compares like with like

`XferSnapshot` now keeps the decode's raw bytes, read from the `BlitObject` that `StoreDraw` passed (`Twin::DecodeSource`), so the snapshot can be read as 32 bpp (rows at field x 2 bytes), 16 bpp or 8 bpp (rows at the field). `XferGuard` compares the surface at the provenance's place in every reading:

| Format | Compare |
|---|---|
| `rgb32`, `bgr32` | RGB exact; a key pixel in the surface matches the decode key or alpha 0 |
| `565` | `Hooks::Convert565toRGB`, the wrapper's own conversion with its rounding and key mapping; exact, and also counted within 8 per channel |
| 8-bit indices, consistent colours | every index maps to one surface colour (key counted as one colour), and the image has more than one. No palette is needed |
| 8-bit indices through the blit's palette | when the hooked indexed blit read this decode, its palette converted exactly as the blit does (`palette \| alpha`, `COLORKEY_AND` test to the key) |

Acceptance (`GuardVerdict`):

1. **Exact**: some format holds every row.
2. **Tolerant**: only for strong provenance, a surface created within the decode's window or whose first trapped write followed it, with exactly the decode's size (width or width + 1). At least 90% of the rows must match in `565` within tolerance, or in one of the indexed readings, and only at the final check.
3. Rejected otherwise, with a reason: the surface is still blank, no format holds the decode, or most rows match but the provenance is not strong enough. Zeroed memory never confirms anything.

The guard is only a safety check. The candidate always comes from provenance.

**Unlock path.** An unlock of a surface more than 4 times the decode's area is no candidate unless its lock rect has exactly the decode's size (`XferLarge`). This removes the 1152x1152 ground atlas, which is locked constantly.

**Indexed blit.** `Pixel_Blit_Indexed_to_565` now reports its source to `Twin::OnIndexedBlit`. When the source lies inside the decode's 8-bit reading at its pitch, the provenance is exact. The blit's palette converts the snapshot, and the blitted part of the decode (all of it, or one frame of a sheet with the pieces clipped to it) is seeded at the blit's destination. A blit inside the decode call is judged when the call returns. Blits into the decode buffer itself are counted separately: if they happen, the decode is the conversion's output and not its input.

### First-write trap

Offscreen surfaces get their own pages: `StateBuffer(size, TRUE)` allocates with `VirtualAlloc` when `bpp32Hooked` and `WriteTrap` are on, from `OpenDrawSurface::CreateBuffer`. The screen buffers (`StateBufferAligned`) and memory twins are never trapped.

- **Arm.** At `OnCreate` every page of the new surface twin becomes `PAGE_READONLY` (`TrapAttach`). It is re-armed at `OnPresent` (`TrapRearm`) when one of its pages was opened since. A surface the game wrote in that period is re-armed every second; one only the wrapper wrote backs off up to 16 s. At most 2048 pages are armed per second, round robin. After 400000 faults in all, nothing is armed again.
- **Our own writes.** Each write the twins hear about first (`SeedWrite`, which every hooked op, copy, fill and `MarkDirty` calls) opens the pages of its rect with one `VirtualProtect` (`TrapOpen`). On a fresh surface those pages are armed again at the next draining twin call, after the write finished (`TrapReopen`, at most 64 times per surface), so the game's first write still traps. Writes of ours without that announcement fault once per page, and the handler opens them silently: EIP in DisciplesGL's code, or EIP outside the game exe with the first return address on the stack in DisciplesGL (a CRT copy we called).
- **Handler.** `TrapHandler` is added first in the vectored chain (`AddVectoredExceptionHandler(1, ...)`), so it runs before any frame handler and the crash filter. It only takes a write access violation inside a trapped range. Anything else, a read, an address outside every range, a page that keeps faulting after it was opened, goes on to the frame handlers and `CrashFilter` as before. For a trapped write it:
  1. opens that one page;
  2. records EIP, the first return address found on the stack (scan of 32 DWORDs, code ranges only, preceded by a call), 4 return addresses from an EBP walk bounded by the thread's stack, the twin and the fault address;
  3. records whether the game thread was inside a decode (`StoreDraw`), inside `Render`, under one of our hooks, or another thread wrote;
  4. continues.

  It allocates nothing, takes no lock and never logs. Events go into a preallocated 4096-entry ring, which the game thread drains at every twin call (`TrapDrain`); module names (`CrashModule`, as the crash handler prints them) are looked up there.
- **Aggregation.** Writers are kept by EIP (up to 256): count of pages opened, distinct twins, a sample twin and x,y, and how many writes were inside a decode, inside Render, under a hooked op, on another thread, or the first write into a fresh surface. Each new EIP is logged at once; the top 20 every 5 s.
- **Transfer provenance.** The first game write into a fresh surface during or right after a decode (same decode, at most 2 frames later) means that surface receives that decode. It becomes a watch `first write into a fresh surface after the decode (trap)`, at 0,0 for a surface of the decode's size or at the first written pixel otherwise. A write inside the decode call is judged when the call returns. The guard confirms it like any other candidate, and the tolerant compare applies.

The arrival line now ends with `tr <n> <eip>`: the trapped game writes into that twin and the last writer.

- **Priority.** The top 1x-only copy sources of the last coverage list (up to 8 surfaces) are re-armed at every present, outside the 2048-page budget (their own budget is 16384 pages a second), and their quick re-arm after our own write is never skipped for the budget. A priority surface that has faulted 4096 times is "hot" and drops to once a second, so a surface the game writes every frame cannot fault-storm. Each new writer and twin pair on a priority surface is logged (`twin trap priority write #n`) with EIP, the export it is in for code outside the game, caller, return addresses, the instruction bytes, ESI and EDI resolved to a twin and pixel or a store image, ECX, EAX, EBX, EDX and the context.
- **Trapped writes mark the 1x.** A trapped game write marks the rows of its page dirty, as a hooked write would, so a surface written by an unhooked CPU loop now gets its 1x instead of keeping a stale twin.
- **Quick re-arms skipped for the budget** are counted (`twin trap priority` line).

### Transfer seeds

The 4K follows the pixels by provenance: the latest named decode, and the lock, write, first trapped write, indexed blit or surface creation that followed it, pick the image and its place. The pixels are never searched or hashed. The guard above only confirms the one place the provenance gives.

1. `SeedArt` keeps the latest store or sheet decode (`XferCapture`): the raw bytes of its 1x, its `BlitObject` format, its pieces and conflicts, name and size. Faces are left out, because their copies are hooked `DrawFaces` copies that replay the memory twin's seed.
2. Candidates for that decode:
   - **Indexed blit from the decode** (`OnIndexedBlit`): exact provenance, place and part from the blit.
   - **First write into a fresh surface** (the trap), during or right after the decode.
   - **Unlock** of a surface twin (`OnUnlock`). "Decoded while locked" when the decode came after the `Lock`, otherwise "decoded before the lock". The place is the lock rect's origin, then 0,0. Not for much larger surfaces (above).
   - **Unlock, hooked write or trapped write inside the decode call** (`StoreDraw` brackets the game's call with `DecodeBegin`/`DecodeEnd`). It is checked when the call returns, against the decode it produced.
   - **Surface created before the decode** (`OnCreate`). The surface waits unbound (at most 2 frames) and binds to the next decode. It is checked at every later twin call, and finally at the next decode or present.
   - **Hooked write** into a surface twin after the decode (`OnWrite`, or a copy or pixel op from untwinned memory). The place is the write's top-left. It is checked at the next twin call, which is after the hook's CPU write, and resolved at the next decode or present.
3. If the guard accepts, the decode's pieces are seeded into the surface twin itself (`XferSeed` → `SeedAt`, the second half of `SeedArt`). That record lives on the surface twin, so it survives the decode buffer's reuse, defers like any seed, and is redrawn after a rebuild like any intact seed.
4. Each surface takes a whole decode once (`Arrival.xferSeq`); parts of one decode from indexed blits can land in the same surface several times. A surface that doesn't hold it is counted once per decode (`rejected`). A decode older than 600 frames is no candidate.

## Streaming

The game thread no longer waits for 4K (`SeedWait=0`). A seed whose art is not decoded yet is deferred: the 1x stays, and `SeedPoll` emits the pieces at a later present once the art is there. The only waits left are while the game is loading (`SeedLoadWait`, when no present came for more than 100 ms), at most 1 s in all between two presents.

- **Prefetch at the store load.** `ImageReplace.cpp:StoreLoad` asks for the 4K of every frame of the loaded file's sheet blocks (`PrefetchSheet`), and of a plain image's own frame (`NoteLoad`), before the game decodes it. The decode itself asks again through `ArtState`.
- **Inline uploads.** A decoded frame is uploaded on the render thread by the replay that first draws it (`SeedTexture`). `SeedEmit` lets only a budget of such frames into each present's list: about 4 ms of uploads at the rate the render thread measured (1 to 32 MB). The rest waits for the next present, or for `ImageReplace`'s own upload pump (4 ms per frame), after which they are plain GPU textures.
- **Writes since the decode.** A write into a seed record's rect now drops only the pieces it touches (`PieceWritten`). The record keeps the rest and still lands them. A write covering the whole record, or touching every piece, drops it as before. A repeat decode over a record with written pieces seeds it again.
- **Seeds carried by copies.** The store converts most art into the 1152x1152 scratch surface (twin 4) and copies it from there into the surface the game keeps. A deferred seed in the scratch would land after the scratch was already reused. So an unkeyed copy out of a rect that holds a pending seed record creates a pending record on the destination with the waiting pieces moved along (`SeedDerive`). When the source record had already uploaded its key-alpha base, the copy carried it, and the new record does not upload it again. Screen-sized targets are left out. The new record is emitted at the next present, after the CPU copy happened.

`twin seed streaming 5s` counts the deferred seeds, the copies that carried pending pieces, the late landings with their latency in frames and ms, frames still decoding, frames held back by the upload budget, and pieces dropped by writes.

## Render-thread stalls

The 16:05–16:08 run had replays of 1179 ms, 450 ms, 176 ms and several of 40–55 ms. From the code:

| Cause | Where | Change |
|---|---|---|
| The automatic screenshots 15 s after the first seed: three PNGs compressed on the render thread (16:05:52.8 to 16:05:54.0, inside the 1179 ms replay) | `Shots`, `WritePng` | PNG compression runs on its own thread (`WritePngAsync`); the automatic shot only with `GpuCheck=1`, otherwise on F12 |
| The GPU check every 5 s: a synchronous `glReadPixels` of the whole screen twin at N (3200x2400, 30 MB) | `Check` | only on F12, or every 5 s with `GpuCheck=1` |
| The shared-block conflict check: uploads both frames' 4K of every conflicting pair (frames nothing else draws) and reads back per batch of 16 | `CheckConflicts` | only after F12 (`seeds.conflictCheck`; F12 also queues the intact seeds again so their pairs are checked) |
| Inline 4K uploads of a whole sheet's frames in one replay | `ReplaySeed` → `SeedTexture` → `ArtUpload` | budget per present, see Streaming |

Every replay dispatch is now timed by command type. `twin replay per command 5s` gives the dispatches, total and max per type and the replays over 50 ms; `twin replay slowest command 5s` and `twin slow replay` (each replay over 50 ms, first 64) name the slowest command with its twin, rect, run length and the inline uploads inside it.

## Map view

### Where its pixels come from

The 1x-only map buffers (twin 82 640x600, twin 83 640x576) and the screen-sized buffers of other screens (twin 101 800x600, twin 120 676x674) each get exactly one copy in from a 1x-only twin, and nothing else (`in 0/1/0`, no writes, no locks, no trapped writes). The coverage lines say which twin: in each interval where they were filled, the 1152x1152 scratch (twin 4) was copied out twice as 1x, and the pixel counts add up exactly: 752640 = 640x600 + 640x576 (16:07:56), 935624 = 800x600 + 676x674 (16:07:51). So these big images are converted into the scratch and copied out of it in one piece, like the sprites, but nothing seeds them there.

No decode of their size passes `StoreDraw` (the largest named decode is 482x512), and the decode buffer is the 8-bit 1152-pitch surface next to the scratch (`data 0F480000, pitch field 1152`). The likely path is an indexed blit into the scratch from indices that are not the latest store decode: the guard line counts 44 such blits (`other sources`). Which images and which loader is settled by the next log (Logs: `twin indexed blit #n from other memory`, `twin copy in #n`, `twin seed load match #n`).

Ruled out in the code: `GetDC` and `SetSurfaceDesc` are stubs, so there is no GDI path; `GetSurfaceDesc` hands out the surface pointer without a lock (a kept pointer), but any CPU write through it is trapped.

### What changed

- **Copies in are logged** (`twin copy in #n`, first 96, for copies of at least 400x400 or into a priority surface): source twin, rect, 4K or 1x, the game code behind the `Blt`/`BltFast` (return address and the caller above the game's wrapper), the source's arrival counters, and the last write that reached that rect of the source: hooked write, indexed blit (with its source pointer), copy from an untwinned surface, fill, pixel op or trapped game write, with the game caller found on the stack. Each twin keeps its last 8 writes for this. The arrival line for the top 4 gets the first and last copy in (`twin arrival copies in`).
- **Indexed blits from other memory are logged** (`twin indexed blit #n from other memory`, first 32): source pointer (twin or store image if known), size, destination twin and pixel, the latest decode, and the game caller.
- **Load seed, behind `LoadSeed`.** The store's file loads are kept with their pack frame (plain images only; sheets go through `StoreDraw`). An indexed blit from unnamed indices of exactly a just-loaded image's size (within 30 frames) is a candidate, seeded at the blit's destination when `LoadSeed=1`. The copy out of the scratch then carries the 4K, or the pending seed, into the map buffer. With `LoadSeed=0` the match is only logged, so the next log shows whether name and size agree.
- **Trapped CPU copies, behind `TrapCopy`.** If the map buffer turns out to be written by a CPU loop, the trap now records the registers and the first bytes of the faulting instruction. A `rep movs` whose ESI lies in another twin becomes a copy hint (source twin, offset, rows written). At the next copy or present, if every 1x row of the hinted rect equals the source's 1x rows at that offset, it is replayed as a twin copy (`twin trap copy #n`). Otherwise it stays the 1x the trapped write marked.

## Bink

The two writers the trap saw (`binkw32.dll+21A13`, `+21B29`) wrote twin 4 at frame 0 only, 675 pages, while the blank `SF.bik` intro played. The game imports `_BinkCopyToBuffer@28`, which the wrapper already hooks (`Hooks.cpp:BinkCopyToBufferHook`) to force 32 bpp; the faults were inside the original call, so they counted as the game's (the stack scan found no return address of ours within 32 DWORDs). Its source is Bink's own frame buffer, never a twin or a decode, so there is nothing to replay as a twin copy: a video frame is a 1x write. The hook now announces the write before calling Bink, so the pages open first and the write is no longer counted as a trapped game write. It is at frame 0, before any ground, so `GroundArt` is not affected. `twin trap bink` names the exports the two offsets fall in and lists what the game imports from Bink, so this is checked in the next log.

## Sheet construction

`-INDEX.OPT` maps each frame name to its sheet file id and the offset of its block in `-IMAGES.OPT`. Animation entries have id -1, and their order matches `-ANIMS.OPT`. The format is the one the Gallean `ImagesExtractor` reads.

For a decoded store file with id F, `SeedSheet` does the following:

1. Collect the unique `-IMAGES.OPT` blocks of F's index entries.
2. For every frame of those blocks, map its name to a pack frame. The name goes through `-ANIMS.OPT` (animation name and frame index) or is used directly as a still, then through `PackFrameFor`. `PackFrameFor` is the old `Resolve` lookup, factored out: name variants, `.PNG`, spell prefix, and the size check for sprite origins.
3. Place the frame's 4K exactly as `ExpectArt` did: `place = sheet pos - canvas pos + (ox, oy)`, plus half the canvas for centre-origin frames, and 0 for sprites.
4. Turn each part into a piece: `to` is the part's sheet rect, `place` is the frame's box in sheet coordinates. These are the same `{dx, dy, sx, sy, w, h}` records that `QueueParts`/`ExpectArt` read from the game's record.
5. Handle shared blocks. A sheet rect already taken by an earlier frame keeps that frame's 4K. Each such rect counts once as a shared block, and each (first frame, other frame) pair is a conflict (at most 256 per sheet).
6. At replay, `CheckConflicts` draws both frames' 4K for the block into 16x16 cells and reads them back. A pair whose mean abs diff of premultiplied RGBA is over 12 differs. The pairs are counted, and so are the distinct blocks with a differing pair. The first 24 are logged.

Additive frames (aura and glow algorithms) are skipped. Their pack textures aren't premultiplied, and how they relate to the 1x additive pixels isn't verified.

## Transparency

- **Seed upload.** The seed rect is uploaded with alpha 0 exactly where the 1x pixel is the decode key `COLORKEY_CHECK` (or the twin's learned key), and alpha 255 elsewhere.
- **Seed draw.** The 4K is drawn over it with `glBlendFunc(GL_DST_ALPHA, GL_ZERO)`. Key texels become (0,0,0,0), and every other texel takes the premultiplied 4K texel with its own alpha. Where a piece's 4K doesn't reach (`art` discard outside the frame), the 1x upscale stays.
- **Keyed copies on the decode key.** The existing `isKey` test (alpha 0) decides transparency from that alpha, which is the 1x key mask. All N×N texels of a 1x key pixel are transparent, so a 4K edge can never cover a pixel the 1x copy left untouched.
- **Keyed copies on another key.** A keyed copy from a source carrying 4K, with a key other than `COLORKEY_CHECK`, is recorded as a `CmdMask` with flag 2. Its R8 mask is the exact 1x key test of the source pixels, computed at record time, and it is sampled with `GL_NEAREST`. It is drawn premultiplied over the destination, which is the CPU's decision per 1x pixel.

  This replaces the 0085d7f `srcMask` idea (sample the source twin's cell centres against the key, dilated by a pixel). On 4K texels a colour compare is never exact, and the dilation can cover untouched pixels.

  Copies larger than 2048 in either direction fall back to the plain keyed copy (`too big`).

## What seed mode bypasses

| Bypassed | Where |
|---|---|
| `ImageReplace::ExpectArt` and the per-render `Queue`/`Touch`/prefetch | `Render` only resolves the entry, for `imagecoverage.txt` |
| `Twin::ExpectArt`, `Twin::LearnArt`, `LearnBlock` at memory twin creation | early returns |
| `IdentifyArt` (expectations, regions, hash lookups, evidence) | `CopyTwins`, `PixelCopyInner` (full-strength keyed, mirrored copy) |
| `twin-index.bin` | neither loaded (`ChooseScale`) nor saved (`OnPresent`) |

## Ground

The game's ground ops are not all replayed on twins. `TwinDiamond` (`Hooks.cpp`) only calls `Twin::OnWrite`, which marks the diamond dirty for a 1x upload. These ops are missing:

- **`DrawGround`**: the 64x32 diamond row copy from the 192-px-wide tile texture (`**(DWORD***)thisObj[1]`), and the alpha-mask path, which replaces per pixel and writes `0xFFFFFF80` substitutions back into the mask. The water path does reach `OnBlendRow`, but its source, the tile texture, has no twin, so it marks the destination dirty as well.
- **`DrawWaterBorders`**: the keyed diamond copy from the 64-wide border data (`vtbl[3]()`).
- **`ClearGround`**: the diamond fill with opaque black.
- **Tile and border textures**: they have no twins. Their pixel decode isn't hooked; `TileFactory`/`BorderFactory` only name the objects.

Ground in seed mode therefore keeps `GroundArt`/`DrawGround` (`CmdGround`), exactly as in match mode.

## Logs

| Line | Period | Content |
|---|---|---|
| `twin mode ...` | once | mode, source, wait |
| `image sheets <archive>: ...` | once per archive | frame entries, animations, `-IMAGES.OPT` size |
| `image seed sheet <file> ...` | first 32 sheets | blocks, frames (unmapped, additive), pieces, shared blocks, conflict pairs, parts outside the sheet |
| `twin seed #n <kind> <outcome> name=... twin <id> <memory\|surface> buf=... base=... rect ... pitch ...` | first 300 events | done, deferred, completed later, transfer done/deferred, or skipped with the reason. `twin` and `buf` are the target: the memory twin of the decode buffer, or the surface twin a transfer seeded |
| `twin seed decode #n <name> WxH: BlitObject format byte ..., pitch field ..., data ..., clip ... \| snapshot readable as 32 16 8 bpp \| first bytes ...` | first 24 decodes | what the decode buffer really holds: format byte 0 means 8-bit indices |
| `twin seed transfer #n via <path>: decode <name> WxH part x,y WxH (#seq, frames ago, seeded into <buffer>) -> twin <id> <buffer> WxH at x,y \| <outcome> \| guard exact\|tolerant <format> (leading rows, matching rows of h, format byte) \| lock caller A<B` | first 200 | every accepted transfer, with the path that found it and the format that confirmed it |
| `twin seed transfer rejected via <path>: <reason> \| decode ... -> twin ... at x,y \| best <format>: leading rows, matching rows, px x,y holds ..., decode ... \| leading rows per format: rgb32 bgr32 565 (near) idx pal` | first 64 | a candidate that did not pass, and why (`-1` = that reading was not available). For indices the "decode" value is the index |
| `twin seed transfer rejected via unlock...: twin ... is over 4x the decode ... and its lock rect ...` | first 8 | the unlock path skipping a big surface such as the ground atlas |
| `twin seed transfer since start: ...` | every 5 s | decodes (named, snapshot failed, format byte 0 / 1 / other / no source), locks and unlocks (untwinned, memory twins, inside a decode), tries and why they were skipped |
| `twin seed transfer done: ...` | every 5 s | transfers per path (unlock during or before the lock, inside the decode call, hooked write, surface created before the decode, first write trapped, indexed blit from the decode), rejected surfaces (blank, partial at the end, mostly matching but not fresh), unlocks of much larger surfaces |
| `twin seed transfer guard since start: ...` | every 5 s | accepts per format, exact and tolerant; indexed blits: from the decode (part of it, no surface twin), into the decode, other sources |
| `twin trap on (...)` / `twin trap off (...)` | once | the trap's state, the code ranges of DisciplesGL and the game, the budget |
| `twin trap writer #n new: eip <addr module+off> \| caller ... \| ret ... < ... < ... < ... \| twin <id> WxH at x,y (page p)[, first write into the fresh surface] \| <where> \| frame, latest decode` | each new EIP | a game write into a trapped surface. `<where>` is inside a decode (StoreDraw), inside Render, under a hooked op, outside all of them, or another thread |
| `twin trap since start: ...` | every 5 s | twins attached (not page-aligned), arms at create, at present and after our own write, budget skips, pages opened by our announced writes, faults (wrapper's own, game, ring drops), writers |
| `twin trap top writers ...` | every 5 s, up to 5 lines | the 20 EIPs with the most faults: module+offset, pages opened, twins with a sample, inside decode / Render / hooked op / other thread, first writes into fresh surfaces |
| `twin arrival top 1x-only ...` | every 5 s, up to 4 lines | for the first 12 coverage-checklist entries, since the twin was created: uploads (n/px), seed uploads, hooked writes, fills, copies in from 4K / 1x / untwinned sources, pixel ops, lock/unlock count, the last `Lock` caller and the caller above the game's lock wrapper, transfers, guard failures, trapped game writes and the last writer EIP. This line names the path that fills each 1x-only surface |
| `twin seed conflict twin ... block ... holds frame A, frame B's 4K ... differs by mean ...` | first 24 | conflicting shared blocks |
| `twin seed store/sheet/face since start: ...` | every 5 s | events, done, deferred (completed later, expired), dropped (memory written, freed), overwritten after seeding, no twin, no art, pieces, px and MB at N×, and the targets: into memory twins, surface twins, by transfer. Transfers are events too |
| `twin seed sheets since start ...` | every 5 s | sheets, frames, shared blocks, blocks with differing 4K, pairs checked |
| `twin seed records ...` | every 5 s | pending, evicted, redraws, repeats, wait avg/max, replayed commands/pieces, art missing at replay, masked keyed copies |
| `image seed sources since start: ...` | every 5 s | store draws, faces, ranges, sheet archives, ground, files and frames prefetched at the store load |
| `twin coverage 5s: ...` | every 5 s | copies from sources carrying 4K, from 1x-only twins, from untwinned memory (count, px) |
| `twin coverage top 1x-only copy sources ...` | every 5 s, up to 4 lines | rank, twin, kind, buffer, size, px, copies, store name. This is the coverage checklist |
| `twin arrival copies in [rank] twin ...` | every 5 s, top 4 | count, the first and the last copy in: source twin and rect, 4K or 1x, game caller, the source's counters and its last write there |
| `twin copy in #n` | first 96 | each copy of at least 400x400 or into a priority surface, as above; also from untwinned surfaces |
| `twin indexed blit #n from other memory` | first 32 | an indexed blit whose indices are not the latest store decode |
| `twin seed load match #n` / `twin seed load matches since start` | first 24 / 5 s | such a blit matching a just-loaded image by size, and whether it was seeded |
| `twin trap priority ...` | every 5 s | the priority surfaces with their trapped writes, arms, pairs, skipped quick re-arms, trapped writes marked dirty, `rep movs` hints replayed or failing the guard |
| `twin trap priority write #n` | first 96 | a new writer on a priority surface, with registers |
| `twin trap copy #n` | first 24 | a `rep movs` hint replayed as a twin copy, or why not |
| `twin trap bink` | once | the exports `binkw32.dll+21A13` and `+21B29` are in, and the game's Bink imports |
| `twin seed streaming 5s` | every 5 s | deferred seeds, carried by copies, late landings with latency in frames and ms, frames still decoding or over the upload budget, pieces dropped by writes, waits while loading |
| `twin replay per command 5s` / `twin replay slowest command 5s` | every 5 s | dispatches, total and max per command type; the slowest command |
| `twin slow replay` | each replay over 50 ms, first 64 | the replay's time and its slowest command |

The log rotates to `DisciplesGL-debug.log.old` at 8 MB, and `test.bat` ships it as `<game>-DisciplesGL-debug.old.log`. In the 14:58 run, Ctrl+F11 also started TextReplace's F11 trace, which flooded the log with `trace bltfast/lock/surface` lines, so the 2 MB rotation lost the first `twin seed #n` lines. Ctrl+F10/F11/F12 now reach only the twin toggles: no F11 trace, no F12 dump, and no BattleLoad bind (F10 remapped to Ctrl+L quick load).

"Carrying 4K" means the source rect meets an art rect tagged with a frame, a seeded sheet, ground, or a merge that included one. The art rects travel with copies and pixel ops, and `ART_MAX` is now 128.

The tile check (`twin tiles check`, `twin repair`), the GPU check (`twin check twin ...`, `twin gpu check`) and the art-tile check (`twin art check ...`, which names `seeded sheet twin N` for seeds) run unchanged in both modes.

## Open gaps

1. **The loader of the big images (map buffers, full-screen backgrounds) is not named yet.** See Map view; the next log's `twin indexed blit #n from other memory` and `twin seed load match #n` lines settle it, and `LoadSeed=1` turns the match into seeds.
1. **The routine that writes the decode into the surface is still unknown.** The 15:32 run showed it uses no lock. The trap names it in the next run (`twin trap writer`), and its first write into a fresh surface is itself a transfer candidate. If the writer is plainly a copy from the decode scratch (or from another seeded record) into the surface, that EIP is the transfer point to hook directly. The 8-bit reading of the snapshot is still a suspicion until the `twin seed decode` lines show format byte 0.
2. **Flipped records** (`Build` with flipped = 1) use a mirrored sheet. Only the decoded, unflipped sheet is seeded. If the game mirrors with a hooked op, the 4K follows. If it uses an unhooked CPU loop, the tile check repairs it to 1x.
3. **The store's decode buffer is reused at once.** Its own seed only matters for hooked copies out of it (none were seen). A transfer copies the pieces to the surface, which defers on its own, so the `SeedWait` wait only decides how soon the surface shows 4K.
4. **Memory twins evicted by the 256 MB budget** come back as plain 1x when they are touched again; there is no seed without a decode. The same goes for 4K carried into other twins before a rebuild.
5. **Keyed pixel blends** (`PixelKeyed`, `PixelAverage` < 255, `PixelEmpty`) over 4K content with a key other than magenta still decide by texel colour.
6. **Shared blocks** show the first frame's 4K in every frame that uses them. The conflict lines measure how much that matters.
7. **Additive frames**, the turn strip (`TurnOrder.cpp`) and ground tile textures are not seeded.
8. **The seed draw assumes `StoreDraw` overwrites its rect.** If the game ever composites a store image onto existing content through `StoreDraw`, then 4K-transparent texels over opaque background 1x pixels come out transparent.
9. **Streaming risks.** Pieces of a sheet now land over several presents, so a frame can briefly show a mix of 1x and 4K. A derived seed assumes the unkeyed copy replaced its rect; a later keyed copy or write over it drops the pieces it touches.
10. **Trap risks.** A write into a trapped page from kernel mode (`ReadFile` into a surface, a GDI call writing it) fails with an error instead of faulting, so the trap would break it. No such path is known. The VEH also sees every exception in the process, but it returns at once for anything that is not a write AV inside a trapped page. Each fault costs a few microseconds; the page budget bounds it at about 2048 a second plus first writes. `WriteTrap=0` turns it off and goes back to heap-allocated surfaces.
