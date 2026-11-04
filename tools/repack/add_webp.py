import glob, os, sys, uuid

proj_dir = sys.argv[1]
os.chdir(proj_dir)
BS = chr(92)
files = sorted(glob.glob('../libwebp/src/**/*.c', recursive=True))
files = [f.replace('/', BS) for f in files]


def edit(path, fn):
    raw = open(path, 'rb').read()
    bom = raw.startswith(b'\xef\xbb\xbf')
    s = raw[3 if bom else 0:].decode('utf-8')
    nl = '\r\n' if '\r\n' in s else '\n'
    s = fn(s, nl)
    open(path, 'wb').write((b'\xef\xbb\xbf' if bom else b'') + s.encode('utf-8'))


def proj(s, nl):
    inc = '$(ProjectDir)..' + BS + 'libwebp;%(AdditionalIncludeDirectories)'
    items = ''.join(
        f'    <ClCompile Include="{f}">{nl}'
        f'      <CompileAs>CompileAsC</CompileAs>{nl}'
        f'      <AdditionalIncludeDirectories>{inc}</AdditionalIncludeDirectories>{nl}'
        f'    </ClCompile>{nl}' for f in files)
    block = f'  <ItemGroup>{nl}{items}  </ItemGroup>{nl}'
    marker = '  <Import Project="$(VCTargetsPath)' + BS + 'Microsoft.Cpp.targets"'
    i = s.rindex(marker)
    return s[:i] + block + s[i:]


def filt(s, nl):
    fid = '{' + str(uuid.uuid4()) + '}'
    fblock = (f'  <ItemGroup>{nl}    <Filter Include="libwebp">{nl}'
              f'      <UniqueIdentifier>{fid}</UniqueIdentifier>{nl}    </Filter>{nl}  </ItemGroup>{nl}')
    items = ''.join(f'    <ClCompile Include="{f}">{nl}      <Filter>libwebp</Filter>{nl}    </ClCompile>{nl}' for f in files)
    block = fblock + f'  <ItemGroup>{nl}{items}  </ItemGroup>{nl}'
    i = s.rindex('</Project>')
    return s[:i] + block + s[i:]


edit('DisciplesGL.vcxproj', proj)
edit('DisciplesGL.vcxproj.filters', filt)
print(len(files))
