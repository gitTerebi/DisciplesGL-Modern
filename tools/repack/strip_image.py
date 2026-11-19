import sys

src, dst = sys.argv[1], sys.argv[2]
lines = open(src, 'rb').read().split(b'\n')
out = []
i = 0
while i < len(lines):
    line = lines[i]
    s = line.rstrip(b'\r').strip()
    if s in (b'#include "ImageReplace.h"', b'ImageReplace::ReleaseGL();') or s.startswith(b'textUpChanged |= ImageReplace::') or s.startswith(b'textChanged |= ImageReplace::'):
        i += 1
        continue
    if s == b'{' and i + 3 < len(lines) and lines[i + 1].strip().startswith(b'ImageReplace::Draw(') and lines[i + 3].rstrip(b'\r').strip() == b'}':
        text = lines[i + 2]
        e = b'\r' if text.endswith(b'\r') else b''
        indent = line[:len(line) - len(line.lstrip(b'\t'))]
        out.append(indent + b'\t' + text.rstrip(b'\r').strip() + e)
        i += 4
        continue
    out.append(line)
    i += 1
data = b'\n'.join(out)
assert b'ImageReplace' not in data
open(dst, 'wb').write(data)
print('ok')
