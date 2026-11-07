# 4K art: handoff

The DLL keeps GPU twins of the game's 32-bit surfaces at N times their size (N = ceil(primary monitor / game mode), 1 to 4, lowered while 12 screens of twin would pass 512 MB or a side would pass 8192). The game thread records every hooked pixel operation as a command. The render thread replays the commands into the twins and shows the screen's twin over the 1x frame. The 1x game memory stays the only truth: the game never reads a twin, and every recorded operation repeats a 1x one, so the worst case is 1x pixels.

4K art enters a twin only where the game decodes a `.ff` record whose name is known exactly: the archive file name plus the record file name from the archive's file table. Nothing looks at pixels, sizes or timing to name anything. A record without an exact name or an `ff4k` entry stays 1x, and the log says so once.

Twins are off when `ff2k-v1.galleanpack` is not next to the game, for the GDI renderer, below GL 3.0, without the 32-bit hook, and in the editor.

## Files

| File | Thread | Holds |
|---|---|---|
| `Pack.cpp/.h` | worker, game, render | Opens `ff2k-v1.galleanpack` (2x art as BC7 blocks, each entry zstd compressed; `tools/repack/ff2k.py` builds it from the 4x WebP pack that `ffpack.py` writes), parses the `Sprites` manifest, and looks entries up by `<ARCHIVE.FF>/<RECORD>` (`Find`) or by record name alone for ground tiles (`FindRecord`). Its worker threads unpack the premultiplied BC7 blocks (128 MB cap) and the render thread uploads them with `glCompressedTexImage2D`, nearest sampled and without mipmaps. Twins run at the pack scale (`PACK_SCALE`, 2), so a seed lands one texel per twin pixel and only the present shaders scale to the screen. `Draw` uploads for at most 4 ms a frame on the render thread and evicts the least recently used textures past half the reported video memory (256 MB to 1.5 GB) |
| `Seed.cpp/.h` | game | The name sources: `.ff` file tables, the image store hooks (`StoreLoad`, `StoreDraw`, `StoreFree`), `FaceLoad`, `OnFaceCopy`, `OnIndexedBlit`. The seed records and their life cycle. Every "image stays 1x" line |
| `Twin.cpp/.h` | game | Twin records keyed by `StateBuffer`, memory twins, dirty rects and uploads, the 4K rect list per twin, the command list, arena and FIFO, and every `On*` entry point the hooks call. `Twin.h` also holds the types that `Seed`, `Ground` and `TwinGL` share (`Cmd`, `List`, `Shared`, `Record`) |
| `TwinGL.cpp/.h` | render | Shaders, the replay of each command type (only consecutive compatible commands are batched), the show pass, `ReleaseGL` |
| `Ground.cpp/.h` | game, render | Records the map ground chunk builds as ops on named tiles and redraws them in 4K (`CmdGround`) |

Callers: `OpenDrawSurface.cpp` (create, release, `Blt`/`BltFast`, colour fill, present), `Hooks.cpp` (pixel routines, indexed blit, ground, faces, `CreateFileA`, `Seed::Load` and `Ground::Load`), `TurnOrder.cpp` (`OnMemory` and `OnWrite` for the strip, which stays 1x), `NewRenderer.cpp` (`Pack::Prepare/Draw/ReleaseGL`, `TwinGL::Prepare/Draw/ReleaseGL`).

## Data flow

1. **Twins.** `OnCreate` makes a surface twin for each 32-bit `StateBuffer`. `Flush` and `SwapBuffers` swap the buffers between surfaces, and the twins move with them. Memory twins exist only where a seed needs one (a 32-bit store decode, a face buffer) or a `DrawFaces` destination needs one (`OnMemory`). They share a 256 MB budget and the least recently used one goes first. Raw pointers are resolved by 2-D containment (`FindAddress`), so a memory twin can be a sub-rect of a larger buffer.
2. **Recording.** Hooks call Twin before they write 1x. A copy uploads the dirty rects of the source that it reads and of the target that it touches, then records `CmdCopy`. Pixel ops record `CmdPixel`, blend rows `CmdMask` (vertically adjacent rows merge), DrawFaces colour layers `CmdTint`, `Pixel_DoubleLighter` `CmdLighter`, and fills, lines and points `CmdFill` (adjacent fills merge). CPU writes only mark dirty rects. A dirty rect uploads its 1x when it is read, drawn into or presented. Uploads encode alpha: key pixels of a keyed twin get 0, and an alpha other than 255 becomes 254.
3. **4K rects.** Each twin keeps up to 64 rects that carry 4K (seeds, ground, and what copies and pixel ops carried there). They decide whether a keyed copy needs an exact mask and whether a source counts as 1x for the log.
4. **Present.** `OnPresent` reads the hotkeys, retries pending ground, polls pending seeds, uploads the screen twin's dirty rects, records `CmdPresent` and appends the list to the FIFO. When more than 64 MB is queued (a stalled renderer), every queued list is dropped and all twins are rebuilt from 1x.
5. **Replay.** `TwinGL::Draw` takes every queued list and replays all of them in order, because twin state accumulates. It then shows the newest present, box-filtered when the twin is larger than the viewport. With a background image it draws only the texels that are opaque in 1x.
6. **Rebuild.** `TwinGL::ReleaseGL` bumps `shared.gen`. The next game-thread call re-creates every twin fully dirty and queues every intact seed again.

