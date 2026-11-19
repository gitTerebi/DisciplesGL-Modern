import sys

p = sys.argv[1]
d = open(p, 'rb').read().decode('ascii')


def rep(a, b, n=1):
    global d
    assert d.count(a) == n, (a, d.count(a))
    d = d.replace(a, b)


rep('''	struct Expect {
		LONG x, y, w, h;
''', '''	struct Expect {
		LONG x, y, w, h;
		LONG sx, sy;
''')
rep('''			e->h = part[5];
			e->item = itemIndex;
''', '''			e->h = part[5];
			e->sx = part[2];
			e->sy = part[3];
			e->item = itemIndex;
''')
rep('BOOL Match(LONG x, LONG y, LONG w, LONG h, DWORD* itemIndex)', 'BOOL Match(LONG x, LONG y, LONG w, LONG h, LONG sx, LONG sy, DWORD* itemIndex)')
rep('if (!e->used && e->x == x && e->y == y && w <= e->w && h <= e->h)',
    'if (!e->used && e->x == x && e->y == y && e->sx == sx && e->sy == sy && w <= e->w && h <= e->h)')
rep('if (!e->used && x >= e->x && y >= e->y && x + w <= e->x + e->w && y + h <= e->y + e->h)',
    'if (!e->used && x >= e->x && y >= e->y && x + w <= e->x + e->w && y + h <= e->y + e->h && sx - e->sx == x - e->x && sy - e->sy == y - e->y)')
rep('BOOL Claim(OpenDrawSurface* dst, LONG x, LONG y, LONG w, LONG h)', 'BOOL Claim(OpenDrawSurface* dst, LONG x, LONG y, LONG w, LONG h, LONG sx, LONG sy)')
rep('!Match(x, y, w, h, &itemIndex)', '!Match(x, y, w, h, sx, sy, &itemIndex)')
rep('BOOL OnPixelBlit(const DWORD* dstData,', 'BOOL OnPixelBlit(const POINT* srcPos, const DWORD* dstData,')
rep('return Claim(surface, offset % width, offset / width, size->cx, size->cy);', 'return Claim(surface, offset % width, offset / width, size->cx, size->cy, srcPos->x, srcPos->y);')
rep('if (Claim(dst, x, y, w, h))', 'if (Claim(dst, x, y, w, h, srcRect->left, srcRect->top))')
open(p, 'wb').write(d.encode('ascii'))
print('ok')
