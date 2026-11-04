# HD twins: architecture and handoff

This page is for an engineer or AI agent picking up the 4K twin renderer with no other context. It describes the code as of commit `97b7cab`. The older design docs (`hd-rewrite.md`, `hd-twins-step1.md`, `hd-pixel-funcs.md`, `hd-hash-identify.md`) explain the intent, but where they disagree with this page the code wins. Section 9 lists the known differences.

> **Work in progress at the time of writing.** While this page was being written, an uncommitted change to `Twin.cpp` appeared in the working tree, about 560 added lines. It includes `INDEX_VERSION 4`, an `Identity.source` field, expectation checks (`ExpectAllowed`, `KnownPiece`, `Provenance`, cached hashes), art tags and a coarser art check in the GPU check. Once it lands, re-check section 4, section 4.5 (index format and entry size), the 5 s lines in section 7, and every `Twin.cpp` line number here.

## TL;DR

- Every 32-bit game surface, and some of the game's own image memory, has a **twin**: a GPU texture at N× (N is 1 to 4). The game thread **records** each pixel operation as a command; the render thread **replays** the commands into the twins, then **shows** the screen's twin over the normal 1x frame.
- The 1x memory stays the **only truth**. The game never reads a twin. Each twin keeps a CPU **mirror** of what it believes the 1x holds; a **tile check** compares that mirror with the real 1x and re-uploads any tiles that differ. The worst case is therefore 1x pixels, never black or wrong art.
- 4K art enters when a copy's source is **identified**. The order is: the game's own render call (an **expectation**), then an **identified region** of the source twin, then a **pixel hash** looked up in a persisted index (`twin-index.bin`). An identified copy records a `CmdArt` that draws the pack texture into the destination twin.
- Map ground is rebuilt in 4K from recorded ground ops (`CmdGround`).
- Almost everything lives in `src/DisciplesGL/Twin.cpp` (5443 lines). `ImageReplace.cpp` supplies the art pack, names, memory ranges and ground ops. `Hooks.cpp` and `OpenDrawSurface.cpp` call into Twin.

## File map

| File | Role for twins |
|---|---|
| `Twin.h` | Public API. Game-thread entry points, plus `Prepare`/`Draw`/`ReleaseGL` for the render thread |
| `Twin.cpp` 122-611 | All state: `Cmd`, `List`, `Record` (twin), `Region`, `IndexEntry`, `Expect`, and the `game`/`index`/`art`/`memory`/`shared`/`gl` blocks |
| `Twin.cpp` 613-3712 | Game thread: recording, mirror, tile check, identification, index, memory twins, entry points, 5 s stats |
| `Twin.cpp` 3714-5442 | Render thread: shaders, replay, show, GPU check, screenshots |
| `ImageReplace.cpp` | Pack (`gamenames-v1.galleanpack` or `assets-v1.galleanpack`), WebP decode worker, GPU texture streaming, image store and memory ranges (`FindMemory`), faces, ground recording, `GroundFlush`, `ExpectArt` from the `Render` hook, F12 |
| `OpenDrawSurface.cpp` | Surface twins (`CreateBuffer`/`ReleaseBuffer`), `Blt`/`BltFast`/colour fill, `Flush`/`SwapBuffers` (present) |
| `Hooks.cpp` | The game's pixel routines, each calling Twin before it writes 1x |
| `TurnOrder.cpp:526` | The turn strip is drawn on the CPU; Twin sees `OnMemory` + `OnWrite` (1x only) |
| `NewRenderer.cpp` | Calls `Twin::Prepare`/`Draw`/`ReleaseGL`. Stops `PixelBuffer::Update` from swapping buffers while twins are active |
| `StdAfx.cpp` | `DebugLog` and the crash handler |

---

## 1. The model

### 1.1 Twins

A twin is a `Record` (`Twin.cpp:233`) in `game.twins[512]`, with a matching `Gpu` slot (`gl.twins[512]`, texture + FBO) on the render thread. Both are addressed by the same small integer **id**.

- **A twin belongs to a `StateBuffer`, not to a surface.** `Flush` and `SwapBuffers` swap `StateBuffer` pointers between surfaces every frame, and the twins move with them. The lookup is `Find(StateBuffer*)` (`:1153`, with a one-entry cache).
- Raw pixel pointers from hooks are resolved by **address** with `FindAddress` (`:1182`). Containment is 2-D (`InTwin`, `:1175`): `ptr` must fall inside `[base, base + pitch*h)` and its column must be `< width`. This matters because memory twins can be sub-rects of a larger sheet whose rows interleave with other images.
- `Record` fields: `owner` (the StateBuffer), `base`, `pitch` (in pixels), `width`, `height`, `mirror` (CPU copy, `width*height`), `dirty[32]` (rects waiting for upload), `regions` (identified art, up to 256), `art[64]` (rects where the twin differs from 1x on purpose; used only by the GPU check), `keyed`/`key` (learned from the first keyed copy *out of* the twin), and `memory`/`anonymous`/`lastUse`.

**Kinds of twin**

| Kind | Created by | Owner |
|---|---|---|
| Surface | `OnCreate` from `OpenDrawSurface::CreateBuffer` (`OpenDrawSurface.cpp:372-380`). Covers 16/32-bpp surfaces the DLL allocates itself, plus the primary's `secondaryBuffer`/`backBuffer`. Surfaces that wrap game memory (`DDSD_LPSURFACE`) get no surface twin | The surface's `StateBuffer` |
| Memory, named | `Adopt` (`:2385`): an operation touches untwinned memory, and `ImageReplace::FindMemory` (`ImageReplace.cpp:1820`) says it is a known store image or face (base, pitch, w, h, pack frame). `CreateMemoryTwin` (`:2319`) uploads it and, when the frame is known, learns it at the source straight away | `new StateBuffer(base)`, non-owning; deleted when the twin is released |
| Memory, anonymous | `OnMemory` (`:2413`): DrawFaces destinations (`Hooks.cpp:3919/3997`) and the turn strip (`TurnOrder.cpp:526`). Any other memory twin that overlaps the region is destroyed first, because its geometry is stale | same |

- Memory twins share a 256 MB budget (`MEMORY_BUDGET`). `EvictMemory` (`:2294`) frees the least recently used one, but never one used in the current frame.
- They are destroyed by `OnMemoryFree(base)` (`:2483`), which only `ImageReplace::DropRange` calls (`ImageReplace.cpp:1772`), and by overlap replacement in `OnMemory`.
- A surface whose `StateBuffer` has no twin, but whose `data` lies inside a memory twin, is reached through `FindByData` (`:2466`): an offset into the memory twin, counted as `addr fallback hits`.

### 1.2 Threads

| Game thread (the thread making DirectDraw and pixel calls) | Render thread (owns the GL context, `NewRenderer`) |
|---|---|
| Owns `game`, `index`, `art`, `learn`, `seen`, `memory`, `trace`. **No locks.** The code assumes one game thread | Owns `gl`. Never touches `Record`s |
| Records commands into `game.list` | `Draw` takes every queued list and replays them in order |
| Hands the list over at `OnPresent` | Publishes counters back into `shared` |

