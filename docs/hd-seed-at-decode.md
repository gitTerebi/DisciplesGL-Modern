# HD twins: seed at decode

This page describes the `seed` mode of the twin renderer. It replaces the old way of deciding which 4K art a copy is: expectations from the render call, identified regions, and pixel hashes in `twin-index.bin`. In seed mode, 4K art is written into a twin only at the moment named 1x art is decoded into game memory. That is the DuckStation-style "replacement at upload". From then on, every copy, pixel op, cache, drag image and popup is a plain twin replay, so the 4K travels with the pixels and nothing downstream identifies anything.

`docs/hd-twin-architecture.md` describes the twin machinery that both modes share.

## Switch

`disciple.ini`, section `[Twin]`:

| Key | Values | Default |
|---|---|---|
| `Mode` | `seed`, `match`, `off`. `off`: `Twin::Active()` is FALSE, so there are no twins, no trap, no tile check, no `StoreDraw` or record-read work, and the renderer uploads the pixel buffer as it did before twins | `seed` |
| `TileCheck` | `1` gives every twin a CPU mirror and compares it with the surface at each copy source and present, repairing tiles the hooks missed; `0` makes no mirrors at all | 0 |
| `Diagnostics` | `1` keeps the per-copy write notes (each one scans the stack for the game caller), copy-in and arrival records, coverage tables and copy traces; `0` skips them | 0 |
| `SeedWait` | ms the game thread may wait at one decode for the pack worker to decode its art (0 to 2000). 0: it never waits, the seed is deferred and lands when the art arrives (see Streaming) | 0 |
| `SeedLoadWait` | ms a decode may wait while the game is loading, that is when no present came for more than 100 ms (0 to 2000). 0: never; the seed is deferred like any other | 0 |
| `WriteTrap` | `1` keeps the pages of every offscreen surface read-only until the game's first write, to name the code that writes it (see First-write trap); `0` off | 0 |
| `TrapCopy` | `1` replays a trapped `rep movs` from one twin into another as a twin copy when the 1x rows are equal (see Map view) | 1 |
| `GpuCheck` | `1` runs the GPU check every 5 s and takes the screenshots 15 s after the first seed, as before; `0` only on F12 | 0 |

The environment variable `DGL_TWIN_MODE=match` (or `off`) overrides the ini, and `DGL_TWIN_TILECHECK=0|1` overrides `TileCheck`. The first log line names the mode and where it came from: `twin mode seed (Twin.Mode=seed, seed wait 0 ms)`. Twins also turn off when no 4K pack (`gamenames-v1.galleanpack` or `assets-v1.galleanpack`) is next to the game; `twin off: ...` says which reason applies, once. `twin scale ...` names the tile check and diagnostics state.

Why these defaults: the tile check repaired 0 tiles in every logged run, while its mirrors copy every pixel the twins see a second time and it compares tiles at every copy source and present (0.15 to 0.25 ms per present in the 18:27 run, at about 235 presents a second). The trap named no game writer in the 18:27 run, but faulted 3311 times on our own writes and re-armed up to 1807 pages a second through `VirtualProtect`. The diagnostics scan up to 96 stack slots per copy for the write note, at about 85000 copies a second. Ctrl+F11 only toggles the check on twins that have a mirror, so with `TileCheck=0` at start it has no effect.

`match` is the old path and is unchanged, so the two modes can be compared. It is due for removal in the next round.

## Seed points

All seeds go through `ImageReplace.cpp:AddRange`. That is the one place where named 1x art is known to sit at a known address, pitch and size. It calls `SeedStore`, which calls `Twin::SeedArt`.

| Source | Where it enters game memory | Seed |
|---|---|---|
| Store image (`StoreLoad` / `StoreDraw`) | `ImageReplace.cpp:StoreDraw`, after the game decoded the file into `desc` (data, rect, pitch) | `SeedStore`: when the file is a packer sheet, a sheet seed (below). Otherwise one piece, the pack frame the file name resolves to (`StoreFrame`), over the whole image, never for a file whose `-IMAGES.OPT` parts move pixels (see Plain seeds never take a packed sheet) |
| Packer sheet behind `Build`/`Render` images | Loaded through the image store too: the sheet PNG is a file of the `.ff`, decoded by `StoreDraw`. Evidence: `_CUDEFAUL` is a `Build` image with one 24x54 part, and its store decode resolved to the same pack frame | `ImageReplace.cpp:SeedSheet` builds the sheet from the archive's `-INDEX.OPT`, `-IMAGES.OPT` and `-ANIMS.OPT` (`LoadSheets`) |
| Face | `ImageReplace.cpp:FaceLoad` decodes it, but the stride is unknown there; `OnFaceCopy` registers it at its first `DrawFaces` with the real pitch. That happens before the hook's twin calls | `SeedStore`, kind `face`, one piece |
| Full-screen backgrounds, dialog panels, battle sheets, the map buffers' art | Also `StoreDraw` (see The loader is StoreDraw). The 8-bit decode is too big for the store's 32 bpp range, so it is captured for the transfer only, and the indexed blit out of the scratch is named by its source pointer | The decode snapshot path (`XferIndexedTry`), or the loader's own pieces (`XferLoaderSeed`), seeded at the blit's destination in the 1152x1152 scratch; the copy out of it carries the 4K |
| Image decoded by a loader other than the store (none seen so far) | The `.ff` record the game read last with that image's size, then the indexed blit of exactly that size out of the scratch (see Record reads) | `RecordDecode` builds the pieces like `SeedStore`/`SeedSheet` |
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

## Record reads: naming every decode

### What the 16:53 run showed

The main menu background and many interface parts were still 1x: `from 1x-only twins 834M px` against 162M px from 4K sources. The top 1x sources (990x600, 730x519, 800x600, 800x664, 688x1014, 748x896) are all filled the same way:

1. 8-bit indices are decoded into the scratch at `1FF30000` (pitch 1152), the same buffer `StoreDraw` decodes into.
2. `Pixel_Blit_Indexed_to_565` converts them into twin 4, the 1152x1152 scratch surface. The first such blit's caller is `Discipl2.exe+105107`.
3. A `Blt` (`+267FB8 < +267EEE`) copies twin 4 into the surface of the image's size, which is then copied to the screen.

The `twin indexed blit #n from other memory` lines show `latest decode #0 -` or a small, older store decode, so these decodes looked as if they did not pass `StoreDraw`, and nothing named them. The 17:54 run showed that they do pass it, and that the store range had rejected them (see The loader is StoreDraw).

The binary is not in the repository, so the loader's address could not be found from the code alone. Every image loader reads its PNG from a `.ff` record, though, and the game does that through its own `ReadFile` import. So the record read is hooked at the import, not inside the game.

### The hook

