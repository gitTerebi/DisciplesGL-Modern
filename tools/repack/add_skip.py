import sys
path = sys.argv[1]
lines = open(path, 'rb').read().split(b'\n')
out = []
copy = add = 0
for i, line in enumerate(lines):
    body = line.rstrip(b'\r')
    e = line[len(body):]
    indent = body[:len(body) - len(body.lstrip(b'\t'))]
    s = body.strip()
    if s.startswith(b'ImageReplace::OnPixelCopy(srcData, srcPitch, srcPos, dstData, dstPitch, dstPos, size,'):
        out.append(indent + b'if (ImageReplace::OnPixelBlit(dstData, dstPitch, dstPos, size))' + e)
        out.append(indent + b'\treturn;' + e)
        copy += 1
        out.append(line)
        continue
    out.append(line)
    if s.startswith(b'TextReplace::TracePixel("add", dst, src,') or s.startswith(b'TextReplace::TracePixel("sub", dst, src,'):
        out.append(e)
        out.append(indent + b'if (ImageReplace::OnPixelBlit(dst, dstPitch, dstPos, size))' + e)
        out.append(indent + b'\treturn;' + e)
        add += 1
assert copy == 5 and add == 4, (copy, add)
open(path, 'wb').write(b'\n'.join(out))
print('ok')
