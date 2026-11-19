import sys, re
p = sys.argv[1]
b = open(p, 'rb').read()
crlf = b'\r\n' in b
d = b.decode('ascii').replace('\r\n', '\n')

def rep(a, c=''):
    global d
    assert d.count(a) == 1, (a[:80], d.count(a))
    d = d.replace(a, c)

rep('\t#define FIRST_DRAW_LIMIT 4000\n')
rep('\t#define TRACE_MS 50\n')
rep('\t#define EVICT_LOG_MS 10000\n')
rep('\t#define STORE_LOG_LIMIT 40\n')
rep('\t\tDWORD logged;\n')
rep('\t\tDWORD firstDraws;\n')
rep('\t\tLONG gpuReportedMB;\n')
rep('\t\tDWORD evicted, dropped;\n\t\tDWORD evictLogTick;\n')
rep('\t\tDWORD until;\n')
rep('\t\tBYTE drawn;\n')
rep('''			if (s && stores.logged < STORE_LOG_LIMIT)
			{
				++stores.logged;
				DebugLog("image store blit %s src %d,%d %dx%d -> %d,%d", s->file ? s->file->name : "?", srcRect->left, srcRect->top, w, h, x, y);
			}

''')
rep('''			DWORD count = LoadArchive(archive);
			DebugLog("image archive %s offset %d: %d files", path, offset, count);
''', '''			LoadArchive(archive);
''')
rep('''		if (stores.logged < STORE_LOG_LIMIT)
		{
			++stores.logged;
			DebugLog("image store load %X -> %s", id, file ? file->name : "?");
		}
''')
rep('''		if (stores.logged < STORE_LOG_LIMIT)
		{
			++stores.logged;
			DebugLog("image store draw %s data %08X rect %d,%d,%d,%d", s->file->name, data, rect[0], rect[1], rect[2], rect[3]);
		}
''')
rep('''		pack.gpuReportedMB = kb[0] >> 10;
''')
rep('''		DebugLog("image GPU memory reported %d MB, texture budget %d MB", pack.gpuReportedMB, pack.gpuBudget >> 20);
''')
rep('''				++pack.dropped;
''')
rep('''			pack.evicted += freed;
''')
rep('''
		DWORD tick = GetTickCount();
		if ((pack.evicted || pack.dropped) && (LONG)(tick - pack.evictLogTick) >= 0)
		{
			pack.evictLogTick = tick + EVICT_LOG_MS;
			DebugLog("image evicted %d textures, dropped %d decoded, gpuMB=%d decodedMB=%d", pack.evicted, pack.dropped, pack.gpu >> 20, pack.decoded >> 20);
			pack.evicted = pack.dropped = 0;
		}
''')
rep('''	VOID Point(CHAR* buffer, const POINT* pt)
	{
		if (pt)
			StrPrint(buffer, "%d,%d", pt->x, pt->y);
		else
			StrCopy(buffer, "-");
	}

''')
rep('''		if (key && !trace.key)
		{
			trace.until = tick + TRACE_MS;
			LogStats();
		}''', '''		if (key && !trace.key)
			LogStats();''')
rep('''			if (TakeFlag())
			{
				trace.until = tick + TRACE_MS;
				LogStats();
			}''', '''			if (TakeFlag())
				LogStats();''')
i = d.index('\t\tBOOL first = entry && !entry->drawn')
j = d.index('\t}\n', i)
d = d[:i].rstrip('\n\t') + '\n' + d[j:]
open(p, 'wb').write((d.replace('\n', '\r\n') if crlf else d).encode('ascii'))
print('ok', crlf)