- `shared` (`:492`) is the only cross-thread block. It holds the FIFO, counters and flags (`show`, `tint`, `glFailed`, `gen`), protected by `Lock()`/`Unlock()` (a lazily created critical section).
- `index.items` is also locked when it is mutated, because `SaveWorker` runs on its own short-lived thread.
- `shared.gen` is bumped by `ReleaseGL`. The next `Sync()` on the game thread sees the change and `Rebuild`s: it re-emits `CmdCreate` for every twin and marks them fully dirty (`:1280`).

### 1.3 Command lists, FIFO, arena

- **List** (`:152`): a growable `Cmd[]`, a byte **arena**, and per-frame `Counters`. Payloads that must be snapshotted at record time go into the arena: converted 1x pixels for uploads, mask bytes, ground ops plus a 1x fallback, the 1x screen for the GPU check, and the art rects. The CPU memory will have changed by the time the render thread replays.
- `Emit(type)` (`:708`) appends a zeroed `Cmd`. `Reserve(bytes)` (`:735`) appends arena space.
- **FIFO**:
  - At `OnPresent` the list is sealed and appended to `shared.head/tail` (`:3626-3670`).
  - `Draw` takes the whole chain (`:5245`), replays **every** list, and shows the newest `CmdPresent`. Twin state accumulates, so a list can never be skipped.
  - Lists are recycled into a pool (at most 4, each arena at most 8 MB).
- **Overflow**: if more than 64 MB is queued (`ARENA_LIMIT`, for example a stalled or minimised renderer), every queued list is dropped. `Rebuild(TRUE)` then emits `CmdReset` plus `CmdCreate` for all twins and marks them dirty. All 4K content is lost until it is re-identified.
- If GL init failed (`shared.glFailed`), lists are recycled unplayed and the game shows plain 1x.

**Command fields** (`Cmd`, `:128`):

| Type | dst | src | rect | clip / place | x, y | value | flags | extra |
|---|---|---|---|---|---|---|---|---|
| Create | id | | 0,0,w,h | | | N | | |
| Destroy | id | | | | | | | |
| Reset | | | | | | | | |
| Copy | id | id | src rect | | dst pos | key | 1 = keyed | |
| Fill | id | | rect | | | colour | 1 = key colour, write transparent | |
| Upload | id | | rect | | | arena offset | | |
| Art | id | src id (for the fallback) | dst rect | clip = src rect, place = art canvas in dst coords | | pack frame | 1 flipped, 2 keyed, 4 half | key |
| Ground | id | arena offset of 1x snapshot | rect | | | arena offset of ops | | ops bytes |
| Pixel | id | id | src rect | | dst pos | key | `PixelMode` | param |
| Tint | id | | rect | | | colour | | weight |
| Mask | id | id | src rect (grows by rows) | | dst pos | key | 1 = keyed | arena offset of mask |
| Lighter | id | | rect | | | key | 1 = lighten | |
| Present | screen id or `TWIN_NONE` | arena offset of art rects | map-zoom rect | `clip.left` = last art source (shot) | twin w, h | arena offset of 1x copy | `PresentFlags` | art rect count |

### 1.4 Why 1x stays authoritative

- The game reads its own memory: hover save/restore, roster read-backs, scratch caches. Twin never reads the GPU back into game memory. Unlike DuckStation, there is no VRAM read-back problem at all.
- Every twin operation is a repeat of a 1x operation, so the 1x result is always a valid fallback:
  - unknown writes become dirty rects that upload 1x;
  - a mismatch found by the tile check uploads 1x;
  - a missing art texture at replay falls back to a 1x copy (`ArtFallback`, `:4381`).
- If you add any path where the twin is the *source of truth* for something the game later reads, you break this property.

### 1.5 Scale N

- `ChooseScale` (`:1296`) runs once per process, on the first twin.
- N = `ceil(primary monitor / game mode)`, clamped to 1..4. It is then lowered while 12 screens of twin would exceed 512 MB (`TWIN_BUDGET`), or while a side would exceed 8192.
- N is never re-chosen. Twin memory is **not** counted in the pack's GPU budget.
- `ChooseScale` also loads the index and reads the env var `DGL_TWIN_TILECHECK=0` (tile check off at start).

---

## 2. Data flow per entry point

Every game-thread entry point returns at once unless `Active()` holds (`:673`): renderer is not GDI, GL ≥ 3.0, `bpp32Hooked`, not the editor. Each is wrapped in `Timed` for the "slowest game-thread call" stat.

| Entry | Called from | Drains syncs | Records | Mirror |
|---|---|---|---|---|
| `OnCreate` | `CreateBuffer` | yes | `CmdCreate` + `MarkAll` (full upload at the next flush) | copy of 1x |
| `OnRelease` | `ReleaseBuffer` | yes | `CmdDestroy` | freed |
| `OnCopy` | `Blt` (`OpenDrawSurface.cpp:872`), `BltFast` (`:939`) | yes | `CopyTwins` | `MirrorCopy` |
| `OnFill` | `Blt` colour fill (`:800`, after the CPU clear; a fill is deterministic) | yes | `CmdFill` | `MirrorFill` |
| `OnWrite` | Bink, palette/RGB decode, DrawGround/ClearGround/WaterBorders diamonds, minimap ground, Blt with a clipper, turn strip, fallbacks | yes | `MarkDirty` only (lazy) | at upload |
| `OnWriteAll` | `Flush` 8-bpp path and redraw path (`:471`, `:558`), colour fill with borders (`:807`) | yes | `MarkAll` | at upload |
| `OnPixelCopy` | pixel routines in `Hooks.cpp` | **no** | `CopyTwins` or `CmdPixel` (+ `CmdArt`) | `MirrorCopy` or deferred |
| `OnFillRect` | FillColor, DrawLine/Horizontal/Vertical, `TwinPoint` | yes | `CmdFill` | `MirrorFill` |
| `OnTintRect` | DrawFaces colour layers | **no** | `CmdTint` | deferred |
| `OnBlendRow` | `Pixel_BlitBlend(*)` via `TwinRows` (`Hooks.cpp:2214`) | **no** | `CmdMask` (vertically adjacent rows merge) | deferred |
| `OnLighter` | `Pixel_DoubleLighter` | **no** | `CmdLighter` | deferred |
| `OnMemory` | DrawFaces dst, turn strip | **no** | anonymous memory twin | copy of 1x |
| `OnMemoryFree` | `ImageReplace::DropRange` | yes | `CmdDestroy` | freed |
| `LearnArt` | `ImageReplace::AddRange` (`ImageReplace.cpp:1815`) | yes | upload of the block, index evidence, region | |
| `ExpectArt` | `ImageReplace::ExpectArt` (`:1635`) from the `Render` hook | no Sync at all | nothing (registers expectations) | |
| `GroundArt` | `ImageReplace::SendGround` (`:2310`) | **no**, plus `DrainSyncIn` | `CmdGround` | copy of 1x |
| `OnPresent` | `SwapBuffers` (`OpenDrawSurface.cpp:428`) | yes | screen flush, verify, `CmdPresent`, hand-off | |

### OnCreate / OnRelease

