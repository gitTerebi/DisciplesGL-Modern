# Pixel functions: how each one reaches the twins

The game draws on the CPU with its own pixel routines, hooked in `src/DisciplesGL/Hooks.cpp`. For the twins, these routines are the "GPU command set". This page decides, for each one, what the twin does with it, the way an emulator handles every command of the console's GPU.

There are three ways to handle a routine:

- **Emulate**: repeat the operation on the GPU between twins, at N×. 4K content in the source survives, so a blend or tint is applied to the 4K art instead of the 1x result.
- **Upload**: the routine creates source pixels, such as decoding an image into memory or a video frame. Uploading its 1x result is the right behaviour, the same as a CPU write into video memory on a console. If 4K art exists for that image, it comes in later through art matching on the copies made from it.
- **Own path**: a dedicated replacement already exists (text, ground).

| Routine | What it does | Handling | Status |
|---|---|---|---|
| DirectDraw `Blt` / `BltFast` | Copy, optionally colour-keyed | Emulate (twin copy) plus art matching | done |
| DirectDraw colour fill | Solid fill | Emulate (fill) | done |
| `Pixel_BlitColorKey(_SSE)` | Keyed copy, optional blend amount, checkerboard below full strength | Emulate; plain keyed copy also gets art matching | done, under test |
| `Pixel_BlitBlendAvarage(_SSE)` | Keyed weighted blend (highlights, dimming) | Emulate | done, under test |
| `Pixel_Add(_SSE)` | Saturating add (glow) | Emulate | done, under test |
| `Pixel_Sub(_SSE)` | Saturating subtract (darken) | Emulate | done, under test |
| `Pixel_BlitEmptyColor` | Checkerboard copy of non-zero pixels | Emulate (smooth half-coverage) | done, under test |
| `Pixel_BlitBlend(_SSE)`, `Pixel_BlitBlendWithColorKey(_SSE)` | Row-wise blend through an 8-bit mask (shadows, soft edges) | Emulate: upload the mask, blend src twin over dst twin through it | hooked (`OnBlendRow`), GPU todo |
| `Pixel_BlendSome(_SSE)` | Returns a blended colour value; the game writes it | None: no memory write in the hook, the tile check repairs the write | n/a |
| `Pixel_DoubleLighter` | In-place lighten; key colour → 0 | Emulate (shader on the dst twin) | hooked (`OnLighter`), GPU todo |
| `FillColor` | Solid rect fill in game memory | Emulate (fill) | hooked (`OnFillRect`), GPU todo |
| `DrawLine`, `DrawLineHorizontal`, `DrawLineVertical` | 1-px lines | Emulate (thin fills) | hooked (`OnFillRect`), GPU todo |
| `DrawCastleBuildings(_SSE)` | Building overlay with blend colour | Emulate | hooked (`PixelKeyedCopy` / `PixelTint`), GPU todo |
| `DrawFaces(_SSE)` | Portraits: masked, optionally mirrored copy | Emulate as a copy; face art through the image store | hooked (`PixelCopy` / `PixelBlend` / `OnTintRect`), GPU todo |
| `DrawMinimapObjects`, `DrawMinimapGround` | Minimap pixels | Upload (the minimap is deliberately pixel art) | done |
| `DrawGround`, `DrawWaterBorders`, `ClearGround` | Map ground built from tiles and masks | Own path: recorded ground ops replayed into the chunk twin | done, under test |
| `DrawSymbol` | Font glyphs | Own path: TextReplace draws HD text; a 1x glyph is the fallback | done |
| `Pixel_Blit_Indexed_to_565`, `Pixel_Blit_RGB_to_565` | Decode palette/RGB image data into image memory | Upload (source creation) | done |
| `Pixel_Blit_By_Masks`, `Pixel_RGB_Swap` | 24-bit conversions, not drawn to 32-bit surfaces | None | n/a |
| `BlitHUD`, `ClearHUD` | 8-bit HUD buffers | None | n/a |
| `BinkCopyToBufferHook(_SSE)` | Video frame | Upload | done |
| Naked asm write `hook_005383F7` | Solid-colour point plot | Emulate (1×1 fills) | hooked (`OnFillRect`), GPU todo |

Anything that writes 32-bit pixels without a hook is caught by the per-frame tile check, which repairs it from 1x and logs it as `twin repair`. A repair in the log means a routine is missing from this table.

## Exact formulas for the remaining routines