## Seeding

| Source | Name | Seed |
|---|---|---|
| 32-bit `StoreDraw` decode | `StoreLoad` maps the store image to a record of the archive's file table (path from the factory, record id minus the archive index times 100000) | the whole record into the memory twin over the decode |
| 8-bit `StoreDraw` decode (every one seen so far) | the same; the decode is remembered as the loader's latest | `Pixel_Blit_Indexed_to_565` calls `Seed::OnIndexedBlit`. When the blit's source pointer lies inside the latest decode at its pitch, the blitted sub-rect is seeded at the blit's destination and samples the matching part of the entry. Blits inside a `StoreDraw` call are judged when it returns |
| Face | the name `FaceLoad` gives, looked up by stem in `Faces.ff` | the whole record into the face buffer's memory twin at its first `DrawFaces`, which gives the pitch |

The entry must have the decode's 1x size. The file table of every `.ff` the game opens through `CreateFileA` is read, so `Faces.ff` can be searched.

A seed record holds the twin, the rect, the place (where the whole entry lands; mirrored when `left > right`) and the entry. Emitting it uploads the rect's 1x with key alpha (alpha 0 where the pixel is `COLORKEY_CHECK`) and records `CmdSeed`, which is drawn with `glBlendFunc(GL_DST_ALPHA, GL_ZERO)`. Key texels stay transparent and the rest take the premultiplied 4K.

- **Deferred.** While the entry is not decoded, the record stays pending and the 1x shows. `Poll` emits it at a later present. Inline uploads of decoded entries are held to 16 MB per present, late key-alpha bases to 1 Mpx per present, and polling stops while the list arena holds 24 MB.
- **Writes.** Any hooked write into a seed's rect drops the record. A repeat decode of the same entry into the same place is skipped while the record is intact.
- **Copies.** An unkeyed copy, or a mirrored face copy, out of a pending seed creates a pending record on the destination, so the 4K lands where the pixels went. Buffers that were presented take none.
- **Redraw.** After a rebuild, every intact record is drawn again.
- **Keyed copies.** A copy keyed on a key other than `COLORKEY_CHECK` from a source that carries 4K becomes a `CmdMask` whose R8 mask is the 1x key test of the source (up to 2048 px a side).

## Ground

`GroundUpdate` wraps each chunk build. The tile and border factories name tiles `<RACE>_<VARIANT>.PNG` and `<RACE>_<BORDER>_<VARIANT>.PNG`, which are looked up by record name alone; a name that two archives hold names nothing. Each blit becomes an op. When the textures of every op are on the GPU, `Twin::GroundArt` records `CmdGround` with a 1x snapshot as the fallback. Otherwise the chunk is pending, and `Ground::Retry` sends it at a later present unless a hooked write touched its twin in between (`Twin::Stamp`). A copy that reads a chunk still being built sends it first (`Ground::Flush`).

## Log lines

| Line | When |
|---|---|
| `twin off: ff2k-v1.galleanpack is not next to the game` | once |
| `image pack ff2k-v1.galleanpack loaded: N entries`, or `could not be opened`, `header not recognised`, `manifest not read`, `not a BC7-ZSTD pack` | once |
| `image stays 1x: no ff4k entry for <ARCHIVE>/<RECORD> (WxH)` | once per record |
| `image stays 1x: the ff4k entry for <key> is WxH, the decode WxH` | once per record |
| `image stays 1x: <key> WxH was decoded before ff2k-v1.galleanpack was loaded` | once per record |
| `image stays 1x: face <name> has no record of that name in Faces.ff` | once per face |
| `image stays 1x: the ff4k entry <key> failed to decode` | once per entry |
| `image pack: BC7 upload of <key> WxH failed with GL error X` | once |
| `image stays 1x: a WxH surface (or memory image) with no 4K at x,y WxH reached the screen through game code at <module+offset> (<how>)` | a copy, pixel op or blend row into a presented buffer from a twin rect that carries no 4K and has no pending seed; once per source twin |
| `image stays 1x: memory no twin covers reached the screen through game code at <module+offset> (<how>)` | the same from untwinned memory or a hooked CPU write; once per address |
| `twin skipped: ...`, `twin texture ... failed`, `twin GL init failed`, `ground GL setup failed`, `twin queue overflow ...` | errors, capped at 16 |
| `image store slots hold ...` | the store hooks do not match the game build |

For copies the address is the Blt caller above the game's DirectDraw wrapper. Otherwise it is the first return address into the game's code on the stack.

## Hotkeys

- **Ctrl+F12** turns the twin display on and off. Off shows the plain 1x frame.

## What stays 1x

- Records with no entry or with an entry of another size, and faces that are not in `Faces.ff`.
- Indexed blits from memory other than the latest `StoreDraw` decode, and CPU writes that no hook sees.
- Turn order strip faces, which `TurnOrder.cpp` draws on the CPU.
- A memory twin evicted by the budget gets 4K again only at the next decode.
- If unhooked code rewrote the scratch between `StoreDraw` and the blit, the blit would take the latest decode's name. No such code is known.
