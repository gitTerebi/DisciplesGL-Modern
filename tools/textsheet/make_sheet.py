import ctypes
import ctypes.wintypes as wt
import os
import struct
import sys
import numpy as np
from scipy.ndimage import distance_transform_edt

FONTS = [
    ("small", "Palatino Linotype", 400, False, 11),
    ("normal", "Palatino Linotype", 400, False, 12),
    ("medbold", "Palatino Linotype", 700, False, 12),
    ("menu", "Palatino Linotype", 700, False, 13),
    ("medium", "Garamond", 400, False, 16),
    ("large", "Garamond", 700, False, 16),
    ("vlarge", "Book Antiqua", 700, True, 21),
]
CHARSETS = [0, 238, 204]
UPPER = {
    204: {
        "small": ("Times New Roman", 400, False, 11),
        "normal": ("Times New Roman", 400, False, 12),
        "medbold": ("Times New Roman", 700, False, 13),
        "menu": ("Times New Roman", 700, False, 13),
        "medium": ("Times New Roman", 400, False, 14),
        "large": ("Times New Roman", 700, False, 14),
        "vlarge": ("Times New Roman", 400, True, 21),
    },
}
TEXELS = 4
SUPER = 8
PAD = 2
DIST_SCALE = 32
DIST_ZERO = 128
REF_ORIGIN = 48
ATLAS_W = 2048

gdi = ctypes.windll.gdi32
ntdll = ctypes.windll.ntdll
H = wt.HANDLE
for fn, res, args in [
    ("CreateCompatibleDC", H, [H]),
    ("CreateDIBSection", H, [H, ctypes.c_void_p, wt.UINT, ctypes.c_void_p, H, wt.DWORD]),
    ("SelectObject", H, [H, H]),
    ("DeleteObject", wt.BOOL, [H]),
    ("SetBkMode", ctypes.c_int, [H, ctypes.c_int]),
    ("SetTextColor", wt.DWORD, [H, wt.DWORD]),
    ("SetTextAlign", wt.UINT, [H, wt.UINT]),
    ("TextOutA", wt.BOOL, [H, ctypes.c_int, ctypes.c_int, ctypes.c_char_p, ctypes.c_int]),
    ("CreateSolidBrush", H, [wt.DWORD]),
    ("BeginPath", wt.BOOL, [H]),
    ("EndPath", wt.BOOL, [H]),
    ("FillPath", wt.BOOL, [H]),
    ("CreateFontA", H, [ctypes.c_int] * 5 + [wt.DWORD] * 8 + [ctypes.c_char_p]),
]:
    getattr(gdi, fn).restype = res
    getattr(gdi, fn).argtypes = args


class BITMAPINFOHEADER(ctypes.Structure):
    _fields_ = [("biSize", wt.DWORD), ("biWidth", wt.LONG), ("biHeight", wt.LONG), ("biPlanes", wt.WORD),
                ("biBitCount", wt.WORD), ("biCompression", wt.DWORD), ("biSizeImage", wt.DWORD),
                ("biXPelsPerMeter", wt.LONG), ("biYPelsPerMeter", wt.LONG), ("biClrUsed", wt.DWORD), ("biClrImportant", wt.DWORD)]


class Canvas:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.dc = gdi.CreateCompatibleDC(None)
        info = BITMAPINFOHEADER(ctypes.sizeof(BITMAPINFOHEADER), w, -h, 1, 32, 0, 0, 0, 0, 0, 0)
        self.bits = ctypes.c_void_p()
        self.bmp = gdi.CreateDIBSection(self.dc, ctypes.byref(info), 0, ctypes.byref(self.bits), None, 0)
        gdi.SelectObject(self.dc, self.bmp)
        gdi.SetBkMode(self.dc, 1)
        gdi.SetTextColor(self.dc, 0x00FFFFFF)
        gdi.SetTextAlign(self.dc, 0)

    def clear(self):
        gdi.GdiFlush()
        ctypes.memset(self.bits, 0, self.w * self.h * 4)

    def ink(self):
        gdi.GdiFlush()
        buf = (ctypes.c_uint32 * (self.w * self.h)).from_address(self.bits.value)
        return (np.frombuffer(buf, np.uint32).reshape(self.h, self.w) & 0x00FFFFFF) != 0


