import io
import json
import multiprocessing
import struct
import sys
import time

import etcpak
import numpy as np
import zstandard
from PIL import Image

SCALE = 2
LEVEL = 19


def read_manifest(path):
    with open(path, 'rb') as f:
        header = f.read(32)
        offset, length = struct.unpack('<qq', header[16:32])
        f.seek(offset)
        return header, json.loads(f.read(length))


source = None


def init(path):
    global source
    source = open(path, 'rb')


def convert(sprite):
    meta = sprite['Metadata']
    source.seek(sprite['Offset'] + 32)
    image = Image.open(io.BytesIO(source.read(sprite['Length']))).convert('RGBA')
    w, h = meta['Width'] * SCALE, meta['Height'] * SCALE
    pixels = np.asarray(image).astype(np.float32)
    if not meta['Additive']:
        pixels[..., :3] *= pixels[..., 3:4] / 255
    sw, sh = image.size
    if sw == w * 2 and sh == h * 2:
        pixels = pixels.reshape(h, 2, w, 2, 4).mean((1, 3))
    else:
        bands = [Image.fromarray(pixels[..., i]).resize((w, h), Image.LANCZOS) for i in range(4)]
        pixels = np.stack([np.asarray(b) for b in bands], -1)
    pw, ph = (w + 3) // 4 * 4, (h + 3) // 4 * 4
    padded = np.zeros((ph, pw, 4), np.uint8)
    padded[:h, :w] = np.clip(pixels + 0.5, 0, 255)
    blocks = etcpak.compress_bc7(padded.tobytes(), pw, ph)
    return zstandard.ZstdCompressor(level=LEVEL).compress(blocks), w, h, (sw, sh)


def main(src, dst):
    header, manifest = read_manifest(src)
    sprites = manifest['Sprites']
    start = time.time()
    resized = 0
    with open(dst, 'wb') as out, multiprocessing.Pool(max(1, multiprocessing.cpu_count() - 1), init, (src,)) as pool:
        out.write(b'\0' * 32)
        offset = 0
        for i, (sprite, (blob, w, h, size)) in enumerate(zip(sprites, pool.imap(convert, sprites, chunksize=4))):
            out.write(blob)
            if size != (w * 2, h * 2):
                resized += 1
            sprite['Offset'] = offset
            sprite['Length'] = len(blob)
            sprite['Metadata'].update(PixelWidth=w, PixelHeight=h, PixelScale=SCALE)
            offset += len(blob)
            if i % 500 == 0:
                print(f'{i}/{len(sprites)} {time.time() - start:.0f} s', flush=True)
        manifest['Format'] = 'BC7-ZSTD'
        manifest['CreatedBy'] = 'ff2k.py'
        data = json.dumps(manifest).encode()
        position = out.tell()
        out.write(data)
        out.seek(0)
        out.write(header[:16] + struct.pack('<qq', position, len(data)))
    print(f'{len(sprites)} sprites, {resized} resized from a non-2x source, {position / 2**30:.2f} GB, {time.time() - start:.0f} s')


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