- `CreateTwin` (`:2532`) releases any old twin for the same buffer, finds a free slot, allocates the mirror, emits `CmdCreate` and marks the whole twin dirty.
- On the GPU, `CreateGpu` (`:4099`) makes an RGBA8 texture of w·N × h·N and clears it to opaque black.
- `Release` (`:1331`) emits `CmdDestroy` and frees the mirror and regions. The next `CreateTwin` may reuse the id; command order keeps that safe.

### OnCopy and the shared copy routine `CopyTwins` (`:2757`)

`OnCopy` resolves dst and src through `Find`, then `FindByData`. A source with no twin counts as `unknown`, and the dst rect is marked dirty. The shared routine then runs these steps:

1. Clip `from` to the source and `to` to the target. When the clip changed the rect, the requested dst rect is marked dirty at the end (see suspected bug 3).
2. For a keyed copy, set `source->keyed/key = key | 0xFF000000`.
3. Call `ImageReplace::GroundFlush(src ptr)`: if this reads a ground chunk that is still being built, its 4K command must be recorded first.
4. `FlushRegion(source, from)` uploads the source's pending dirty pixels. `Verify(source, from)` then runs the tile check at the copy source.
5. `FlushRegion(target, to, TRUE)` uploads the target's pending dirty pixels that intersect `to`.
6. `IdentifyArt(..., &plan)` (section 4).
7. **Keyed with art**: `RecordArt` only (art alone), `MirrorCopy`, drop the target's regions, `AddArt`, done.
8. Otherwise (unkeyed, or keyed without art):
   - unkeyed only: `Cover` drops dirty rects inside `to`;
   - flush again;
   - carry the source's art rects onto the target;
   - drop the target's regions;
   - emit `CmdCopy`; `MirrorCopy`.
   - Then, if art was found, `RecordArt` **on top** of the copy.

`PixelCopyInner` also routes into `CopyTwins` for `PixelCopy` (not mirrored), `PixelKeyedCopy`, and `PixelAverage` at full strength (≥255).

### OnFill / OnFillRect → `FillTwin` (`:2889`)

- Clip; `Cover` + flush; drop art rects and regions; `MirrorFill`.
- The fill is merged into the previous `CmdFill` when it has the same dst, colour and flags, and is adjacent to it or contained in it (`fill rects (merged)`).
- If the twin is keyed and the colour equals its key, the fill writes transparent (`flags = 1`).

### OnWrite, dirty rects and the lazy flush

- `OnWrite` (`:2931`) converts `ptr` plus pitch into a rect. If the pitch differs from the twin's, it falls back to full-width rows. The result goes to `MarkDirty` (`:812`).
- `MarkDirty`:
  - drops the regions it touches;
  - merges with an existing dirty rect when the bounding box wastes at most 25%;
  - appends.
- `Append` (`:796`): when 32 rects are pending, it uploads all of them *exactly*, not as one bounding box. A bounding box would wipe art that was drawn in between.
- **Nothing uploads at mark time.** `FlushRegion` (`:1117`) uploads dirty rects:
  - before they are read (as a copy source);
  - before they are drawn into (as a copy or pixel dst);
  - at present (the screen twin).
- With `whole = FALSE` (source reads), a dirty rect that is less than half covered by the read uploads only the overlap and keeps the rest as up to 4 pieces.
- `Upload` (`:908`) snapshots the 1x pixels into the arena through `Convert` (`:852`) and emits `CmdUpload`. It also refreshes the mirror for that rect and removes contained art rects.
- `Convert` (`:852`) encodes alpha so the GPU can tell pixels apart:
  - **key pixel** (when the twin is keyed): `0x00000000`, transparent;
  - **alpha byte other than 255**: alpha becomes 254;
  - **opaque**: unchanged, alpha 255.

### OnPixelCopy modes (`PixelCopyInner`, `:3008`)

`OnPixelCopy` first calls `GroundFlush(src)`. Then:

1. `Adopt(dst)`. If the destination isn't in a twin it is ignored. A pitch or rect that doesn't fit falls back to `OnWrite`.
2. `Adopt(src)`. A source with no twin, or one that doesn't fit, means the dst is marked dirty (`untwinned src`).
3. `DrainSyncIn(source, from)`.
4. Copy-like modes go to `CopyTwins` (above).
5. If `source == target`, the dst is marked dirty (`self`).
6. Flush and verify the source, flush the target, drop the target's regions.
7. `PixelKeyed` at full strength (BlitColorKey with flag·2 ≥ 255) first tries `IdentifyArt` with `half = TRUE`. On a hit, the **art is drawn alone** at weight 0.5, and the call ends with `DeferSync`.
8. A mirrored `PixelCopy` (DrawFaces with `isMirror`) asks `IdentifyArt(mirror = TRUE)` for a plan.
9. Emit `CmdPixel`, carry the art rects, `AddArt` for modes the GPU does on purpose differently from the 1x dither, then optional `RecordArt` on top, then `DeferSync(target, to)`.

| Mode | Hook | GPU replay (`ReplayPixels`, `:4602`) |
|---|---|---|
| `PixelKeyed` | `Pixel_BlitColorKey(_SSE)` | keyed, premultiplied over, weight `0.5 × strength` (half coverage replaces the checkerboard) |
| `PixelAverage` | `Pixel_BlitBlendAvarage(_SSE)` | keyed, weight `(p+1)/256`; at ≥255 it becomes a keyed copy instead |
| `PixelAdd` / `PixelSub` | `Pixel_Add/Sub(_SSE)` | `ONE, ONE` (reverse subtract for Sub), alpha write masked off |
| `PixelEmpty` | `Pixel_BlitEmptyColor` | discard zero pixels, weight 0.5 |
| `PixelCopy` | DrawFaces, minimap objects (no key) | not mirrored: via `CopyTwins`; mirrored (faces): replace, with u swapped |
| `PixelBlend` | DrawFaces mask layer | weight `param/256` over, mirrored like the copy |
| `PixelKeyedCopy` | DrawCastleBuildings, minimap objects | always via `CopyTwins` |
| `PixelTint` | DrawCastleBuildings with a blend line | keyed, `mix(src, colour, 0.5)`, replace |

### OnBlendRow (`:3205`)

- One call per mask row. The row's mask bytes are copied into the arena.
- A row extends the previous `CmdMask` when it has the same src/dst/key, is vertically adjacent, has the same columns, and its arena bytes are contiguous. Otherwise a new `CmdMask` is emitted.
- Replay (`ReplayMask`, `:4821`):
  - the mask texture is R8 at 2048², sampled **linearly**, so 4K edges come out smooth;
  - the keyed variant tests the **destination** against the key, using a scratch copy of dst on texture unit 2.

### OnFillRect / OnTintRect / OnLighter

- **OnTintRect**: `blend(dst, colour, weight)` as a premultiplied fill with alpha `weight/256` (`ReplayFills(tint)`).
- **OnLighter**: copies dst to scratch, then draws it back with `lighter` mode 1 (key → 0) or 2 (also doubles channels when their sum is over 56).
- Both use `DeferSync`.

### OnPresent (`:3488`): seal, tile check, show