def make_font(face, weight, italic, height, charset, quality):
    return gdi.CreateFontA(-height, 0, 0, 0, weight, int(italic), 0, 0, charset, 7, 0, quality, 0, face.encode())


def bbox(mask):
    ys, xs = np.nonzero(mask)
    if not len(xs):
        return None
    return xs.min(), ys.min(), xs.max() + 1, ys.max() + 1


def reference(canvas, font, ch):
    canvas.clear()
    old = gdi.SelectObject(canvas.dc, font)
    gdi.TextOutA(canvas.dc, REF_ORIGIN, REF_ORIGIN, bytes([ch]), 1)
    gdi.SelectObject(canvas.dc, old)
    mask = canvas.ink()
    box = bbox(mask)
    if not box:
        return None
    l, t, r, b = box
    bits = np.packbits(mask[t:b, l:r], axis=1)
    return l - REF_ORIGIN, t - REF_ORIGIN, r - l, b - t, bits.tobytes()


def outline_mask(canvas, font, ch, ox, oy):
    canvas.clear()
    old = gdi.SelectObject(canvas.dc, font)
    brush = gdi.CreateSolidBrush(0x00FFFFFF)
    oldBrush = gdi.SelectObject(canvas.dc, brush)
    gdi.BeginPath(canvas.dc)
    gdi.TextOutA(canvas.dc, ox, oy, bytes([ch]), 1)
    gdi.EndPath(canvas.dc)
    gdi.FillPath(canvas.dc)
    gdi.SelectObject(canvas.dc, oldBrush)
    gdi.SelectObject(canvas.dc, old)
    gdi.DeleteObject(brush)
    return canvas.ink()