Conventions for this section:
- Pixels are 32-bit DWORDs. "Colour channels" means the low three bytes (bits 0-23), handled the same way, so byte order doesn't matter. "Alpha" is the top byte. Pitches are in pixels.
- **Key tests** compare the whole DWORD, alpha included. Every key passed to Twin has already gone through `Convert565toRGB`, so it has alpha 0xFF (`COLORKEY_CHECK` = 0xFFFF00FF).
- **`blend(a, b, m)`** is the game's `BlendNoCheck`. Per colour channel it gives `a + floor((b - a) * m / 256)`, with m from 1 to 256. The output alpha is always 0xFF. The x86 code does this with packed DWORD maths and the SSE code with `mulhi_epu16`. They can differ by ±1 per channel when b < a; that difference can be ignored.
- **Source memory:** Twin finds which twin a pointer belongs to by its address. Sources marked "game image memory" are normally not inside a DirectDraw surface, but nothing stops them being one. Twin should treat every source pointer as "maybe a twin, otherwise read the 1x pixels".

### `Pixel_BlitBlend(_SSE)` and `Pixel_BlitBlendWithColorKey(_SSE)` → `OnBlendRow(src, dst, length, mask, keyed, key)`
- **Calls:** one call per BlendData row with `length > 0`. `mask` holds `length` bytes, one per pixel. Twin must copy the mask at record time.
- **Per pixel** i, with `m = mask[i]`:
  - `m == 255`: `dst = src` exactly, keeping the source alpha.
  - `m == 0`: dst is unchanged.
  - otherwise: `dst = blend(dst, src, m + 1)`, alpha 0xFF.
- **Keyed variant:** a pixel is skipped when **dst** equals `key`. The test is on the destination, not the source.
  - The game's own calls pass any key.
  - `DrawGround` (water) calls it with `COLORKEY_CHECK_16`, which becomes 0xFFFF00FF.
- **Inputs:**
  - The unkeyed variant receives one row by value; the keyed variant receives an array of rows.
  - Sources: game image memory such as ground tiles, shadows and soft-edge art. Destinations are surfaces or image memory.

### `Pixel_BlendSome(_SSE)` (and `Pixel_Blend(_SSE)`)
- These return a value, `Blend(pix, r | g << 8 | b << 16, msk)`, and write nothing to memory. The game stores the result itself in unhooked code.
- Note that the colour argument has alpha 0, so `msk == 255` returns a pixel with alpha 0.
- Nothing can be emulated at the hook; the tile check repairs the writes.