1. `Sync()` (drains), then read the hotkeys (section 7).
2. Find the screen twin. If there is none, log `present without twin` at most once a second.
3. Flush all of its dirty rects, then `Verify` the whole screen, skipping rows the flush just uploaded. Mismatches are **repaired** (section 3).
4. Every 5 s (first after 3 s), or on request, copy the whole 1x screen plus the art-rect list into the arena for the GPU check (`PresentCheck`). A request comes from F12, the first art draw, or a screenshot.
5. Emit `CmdPresent` with:
   - the map-zoom rect;
   - flags `MapZoom` / `Back` (background image on) / `Zoomed` / `Check` / `Shot`;
   - the twin size.
6. Frame bookkeeping:
   - `++frameNo`;
   - `ExpireArt()`: **expectations die at every present**;
   - verify timing.
7. Hand off to the FIFO (or overflow handling), write the 5 s stats, and start an index save if one is due.

### GroundArt / GroundFlush (map ground)

`ImageReplace` records each ground chunk build as ops (`OnGround`, `ImageReplace.cpp:2202`):

- `GroundUpdate` (`:3125`) wraps the game's chunk build. `BeginChunk` resets the op list and arms `ground.flush` for that chunk.
- At the end of the build, if every op's tile and mask textures are uploaded, `SendGround` → `Twin::GroundArt`. Otherwise the chunk is marked `pending` and `RetryGround` resends it from the `Render` hook, but only while the chunk's 1x hash is unchanged.
- `GroundFlush(ptr)` (`:2313`) does the same mid-build, when a copy or pixel op reads from the chunk being built.

`GroundArt` (`:2605`) then does the following:

1. `Sync(FALSE)`, find the twin by address (the pitch must match), `DrainSyncIn`.
2. Drop contained dirty rects, flush the rest.
3. **Copy the real 1x into the mirror**. The ground is trusted as built.
4. Snapshot the ops and the 1x pixels into the arena and emit `CmdGround`.
5. Drop regions, `AddArt(area)`.

On replay, `ImageReplace::DrawGround` paints the ops at N× into the chunk area (viewport plus scissor). If textures were evicted meanwhile, the 1x snapshot is uploaded instead (`ground fallbacks`).

### LearnArt / OnMemoryFree

- `AddRange` registers a store image or face range at its real pitch. It is called from `StoreDraw` (`ImageReplace.cpp:3041`) and from the first DrawFaces of a face (`OnFaceCopy`, `:2163`). When the pack frame size matches the image, it calls `Twin::LearnArt(first, pitch, {0,0,w,h}, frame, canvas = same, w, h)`.
- `LearnBlock` (`:2203`) works in these steps:
  1. find or `Adopt` the twin;
  2. drain syncs and flush the block (the decode is still pending as dirty);
  3. hash it, `AddEvidence(weight 100)`;
  4. create a **learned region** for the whole image.
- Partial copies out of the image then hit the region with an offset (this is the fix in commit `97b7cab`).
- `OnMemoryFree` destroys the memory twin whose base equals the freed range's first pixel.

### ExpectArt

`ImageReplace::Render` (`ImageReplace.cpp:3256`) is the hook for the game's image render call. For a named image that isn't nested (`art.depth == 0`), `ExpectArt` (`:1553`) does the following:

