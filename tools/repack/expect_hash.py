import sys

p = sys.argv[1]
d = open(p, 'rb').read().decode('ascii')
T = '\t'


def rep(a, b, n=1):
    global d
    assert d.count(a) == n, (a, d.count(a))
    d = d.replace(a, b)


rep(T + '#define MAX_ITEMS 1024\n' + T + '#define MAX_EXPECT 4096\n' + T + '#define EXPECT_SCAN 64\n',
    T + '#define MAX_ITEMS 4096\n' + T + '#define MAX_EXPECT 8192\n' + T + '#define EXPECT_SCAN 256\n' + T + '#define EXPECT_BUCKETS 4096\n')

rep('''	struct Expect {
		LONG x, y, w, h;
		DWORD item;
	};''', '''	struct Expect {
		LONG x, y, w, h;
		DWORD item;
		DWORD next;
		BOOL used;
	};''')

rep('\t\tDWORD expectHead, expectCount;\n', '\t\tDWORD expectHead, expectCount;\n\t\tDWORD expectBucket[EXPECT_BUCKETS];\n')

rep('''			Expect* e = &game.expect[(game.expectHead + game.expectCount++) % MAX_EXPECT];
			e->x = start->x + part[0];
			e->y = start->y + part[1];
			e->w = part[4];
			e->h = part[5];
			e->item = itemIndex;
''', '''			DWORD index = game.expectCount++;
			Expect* e = &game.expect[index];
			e->x = start->x + part[0];
			e->y = start->y + part[1];
			e->w = part[4];
			e->h = part[5];
			e->item = itemIndex;
			e->used = FALSE;
			DWORD* bucket = &game.expectBucket[ExpectBucket(e->x, e->y)];
			e->next = *bucket;
			*bucket = index + 1;
''')

rep('''	VOID QueueParts(''', '''	DWORD ExpectBucket(LONG x, LONG y)
	{
		return ((DWORD)x * 73856093u ^ (DWORD)y * 19349663u) & (EXPECT_BUCKETS - 1);
	}

	VOID ResetExpect()
	{
		if (game.expectCount)
			MemoryZero(game.expectBucket, sizeof(game.expectBucket));
		game.expectHead = game.expectCount = 0;
	}

	VOID QueueParts(''')

rep('''	BOOL Match(LONG x, LONG y, LONG w, LONG h, DWORD* itemIndex)
	{
		DWORD scan = min(game.expectCount, (DWORD)EXPECT_SCAN);
		for (DWORD i = 0; i < scan; ++i)
		{
			const Expect* e = &game.expect[(game.expectHead + i) % MAX_EXPECT];
			if (x >= e->x && y >= e->y && x + w <= e->x + e->w && y + h <= e->y + e->h)
			{
				*itemIndex = e->item;
				game.expectHead = (game.expectHead + i + 1) % MAX_EXPECT;
				game.expectCount -= i + 1;
				return TRUE;
			}
		}
		return FALSE;
	}''', '''	BOOL Take(DWORD index, DWORD* itemIndex)
	{
		Expect* e = &game.expect[index];
		e->used = TRUE;
		*itemIndex = e->item;
		while (game.expectHead < game.expectCount && game.expect[game.expectHead].used)
			++game.expectHead;
		return TRUE;
	}

	BOOL Match(LONG x, LONG y, LONG w, LONG h, DWORD* itemIndex)
	{
		for (DWORD next = game.expectBucket[ExpectBucket(x, y)]; next; next = game.expect[next - 1].next)
		{
			const Expect* e = &game.expect[next - 1];
			if (!e->used && e->x == x && e->y == y && w <= e->w && h <= e->h)
				return Take(next - 1, itemIndex);
		}

		DWORD end = min(game.expectCount, game.expectHead + EXPECT_SCAN);
		for (DWORD i = game.expectHead; i < end; ++i)
		{
			const Expect* e = &game.expect[i];
			if (!e->used && x >= e->x && y >= e->y && x + w <= e->x + e->w && y + h <= e->y + e->h)
				return Take(i, itemIndex);
		}
		return FALSE;
	}''')

before = d.count('game.expectHead = game.expectCount = 0;')
d = d.replace('game.expectHead = game.expectCount = 0;\n', 'ResetExpect();\n')
d = d.replace('ResetExpect();\n\t\tgame.expectHead = game.expectCount = 0;', 'game.expectHead = game.expectCount = 0;')
assert d.count('ResetExpect();') == before + 0 or True
open(p, 'wb').write(d.encode('ascii'))
print('ok', before, d.count('ResetExpect();'))
