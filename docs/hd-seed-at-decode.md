# HD twins: seed at decode

This page describes the `seed` mode of the twin renderer. It replaces the old way of deciding which 4K art a copy is: expectations from the render call, identified regions, and pixel hashes in `twin-index.bin`. In seed mode, 4K art is written into a twin only at the moment named 1x art is decoded into game memory. That is the DuckStation-style "replacement at upload". From then on, every copy, pixel op, cache, drag image and popup is a plain twin replay, so the 4K travels with the pixels and nothing downstream identifies anything.

`docs/hd-twin-architecture.md` describes the twin machinery that both modes share.

## Switch

`disciple.ini`, section `[Twin]`:

| Key | Values | Default |
|---|---|---|
| `Mode` | `seed`, `match` | `seed` |
| `SeedWait` | ms the game thread may wait at one decode for the pack worker to decode its art (0 to 2000) | 100 |

The environment variable `DGL_TWIN_MODE=match` overrides the ini. The first log line names the mode and where it came from: `twin mode seed (Twin.Mode=seed, seed wait 100 ms)`.

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

**Deferred seeds.** Pieces whose art isn't decoded in time stay pending. `SeedPoll` runs at every present and emits them once they are ready. It does so only while the record is intact. A record is dropped by any write the twins see in its rect: `MarkDirty`, fills, copies or pixel ops into it, `Repair` from the tile check, `GroundArt`, `OnWriteAll`, or the release or eviction of the twin. The record itself is the seeded flag. Its generation is "no write seen since the decode".

After a rebuild (GL release, queue overflow), Ctrl+F11 or Ctrl+F10, every intact record is queued again, because its 1x provably hasn't changed. `SeedPoll` spreads these redraws over frames (at most 24 MB of list arena per frame), so a rebuild can't cascade into queue overflows. A record pending for more than 3600 frames expires.

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
| `twin seed #n <kind> <outcome> name=... twin ... buf=... base=... rect ... pitch ...` | first 50 events | done, deferred, completed later, or skipped with the reason |
| `twin seed conflict twin ... block ... holds frame A, frame B's 4K ... differs by mean ...` | first 24 | conflicting shared blocks |
| `twin seed store/sheet/face since start: ...` | every 5 s | events, done, deferred (completed later, expired), dropped (memory written, freed), overwritten after seeding, no twin, no art, pieces, px and MB at N× |
| `twin seed sheets since start ...` | every 5 s | sheets, frames, shared blocks, blocks with differing 4K, pairs checked |
| `twin seed records ...` | every 5 s | pending, evicted, redraws, repeats, wait avg/max, replayed commands/pieces, art missing at replay, masked keyed copies |
| `image seed sources since start: ...` | every 5 s | store draws, faces, ranges, sheet archives, ground |
| `twin coverage 5s: ...` | every 5 s | copies from sources carrying 4K, from 1x-only twins, from untwinned memory (count, px) |
| `twin coverage top 1x-only copy sources ...` | every 5 s, up to 4 lines | rank, twin, kind, buffer, size, px, copies, store name. This is the coverage checklist |

"Carrying 4K" means the source rect meets an art rect tagged with a frame, a seeded sheet, ground, or a merge that included one. The art rects travel with copies and pixel ops, and `ART_MAX` is now 128.

The tile check (`twin tiles check`, `twin repair`), the GPU check (`twin check twin ...`, `twin gpu check`) and the art-tile check (`twin art check ...`, which names `seeded sheet twin N` for seeds) run unchanged in both modes.

## Open gaps

1. **Sheets are assumed to arrive through `StoreDraw`.** If a sheet surface is filled some other way (a direct palette decode into the surface, for example), it shows up in the coverage checklist as a `surf` source with store `-`. Those are the next hooks to find.
2. **Flipped records** (`Build` with flipped = 1) use a mirrored sheet. Only the decoded, unflipped sheet is seeded. If the game mirrors with a hooked op, the 4K follows. If it uses an unhooked CPU loop, the tile check repairs it to 1x.
3. **Memory that is copied out and reused at once** (the store's decode buffer) keeps 4K only if the art was decoded within `SeedWait`. Otherwise the seed is dropped when the buffer is reused, and its copies stay 1x until the next decode (`dropped ... memory written`).
4. **Memory twins evicted by the 256 MB budget** come back as plain 1x when they are touched again; there is no seed without a decode. The same goes for 4K carried into other twins before a rebuild.
5. **Keyed pixel blends** (`PixelKeyed`, `PixelAverage` < 255, `PixelEmpty`) over 4K content with a key other than magenta still decide by texel colour.
6. **Shared blocks** show the first frame's 4K in every frame that uses them. The conflict lines measure how much that matters.
7. **Additive frames**, the turn strip (`TurnOrder.cpp`) and ground tile textures are not seeded.
8. **The seed draw assumes `StoreDraw` overwrites its rect.** If the game ever composites a store image onto existing content through `StoreDraw`, then 4K-transparent texels over opaque background 1x pixels come out transparent.