### `Pixel_DoubleLighter` → `OnLighter(first, pitch, width, height, key, lighten)`
In place, over the whole width × height rect, per pixel p:
- If `p == key`: write `0x00000000`, so alpha becomes 0.
- Else, if `lighten` (the hook's `flag != 0`):
  - Let `s = c0 + c1 + c2`, the sum of the three colour bytes.
  - If `s > 56`: each colour channel becomes `min(255, 2 * c)` and alpha 0xFF.
  - Otherwise: write `0x00000000`.
- Else: unchanged.

There is no source. The destination is a surface or image memory.

### `FillColor` → `OnFillRect(first, pitch, width, height, color)` (32-bit objects only)
- Every pixel in the rect gets exactly `color`, including its alpha. The colour is `Convert565toRGB(color)`:
  - `COLORKEY_CHECK_16` → 0xFFFF00FF;
  - 0 → 0xFF000000;
  - 0xFFFF → 0xFFFFFFFF;
  - otherwise the converted 565 value with alpha 0xFF (BGR order under GDI).
- The write is skipped when width or height is 0.
- The destination is `obj->data` (pitch `obj->pitch >> 1`), usually game image memory.
- `DrawCastleBuildings` calls `FillColor` first with `COLORKEY_CHECK_16` over `obj->rect`.

### `DrawLine`, `DrawLineHorizontal`, `DrawLineVertical` → `OnFillRect` with a height of 1 or a width of 1
- **`DrawLine(obj, loc, count, color)`:** `count` pixels from `obj->data + loc`, row pitch `obj->pitch >> 1`. There is no clipping.
- **`DrawLineHorizontal` / `DrawLineVertical`:** the start point is clipped to `0..sizePitch` (`sizePitch->cx` is the pitch).
  - They write the **unclipped** `width` / `height` count from the clipped start. That bug is in the game code; the call reports exactly what is written.
- **Colour:** `Convert565toRGB(color)`, written exactly, alpha 0xFF.

### `DrawCastleBuildings(_SSE)` → `OnPixelCopy(PixelKeyedCopy | PixelTint, ...)`
- **Source and destination:**
  - The source is the building image, `*((DWORD**)thisObj[11] + 1)`. It is game image memory with pitch = width (tightly packed).
  - The destination is `obj->data` at (0, 0), pitch `obj->pitch >> 1`, size `thisObj[12..13]`.
- **Key:** `COLORKEY_CHECK` (0xFFFF00FF), tested on the **source**. Key pixels leave dst unchanged; it was just filled with 0xFFFF00FF.
- **`PixelKeyedCopy`** (no blend line): `dst = src` exactly, keeping the source alpha. Every pixel is a candidate; there is no checkerboard.
- **`PixelTint`**, with `param` = colour: `dst = blend(src, param, 128)`. Per channel that is `src + floor((param - src) / 2)`, alpha 0xFF. The current dst does not take part.
- **Tint colours:**
  - type 1 (selected): 0xFFC40000, or 0xFF0000C4 under GDI;
  - type 2 (unavailable): 0xFF0000FF, or 0xFFFF0000 under GDI;
  - type 3 (other line): 0xFF000000.

### `DrawFaces(_SSE)` → `PixelCopy`, then optional `PixelBlend` / `OnTintRect` layers
The CPU routine computes per pixel:
```
pix = src
if mask image or colour 1:   pix = blend(pix, mask ? maskPixel : colour1, w)
if row >= minIdx and colour 2: pix = blend(pix, colour2, w)
dst = pix
```
- The routine returns early when `flag == 0`.
- `w = (flag + 1) & 0xFF`. If that is 0 (flag's low byte is 255), the routine just copies: `dst = src` exactly.
- There is no colour key and no checkerboard; every pixel in `width × (bottom - top)` is written.
- **Mirroring:** destination column `width - 1 - j` gets source column j. The mask image is indexed by source column too, so it mirrors with the source.
- **Alpha:** when no blend applies, alpha is the source's. Otherwise it is 0xFF.

The hook sends the same result as a sequence of commands, which is exact because each layer blends the previous result:
1. `OnPixelCopy(PixelCopy, src, srcPitch, dst, dstPitch, width, rows, 0, mirror)`.
   - `PixelCopy` means `dst = src` for every pixel, keeping the source alpha. `param & PixelMirror` mirrors horizontally.
2. If `w != 0`:
   - with a mask image: `OnPixelCopy(PixelBlend, mask, maskPitch, dst, dstPitch, width, rows, 0, w | mirror)`. `PixelBlend` means `dst = blend(dst, src, param & 0xFFFF)` for every pixel, alpha 0xFF, no key; `PixelMirror` mirrors src as above.
   - otherwise, with colour 1: `OnTintRect(dst, dstPitch, width, rows, colour1, w)`. `OnTintRect` means `dst = blend(dst, colour, weight)` for every pixel, alpha 0xFF.
3. If colour 2 and `minIdx < rows`: `OnTintRect(dst + minIdx rows, dstPitch, width, rows - minIdx, colour2, w)`.

More detail:
- The colours are `pixData->color`, byte-swapped under GDI.
- The source is the face image: Faces.ff data loaded by the face cache, or a scratch copy. It is game image memory.
- The mask is a DWORD image in game memory.
- The existing `ImageReplace::OnFaceCopy` call is unchanged.

### Naked asm `hook_005383F7` (named after its address in one build; `hookSpace->unknown_2`) → `OnFillRect(at, 1, 1, 1, color)` per pixel
- **What the game routine does:** it plots single pixels in one solid colour, in a loop counted by `edi`.
  - Each point is read from a two-DWORD record `{x, y}` at `eax`.
  - The row base is advanced by `y * pitch`; the 16-bit pitch in `ebx` is doubled to 32-bit.
  - The pixel at `esi + x * 4` gets the colour at `[ebp-0x1C]`. `hook_005383A5` (`unknown_1`) has already converted that colour to 32-bit with `Convert565toRGB`, so alpha is 0xFF.
- Which screen feature uses it isn't identified; it looks like a dot or marker plotter.
- **What the hook does:** right after the write it calls `TwinPoint(address, colour)` inside `pushfd`/`pushad` … `popad`/`popfd`, so the game's registers and flags are kept.

### Earlier modes: correction
The table above says `Pixel_BlitColorKey` uses a "checkerboard below full strength". In the CPU code the checkerboard applies at **every** strength, including the plain-copy case where `min(255, flag*2) == 255`. Only pixels whose `x + y` is odd (parity carried in `param` bit 8) are written.
