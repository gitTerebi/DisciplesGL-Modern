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
| `Pixel_BlitBlend(_SSE)`, `Pixel_BlitBlendWithColorKey(_SSE)` | Row-wise blend through an 8-bit mask (shadows, soft edges) | Emulate: upload the mask, blend src twin over dst twin through it | todo |
| `Pixel_BlendSome_SSE` | Blend towards a colour by a mask | Emulate | todo |
| `Pixel_DoubleLighter` | In-place lighten; key colour → 0 | Emulate (shader on the dst twin) | todo |
| `FillColor` | Solid rect fill in game memory | Emulate (fill) | todo |
| `DrawLine`, `DrawLineHorizontal`, `DrawLineVertical` | 1-px lines | Emulate (thin fills) | todo |
| `DrawCastleBuildings(_SSE)` | Building overlay with blend colour | Emulate | todo |
| `DrawFaces(_SSE)` | Portraits: masked, optionally mirrored copy | Emulate as a copy; face art through the image store | todo |
| `DrawMinimapObjects`, `DrawMinimapGround` | Minimap pixels | Upload (the minimap is deliberately pixel art) | done |
| `DrawGround`, `DrawWaterBorders`, `ClearGround` | Map ground built from tiles and masks | Own path: recorded ground ops replayed into the chunk twin | done, under test |
| `DrawSymbol` | Font glyphs | Own path: TextReplace draws HD text; a 1x glyph is the fallback | done |
| `Pixel_Blit_Indexed_to_565`, `Pixel_Blit_RGB_to_565` | Decode palette/RGB image data into image memory | Upload (source creation) | done |
| `Pixel_Blit_By_Masks`, `Pixel_RGB_Swap` | 24-bit conversions, not drawn to 32-bit surfaces | None | n/a |
| `BlitHUD`, `ClearHUD` | 8-bit HUD buffers | None | n/a |
| `BinkCopyToBufferHook(_SSE)` | Video frame | Upload | done |
| Naked asm write `hook_005383F7` | Unknown pixel write | Caught by the tile check | todo: identify |

Anything that writes 32-bit pixels without a hook is caught by the per-frame tile check, which repairs it from 1x and logs it as `twin repair`. A repair in the log means a routine is missing from this table.