- `Hooks.cpp:CreateFileHook` passes every `.ff` the game opens to `ImageReplace::OnFileOpen`. That function finds or loads the archive by full path (`FindArchive`, shared with the store's `FindArchiveFile`) and opens a reader handle of its own.
- `Hooks.cpp:ReadRecordHook` (the game's `ReadFile` import, V2 only) takes the file position before the read and calls `OnFileRead` after it. Handles that are not tracked `.ff` files pass straight through.
- `OnFileRead` works out which record was read. A read starting with `MQRC` carries its 28-byte header, and a read of 28 bytes or fewer is the header alone. Otherwise the 28 bytes in front of the read, read through the reader handle, must be an `MQRC` header whose size covers the read. The record id gives the file name (the archive's file table, record 2). For a `.PNG`, the PNG header (`IHDR` at +16) gives the size, from the buffer or the reader. The read goes into a ring of 64 with its caller and the frame. On the game thread, the 4K is asked for right away (`PrefetchSheet` or `PrefetchPlain`).
- `CloseHandleHook` forgets the handle.

### Naming a decode by its record

When an indexed blit comes from memory that is not the latest store decode (`XferIndexedTry`'s "other" case), `Twin::XferRecordSeed` asks `ImageReplace::RecordDecode` for the newest unclaimed record read of exactly the blit's size within 600 frames. A store decode (`StoreDraw`) claims the read of its own file first (`RecordTaken`), so a store image never names a later blit. The read that matches is claimed, so it names only one decode.

`RecordDecode` builds the pieces the same way the store decode does:
- **sheet**: when an `-INDEX.OPT` entry points at the file, `SheetPieces` (the old `SeedSheet` body, factored out);
- **plain**: otherwise one piece, the pack frame of the file name (`PlainFrame`), if the frame has the image's size and is not additive.

They are seeded at the blit's destination (`SeedAt`, as a transfer), so into twin 4 at 0,0. The copy out of twin 4 into the sized surface then carries the 4K, or, while the art is still decoding, the pending pieces (`SeedDerive`). This is identity by provenance; no pixels are compared.

`SeedDerive` used to skip every screen-sized target, which included the 800x600 main menu background in 800x600 mode. Now it skips only twins that were presented (`Record.screen`, set in `OnPresent`).

`LoadSeed` (b087412) is removed. It matched the blit against the store's `StoreLoad` calls, which these loaders never make: the 16:53 log had `seed load matches 0`. The record read replaces it. `StoreLoad` still prefetches, now also for plain images in archives that have sheets (`PrefetchPlain`; the old `NoteLoad` skipped the whole archive).

### Store decodes without 4K

`FOG.PNG`, `FOGCORNERS.PNG`, `G000UU5130STO27.PNG` and others are named by the store, but `SeedSheet` found no piece. `FOG*` frames are `no-pack` in `imagecoverage.txt`, and the `STO27` sheet's 8 frames are all additive. Their decode kept the name but was not `valid`, so its blit was logged as `latest decode #n -` and counted as unnamed memory. Now `XferCapture` keeps the 8-bit source of a named decode without art, and its blits are logged as `twin decode without 4K #n` with the reason (additive, not in the pack, no frame). They stay 1x because there is no 4K to seed. Sheets that seed nothing are logged further (`image seed sheet`, 64 more), so `MRK*` shows whether it is unmapped or additive.

### Faces

Faces are seeded on their memory twin at their first `DrawFaces`, and the same call copies them on into the scratch, mirrored for units facing the other way (`PixelCopy` with `PixelMirror`). Since b087412 (`SeedWait=0`), the face seed is deferred when its 4K is not decoded yet. Only the unkeyed `CopyTwins` path carried pending pieces; the mirrored pixel copy did not, so the scratch, and the 70x85 surface copied from it, stayed 1x for good: `twin arrival` shows a single copy in. Two changes:

- **Mirrored copies carry pending pieces.** `SeedDerive(..., mirror)` mirrors each piece's rect about the copy and mirrors its place too (`place.left > place.right`). `PieceUv` then samples the 4K right to left, and `SeedAt` now accepts a mirrored place.
- **Prefetch at `FaceLoad`.** The face's 4K is asked for when the face cache reads it, so it is usually decoded by the first `DrawFaces`.

The 70x17 and 116x41 bars next to the faces come out of the same scratch through the same copy. If they are store or record images, they now follow the same way; otherwise the unnamed line names their loader.

### Code dump

The DLL writes the game's code around the addresses the logs name to `twin-code.bin` in the game folder (`Twin::CodeDump`, `CodeFlush` at the 5 s log), and `test.bat` collects it. The layout is:

- `DGLCODE1`;
- region count, exe base, `SizeOfImage` and `TimeDateStamp` (4 DWORDs);
- per region: its address, size, file offset and a 20-byte reason;
- the bytes.

There is at most 160 KB in 128 regions; overlapping regions are merged. The regions are:

- v2.02b only: ±0x400 around `0x00505107`, `0x0050515E`, `0x00667FB8`, `0x00667EEE` and `0x0067298E`;
- v2.02b only: ±0x600 around `StoreDraw`'s caller `0x0052AF88`, the record open and size `0x00523A00`/`0x00523A9D`, the row remap `0x005220A1`, the conversion's `0x0066DD79`/`0x0066E011`, and the `ReadFile` callers `0x005208FE`, `0x00520DE5`, `0x005A5FC4`, `0x0051FEE4`;
- ±0x600 around `StoreDraw`'s caller, each frame of a loader-named blit's chain, and each return address the stack scan found above it (`Twin::StackCallers`: up to 2048 DWORDs above the blit's slot, values into the game that follow a call; the conversion keeps no frame pointer, so its caller is only found this way, and a stale value can be among them);
- ±0x400 around `StoreDraw` and `StoreLoad`, and ±0x200 around the `FaceLoad` call;
- ±0x200 around each of the first 6 `call` targets in `StoreDraw`'s first 0x200 bytes (the decode routine is one of them);
- ±0x400 around each frame of every new call chain of an unnamed or record-named indexed blit, and around each `.ff` `ReadFile` caller.

Call chains come from `Twin::WalkFrames`. It takes the return address in the blit's own slot (`_AddressOfReturnAddress()` in `Pixel_Blit_Indexed_to_565`), then the first saved-EBP and return-address pair above it, then the EBP chain, up to 6 frames, each checked to follow a `call` in the game. To disassemble one region: `dd if=twin-code.bin bs=1 skip=<offset> count=<size> of=r.bin` and `objdump -D -b binary -m i386 --adjust-vma=<address> r.bin`.

## The loader is StoreDraw (17:54 run)

### What the run showed

The city development screen, the main menu background and most panels were still 1x. Every unnamed indexed blit (800x600, 688x1014, 748x896, 800x664, 640x600, 483x600, 419x600, 990x600, 800x1095 and others) came right after a `twin decode named #n ... found by the store decode` line of exactly its size: `_WATER.PNG 688x1014`, `DLG_MAIN_MENU.PNG 800x664`, `DLG_BUILD_STRUCTURE.PNG 483x600`, `BG_UNDEAD.PNG 640x600`, `CITY_0_BG.PNG 990x600` and so on. So these images do pass `StoreDraw`, and their names are known. No other loader was seen.

They were still unnamed for two reasons:

1. **`AddRange` rejected the decode.** The store range reads the decode as 32 bpp at half the pitch field: 576 DWORDs, 2304 bytes a row. The decode is 8-bit, 1152 bytes a row, in a 1152x1152 scratch. Any image wider than 576 failed `pitch < w`, and any image of more than about 576 rows failed `IsBadReadPtr`, because the 32 bpp reading runs past the scratch. `SeedStore` never ran, so there was no capture, no `twin seed decode` line and no seed. Small sprites fit, which is why they worked. In the 17:54 log, 6 of the first 11 store draws have no art (`decodes with art store 5`), and they are exactly the 6 unnamed blits.
2. **The record read could not stand in.** `StoreDraw` reads its own record (below), so `RecordTaken` claimed the read, and the blit found `no record read of this size`. Most reads were not even seen. `StoreDraw` opens the `.ff` again for every record, and the long-lived handles had filled the 48-slot handle table: `untracked` grew with every store draw (62, 154, 178). Only 1 read was taken by `StoreDraw`, and none named a blit.

### Disassembly (v2.02b, Discipl2.exe stamp 3FFD92DC, `twin-code.bin` of 17:54)

| Address | What | Convention and arguments |
|---|---|---|
| `0x00520C53` | `StoreDraw`, the store image's draw slot (`image_store_draw`, already hooked through the vtable slot `0x006D6854`) | `__thiscall`, `ecx` = store image (`+4` archive holder, `+8` record id, `+0x10` palette holder with the palette index at byte `+0x408` and the row remap flag at byte `+0x41C`); one stack argument, the destination `BlitObject` (format byte `+0`, data `+4`, clip rect `+8..+0x14`, pitch field `+0x18`); `ret 4`. It checks the clip against the record's size, reads the record (`0x00520D81`), gets a decoder (`0x00544A80`), wraps the buffer in a stream (`0x0057F8C9`) and calls the decoder's `+8` with the data, pitch and height, so the PNG is decoded straight into `desc->data` at `desc->pitch`. With the remap flag set, every row goes through `0x005220A1`. It returns to `0x0052AF88` |
| `0x00520D81` | reads one record into a buffer | `__thiscall`, `ecx` = buffer; stack: archive holder, record id, a third argument. It opens the record's file at its data (`0x00523A00`), takes its size (`0x00523A9D`), resizes the buffer (`0x0051AFA4`), makes one `ReadFile` (return address `0x00520DE5`, the record-read caller in the log) and calls `CloseHandle` |
| `0x00520884` | reads a record by name (`-INDEX.OPT`, `-IMAGES.OPT`) | opens it by name (`0x0052396F`), takes its size (`0x00523A5B`), and its `ReadFile` returns to `0x005208FE` |
| `0x0051FEE4` | the MqDB's chunked reader on its long-lived handle: `SetFilePointer`, then `ReadFile` | these are the 10654 reads "inside a record" |
| `0x005A5FC4` | ground and border tile reads (the `TileFactory`/`BorderFactory` path) | not converted through the scratch |
| `0x0066DDC0` | converts an 8-bit DirectDraw surface into a 16 bpp one | `__cdecl (src surface, dst surface, rect)`. It gets `GetSurfaceDesc` of both surfaces, the source's palette (`GetPalette`, `GetEntries(0, 256)`) and its source colour key (`GetColorKey(DDCKEY_SRCBLT)`, mapped to magenta), locks both and calls `0x0066DD50`. It has no frame pointer, so `WalkFrames` stops in it |
| `0x0066DD50` | thunk into the pixel function table (`[0x008304EC]`, slot `+4`, `Pixel_Blit_Indexed_to_565`) | 6 stack arguments, `ret 0x18`. Its return address `0x0066DD79` is the blit's slot; `0x0066E011` is the return into `0x0066DDC0` |
| `0x0066DC20`, `0x0066DCC0` | methods that call `0x0066DDC0`, of the object that owns the two surfaces (`+0x10` source, `+0x14` destination, `+0x18` palette, `+0x08`/`+0x0C` size) | `__thiscall`, no stack arguments |

So the store's own PNG decoder inside `StoreDraw` writes the 8-bit data into the scratch (`1FFB0000` or `0F1C0000`, an 8-bit 1152x1152 surface). `0x0066DDC0` then converts it into twin 4. At `StoreDraw`, the image name, the record id (`this+8`), and the scratch pointer and size (`desc`) are all known. This dump does not hold `StoreDraw`'s caller at `0x0052AF88` or the caller of `0x0066DDC0`; the next one does (see Code dump).

### The hook

No new game hook is needed: `StoreDraw` is the loader, and it is hooked in every build that has `image_store` addresses (v2.02b and v3.01b).

- **Loader record.** `ImageReplace.cpp:StoreDrawn` keeps the latest decode (`loader`): file, archive, `BlitObject` data, pitch field, format byte, clip rect, frame, caller, a sequence number and the transfer capture it got.
- **Capture without a 32 bpp range.** When the 32 bpp reading does not fit and the decode is 8-bit, `AddRange` still calls `SeedStore`, with `memoryTwin = FALSE`. `SeedArt` then only captures the decode for the transfer (`XferCapture`, snapshot readable as 16 and 8 bpp). It makes no memory twin over the scratch.
- **Naming the blit.** `XferIndexedTry` asks `ImageReplace::LoaderDecode` whether the blit's source lies inside the latest decode, at its pitch, within 600 frames. If it does, the name is certain: there is no size match and no pixel compare.
  - If the capture belongs to that decode, the existing "indexed blit from the decode" path seeds it (guard and `XferSeed`), and the loader line says `the decode snapshot`. If its guard fails, the loader's pieces are seeded anyway.
  - Otherwise `XferLoaderSeed` builds the pieces from the file. `NamePieces` takes a sheet's pieces from `-INDEX.OPT`/`-IMAGES.OPT`/`-ANIMS.OPT`, or a plain image's pack frame. `XferLoaderSeed` clips the pieces to the blitted part and seeds them at the blit's destination (`SeedAt`, as a transfer). Twin 4 then carries the 4K into the surface the game copies it to.
  - Once a newer decode in the scratch has no capture of its own, the capture of an older decode is no longer used, because the scratch now holds the newer image.
- **Sheets.** 688x1014, 748x896, 640x1072 and the battle sheets are sheets. Their pieces come from the same `SheetPieces` as a store sheet's. A sheet with no piece is logged with the reason (unmapped, additive or outside), either on its loader line or as `twin decode without 4K #n`.

### Record reads

- The handle table has 1024 slots. The summary line now says why a `.ff` open was not tracked (table full, or an archive without a file table), and how many tracked handles are open now and at most.
- With the table no longer full, `StoreDraw`'s own reads are records again. `RecordTaken` claims them, so the record path stays a fallback for a loader that does not pass `StoreDraw`. None has been seen.

### Coverage by screen

- **Per screen.** Each large plain image (at least 480x480, not a sheet) that `StoreDraw` decodes names the current screen (`Twin::ScreenLabel`). Every copy is counted under that screen, 4K px against 1x px, in `twin coverage per screen since start` (24 screens; the one with the fewest px makes room).
- **Names of 1x sources.** Every twin keeps the latest named image that reached it (`Arrival.known`), whether seeded into it or carried in by a copy. The coverage list shows the store name or, marked `~`, that name.

## The unit card portrait (18:27 run)

### What the run showed

The unit card (`_PG0500IX.PNG`, 710x564) showed its portrait, `G000UU5017.PNG` 301x364 (`Events.ff`, record 2747), in 4K but with its blocks out of order: horizontal bands of the face stacked in the wrong order, as the packed sheet holds them.

- The portrait did not take the new loader-pieces path. The run has `by loader pieces: seeded 0`: every loader seed went through the decode snapshot. The portrait's `twin seed #228 sheet transfer` into twin 4 came from the guarded snapshot path (the hooked write after the decode), and twin 134, the 301x364 surface the game copies it to, took the pending pieces by `SeedDerive`. The 17:48 run (before 1bc60e4) seeded it the same way, with the same 14 pieces of pack frame 479.
- The loader path and the snapshot path place a blitted part the same way: the pieces are clipped to the part, and both their sheet rect and their frame box move by the part's origin. Both now go through `Twin::ClipPieces`.
- `SheetPieces` follows Gallean's `ImagesExtractor`: a part is `{canvas x, canvas y, sheet x, sheet y, w, h}` (Gallean's `SourceX/Y` is the canvas, `DestX/Y` the sheet), and the frame's 4K is placed at `sheet - canvas` (plus the pack offset, or half the canvas for centre origins). The math matches the extractor. What it cannot know is what the pack entry holds.

### Cause (suspected)

The pack entry the frame resolves to may hold the file's own layout, not the frame's canvas. The user's pack is `gamenames-v1` (DLG frames resolve in it; the assets pack has no names for them), which keys entries by game name with a canvas origin. The portrait's only frame is named like its file, so `PackFrameFor` finds the entry `G000UU5017`. `PlainFrame`/`StoreFrame` read that same entry as the file itself, the identity layout, while `SheetPieces` reads it as the frame's canvas. Both readings fit geometrically, since the canvas is the size of the sheet. The parts reorder the sheet into strips, so at most one of the two readings is right. If the entry holds the file's layout, the sheet seed sends each strip the 4K of the strip that sits at its canvas place in the file. Once the game reassembles the parts, that shows the file's layout, which is what the screenshot shows. The 18:27 log has no piece list, so this is not confirmed. The next log shows it (see Checks).

### Fix (f792150, partly withdrawn, see below)

A sheet seed keeps only pieces it can place with certainty. Everything else stays 1x and is logged:

- **Ambiguous pack entry.** Withdrawn in the next round. f792150 left 1x every frame whose pack entry is named like the sheet file, is its size and whose parts reorder the sheet. The 19:07 run showed that this caught 42 frames that were fine before (`_MENU_BACK_N`, the `G000MG01xx` icons, `MOVE`, the `STOP` stills), while the event portrait that was still wrong in that run (`G000UU0013.PNG`) was not caught at all. See "What pack entries hold".
- **Block of another sheet.** A `-IMAGES.OPT` block whose stored sheet size is not the decode's size is skipped. Gallean ties a block to its file by that size, and a clipped decode fails it too.
- **Overlaps.** Two pieces whose sheet rects overlap without being the same block, and which do not take the same 4K there (same frame and frame box), are uncertain. Across frames the first keeps the overlap, as a shared block does; within one frame both are dropped. A block that one frame draws at two places is dropped too, since it can carry only one of them.
- **Sheet rects** outside the decode were already left out (`parts outside`).

### Checks

- `image sheet pieces <file> ...`: the full piece list for the first 20 sheet seeds, for every sheet with an uncertain piece or a file-named frame whose readings differ (64 more), and for every Events.ff and Faces.ff sheet and every sheet with a multi-part frame of the decode's size (96 more). The header line has the frames, pieces, the dropped count and the pack. Then one line per frame (up to 12): the frame name, canvas, the number of parts and their union in the canvas, pack entry (and whether only a name variant matched), pack frame, size, offset, origin, `ENTRY OUTSIDE THE FRAME'S PARTS` and the file-named mark. Then the pieces, about 15 per line: `sheet x,y wxh->[frame]#pack frame canvas x,y place x,y`, with `DROPPED` for uncertain ones. Sheets are never flipped.
- `image sheet checks since start` (every 5 s): pieces left 1x, blocks of another size, file-named frames (and how many an identity reading would place differently), frames whose entry reaches outside their parts, overlaps, double-placed blocks, sheets too big for the overlap check (over 1024 pieces).
- The no-4K reason on a loader line or `twin decode without 4K` names the uncertain case.

The layout question is settled (see What pack entries hold); the portrait's piece list in the next log decides between the two remaining causes.

### What pack entries hold (confirmed from Gallean's source and the 19:07 log)

An entry named like a frame holds that frame, built from its parts in the frame's canvas, never the raw packed file, even when the file has the same name:

- `ImagesExtractor.GetImage(name)` (`tools/GameContent/Extract/Images/ImagesExtractor.cs:172-194`) looks the name up among the `-IMAGES.OPT` frames first (line 179) and builds the frame (`BuildImage`, line 183). Only a name that is no frame falls through to the raw file (`archive.TryGetFile`, line 186, then `PrepareBrushKeyedImage`, line 193). The frame `G000UU5017` therefore never reaches the file `G000UU5017.PNG`.
- `BuildImage` (lines 510-541) copies each part from the sheet at `DestX/DestY` (line 527) to the canvas at `SourceX/SourceY` (line 528), into a bitmap the size of `GetImageBounds` (lines 487-508), the union of the parts' canvas rects. `OriginalWidth/Height` stay the canvas.
- `GameImages.TryGetSprite`/`TryGetAnchoredSprite`/`TryGetCompositeSprite` (`tools/GameContent/Content/GameImages.cs:33-58, 149-183`) all go through that lookup. Event portraits are `TryGetCompositeSprite` (`GamePortraitSource.cs:82-88`): every frame of the portrait's file composed at its bounds on the canvas (`SpriteOperations.Composite`, `Helpers/SpriteOperations.cs:5-37`). The extractor's `extract-images` command (`tools/Extractor/Cli/ExtractImagesCommand.cs:54-62`) calls the same `GetImage` for each file name.
- `SpriteOperations.ClipToCanvas` (`SpriteOperations.cs:126-166`) crops such a bitmap to its non-transparent box and keeps the box's place in the canvas as `OffsetX/OffsetY`. That is what the gamenames entries look like: in the 19:07 log every one of the 18 file-named frames f792150 left 1x has a pack box exactly equal to the union of its parts' canvas rects, with a non-zero offset where the parts are (`G000MG0116` 64x54 at 124,123 in a 320x320 canvas, `_MENU_BACK_N` 61x35 at 3,3 in 67x41, `MOVE` 24x54 at 160,160 in 321x321). A raw file has no canvas offset. Of the 67 canvas-origin frames with a piece list in that log, the 65 whose parts are all listed have their pack box inside the union of their parts; the other two (`DLG_SCENARIO_BRIEFING` buttons) share blocks that the list shows under the first frame only.

So `SheetPieces` reads every entry in canvas layout, as `ImagesExtractor` builds it: `place = sheet - canvas + offset`. There is no ambiguity rule any more. Entries named like the file are only marked in the log (`named like the file, both readings agree` when the entry over the file at 0,0 would place every piece the same, as for `_MENU_BACK_N`; `canvas reading (an identity reading would differ)` otherwise, as for `G000MG0116`).

One geometric check replaces it. The entry's box must lie inside the union of the frame's parts in the canvas, since it is cropped from them. An entry that reaches outside is another image of that name, from another archive or a raw file, and the frame stays 1x (`ENTRY OUTSIDE THE FRAME'S PARTS`, `frames whose pack entry reaches outside their parts`). The check covers stills looked up by their own name in the gamenames pack (event portraits, icons, dialog parts), not animation frames: no log has shown yet whether an animation frame's entry is cropped to the frame or to the whole animation. The pack's own duplicate names are now logged at load (`image pack duplicate name`): only the first image of a name is reachable.

### The portrait in terms of this layout

With canvas-layout entries the sheet seed applies the same transform as `BuildImage`, so its strips cannot come out in the sheet's order through `SheetPieces` alone. f792150's explanation (the entry holds the file) is refuted by the source and the log. What the logs show:

- 18:27: `G000UU5017.PNG` 301x364 seeded 14 pieces of pack frame 479 through the decode snapshot into the scratch (twin 4) and on into twin 134, the 301x364 surface; no piece list was logged.
- 19:07: `G000UU5017.PNG` was never decoded. The only Events.ff image was `G000UU0013.PNG` 437x277 (record 2093, 19:07:31.117), a sheet of 6 pieces, seeded through the decode snapshot like every other sheet of that run, not flagged by the old rule, and still wrong on screen. Its piece list was not logged (only the first 20 sheets were).

What remains possible, and what the next log shows:

1. The pack entry of that name is not this frame: another archive's image of the same name, or Gallean's event portrait key, which composes the portrait of the unit's face graphics id (`ResolveUnitFaceGraphicsId`), not necessarily the file of that name. If its box differs, the new check leaves the frame 1x and says so; the duplicate-name lines name any collision.
2. The portrait's 1x reaches the screen through a path no hook replays (the card's reassembly of the parts), so the copy of the sheet's 4K stays in sheet order on the GPU while the 1x is reassembled. Since f792150, `TileCheck` and `WriteTrap` are off by default, so such a path is not caught. Running once with `TileCheck=1` and `WriteTrap=1` names it.

Every Events.ff and Faces.ff sheet, and every sheet with a multi-part frame of the decode's size, now logs its full piece list (`image sheet pieces ... (event portrait, face or multi-part frame of the sheet's size)`, 96 more), with each frame's parts union next to its pack box. That settles case 1 against case 2.

### How the pack was built (repack.py)

`gamenames-v1.galleanpack` was not built by Gallean. It came from a Python script, `repack.py`, which lived uncommitted in the user's temp scratchpad on their PC. It is now committed with its helpers in `tools/repack/` (5a9ffdb), and that copy is the ground truth for names and offsets. What it does:

- Entries are Gallean's assets-v1 WebP blobs, "copied byte for byte and renamed to game names". The DLL team's own upscale (own-v1) fills in what Gallean lacks. Gallean's entries are added first, and `add()` skips a name it already has, so Gallean wins on duplicates.
- Names come from `game_name()`, a Python copy of the DLL's `GameName()`:
  ```python
  if c in ('spell-icon', 'unit-event-portrait'):
      return unit or None, SPRITE
  if c == 'unit-face':
      return unit + 'FACE.PNG', SPRITE
  if c == 'unit-portrait':
      return unit + 'FACEB.PNG', SPRITE
  ```
  `item-icon`, `building-icon`, `lord-face`, `city-image` and `capital-image` are SPRITE as well. `resolve()` tries the name, then the name with `.PNG` added or removed, against the .ff's stills (the assembled `-IMAGES.OPT` frame names that are no animation frame), anims, and raws (every other file in the archive):
  ```python
  cands = [name]
  cands.append(name[:-4] if name.endswith('.PNG') else name + '.PNG')
  for n in cands:
      if n in anims or n in stills or n in raws:
          return [n]
  ```
- Offsets are rewritten:
  ```python
  if origin == CENTRE:
      w, h = dims[min(k, len(dims) - 1)]
      md['OffsetX'] += w // 2
      md['OffsetY'] += h // 2
  elif origin == SPRITE:
      md['OffsetX'] = md['OffsetY'] = 0
  ```
  The manifest keeps Width/Height. `PackName()` reads every entry as `OriginCanvas`.

So a Gallean `unit-event-portrait/<unit>` entry is Gallean's composite portrait (`GamePortraitSource.cs:82-88`, `GameImages.TryGetCompositeSprite`, `SpriteOperations.Composite`). Every frame of the file `<unit>` (or `<unit>.PNG`) is laid on one canvas as wide and tall as the largest frame. The entry is filed under the game's still or file name (for example `G000UU0013`) with its offset set to 0. The same SPRITE zeroing applies to spell icons, unit faces (`FACE.PNG`) and unit portraits (`FACEB.PNG`).

What Gallean's source says about those offsets (`/home/user/GalleanDev/Gallean`, 0.12.2): `SpriteOperations.Composite` returns the full canvas with offset 0,0 and does not crop. `GameImages.TryGetSprite` goes through `ToFullCanvasSprite`, which is also the full canvas at 0,0. `ClipToCanvas`, which crops to the non-transparent box and keeps the box's place as the offset, runs only for anchored sprites (`ToAnchoredSprite`: cursors, bags, tombstones, mountains, world objects; these are CENTRE in the repack and convert correctly) and for positioned animations. The Packer's filters do not trim these categories. If the pack's assets-v1 was built from this Gallean version, the zeroing lost nothing: each SPRITE entry is the frame's (or the file's) full canvas, and 0,0 is its real place. The faces agree: `G000UU5017FACEB.PNG` 130x142 and the other faces seed through `StoreFrame`, which requires the entry to be the file's size, so their entries are not crops. Nothing in the logs shows an event portrait's entry size yet, so the DLL now handles both readings by geometry (below).

### Zeroed offsets: where 0,0 cannot be the place

`SheetPieces` places a frame's 4K at `place = sheet - canvas + offset`, with the entry's offset as stored. For an entry at 0,0 this means the entry's top-left is the canvas origin. If such an entry were a crop with its real offset `u` (the box's place in the canvas) set to 0, every part would sample the entry at its own canvas position instead of at canvas minus `u`. That is the content that belongs `u` further right and down. The pieces stay in order, so the card reassembles a coherent 4K portrait, shifted up and left by `u` inside the 1x silhouette. The key-alpha cut keeps the 1x outline, and the shift moves the face within it. Where a part samples past the entry's right or bottom edge, `GL_CLAMP_TO_EDGE` smears the edge texels into streaks; the strips at those edges are each `u` wide or tall. The 4K for the first `u` columns and rows is never shown. This is a uniform shift with edge streaks. It is not a reordering of bands into sheet order, which is what an identity reading (the entry over the sheet at 0,0) would produce. Which of the two the screenshot shows tells them apart.

48908cf's outside check had a related problem. A full-canvas entry at 0,0 whose parts union does not cover the whole canvas reaches outside the union, so that check left it 1x. That is the reading Gallean's source gives for event portraits, and for every frame whose parts the game's packer trimmed.

`DeriveOffset` (called from `SheetPieces`) now handles this. Its input is a still in the gamenames pack whose entry is at 0,0, where either the union of its parts does not start at 0,0, or the entry has a SPRITE-origin name and is smaller than that union. SPRITE-origin names are everything in Events.ff or Faces.ff, `G000SSnnnn` spell icons, and `...FACE.PNG` and `...FACEB.PNG`. For an Events.ff entry named like the file, the union and the canvas are those of every frame of the file, since the entry is their composite. The outside check then uses the same union.

| Entry | Taken as | Log |
|---|---|---|
| the whole canvas (the largest frame's for a composite) | Gallean's full-canvas sprite or composite: 0,0 is real; exempt from the outside check | `image pack offset full canvas` |
| the size of the parts union | the union: offset = union origin | `image pack offset derived ... the entry has the parts union's size` |
| smaller than the union | the composite cropped to what is not transparent: offset = origin of the 1x decode's non-transparent box, when that box has the entry's size | `image pack offset derived ... 1x key box x,y wxh` |
| smaller, and the box is another size, or two parts of a frame overlap | unplaceable: the frame stays 1x | `image pack offset left 1x` |
| smaller, and the 8-bit decode is not at hand | stays 1x this time, retried at the next decode | `image pack offset left 1x ... 8-bit indices are not at hand` |
| larger than the union, not the canvas | not derived; the outside check leaves it 1x | `ENTRY OUTSIDE THE FRAME'S PARTS` |

The non-transparent box comes from the game's own 1x alpha key, not from matching pixels. `KeyBox` reads the 8-bit indices of the latest `StoreDraw` decode, which is this file's whole sheet (`KeyIndices`: same archive and record, format byte 0, the decode's size). A pixel is transparent where its index is the block's transparent index or a brush colour. This is Gallean's `GetPaletteOpacity`: opacity is the block's algorithm byte, 0 for `#F8xxF8`-class brush colours and for the transparent index. Each part's opaque pixels are placed at their canvas position and clipped to the canvas, as `BuildImage`, `Composite` and `ClipToCanvas` do. That box is exactly the box Gallean's crop would keep, because it is computed from the same 1x pixels by the same rule. Two checks make it safe:

- The box must have the entry's 1x Width/Height.
- A frame whose parts overlap is refused, because its later part would overwrite the earlier one and the union of opaque parts would not be the frame.

Frames whose union starts at 0,0 and which are not SPRITE-origin names are never touched, so entries genuinely at 0,0 stay as they were. Animation frames are not touched either. Faces are not sheets: they seed through `StoreFrame`, which needs the entry to be the file's size, and nothing there changed.

Logs:

- `image pack offset derived|full canvas|left 1x: <entry> #<pack frame> entry WxH at 0,0 (offset zeroed by the repack) | <file> frame <name> canvas WxH, parts union x,y wxh[, 1x key box x,y wxh] | offset x,y: <how>`. This is one line per entry the first time it is met, 128 in all. The result is kept per pack frame and file.
- `image sheet pieces ... frame [n]`: `ZEROED OFFSET DERIVED: placed at x,y`, `full-canvas entry, 0,0 is its place` or `ZEROED OFFSET NOT DERIVABLE: left 1x`. For an event portrait's composite, the union shown is that of every frame of the file.
- `image pack offsets zeroed by the repack, since start` (every 5 s, after `image sheet checks`): the counts for each case.

Repack-side fix (proposed, not applied): in `repack.py`, keep Gallean's offsets for SPRITE origins instead of zeroing them (`elif origin == SPRITE: pass`). For Gallean's full-canvas sprites they are 0 anyway, and a crop would keep its real place. `tools/repack/bbox.py` already compares each pack box with the parts union, and `verify.py` computes the non-transparent 1x box, so the tooling can check every SPRITE entry: its box must be the frame's canvas at 0,0 or a crop inside the union.

### Plain seeds never take a packed sheet

The screenshot taken before f792150 shows the unit card's event portrait (`G000UU5017.PNG` 301x364, `Events.ff` record 2747) as a coherent 4K portrait cut into about 4 full-width bands, each a different vertical slice in the wrong order. That is the 4K read in the sheet's own coordinates: an entry laid over the file at 0,0 (identity) instead of mapped part by part from canvas to sheet. It is not the uniform shift with edge streaks that a zeroed crop offset gives (above).

**Every path that can lay a pack entry over a decode at identity** (one piece, `to` = `place` = the whole file):

| Path | Where | Reached when |
|---|---|---|
| plain store or face seed | `ImageReplace.cpp:SeedStore`, the fallback after `SeedSheet` (one piece from `StoreFrame`) | the file has no archive (faces), its archive has no sheet tables, or no `-INDEX.OPT` entry points at its record (`SheetPieces` returns FALSE only then, at its `if (!blocks)`) |
| plain loader or record seed | `ImageReplace.cpp:NamePieces`, the fallback after `SheetPieces` (one piece from `PlainFrame`), used by `LoaderDecode` (`XferLoaderSeed`) and `RecordDecode` (`XferRecordSeed`) | the same |
| match mode | `AddRange` (`LearnArt` over the range with `StoreFrame`) and memory twins (`FindMemory` → `CreateMemoryTwin` → `LearnBlock`) | `Twin.Mode=match`. Here a sheet file was learned whole at identity whenever an entry of its size resolved from the file name |
| transfers and derived copies | `Twin.cpp:XferSeed`, `XferLoaderSeed`, `SeedDerive` | never by themselves: they move the pieces the decode was given, clipped and offset (`ClipPieces`) |

**Which one the portrait took.** None of the identity paths. In every logged run the portrait was seeded as a sheet: 17:48 (`f758520`) and 18:27 (`87ff310`) have `twin seed #258/#227 sheet ... G000UU5017.PNG ... pieces 14 ... frames 479..479` on the decode, `sheet transfer` into twin 4, and `sheet completed later` into the 301x364 surface (twin 142, twin 134); `G000UU0013.PNG` (6 pieces, pack frame 170) likewise in 18:27, 19:07 and 19:26. `twin seed store since start: events 0` in all of them: the plain path never seeded anything but faces. That is expected from the code, because `Events.ff` has sheet tables (`image sheets ... Events.ff: 215 frame entries`) and the portrait's record has `-INDEX.OPT` entries, so `SeedStore` and `NamePieces` always go through `SheetPieces`. So the bands came through `SheetPieces` with pack frame 479. That gives the file's layout on screen only if frame 479 holds the file's layout and not the frame's canvas (an entry of the file's size, named like the file), or if the card's reassembly of the parts is not replayed on the twins. The current code seeds the portrait exactly as the 18:27 build did (`DeriveOffset` takes a 301x364 entry at 0,0 as the full canvas), so if the cause is the entry, the bands come back with it. The `image sheet pieces G000UU5017.PNG ... (event portrait ...)` lines in the next log list the parts and settle it.

**Guard.** The identity reading is now refused for any file whose `-IMAGES.OPT` frames move pixels, whatever path asks for it:

- `StoreFrame` (the plain store and face seed, and match mode's `LearnArt` and memory twins) asks `PlainAllowed`, worked out once per store image (`StoreImage.plainState`, reset with the store).
- `PlainFrame` (the loader and record seeds) asks `PlainCheck` itself.
- `PlainCheck` reads every `-IMAGES.OPT` block of the file's record (`FileFrames`, the same `-INDEX.OPT` lookup as `SheetPieces`, but with no block-size filter). If any part's canvas position `{SourceX, SourceY}` differs from its sheet position `{DestX, DestY}`, the file is a packed sheet and is never seeded as one plain image. It is mapped by `SheetPieces` (canvas → sheet, with fcb7132's offsets) or stays 1x. A file without frames, or whose every part is at its canvas position, is let through.
- Faces carry no archive, so their file is looked up by name (`FindFileByName`, `Faces.ff` first) and checked the same way. A face whose file is in no tracked archive is let through as before and counted as not verifiable.

In seed mode, store and loader decodes reach the plain path only for files with no `-INDEX.OPT` entry, so the guard should never refuse there. If it does, a file is reached by a sheet frame that the record lookup missed. In match mode it refuses every packed sheet that used to be learned whole.

Logs:

- `image plain seed refused: <file> WxH (<archive> record <id>)[ face] | a packed sheet: <n> -IMAGES.OPT frames, <n> parts, <n> of them not at their canvas position (frame <name>: canvas x,y <- sheet x,y wxh) | ...`: one line per refused file (256 files). The loader line's no-4K reason then reads `its -IMAGES.OPT parts move pixels (a packed sheet) ...`.
- `image plain seed check <file> ...: allowed, no -IMAGES.OPT frame (...)` or `allowed, <n> -IMAGES.OPT frames with <n> parts, every part at its canvas position (identity)`, or `allowed, not verified: ...`: the first 24 faces and identity-frame files. Each face's first line shows that faces are plain files.
- `image plain seeds (one pack entry over the whole file at 0,0) since start` (every 5 s): checks; allowed with no frame, with identity frames, not verifiable; refused files and calls.

### Sheets with 0 pieces

`G000UU0019HMOVA2A00.PNG` 577x590 (`BatUnits.ff`) is the attack aura layer (`A2`) of unit 0019, and every frame in it is additive. The same holds for `MRKCURSMALLA.PNG`, `DEATH_NEUTRAL_S10.PNG` and `G000UU9999DYIN8.PNG`. Their lines say `every frame is additive (aura or glow), which is not seeded`. `FOG.PNG` is not in the pack. All of this is by design (see Sheet construction).

### 8-bit decodes get no memory twin

Every `StoreDraw` decode is 8-bit (format byte 0). A small one used to fit the 32 bpp range and got a memory twin over the scratch, read at half its pitch, plus a seed into it (`twin seed #227 ... twin 129 memory ... pitch 576`). That twin is no image, and nothing copies out of it. Now every 8-bit decode is captured for the transfer only, as the big ones already were. This saves an upload and a seed per decode.

## Game-thread cost and hitches

With twins on, units moved at about half speed. The game thread is now measured by category, and the costly parts can be switched off.

- **`Twin::Cost` scopes** charge rdtsc time to the innermost category: record (copies, fills, pixel ops, present), mirror (CPU mirror updates and syncs), tile check (verify, repair), seed (seeds, transfers, unlock compares, indexed blits), trap, lookup (twin lookup, match-mode art lookup), diagnostics (notes, copy-in, coverage, the 5 s logs) and image (`StoreDraw`'s work, ground, face copies). Only the game thread counts. The first present picks it.
- `twin game-thread cost 5000ms` (every 5 s): presents, the present interval avg/max and its histogram (<17, <33, <50, <100, >=100 ms), hitches, and the avg/max ms per present of each category, then the total and its share of the game thread.
- `twin hitch` (each present interval of 50 ms or more, at most 20 per 5 s and 400 in all): the interval, the twin and image time in that frame by category, the slowest call (`Timed`) and its time, the seeds, uploads (Kpx), copies and fills of the frame, and the ground blits (`the map view was drawn`, the scrolling signal).
- `twin render show interval` (every 5 s): the render thread's time between two `Draw`s that had frames, as a histogram, and its max.

Per-frame cost removed:

- `WriteTrap`, `TileCheck` and `Diagnostics` default to 0 (see Switch).
- An unlock of a surface smaller than the latest decode is not compared, and a surface whose guard already failed 4 times for a decode is not compared again for it. The 18:27 run had 4.9 million unlocks and 3.3 million transfer tries in 70 s. `twin seed transfer done` counts both cases.
- Twin lookup by surface has a 256-slot cache in front of its linear search (about 200 twins, twice per copy).
- The `.ff` handle table (1024 slots since 1bc60e4) is searched only up to its highest used slot. That search runs on every `ReadFile` and `CloseHandle` of the game.

Still costly in the 19:07 run, and changed since: see Seed hitches.

### Seed hitches (19:07 run)

Every hitch of 50 ms or more was the first appearance of new art, with seed time up to 65.7 ms in a frame. The 4K is not decoded on the game thread: the pack worker decodes it and the render thread uploads it (`ReplaySeed`). What the game thread did:

| Cost | Evidence | Change |
|---|---|---|
| `SeedLoadWait`: a decode after a 100 ms present gap waited for its 4K | `#216 ... wait 59352 us`, inside the 61.7 ms `OnIndexedBlit` of the 78 ms frame 3574; `#196 ... wait 6940 us` | default 0: the seed is deferred and lands through the streaming path like any other |
| Log lines: `DebugLog` opened, appended and closed the file on the calling thread for every line | 100 to 370 lines in each hitch frame; seed time fits about 30 us a line plus about 3 ms per Mpx decoded | the log has its own writer thread (`StdAfx.cpp`): a line is formatted into a 512 KB buffer under a short lock, the writer appends it to a handle it keeps open at most 50 ms later. A full buffer, a crash (`CrashFilter`) and the unload write it out at once (`DebugLogFlush`) |
| The transfer guard on an indexed blit compared every row in five readings | about 7 ns a pixel, 3 ms for an 800x600 decode | the blit's source pointer already names the decode, so the guard checks every 4th row and the last through the blit's own palette (`XferGuardQuick`), and falls back to the full guard only if that fails |
| The decode snapshot copied the 32 bpp span of an 8-bit decode (2304 bytes a row at pitch 1152) | every `StoreDraw` decode is format byte 0 | an 8-bit decode keeps only its 8-bit rows (half the bytes), and the guard does not try readings it cannot be |
| The seed's key-alpha base: dirty rects reaching into the seed rect were uploaded whole before the seed rect itself was uploaded | the scratch's merged dirty boxes, up to 1152 px wide | `CutRegion` keeps their outer parts dirty and drops the inner part, which the base upload covers |
| `Convert`, which every upload runs on the game thread | 5.4 Mpx in frame 242 | SSE2, four pixels at a time, same rule |
| Late seeds landing together at one present, each converting its base | `SeedPoll` | at most `SEED_POLL_BASE_PX` (1 Mpx) of late bases per present; the rest wait a present, their 1x still in place (a write since the decode drops pieces as before) |
| The stack scan above each indexed blit (2048 slots) | only feeds log lines and the code dump | skipped once those log budgets are spent, unless `Diagnostics=1` |

The 1x stays authoritative while pieces are pending: nothing about pending records, `SeedDerive`, faces or sheets changed except when a late base is uploaded.

The hitch line now says which sub-step took the time (`seed steps:`): `wait for 4K`, `pieces` (`SheetPieces`), `decode capture` (`XferCapture`, the snapshot), `guard`, `base upload`, `stack walk`, `transfer candidates` (`XferResolve`). Steps are exclusive like the cost categories. It also gives the log lines written on the game thread and the time `DebugLog` took for them, and the base px uploaded.

## Streaming

The game thread no longer waits for 4K (`SeedWait=0`, and since the 19:07 run `SeedLoadWait=0` too). A seed whose art is not decoded yet is deferred: the 1x stays, and `SeedPoll` emits the pieces at a later present once the art is there. `SeedLoadWait` can still be set to wait while the game is loading (no present for more than 100 ms), at most 1 s in all between two presents.

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
- **Record seed** (replaces the load seed of b087412, see Record reads). An indexed blit from unnamed indices of exactly the size of a `.ff` record the game just read is seeded at the blit's destination, and the copy out of the scratch carries the 4K, or the pending seed, into the map buffer.
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
| `twin coverage top 1x-only copy sources ...` | every 5 s, up to 4 lines | rank, twin, kind, buffer, size, px, copies, store name, or `~` and the latest named image that reached the twin. This is the coverage checklist |
| `twin arrival copies in [rank] twin ...` | every 5 s, top 4 | count, the first and the last copy in: source twin and rect, 4K or 1x, game caller, the source's counters and its last write there |
| `twin copy in #n` | first 96 | each copy of at least 400x400 or into a priority surface, as above; also from untwinned surfaces |
| `twin indexed blit #n from other memory` | first 32, then each new size and call chain (96 more) | an indexed blit that no store decode, no loader decode and no record read names: source, size, destination, the latest decode, the call chain (6 frames, module+offset) and the stack scan above it |
| `image record read #n` | first 48 | a `.ff` read by the game: archive, record id, file name, PNG size, position, bytes, game or other thread, `ReadFile` caller < its caller |
| `image record reads since start` | every 5 s | `.ff` opens (untracked: table full, no file table; open now, max), reads (records, PNG, sheet or plain, header only, inside a record), reads on other threads, decodes named by a read, reads taken by `StoreDraw`, decodes no read matched, the distinct `ReadFile` callers |
| `twin decode named #n` | first 240 | a named decode: name, size, record, archive, the loader and how the name was found. Store decodes: `StoreDraw`'s caller, first decode of each loaded file. Record reads: the `ReadFile` caller and the blit's call chain, frames between read and blit, other unread reads of the size, the target and the seed outcome or why there is no 4K |
| `twin decode named #n via loader` | first 160 | a blit named by the latest `StoreDraw` decode: name, decode size, record, archive, sheet or plain, pieces (all, and in the blitted part), the part, decode number and age, `StoreDraw`'s caller, the blit's chain and the stack scan above it, the destination twin and pixel, the path (the decode snapshot, or the loader's pieces and why) and the outcome or why there is no 4K. Blits through the decode snapshot are logged from 256x256 |
| `twin decode without 4K #n` | first 32 | a blit of a store decode that has a name but no 4K, with the reason; small loader-named sprites without 4K land here too |
| `twin decode unnamed since start` | every 5 s | unnamed indexed blits (total, in 5 s, distinct sizes and chains), named via loader (sheets, by the decode snapshot, by loader pieces: seeded, deferred, no 4K, no twin; 8-bit decodes captured without a memory twin), record seeds (seeded, deferred, no 4K, no twin), blits of named decodes without 4K, and the top 5 unnamed sizes with count, age and the first 3 frames of their chain |
| `image loader decodes since start` | every 5 s | `StoreDraw` decodes (8-bit), decodes captured without a 32 bpp range (and 32 bpp readings that were unreadable), indexed blits from the latest decode (part of one), from a decode older than 600 frames |
| `twin coverage per screen since start` | every 5 s | per screen (named by its background, `*` = now): share of 4K px, 4K and 1x Mpx, untwinned Kpx; the top 10 by px |
| `twin code dump #n` | when a region was added, at the 5 s log | `twin-code.bin`: regions, bytes, refused regions, exe base and timestamp, each region's exe offset and size |
| `twin trap priority ...` | every 5 s | the priority surfaces with their trapped writes, arms, pairs, skipped quick re-arms, trapped writes marked dirty, `rep movs` hints replayed or failing the guard |
| `twin trap priority write #n` | first 96 | a new writer on a priority surface, with registers |
| `twin trap copy #n` | first 24 | a `rep movs` hint replayed as a twin copy, or why not |
| `twin trap bink` | once | the exports `binkw32.dll+21A13` and `+21B29` are in, and the game's Bink imports |
| `twin seed streaming 5s` | every 5 s | deferred seeds, carried by copies, late landings with latency in frames and ms, frames still decoding or over the upload budget, pieces dropped by writes, waits while loading, key-alpha base Kpx and late seeds held by the base budget |
| `twin replay per command 5s` / `twin replay slowest command 5s` | every 5 s | dispatches, total and max per command type; the slowest command |
| `twin slow replay` | each replay over 50 ms, first 64 | the replay's time and its slowest command |
| `twin off: ...` | once | twins are off: `Twin.Mode=off`, or no 4K pack |
| `image sheet pieces <file> ...` | first 20 sheet seeds, then sheets with uncertain pieces or file-named frames read differently (64), then Events.ff, Faces.ff and multi-part frames of the sheet's size (96) | frames with their parts union, pack entries and the full piece list (see The unit card portrait) |
| `image sheet checks since start` | every 5 s | pieces left 1x as uncertain, and why |
| `image pack offset derived` / `full canvas` / `left 1x` | first 128 entries (32 more for a missing 8-bit decode) | a gamenames entry at 0,0 that cannot start there: entry size, parts union, key box, the offset used and how it was derived (see Zeroed offsets) |
| `image pack offsets zeroed by the repack, since start` | every 5 s | entries placed at the union, at the key box, full-canvas entries kept, left 1x |
| `twin game-thread cost 5000ms` | every 5 s | present interval histogram, ms per present by category, total |
| `twin hitch` | each present interval of 50 ms or more (20 per 5 s) | the frame's twin time by category, the seed sub-steps, log lines written on the game thread and their time, slowest call, seeds, uploads, base uploads, copies, ground blits |
| `image plain seed refused` | one per file (256) | a file whose -IMAGES.OPT parts move pixels was refused the plain (identity) seed, with the first moving part (see Plain seeds never take a packed sheet) |
| `image plain seed check` | first 24 faces and identity-frame files | why the plain seed was allowed: no frame, identity frames, or not verifiable |
| `image plain seeds ... since start` | every 5 s | plain-seed checks, allowed and refused |
| `image pack duplicate name` | first 32, at load | a name the pack holds twice: the kept and the dropped image's size and offset |
| `twin render show interval` | every 5 s | render-thread time between shows, as a histogram |

The log is written by its own thread, at most 50 ms after a line (see Seed hitches); a crash and the unload write the rest at once. It rotates to `DisciplesGL-debug.log.old` at 8 MB, and `test.bat` ships it as `<game>-DisciplesGL-debug.old.log`. In the 14:58 run, Ctrl+F11 also started TextReplace's F11 trace, which flooded the log with `trace bltfast/lock/surface` lines, so the 2 MB rotation lost the first `twin seed #n` lines. Ctrl+F10/F11/F12 now reach only the twin toggles: no F11 trace, no F12 dump, and no BattleLoad bind (F10 remapped to Ctrl+L quick load).

"Carrying 4K" means the source rect meets an art rect tagged with a frame, a seeded sheet, ground, or a merge that included one. The art rects travel with copies and pixel ops, and `ART_MAX` is now 128.

The tile check (`twin tiles check`, `twin repair`), the GPU check (`twin check twin ...`, `twin gpu check`) and the art-tile check (`twin art check ...`, which names `seeded sheet twin N` for seeds) run unchanged in both modes.

## Open gaps

1. **The loader match is by pointer into the latest decode.** If other code wrote the scratch between `StoreDraw` and the blit (none is known; the 17:54 log has none), the blit would take the latest decode's name. The 600-frame limit bounds it, and `image loader decodes` counts the matches. The capture-only seed reads the decode as 16 and 8 bpp; a true-colour decode too big for its 32 bpp reading is still not seeded.
1. **Record reads are assumed, not seen.** If the game memory-maps the `.ff` files or reads them through a CRT DLL, `ReadRecordHook` sees nothing: `image record reads since start` then shows 0 reads, and the unnamed line still names the loader's call chain. `twin-code.bin` holds that code for the next round, which can hook the record read or the loader directly. A record read that is not followed by its decode, then another image of the same size decoded with no read, would take the wrong name; `twin decode named` logs the number of unread reads of the size, and the read is claimed by its first match.
1. **The routine that writes the decode into the surface is still unknown.** The 15:32 run showed it uses no lock. The trap names it in the next run (`twin trap writer`), and its first write into a fresh surface is itself a transfer candidate. If the writer is plainly a copy from the decode scratch (or from another seeded record) into the surface, that EIP is the transfer point to hook directly. The 8-bit reading of the snapshot is still a suspicion until the `twin seed decode` lines show format byte 0.
2. **Flipped records** (`Build` with flipped = 1) use a mirrored sheet. Only the decoded, unflipped sheet is seeded. If the game mirrors with a hooked op, the 4K follows. If it uses an unhooked CPU loop, the tile check repairs it to 1x.
3. **The store's decode buffer is reused at once.** Its own seed only matters for hooked copies out of it (none were seen). A transfer copies the pieces to the surface, which defers on its own, so the `SeedWait` wait only decides how soon the surface shows 4K.
4. **Memory twins evicted by the 256 MB budget** come back as plain 1x when they are touched again; there is no seed without a decode. The same goes for 4K carried into other twins before a rebuild.
5. **Keyed pixel blends** (`PixelKeyed`, `PixelAverage` < 255, `PixelEmpty`) over 4K content with a key other than magenta still decide by texel colour.
6. **Shared blocks** show the first frame's 4K in every frame that uses them. The conflict lines measure how much that matters.
7. **Additive frames**, the turn strip (`TurnOrder.cpp`) and ground tile textures are not seeded.
8. **The seed draw assumes `StoreDraw` overwrites its rect.** If the game ever composites a store image onto existing content through `StoreDraw`, then 4K-transparent texels over opaque background 1x pixels come out transparent.
9. **Streaming risks.** Pieces of a sheet now land over several presents, so a frame can briefly show a mix of 1x and 4K. A derived seed assumes the unkeyed copy replaced its rect; a later keyed copy or write over it drops the pieces it touches.
10. **Trap risks.** A write into a trapped page from kernel mode (`ReadFile` into a surface, a GDI call writing it) fails with an error instead of faulting, so the trap would break it. No such path is known. The VEH also sees every exception in the process, but it returns at once for anything that is not a write AV inside a trapped page. Each fault costs a few microseconds; the page budget bounds it at about 2048 a second plus first writes. `WriteTrap=0`, now the default, turns it off and goes back to heap-allocated surfaces.
11. **Event portraits are still wrong on screen** (`G000UU0013.PNG` in the 19:07 run) for a reason the layout does not explain. See The portrait in terms of this layout. The plain (identity) path did not cause it: every logged event portrait went through `SheetPieces`, and the plain path is now refused for packed sheets anyway (see Plain seeds never take a packed sheet).
12. **No tile check by default.** A CPU write that no hook sees now stays on the GPU twin as stale pixels until the twin is next uploaded. In every logged run the check repaired nothing.