def distance(canvas, font, ch, em):
    step = SUPER
    ox, oy = em, em // 2
    mask = outline_mask(canvas, font, ch, ox, oy)
    box = bbox(mask)
    if not box:
        return None
    l, t, r, b = box
    pad = PAD * TEXELS * step
    tl = (l - ox - pad) // step
    tt = (t - oy - pad) // step
    tr = -(-(r - ox + pad) // step)
    tb = -(-(b - oy + pad) // step)
    x0, y0 = ox + tl * step, oy + tt * step
    x1, y1 = ox + tr * step, oy + tb * step
    region = np.zeros((y1 - y0, x1 - x0), bool)
    sx0, sy0 = max(x0, 0), max(y0, 0)
    sx1, sy1 = min(x1, canvas.w), min(y1, canvas.h)
    region[sy0 - y0:sy1 - y0, sx0 - x0:sx1 - x0] = mask[sy0:sy1, sx0:sx1]
    inside = distance_transform_edt(region)
    outside = distance_transform_edt(~region)
    signed = np.where(region, inside - 0.5, 0.5 - outside) / (TEXELS * step)
    h, w = signed.shape[0] // step, signed.shape[1] // step
    field = signed.reshape(h, step, w, step).mean(axis=(1, 3))
    data = np.clip(np.round(DIST_ZERO + field * DIST_SCALE), 0, 255).astype(np.uint8)
    return int(tl), int(tt), data


def compress(raw):
    fmt = 2 | 0x100
    ws, fs = wt.ULONG(), wt.ULONG()
    ntdll.RtlGetCompressionWorkSpaceSize(fmt, ctypes.byref(ws), ctypes.byref(fs))
    work = ctypes.create_string_buffer(ws.value)
    out = ctypes.create_string_buffer(len(raw) + len(raw) // 8 + 4096)
    size = wt.ULONG()
    status = ntdll.RtlCompressBuffer(fmt, raw, len(raw), out, len(out), 4096, ctypes.byref(size), work)
    if status:
        raise SystemExit("RtlCompressBuffer failed %08X" % (status & 0xFFFFFFFF))
    return out.raw[:size.value]


def main(path):
    ref_canvas = Canvas(160, 160)
    images = {}
    order = []
    glyphs = []
    refs = bytearray()
    for name, face, weight, italic, size in FONTS:
        em = max(size, 21) * TEXELS * SUPER
        big = Canvas(em * 3, em * 2)
        per_font = []
        for charset in CHARSETS:
            specs = [(face, weight, italic, size), UPPER.get(charset, {}).get(name, (face, weight, italic, size))]
            fonts = [(make_font(f, w, i, z, charset, 3), make_font(f, w, i, z * TEXELS * SUPER, charset, 4)) for f, w, i, z in specs]
            table = []
            for ch in range(256):
                small, large = fonts[1 if ch >= 0x80 else 0]
                rec = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
                ref = reference(ref_canvas, small, ch) if ch > 32 else None
                if ref:
                    rx, ry, rw, rh, bits = ref
                    rec[6:11] = [rx, ry, rw, rh, len(refs)]
                    refs += bits
                    sdf = distance(big, large, ch, em)
                    if sdf:
                        left, top, data = sdf
                        key = (data.shape, data.tobytes())
                        if key not in images:
                            images[key] = len(order)
                            order.append(data)
                        rec[0] = images[key]
                        rec[1] = 1
                        rec[4], rec[5] = left, top
                table.append(rec)
            for small, large in fonts:
                gdi.DeleteObject(small)
                gdi.DeleteObject(large)
            per_font.append(table)
        glyphs.append((name, per_font))
        print(name, "done", len(order), "images")

    rects = [None] * len(order)
    x = y = row = 0
    for index in sorted(range(len(order)), key=lambda i: -order[i].shape[0]):
        h, w = order[index].shape
        if x + w + 1 > ATLAS_W:
            x, y, row = 0, y + row + 1, 0
        rects[index] = (x, y)
        x += w + 1
        row = max(row, h)
    atlas_h = (y + row + 4) & ~3
    atlas = np.zeros((atlas_h, ATLAS_W), np.uint8)
    for index, (ax, ay) in enumerate(rects):
        h, w = order[index].shape
        atlas[ay:ay + h, ax:ax + w] = order[index]

    delta = atlas.astype(np.int16)
    delta[:, 1:] -= atlas[:, :-1]
    packed = compress((delta & 255).astype(np.uint8).tobytes())
    out = bytearray()
    out += struct.pack("<4s10I4B", b"D2TS", 1, TEXELS, DIST_SCALE, DIST_ZERO, ATLAS_W, atlas_h,
                       len(FONTS), len(CHARSETS), len(refs), len(packed), *(CHARSETS + [0]))
    for name, per_font in glyphs:
        out += struct.pack("<16s", name.encode())
        for table in per_font:
            for image, has, _, _, left, top, rx, ry, rw, rh, roff in table:
                if has:
                    h, w = order[image].shape
                    ax, ay = rects[image]
                else:
                    ax = ay = w = h = 0
                out += struct.pack("<4H4h2BHI", ax, ay, w, h, left, top, rx, ry, rw, rh, 0, roff)
    out += refs
    out += packed
    open(path, "wb").write(out)
    print("atlas %dx%d, %d images, refs %d bytes, atlas %d -> %d bytes, file %d bytes" % (
        ATLAS_W, atlas_h, len(order), len(refs), atlas.size, len(packed), len(out)))
    if len(sys.argv) > 2:
        from PIL import Image
        Image.fromarray(atlas).save(sys.argv[2])


if __name__ == "__main__":
    main(sys.argv[1])
