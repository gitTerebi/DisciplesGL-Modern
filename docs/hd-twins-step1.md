# Step 1 design: 1x twins

This is for step 1 of `docs/hd-rewrite.md`. Every game surface gets a GPU twin at N× scale. Every game write is repeated on the twin, and the screen's twin is what gets shown. In this step the twins hold only 1x content, so the game should look exactly like plain 1x.

## What I found in the code

- **Game thread and render thread are separate.** The game writes plain CPU memory (`StateBuffer::data`). `Flush` hands a finished `StateBuffer` to the render thread, which owns the GL context (`NewRenderer.cpp`) and uploads it through `PixelBuffer::Update`. So the game thread cannot touch GL. Every twin operation has to be **recorded** on the game thread and **replayed** on the render thread. DuckStation does the same with its GPU thread and command FIFO.
- **Screen memory moves around.** `Flush` (non-redraw path) and `SwapBuffers` swap `StateBuffer` pointers between the secondary surface, the primary's `backBuffer` and `indexBuffer`/`secondaryBuffer`. A surface's memory address changes from frame to frame. So **a twin belongs to a `StateBuffer` (the memory), not to an `OpenDrawSurface`.** When the buffers swap, their twins go with them.
- **The 1x copy stays authoritative.** Everything keeps running at 1x exactly as today. The twin is only for display. This is the one big way D2 is easier than a PS1. DuckStation has to read VRAM back from the GPU when the CPU reads it. We never do, because the game reads the 1x copy. That is why hover save/restore and roster read-backs can't go black under this method: the game reads 1x, and the twin repeats the same copies.
- **Writes reach memory in three ways:**
  1. **DirectDraw `Blt`/`BltFast`/colour fill** (`OpenDrawSurface.cpp`). These have a source and destination surface, rects and an optional colour key. They become twin→twin copies.
  2. **Pixel hooks in `Hooks.cpp`** that take a raw `dstData` pointer and a pitch: `Pixel_Blit_Indexed_to_565`, `Blit_By_Masks`, `Blit_RGB_to_565`, `RGB_Swap`, `BlitBlend(_SSE)`, `BlitBlendWithColorKey(_SSE)`, `BlitBlendAvarage(_SSE)`, `Add/Sub(_SSE)`, `BlitColorKey(_SSE)`, `BlitEmptyColor`, `DoubleLighter`, `DrawMinimapObjects`, `FillColor`, `DrawWaterBorders`, `DrawGround`, the clear-diamond hook, `DrawSymbol` (glyphs) and `DrawFaces`. Their destination is either a DirectDraw surface's memory or the game's own image memory (`BlitObject::data`).
  3. **Direct writes through a `Lock` pointer.** These are never hooked. Step 2's tile check catches them.
- `PixelBuffer::Update` already compares frames in blocks. The tile check in step 2 can reuse its compare loops.
- `GLib` has FBOs and `glReadPixels`, but no `glBlitFramebuffer`. All twin operations will be textured quads drawn into FBOs, which also covers colour keys.

## Design

### New module `Twin.cpp` / `Twin.h`

**Registry (game thread).**
- A `Twin` record: `StateBuffer* owner`, `BYTE* base`, `pitch`, `w`, `h`, `id`, a dirty-rect list, and a `keyed`/`key` snapshot.
- It's created in `OpenDrawSurface::CreateBuffer` for every 32-bpp surface buffer: primary, secondary, `backBuffer`/`secondaryBuffer` and offscreen surfaces. It's released in `ReleaseBuffer`.
- An address-range lookup (a sorted array of `[base, base+pitch*h)`) turns a raw `dstData` from a pixel hook into a twin plus x/y.
- If a pointer is in no twin, it is the game's own image memory. That isn't twinned in step 1: writes into it are ignored, and when it is later copied into a twinned surface, that copy counts as an unknown 1x write. Step 3 decides which game images get twins (turn strip, faces).

**Command recorder (game thread).** The commands are POD records in a per-frame list:

| Command | From | What the render thread does |
|---|---|---|
| `Create(id, w, h)` / `Destroy(id)` | CreateBuffer / ReleaseBuffer | Allocates or frees an RGBA8 texture of size w·N × h·N and its FBO |
| `Copy(src, srcRect, dst, x, y, keyed, key)` | Blt / BltFast | Draws a quad from the src twin into the dst FBO. A keyed copy discards key texels. Overlapping self-copies go through a scratch texture |
| `Fill(dst, rect, colour)` | Blt COLORFILL, FillColor | Scissored clear |
| `Upload(dst, rect, arenaOffset)` | dirty flush (see below) | Packs the 1x pixels into a staging texture, then draws them nearest-neighbour ×N into the dst FBO |
| `Present(id, left, top, w, h)` | `Flush` | Marks which twin and region to show for this frame |

**Unknown writes become dirty rects.** In step 1, every pixel hook just calls `Twin::OnWrite(dstData, pitch, rect)`, which adds the rect to the destination twin's dirty list. This covers blends, add/sub, glyphs, ground and faces. Nothing gets uploaded right away. Dirty rects are **flushed lazily**, the way DuckStation tracks dirty VRAM:
- Before any `Copy` that reads a dirty region of its source, an `Upload` is recorded first, so the ordering is right (for example: write to scratch, copy to screen, overwrite scratch).
- Before any `Copy`/`Fill` that fully covers a dirty rect of the destination, that rect is simply dropped.
- At `Present`, whatever is left dirty on the screen twin gets uploaded.
- An `Upload` copies the 1x pixels into the frame's arena **at record time**, because the CPU memory will have changed by the time the renderer replays it.

