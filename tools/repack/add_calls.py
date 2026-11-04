import os, re, sys

root = sys.argv[1]


def load(name):
    return open(os.path.join(root, name), 'rb').read()


def save(name, data):
    open(os.path.join(root, name), 'wb').write(data)


def lines_of(data):
    return data.split(b'\n')


def eol(line):
    return b'\r' if line.endswith(b'\r') else b''


def duplicate_after(data, pattern, transform, expected):
    out = []
    hits = 0
    for line in lines_of(data):
        out.append(line)
        if pattern in line:
            hits += 1
            out.append(transform(line.rstrip(b'\r')) + eol(line))
    assert hits == expected, (pattern, hits, expected)
    return b'\n'.join(out)


def add_include(data, after, include):
    out = []
    done = False
    for line in lines_of(data):
        out.append(line)
        if not done and line.rstrip(b'\r') == after:
            out.append(include + eol(line))
            done = True
    assert done, after
    return b'\n'.join(out)


swap = lambda line: line.replace(b'TextReplace::', b'ImageReplace::')

# OpenDrawSurface.cpp
d = load('OpenDrawSurface.cpp')
d = add_include(d, b'#include "TextReplace.h"', b'#include "ImageReplace.h"')
d = duplicate_after(d, b'TextReplace::OnRelease(this);', swap, 1)
d = duplicate_after(d, b'TextReplace::OnPresent(surface, this->backBuffer, textLeft, textTop);', swap, 1)
d = duplicate_after(d, b'TextReplace::OnFill(this);', swap, 1)
d = duplicate_after(d, b'TextReplace::OnBlit(this, lpDestRect->left, lpDestRect->top, surface, lpSrcRect, sWidth, FALSE, 0);', swap, 1)
out = []
hits = 0
for line in lines_of(d):
    out.append(line)
    if b'TextReplace::OnBlit(this, dwX, dwY, surface, lpSrcRect, sWidth, surface->colorKey.enabled, surface->colorKey.high);' in line:
        hits += 1
        indent = line[:len(line) - len(line.lstrip(b'\t'))]
        e = eol(line)
        out.append(indent + b'if (ImageReplace::OnBlit(this, dwX, dwY, surface, lpSrcRect, sWidth, surface->colorKey.enabled, surface->colorKey.high))' + e)
        out.append(indent + b'\treturn DD_OK;' + e)
assert hits == 1
save('OpenDrawSurface.cpp', b'\n'.join(out))

# Hooks.cpp
d = load('Hooks.cpp')
for name, count in ((b'OnFillRect(', 4), (b'OnPixelRows(', 4), (b'OnPixelCopy(', 5), (b'OnDiamond(', 3)):
    d = duplicate_after(d, b'TextReplace::' + name, swap, count)
save('Hooks.cpp', d)

# NewRenderer.cpp
d = load('NewRenderer.cpp')
d = add_include(d, b'#include "TextReplace.h"', b'#include "ImageReplace.h"')
d = duplicate_after(d, b'TextReplace::ReleaseGL();', swap, 1)
d = duplicate_after(d, b'BOOL textUpChanged = TextReplace::Prepare(stateBuffer, textUpActive);',
                    lambda l: l.replace(b'BOOL textUpChanged = TextReplace::Prepare', b'textUpChanged |= ImageReplace::Prepare'), 1)
d = duplicate_after(d, b'BOOL textChanged = TextReplace::Prepare(stateBuffer, textActive);',
                    lambda l: l.replace(b'BOOL textChanged = TextReplace::Prepare', b'textChanged |= ImageReplace::Prepare'), 1)
lines = lines_of(d)
out = []
i = 0
hits = 0
while i < len(lines):
    line = lines[i]
    stripped = line.rstrip(b'\r').strip()
    if stripped in (b'if (textUpActive)', b'if (textActive)') and i + 1 < len(lines) and b'TextReplace::Draw(' in lines[i + 1]:
        e = eol(line)
        indent = line[:len(line) - len(line.lstrip(b'\t'))]
        draw = lines[i + 1].rstrip(b'\r').strip()
        out.append(line)
        out.append(indent + b'{' + e)
        out.append(indent + b'\t' + swap(draw) + e)
        out.append(indent + b'\t' + draw + e)
        out.append(indent + b'}' + eol(lines[i + 1]))
        i += 2
        hits += 1
        continue
    out.append(line)
    i += 1
assert hits == 2, hits
save('NewRenderer.cpp', b'\n'.join(out))
print('ok')