1. Resolve the pack frame. If the texture isn't uploaded, request it and skip.
2. Compute `place` (the frame's 1x box, flipped if needed, using the canvas/centre/sprite origin) and `clip`.
3. Skip images under the map-zoom rect.
4. Pass up to 64 parts (`{dst x, dst y, sheet x, sheet y, w, h}` from the image record) to `Twin::ExpectArt`.

Twin only **registers** them (`ExpectParts`, `:1407`); see section 4.1. No commands are recorded.

---

## 3. Mirror, tile check, deferred syncs, ordering

### The mirror

`Record::mirror` is what the twin believes the 1x holds. Every recorded operation must leave the mirror equal to the 1x **after** the operation:

| Operation | How the mirror is updated |
|---|---|
| Upload / Repair | copy of 1x (`Upload`) |
| Copy | `MirrorCopy` (`:1059`), simulated at record time from the **real** source 1x, with the same key test as the CPU (`key | alpha`) |
| Fill | `MirrorFill` (`:1102`) |
| Ground | copy of 1x at `GroundArt` |
| Blend, tint, mask, lighter, other pixel modes | **deferred sync**: copy of 1x after the hook has written, `DeferSync` (`:1258`) |
| Create | copy of 1x |

### Tile check

- `Verify(twin, rect)` (`:1000`) compares real 1x rows with mirror rows (SSE). Each 16-row band with a difference is split into 16×16 tiles, and runs of bad tiles are passed to `Repair` (`:971`).
- `Repair` drops the regions there, `Upload`s the 1x, counts the repair, and logs `twin repair ...` (at most 32 per session, 4 per frame).
- A repair means some 32-bit write reached that memory without a Twin call: an unhooked routine, a mod's `DrawFrame`, `Pixel_BlendSome`, or a sync drained too early.
- It runs:
  - on **every copy and pixel source** rect, before the twin reads it;
  - on the **whole screen** at present.
- It does not scan idle twins.
- It is toggled by Ctrl+F11 (turning it on marks every twin fully dirty) or `DGL_TWIN_TILECHECK=0`. With the check off, mirrors are still maintained.

### Deferred mirror syncs

- Hooks call Twin **before** the CPU write, and some hooks send several Twin calls before they write anything:
  - DrawFaces sends copy, then blend, then tint;
  - `Pixel_BlitBlendWithColorKey` sends every row first.
- So emulated pixel ops can't copy 1x into the mirror at record time. Instead they queue `{owner, id, rect}` in `game.syncs[64]`, and vertical neighbours merge.
- `DrainSync` (`:1224`) copies 1x into the mirror for each queued rect. It is safe only once the writes have happened, so the rules are:
  - **Entry points that are called only between hooks drain** (`Sync()` / `Sync(TRUE)`): `OnCreate`, `OnCopy`, `OnFill`, `OnWrite`, `OnWriteAll`, `OnFillRect`, `OnPresent`, `LearnArt`; `OnRelease` and `OnMemoryFree` call `DrainSync` directly.
  - **Pixel entry points don't** (`Sync(FALSE)`): `OnPixelCopy`, `OnTintRect`, `OnBlendRow`, `OnLighter`, `OnMemory`, `GroundArt`.
  - **Targeted drains**: `DrainSyncIn(twin, rect)` drains **everything** if any queued rect of that twin intersects `rect`. It is used on pixel *sources* (a twin about to be read must have a current mirror), on `LearnBlock`, and on `GroundArt`.
  - **Full queue**: when 64 rects are queued, `DeferSync` drains everything (see suspected bug 1).

### Ordering rules for hooks

1. **Call Twin before writing 1x** for pixel ops and copies. `FlushRegion(target)` uploads the dst's pending dirty 1x so the GPU op lands on the *pre-op* state; called after the write, the op would be applied twice. `IdentifyArt` and `Verify` read the source.
2. **Lazy calls may come after the write**:
   - `OnWrite` only marks dirty (Bink at `Hooks.cpp:1758`, the turn strip);
   - fills are deterministic (`Blt` colour fill, and `TwinPoint`, which the asm at `Hooks.cpp:4104` calls right after the write).
3. **Don't call a draining entry point in the middle of a hook's Twin sequence.** `OnFillRect` is safe in DrawCastleBuildings only because FillColor runs before the copy.
4. **Never reorder commands.** Batching only merges *consecutive* compatible commands, on both sides.

---

## 4. Art identification as implemented

`IdentifyArt` (`:2006`) runs on copies (`CopyTwins`), on full-strength keyed pixel copies and on mirrored pixel copies. Blocks under 8×8 never identify.

### 4.1 Expectations first (context)

- `ExpectParts` stores each part as an `Expect` (`MAX_EXPECT` 32768 per frame, `MAX_IMAGES` 8192), hashed into 8192 buckets by `(dx, dy)`.
- **O(1) dedupe**: a `Seen` table (65536 slots) keyed by `(frame, flipped, dx, dy, sx, sy, w, h)`. Slots are valid only when their stamp equals `seen.stamp`, which `ExpireArt` increments every present, so clearing it costs nothing.
  - If part 0 is already registered this frame and its expectation is still **unused**, the whole call is a duplicate and returns at once.
  - Otherwise each repeated part is **re-armed** (`used = FALSE`), so a second copy of a re-rendered image still gets context.
  - A recent log shows about 200k duplicates per 5 s.
- `Match` (`:1509`), for a non-mirrored copy:
  1. the bucket chain for an unused expectation with the exact `dx, dy, sx, sy` and a size that fits, preferring the exact size;
  2. otherwise a linear scan of 256 entries from `art.head` for an expectation that contains the copy with a consistent offset.
- `PlaceSane` rejects parts lying more than 1 px outside the image's place.
- On a match:
  1. mark it used;
  2. if the copy is the **whole part**, hash the source block and `AddEvidence(weight 1)`. This is learning from expectations;
  3. create or refresh a **region** with this identity, so later cached copies (with no new render call) keep it;
  4. draw with the image's place. This is a `context draw`.

### 4.2 Identified regions

- Each twin holds up to 256 `Region`s (rect → frame, flip, canvas size, offset in the canvas). When full, the least recently used one is replaced.
- A copy whose `from` lies **inside** a region uses that region's identity plus the offset, with no hashing (`region hits`).
- A region with `frame = -1` is a **known miss**, a negative cache that stops repeated hashing.
- Regions are dropped by any write that intersects them: `MarkDirty`, fills, copies or pixel ops into the twin, `Repair`, `GroundArt`, `MarkAll`. `Upload` keeps them, because an upload only brings unchanged 1x.

### 4.3 Pixel hash index

- `HashBlock` (`:1646`): two 32-bit running hashes over RGB. Alpha is ignored; `0xFF00FF` and the twin's learned key both count as 0; `w`/`h` are mixed into the seed. It also reports **solid** blocks (one colour), which are never identified. An `IsBadReadPtr` guard covers geometry that might be wrong.
- Index: open-addressing table keyed by `(hash, w, h)`. It starts at 4096 entries and doubles at 70% load.
- `IndexEntry` (`:194`) holds up to **4 identities** `{frame, cx, cy, canvasW, canvasH, flipped, count}`. Multiple identities exist because the packer dedupes identical 1x blocks across animation frames, so one block can legitimately be several pieces.
- `AddEvidence` (`:1792`) bumps a matching identity, or inserts a new one, replacing the lowest count when all 4 are used.
- `Dominant` (`:1773`) picks the identity with the highest count, with **at least 2** sightings; ties go to the lower slot.
- Weights:
  - learning at the source is worth 100, so it is dominant at once;
  - an expectation sighting is worth 1, so it needs two agreeing sightings (commit `b4fafcc`: wrong art can't stick).
- **Content draw**: with no expectation and no region, hash, then `IndexFind`, then `Dominant`.
  - A hit creates a region and draws (`hits`).
  - A miss or "known but not dominant" creates a known-miss region (`misses`).

### 4.4 Learning at the source vs from expectations

| Source | When | Weight | Notes |
|---|---|---|---|
| `LearnArt` → `LearnBlock` | store images at `StoreDraw`, faces at their first DrawFaces, memory twins created with a known frame | 100 | whole image, `cx = cy = 0`, canvas = image; also creates a `learned` region |
| Expectation match | whole-part copy matching an expectation | 1 | part offset in the image's place |

Images built through the image hook (`Build`) are **not** learned at the source. They rely on expectations.

### 4.5 `twin-index.bin`

- Stored next to the game exe; written as `twin-index.bin.tmp` and then `MoveFileEx(REPLACE_EXISTING)`.
- Layout, little-endian:
  - header: 4 DWORDs `{0x58495754 ("TWIX"), INDEX_VERSION = 3, sizeof(IndexEntry) = 136, count}`;
  - then `count` raw `IndexEntry` records.
- Only entries with at least one identity are written.
- **Load** (`:1839`, from `ChooseScale`):
  - a wrong magic, version or entry size rejects the whole file, which is then logged and starts empty;
  - entries under 8 px or with a bad `idCount` are skipped.
- **Save** (`:1893`):
  - every 60 s or more when `index.dirty`, on a worker thread (`SaveWorker`);
  - also from `ReleaseGL`.
- **Bump `INDEX_VERSION`** whenever the hash function, the meaning of `Identity` (offsets, canvas), or the struct layout changes. A hash change alone doesn't change `sizeof`, so nothing would catch it.

### 4.6 When the 1x copy is drawn under the art, and when the art is drawn alone

| Path | 1x recorded | Art |
|---|---|---|
| Unkeyed copy (`Blt`, `BltFast` without a key, `PixelCopy` not mirrored) | `CmdCopy` first | `CmdArt` over it. The copy replaces the rect, so transparent parts of the art show the source and not stale dst (`:2849`) |
| Keyed copy (`BltFast` with a key, `PixelKeyedCopy`, `PixelAverage` ≥255) | none | `CmdArt` alone. The art's alpha replaces the key test; if the texture is gone at replay, `ArtFallback` does the keyed 1x copy |
| `PixelKeyed` at full strength | none | `CmdArt` alone at weight 0.5 (flag 4) |
| Mirrored `PixelCopy` (faces) | `CmdPixel` | `CmdArt` over it, flipped |
| Every other pixel mode | `CmdPixel` | none; the op applies to whatever 4K content the source twin already holds |
| Ground | none | `CmdGround` replaces the area |

`RecordArt` (`:1536`) refuses when `ImageReplace::ArtReady(frame)` is false; that call also requests the decode. The caller then falls back to plain 1x.

---

## 5. Render side

### Draw (`:5240`)

Called by `NewRenderer` after it has drawn the normal 1x frame, including its own map zoom (`NewRenderer.cpp:695/856`). Then `ImageReplace::Draw` runs (texture streaming only), then `TextReplace::Draw`.

1. Take every queued list.
2. **Save GL state**: program, VAO, `ARRAY_BUFFER`, active texture, draw and read FBO, viewport, scissor box and test, clear colour, blend enable and `BLEND_SRC/DST`, 2-D texture bindings on units 0-2.
3. `InitGL` lazily. On failure, twins switch off for good (`glFailed`).
4. `Replay` each list in order (`:5046`).
5. `Show` if `shared.show`.
6. Restore state, recycle the lists, publish counters.

Not saved but reset explicitly: blend equation (back to `FUNC_ADD`) and colour mask (back to all channels). Left changed: `UNPACK_ALIGNMENT`/`PACK_ALIGNMENT` (4) and `UNPACK_ROW_LENGTH` (0).

`Twin::Prepare` returns TRUE while lists are pending, which forces the renderer to redraw.

**Required renderer behaviour**: `PixelBuffer::Update(..., swap = ready && !Twin::Active())` (`NewRenderer.cpp:401/737`). While twins are active, the renderer must **copy** the frame and must not swap its own `StateBuffer` into the game's rotation. Swapping breaks the StateBuffer-to-twin mapping and causes flashing (commit `cd05d25`).

### Shaders (`InitGL`, `:3767`)

| Program | Use |
|---|---|
| copy (multi-purpose) | copies, uploads (nearest ×N from the 2048² stage atlas), fills (`fill`/`solid`), art (`art`: discard outside [0,1]; `tint`: magenta multiply), keyed (`isKey`: `a < 0.5/255`, or `a ≈ 1` with rgb == key; restores alpha 254 to 255), `empty`, `tinted`, `lighter`, `weight` multiplier |
| mask | `OnBlendRow` (described in section 2) |
| show | box-filter minify to the viewport: average of the covered texels weighted by overlap (footprint at most 8 per axis), plain bilinear at 1 texel. `opaque` (with a background image) discards texels with alpha < 254.5, so the renderer's own 1x-over-background blend shows there |
| check | `texelFetch` at the centre texel of each N×N cell |

**Blending**

| Mode | Used by |
|---|---|
| premultiplied over (`ONE, ONE_MINUS_SRC_ALPHA`) | keyed copies, art, pixel blends, mask, tints |
| replace (blend off) | unkeyed copies, uploads, fills, `PixelCopy`, `PixelTint`, lighter |
| additive (`ONE, ONE`, plus reverse subtract), alpha write off | Add / Sub |

Pack textures are premultiplied at decode (`ImageReplace.cpp:1288`), except frames marked `additive`.

### Batching

| Command | Merged into one draw when consecutive and |
|---|---|
| Copy | same src, dst, keyed, key (self-copies go through a scratch FBO, one at a time) |
| Pixel | same src, dst, mode, key, param |
| Fill / Tint | same dst, colour, flags, extra |
| Art | same dst, pack frame, half flag |
| Upload | any run: each rect is shelf-packed into the stage texture with one `glTexSubImage2D`, then one draw per dst |
| Mask / Lighter / Ground | one draw each (mask rows were already merged on the game thread) |

### Show (`:5134`)

- Draws only the centred `frameWidth × frameHeight` part of the screen twin into the renderer's FBO.
- When map zoom is active (and the game isn't zoomed), it leaves a hole for the zoom rect, so the renderer's 1x map zoom shows through.
- GPU time is sampled every 256th show with `glFinish`.
- If there is no valid twin (`show invalid/gone`), nothing is drawn, so the 1x stays visible.

### GPU check (`Check`, `:4463`)

1. Render the screen twin down to 1x with the check program.
2. `glReadPixels` the result and compare RGB per pixel with the 1x copy in the arena, in 16×16 tiles.
3. Pixels inside the union of the twin's art rects are classified separately:
   - a tile fully covered by art counts as an art tile ("differ from 1x" is expected there);
   - any other tile with a difference is **bad**.
4. Log `twin check ...` with the 10 worst tiles. The result is mirrored into `shared.check` for the 5 s line.

### Screenshots (`Shots`, `:5032`)

Taken once, 15 s after the first art draw, and on every F12. Written next to the exe:

- `twin-1x.png`: the 1x screen;
- `twin-screen.png`: centre crop of the screen twin at N×, at most 1600×1200;
- `twin-sheet.png`: top-left crop of the twin last used as an art source, at most 2048².

---

## 6. Invariants, gotchas, risks

### Invariants (things that break if you change them)

- **1x is authoritative.** Never make the game read the twin. Never skip a 1x write because the twin "has it" (the old matching code did, and the result was black areas).
- **Twin calls come before CPU writes** for copies and pixel ops (section 3). Pixel entry points must not drain syncs.
- **Every recorded op must update the mirror**, or the tile check will "repair" (wipe) its 4K result the next time that rect is read or shown.
- **Command order is the semantics.** Batch only consecutive commands. Arena snapshots are mandatory for anything read from CPU memory.
- **Twins are keyed by `StateBuffer*`.** Anything that swaps or replaces a twinned `StateBuffer` without `OnRelease`/`OnCreate` desyncs the twins. This is why `PixelBuffer` must not swap.
- **Alpha encoding**:
  - A = 0 means key or transparent;
  - A = 254 means the 1x alpha was not 255;
  - A = 255 means opaque.

  `Convert`, the fill colour rule (`:4761`), the `isKey` shader and the show `opaque` threshold all depend on this encoding.
- **Expectations live for one frame** (`ExpireArt` at present). Context can't be carried across frames except through regions.
- **Index format**: bump `INDEX_VERSION` on any change to the hash or to identity semantics.
- **`Active()` must be constant for a run.** Most paths, `Release` included, check it.
- **One game thread.** No locking protects `game`/`index`/`art`.
- `DebugLog` formats into a 512-byte buffer with `wvsprintf`. Keep log lines under about 450 characters.

### Gotchas

- `ExpectArt` only registers expectations when the pack texture is already uploaded. On the first frames of a new image, the copies draw 1x; art arrives a frame or more later.
- Known-miss regions persist until the twin is written. A block that misses once stays 1x in that twin until it is rewritten, unless an expectation arrives.
- Ctrl+F11 (tile check on) and any queue overflow mark **every** twin dirty, which wipes all 4K content until it is re-identified.
- `game.bytes` (shown as "twins … MB") already exceeds the 512 MB `TWIN_BUDGET` in real logs (158 twins, 532.5 MB at N=4). The budget only picks N; nothing enforces it. The pack has its own budget on top (up to 1.5 GB).
- Self-overlapping copies differ: the CPU smears row by row while the GPU copies through a scratch. The tile check repairs the difference.
- With `Ctrl+F10`, only art and ground drawn after the toggle are tinted, because the tint is applied at replay.
- `FindAddress`/`Find` are linear scans over up to 512 twins (with a one-entry cache). Many memory twins make every pixel op slower.

### Known gaps / TODO

- **Turn strip**: drawn on the CPU (`TurnOrder.cpp`), uploaded at 1x. The 4K faces and gems planned in `hd-rewrite.md` are not done.
- **No learning at `Build`**: images built through the image hook are identified only through expectations.
- **Game `ImageFree` doesn't release memory twins.** Only store and face range drops do (suspected bug 9).
- **N** comes from the primary monitor, once per run. The design wanted it from the viewport, rebuilt on resize.
- **Dead state**:
  - `learn.*` (`FrameLearned`, `CountLearned`, `MarkMulti`, `pieces`; about 3.4 MB of statics) is left over from the removed "learned image" fast path;
  - `index.learnedExpect`, `index.conflicts` and `learn.skipped` are never incremented;
  - `art.cachedDraws` is counted but never logged.
- Twin GPU memory isn't in `ImageReplace`'s GPU budget.

### Suspected bugs (not fixed)

1. **A full sync queue drains mid-hook.** (`Twin.cpp:1272-1273`) `Pixel_BlitBlendWithColorKey` sends all its rows before writing any (`Hooks.cpp:2243`). Rows with different column spans don't merge in `DeferSync`, so more than 64 of them drain the queue before the CPU write. The mirror then gets pre-write 1x, and the next verify "repairs" those rows, putting 1x over the 4K blend. *Medium confidence.*
2. **The `MarkDirty` merge can cover art.** (`:830-838`) Up to 25% of a merged bounding box was never written. If 4K art was drawn there since, the later upload replaces it with 1x. `Append`'s comment shows the hazard is known, but the merge still has it. *Medium-low.*
3. **Clipped copies wipe their own result.** (`:2853-2863`) When `from` was clipped, the whole requested dst rect is marked dirty, `to` included, so the copy and the art just recorded are overwritten by a 1x upload at the next flush. The keyed-art path returns at `:2810`, before this check, so it behaves differently. CPU `Blt`/`BltFast` don't clip, so this only bites when a rect exceeds a twin. *Medium-low.*
4. **The GPU check can verify nothing.** (`:886-890`) When a twin's art list overflows 64 rects, `AddArt` collapses it to one bounding box. The GPU check then treats the whole screen as art. Current logs show `bad 0 of 0 non-art tiles`, so the check is silently verifying nothing. *High (seen in logs); diagnostic only.*
5. **`FindByData` ignores the surface pitch.** (`:2466-2481`, used at `:2733/2741`) A surface wrapping memory inside a memory twin is copied using the twin's pitch, while the CPU `Blt` uses the surface's `mode.width`. If they differ, the twin copies the wrong pixels until the tile check repairs them. *Low-medium.*
6. **Stale regions after a keyed self-copy.** (`:2807`) The keyed art path skips `DropRegions(target)` when `target == source`, so identified regions over the overwritten rect survive. *Low.*
7. **`OnWrite` fallbacks drain syncs mid-hook.** (`OnWrite` at `:2937`, reached from `:3028`, `:3146`, `:3223`) `OnWrite` drains syncs, and it is called as a fallback from inside pixel sequences, before the hook's write. *Low.*
8. **A double flush makes `Cover` dead.** (`:2799` vs `:2816`) The target flush before `IdentifyArt` uploads dirty rects that an unkeyed copy is about to cover completely, so the `Cover` at `:2816` never has anything left to drop. This is wasted uploads, not wrong pixels.
9. **Stale memory twins over freed memory.** Anonymous twins and twins over game image memory are never released when the game frees that memory (`ImageReplace.cpp:3084` doesn't call `OnMemoryFree`). `Upload`/`DrainSync`/`Verify` read `twin->base`, which can touch freed memory, or decommitted memory for large blocks, and crash. Only `HashBlock` and `CreateMemoryTwin` guard with `IsBadReadPtr`. *Risk.*
10. **Additive frames get the wrong blend.** Pack frames marked `additive` are not premultiplied (`ImageReplace.cpp:1288`), but `ReplayArt` always blends premultiplied over (`:4448`). The result is wrong if such a frame is ever identified. *Unverified.*
11. **`HashKey` depends on history.** (`:1698`) The key used when hashing depends on whether the twin has seen a keyed copy yet, so learn-time and lookup-time hashes can differ for keys other than magenta. *Low.*
12. **Concurrent saves.** `ReleaseGL` (`:5392`) can run `SaveIndex` while `SaveWorker` is running. The second save fails to open the tmp file and logs `save failed`. Benign.
13. **Hotkey overlaps.**
    - *Fixed.* Ctrl+F12 also fired the plain F12 dump, and Ctrl+F11 started TextReplace's F11 surface trace, which flooded the log and rotated the start of the run away. Both now ignore the key while Ctrl is held.
    - *Fixed.* F10 is the default `BattleLoad` bind, which remaps to the stock Ctrl+L quick load; with Ctrl held, Ctrl+F10 loaded `BattleSave.sg` on the map. `Config::IsTwinChord` makes `FindBind` skip Ctrl+F10/F11/F12, and the window procedure swallows their key-down, so neither binds nor the DLL's own F-key options see them.
14. **Zero-height blits can loop near-forever** (outside Twin). `Hooks.cpp:2323` and `:2385` guard with `size->cy < 0` instead of `<= 0`, so a zero-height call runs `do … while (--height)` about 4 billion times.

---

## 7. Diagnostics

All output goes to `DisciplesGL-debug.log` next to the game exe. It is rotated to `DisciplesGL-debug.log.old` at 8 MB (`StdAfx.cpp`; it was 2 MB until the seed transfer round).

### Hotkeys (polled in `OnPresent`, `:3506-3532`, except F12)

| Key | Effect |
|---|---|
| Ctrl+F12 | Show twin ↔ show plain 1x (`twin display twin/1x`). The twins keep replaying |
| Ctrl+F11 | Tile check on ↔ off. Turning it on marks every twin dirty |
| Ctrl+F10 | Art drawn magenta-tinted, ground green-tinted (`twin art tint`) |
| F12 | In `ImageReplace::Render`: `MapOrder::Dump`, the `twin F12 …` line, a GPU check and screenshots on the next present, `imagecoverage.txt`, and the `image …` lines. Creating a file named `imagetrace.flag` next to the exe does the same (polled each second) |

### The 5 s summary (`LogInterval`, `:3377`)

Counters reset every interval unless noted.

| Line | Fields |
|---|---|
| `twin stats` | **frames**: presents handed over. **copy/keyed/fill/upload/px/writes/unknown**: commands recorded: copies (keyed subset), fill commands (merged ones aren't counted), uploads and their pixel count, `OnWrite` hits, copies from an untwinned source. **replays/draws/calls**: lists replayed, `Draw` calls that replayed something, GL draw calls. **queuedMax**: deepest FIFO. **replay avg/max**: CPU time issuing GL per `Draw`. **twins N MB**: live twins and estimated GPU bytes (memory twins included). **N**. **resets**: queue overflows |
| `twin tiles check` | on/off, `Verify` cost per frame, **repaired** tiles/rects/px, and the top 3 twins by repairs. Any repair means an unhooked write |
| `twin art draws` | **draws/px**: `CmdArt` recorded and their 1x area. **notready**: identified, but the texture wasn't uploaded. **region hits**. **hash lookups/hits/misses**. **learned expect** and **conflicts**: always 0. **source**: `LearnBlock` learns. **rejected**: place or canvas sanity. **index entries** (cumulative). **expectations** (registered, dup, used, overflow). **replayed/calls/fallbacks**: render-side art quads, draw calls, 1x fallbacks |
| `twin index identities` | multi-identity entries (cumulative), context draws vs content draws, evidence calls, `LearnArt` calls |
| `twin slowest game-thread call` | slowest `Timed` scope and its µs, `ExpectArt` cost per frame, hashes of 64×64 or more that took over 500 µs |
| `twin memory twins` | count and MB (current); created, anonymous, replaced (overlap), evicted, refused (unreadable); addr fallback hits (`FindByData`); art drawn from memory twins |
| `twin pixel ops` | pixel ops per mode with a twinned dst; fill rects (merged); tint rects; blend rows (merged into the previous mask); lighter; art; untwinned src; self; fallbacks (to `OnWrite`); render-side pixel commands replayed and draw calls |
| `twin ground arts` | `CmdGround` recorded, replayed in 4K, fallen back to the 1x snapshot, no twin (or pitch mismatch) |
| `twin show shown` | shows (opaque vs over the background image); hidden because of the 1x toggle, invalid (no twin at present / twin gone), other; show GPU µs (sampled) |
| `twin gpu check` | age, twin, bad of non-art tiles, px, art tiles (and how many differ from 1x, which is expected); or `none yet` |

### One-off and capped lines

- **Start**:
  - `twin scale N=…` and `twin display config …`;
  - `twin index loaded/none/rejected`;
  - `twin first present …`;
  - `twin GL ready` / `GL init failed`, `shader compile failed`.
- **Twin lifetime**:
  - `twin create/destroy` (first 64, plus 64 screen-sized);
  - `twin skip … too large/no free slot`, `twin mirror … out of memory`, `twin texture … failed`;
  - `twin rebuild after GL release`, `twin GL released`, `twin queue overflow`.
- **Per event, capped**:
  - `twin present without twin` (at most 1 a second);
  - `twin repair` (32 per session, 4 per frame);
  - `twin art …` (32, 4 per frame), `twin art from memory twin` (16), `twin art not ready` (24);
  - `twin expectation rejected place` (16);
  - `twin copy from source-learned region` (16);
  - `twin learn at source …`, `… refused`, `… solid` (16 together), `twin learn rejected at source` (32);
  - `twin memory twin … upload recorded/MISSING` (16 named, 16 anonymous), `… refused` (16);
  - `twin ground no twin` (8);
  - `twin trace …`: the outcome of a copy or pixel op touching a memory twin (24);
  - `twin clipped keyed copy` (16), `twin untwinned src … FindMemory …` (24), `twin slow hash` (32).
- **Results and output**:
  - `twin check twin …` (every GPU check);
  - `twin show viewport …` (on change, 32);
  - `twin shot written/failed/skipped`, `twin index saved`;
  - the hotkey toggles.
- **ImageReplace**: `image art name=…` (32), `image ground art chunk=…` (16), `image pack loaded/not found`.

### Crash handler (`StdAfx.cpp:104`)

- Armed in `DllMain` and again in `NewRenderer::Begin`, as the unhandled-exception filter. It runs once.
- It logs:
  - `CRASH code … at … module+offset thread`;
  - for access violations, the read/write/execute address;
  - all registers;
  - up to 32 stack values that point into code (`module+offset`).
- It writes `DisciplesGL-crash.dmp` (MiniDump type 0x40, indirectly referenced memory), then chains to the previous filter.
- Resolve offsets with `build\C4dll-R.map`, which `test.bat` ships.

### How `test.bat` ships logs

1. `git pull --rebase --autostash`.
2. `build.bat`: MSBuild Release Win32 into `build\`. For each of the four game folders under `G:\games\` it renames the old log to `DisciplesGL-debug.prev.log`, deletes the old dump, and copies `C4dll-R.dll`. It finishes with the D2MT check.
3. Wait while the user plays.
4. Clear `logs\` of old per-game files, and copy the map file.
5. For each game, copy into `logs\<d2|rote|dzieje|semga>-…`: the **last 60000 lines** of `DisciplesGL-debug.log`, `.prev.log` and the rotated `DisciplesGL-debug.log.old` (as `<game>-DisciplesGL-debug.old.log`), the three PNGs, the crash dump, and `imagecoverage.txt`.
6. Commit `logs: test run <date>` and push.

`test.bat send` skips the build. It does not ship `twin-index.bin`. `build.bat` deletes a stale `DisciplesGL-debug.log.old` before the run, so the shipped one is always from this run.

---

## 8. How to work on it

- **Ground rules** (from `hd-rewrite.md`):
  - never launch, drive or close the game; the user tests;
  - don't commit unless asked;
  - no comments by default, only for a non-obvious why.
- **Build and test is MSVC on Windows**, by the user: `build.bat` / `test.bat`, VS 2022, Release, Win32. Their logs come back in `logs/` through git. After a change, read the latest `logs/d2-DisciplesGL-debug.log`.
- **Syntax check here** (Linux):

  ```
  /tmp/claude-0/-home-user-DisciplesGL-Modern/bd460338-a0e4-562d-bf5f-b44927248318/scratchpad/check.sh [Twin.cpp ImageReplace.cpp …]
  ```

  - It runs clang (`--target=i686-w64-mingw32 -fsyntax-only -fms-extensions`) against MinGW and Windows headers kept next to the script (`ci/`, `cxx/`).
  - It prints only errors that are **new** compared with a stored baseline (`base/<file>.txt`), and ends with `check finished`. No arguments means every `.cpp`.
  - It does **not** verify MSVC inline asm (`__asm` blocks such as `hook_005383F7`), and it can't link.
  - The path lives in one session's scratchpad. A fresh session may not have it; rebuild it from `chk.sh` beside it, or ask the user.
- **Reading the logs after a change**:
  - `repaired` > 0 means a write Twin didn't see;
  - GPU check `bad` tiles mean the twin diverged from 1x outside art (once suspected bug 4 is fixed, this becomes meaningful again);
  - `notready`/`fallbacks` mean texture streaming;
  - `untwinned src` / `twin trace` lines mean identification or memory-twin coverage;
  - `queue overflow` / `queuedMax` mean the render thread is falling behind;
  - `slowest game-thread call` means stutter.
- **Adding a new pixel routine**:
  1. Call the matching `Twin::On*` in the hook *before* its 1x write.
  2. If the operation can't be represented, call `OnWrite` instead. That gives an exact 1x upload.
  3. If it is emulated, make sure its mirror is updated (directly, or with `DeferSync`) and that it calls `Sync(FALSE)`.
  4. Add a replay case and keep the batching consecutive-only.
- **Testing a change without art**: Ctrl+F12 compares against plain 1x, and Ctrl+F10 shows exactly which pixels came from 4K art.

## 9. Where the older docs are out of date

- `hd-pixel-funcs.md`: every "GPU todo" row is implemented now (mask, lighter, fill rects, lines, castle tint, faces, point plotter). Minimap objects are pixel copies, not uploads. The FillColor no-braces leftover in `hd-rewrite.md` is fixed (`Hooks.cpp:3214`).
- `hd-hash-identify.md`:
  - expectations still decide what is drawn, and they come **first** (commit `d9f69e8`);
  - entries hold up to 4 identities with counts, instead of "first identity wins";
  - `Build` learning and the `ImageFree` → `OnMemoryFree` wiring were not done.
- `hd-twins-step1.md`:
  - N comes from the monitor, not the viewport;
  - twin memory isn't in `GpuBudget`;
  - the 1x/twin toggle is Ctrl+F12;
  - the tile check (step 2) runs at copy sources and at present, not over every twin.