**Handing frames over.**
- At `Flush`, the frame's command list is sealed and pushed onto a FIFO, protected by a critical section.
- The render thread replays **every** pending list in order, then shows the newest `Present`. Twin state builds up over time, so a frame the renderer skips can't drop its commands. (Today's slot system works per frame and keeps only the latest, which is wrong for this.)
- If the FIFO grows past a limit, for example a stalled renderer or a minimised window, the twins are dropped and rebuilt. Each one is marked fully dirty, so it re-uploads from 1x the next time it is read.

**Batching on replay.**
- Consecutive `Copy`s with the same src/dst/key mode become one draw call: one VBO of quads.
- All `Upload`s in a frame share one staging texture, with one `glTexSubImage2D` per frame or a few rows of an atlas, then one draw per destination twin.
- F12 shows the number of commands and draw calls, so we can see right away whether thousands of small blits are a problem (the speed risk in the handoff).

**Choosing N.** N = ceil(viewport / game resolution), clamped to 1..4, and fixed when a twin is created. If the viewport size changes, twins are rebuilt as fully dirty. Twin memory counts against the same GPU budget as the pack (`GpuBudget`). If the budget runs out, N is lowered.

**Present (render thread, `NewRenderer.cpp`).**
- When twins are active, `Replay()` runs before drawing. Then the screen twin texture is bound where `textureId.primary` is bound today, and drawn with linear filtering to the viewport. The FSR/xBRZ upscale filters are skipped, because the content is already at N×.
- Borders, `isBack`, wide battle and map zoom keep their current code, but sample the twin. Map zoom's source rect gets multiplied by N.
- TextReplace keeps drawing on top unchanged.
- `Prepare` reports "changed" whenever a list was replayed, so the frame gets redrawn.

**Off switch.** Twins run only under the OpenGL renderer at 32 bpp, outside the editor. That's the same condition as `ImageReplace::Enabled` today. A hotkey switches between showing the twin and showing the 1x frame, so you can compare them directly while testing. GDI and the old renderer stay at 1x.

### Removing the matching code from ImageReplace in step 1

The user asked for the matching to be ripped out, so it goes now and not in step 3. Without it, step 1 really is plain 1x, which is what "identical" requires.

- **Delete:** `Item`/`Expect`/`DrawItem`/`Slot`, `Claim`/`Match`/`Take`/`Inside`/`QueueParts`/`ResetExpect`, `Cover`/`PlaneEnsure`/the hole-punch plane and `gl.planeTexture`, all the `Strip*` functions (`OnStrip`, `FollowStrip`, `ClaimStrip`, `FindStrip`, `StripMatches`, `DropStripLinks`), `ClaimStore`, `ClaimGround`, the stale checks, `OnPresent`'s slot building, the `drawList` half of `Prepare`/`Draw`, and `DumpItems`.
- **Keep:** the manifest/pack, WebP decoding, the decode/upload/evict streaming and GPU budget, `Entry`/`Find`/`Resolve`, the image store (`StoreImage`, archive lookup), the face hooks (`OnFaceCopy`, `TakeFace`, `VerifyFace` and the rest), ground recording (`OnGround`, chunks, ops, `RenderGround`), the `Hooks.cpp`/`Hooks.h` address tables, and TextReplace.
- **Callers:** in `Hooks.cpp`, the `if (ImageReplace::OnPixelBlit(...)) return;` early-outs go away, because the 1x write must always happen. `OpenDrawSurface::BltFast`/`Blt` call `Twin` instead of `ImageReplace::OnBlit`. `ImageReplace::OnStrip` disappears from `TurnOrder.cpp`; it's already a no-op there.
- **Temporary loss:** the 4K art goes away until step 3. That's expected: step 1 is about proving the twins are correct and fast enough.

### F12 counters
- Twins alive, twin MB and N.
- Per frame: Copy / keyed Copy / Fill / Upload commands, uploaded pixels, draw calls after batching, and how many frames are queued.
- Replay time on the render thread, in ms.
- **A debug check, toggled from F12:** read the screen twin back, take every Nth texel and compare it with the 1x frame in 16×16 tiles, then log the count and the worst tiles. It only reports in step 1; step 2 turns it into the per-frame repair. It shows straight away which writes still aren't hooked.

## Open questions before coding
1. **Colour key and alpha.** Step 1 compares the key on 1x texels, which is exact. In step 3, 4K art written into a keyed scratch surface needs real alpha, or the edges will pick up a fringe of the key colour. Plan: in a twin texel, A=0 means "key/transparent". Uploads set A=0 where the pixel equals the surface's key, and keyed copies use the source alpha. Is it OK to build alpha in from step 1?
2. **Hotkey** for switching between twin and 1x display: any preference? Otherwise I'll put it next to the existing F12 diagnostics.
3. **Removing the matching code now** means no 4K art between step 1 and step 3. Fine, or do you want it left behind a switch until step 3 lands?

When you give the go-ahead I'll start with `Twin.cpp`, then the hooks in `OpenDrawSurface`/`Hooks`, then the present path, and then remove the matching code.
