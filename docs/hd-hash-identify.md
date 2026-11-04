# Design: identify 4K art by pixel content

## Why
The twins put the picture together correctly: every game operation is repeated in order, and the game only ever reads its own 1x memory. What still breaks is **identification**, deciding which 4K image a 1x copy is. That still uses the old idea: the render hook says "image X goes here", and copies arriving later get matched against that "expectation". Every recent art bug comes from that guess:

- **Deferred copies:** the game queues an image, and the copies arrive later in another order.
- **Cached images:** the game renders once into a cache and copies the cache every frame with no new render call. The art cache was added for this.
- **Shared blocks:** the packer dedupes identical 1x blocks across animation frames.
- **Clipping and tiling:** pieces that don't sit inside the expected place.
- **Game image memory:** portraits come from the face cache, not a surface.

DuckStation doesn't track draw calls to find textures. It hashes the texture's pixels where they live, and the hash picks the replacement. We do the same.

## The idea
**An image is identified by its 1x pixels.** When the twins see a copy, they look at the 1x source pixels of that rect. If those pixels are a known piece of a known image, the 4K piece is drawn at the copy's destination. Nothing else matters: not when it was rendered, where it came from, how many caches it passed through, or which buffer holds it.

## Parts

### 1. The art index: pixel hash → piece of a pack frame
- **Key:** a 64-bit hash of a w×h block of 1x pixels (alpha ignored, key colour normalised), plus w and h.
- **Value:** the pack frame, the block's rect inside that frame's canvas, the canvas size, and flip.
- **Filled ("learned") wherever pixels and identity are known together:**
  - **At the source.** When the game decodes an image whose name we know, we hash each of its parts where they land:
    - images through the image store (`StoreDraw` gives data + rect + name);
    - faces (`FaceLoad` gives the decoded face pixels + name);
    - images built by the image hook (`Build`, plus each part's sheet rect).

    This is the DuckStation-style "replacement at upload" and needs no copy matching at all.
  - **From expectations, once.** If a piece wasn't learned at the source, the first copy that matches an expectation hashes its source block and stores it. Expectations become a learning aid only. They no longer decide what gets drawn.
- **Saved to disk** (`twin-index.bin`, next to the game) and loaded at start. After one session, every piece seen before is known on frame 1.
- **Shared blocks:** identical 1x pixels give one hash. The first identity learned is kept, so a shared block always shows the same 4K piece and doesn't flicker. Because the 1x is identical, the 4K pieces are near-identical too.

### 2. Lookup on copy
- On every copy (Blt, BltFast, keyed/plain pixel copies), the source block's hash is looked up.
- **Hashing only on change:** each source twin keeps a small per-twin list of **identified regions** (rect → index entry). A region is created on a successful lookup and dropped the moment its rect is written. All writes are already tracked: dirty marks, uploads, fills, copies in and pixel ops. A repeat copy of an unchanged spot (the common case) never rehashes.
- **Partial copies:** a copy whose source rect lies inside an identified region uses that region's identity with the offset. Clipped and tiled copies work this way.
- **A hit draws the 4K piece at the destination,** exactly as the current art draw does: premultiplied, batched, key semantics kept. A miss is a normal twin copy (1x).
- **Self-checking:** an identified region is created only from a real hash match of the current 1x pixels, so stale identities can't survive a content change the hooks saw. The tile check covers changes they didn't.

### 3. Game image memory gets twins
- Faces and other images live in the game's own memory, not DirectDraw surfaces. When an operation touches untwinned memory, Twin asks `ImageReplace::FindMemory` whether it's a known image buffer (store or face). If it is, a **memory twin** is created on the spot. It's uploaded from 1x and immediately identified through the index, so its 4K art is drawn in at the source.
- `StoreFree` / `ImageFree` / face cache frees call `Twin::OnMemoryFree`. Memory twins are capped (LRU, about 256 MB). Memory reused behind our back is caught by the tile check.
- Then `DrawFaces` copies twin→twin (mirror included), and every later copy carries the 4K face.

### 4. What gets removed
- expectation matching as a drawing decision (`Match`/`Inside` in OnCopy and pixel copies);
- the per-source art cache;
- the place/aspect rejection;
- dedupe and overflow handling for expectations;
- most `rejected`/`unmatched` counters.

Expectations stay only as an optional learning aid behind the index.

## Cost
- **Hashing:** only on a region's first use and after it's rewritten. That's a few hundred small blocks when a screen first appears, then close to zero per frame.
- **Index:** about 40 bytes per piece. Tens of thousands of pieces is a few MB on disk and in RAM.
- **Lookup:** one hash-table probe per copy, cheaper than the current expectation scan.

## Risks
- **Different images with identical 1x pixels:** for example, recoloured variants that happen to match, or tiny blocks such as a 2×2 solid. Blocks under 8×8, or a single solid colour, are never identified and stay 1x.
- **Key colour:** pixels must hash the same however they reached memory, so the hash uses the 1x RGB with the surface's key normalised.
- **Learning at the source** needs the exact sheet placement of each part. `QueueParts` already reads it from the record. If an image can't be learned at the source, it learns at its first expectation match, as today, and from then on it's content-based.

## Steps
1. Index plus lookup plus identified regions. Learn from expectations only. Remove the art cache. This alone fixes cached-copy flicker and makes repeats free.
2. Learn at the source: store, faces, build. Add memory twins for game image memory (portraits).
3. Persist the index to disk.
4. Remove expectation-driven drawing and its counters. Log per screen: identified, learned, misses.
