import json, sys, collections

ff = json.load(open(sys.argv[1]))
man = json.load(open(sys.argv[2], encoding='utf8'))

stills, anims, raws = {}, {}, {}
for f, r in ff.items():
    for k, v in r['stills'].items(): stills.setdefault(k, (f, v))
    for k, v in r['anims'].items(): anims.setdefault(k, (f, v))
    for k in r['raw']: raws.setdefault(k, f)

RACE = {'human': 'HU', 'undead': 'UN', 'heretic': 'HE', 'dwarf': 'DW', 'neutral': 'NE', 'elf': 'EL'}
RACEID = {'human': '0000', 'undead': '0003', 'heretic': '0002', 'dwarf': '0001', 'neutral': '8888', 'elf': '0005'}
SITE = {'mage': 'MAGE', 'merchant': 'MERH', 'mercenary': 'MERC', 'trainer': 'TRAI'}
CRYSTAL = {'gold': 'GL', 'death-mana': 'RG', 'rune-mana': 'WH', 'life-mana': 'YE', 'infernal-mana': 'RD', 'grove-mana': 'GR'}
STATUS = {'poison': 'POISONANIM', 'frostbite': 'FROSTBITEANIM', 'blister': 'BLISTERANIM', 'revive': 'REVIVEANIM', 'reduce-level': 'DRAINLVL',
          'transform': 'TRANSF', 'doppelganger': 'DOPPEL', 'unsummon': 'USUMMONANIM', 'restore-level': 'UDRAINLVL', 'untransform': 'UTRANSF', 'undoppelganger': 'UDOPPEL'}
DEATH = {('human', 'small'): 'DEATH_HUMAN_S13', ('heretic', 'small'): 'DEATH_HERETIC_S13', ('dwarf', 'small'): 'DEATH_DWARF_S15',
         ('undead', 'small'): 'DEATH_UNDEAD_S15', ('neutral', 'small'): 'DEATH_NEUTRAL_S10', ('dragon', 'small'): 'DEATH_DRAGON_S15',
         ('ghost', 'small'): 'DEATH_GHOST_S15', ('elf', 'small'): 'DEATH_ELF_S15', ('elf', 'large'): 'DEATH_ELF_L15'}
POSE = {'idle': 'IDLE', 'attack': 'HMOV', 'hit': 'HHIT'}
SIDE = {'attacker': 'A', 'defender': 'D'}
LAYER = {'shadow': 'S1', 'body': 'A1', 'aura': 'A2'}
ASIDE = {'attack': 'A', 'defend': 'D', 'both': 'B'}
ITEM = {'potion': 'G000IG1111', 'orb': 'G000IG2222', 'talisman': 'G000IG3333'}
CURSOR = {'default': 'DEFAULT', 'wait': 'WAIT', 'alliance': 'ALLIANCE', 'attack': 'BATTLE', 'negotiate': 'NEGO', 'enter-city': 'ENTERCITY',
          'move': 'MOVE', 'select-unit': 'SELECTUNIT', 'select-city': 'SELECTCITY', 'view-city': 'VIEWCITY', 'manage-stack': 'MANAGESTK',
          'use-scroll': 'SCROLLVAL', 'use-scroll-invalid': 'SCROLLINV', 'cast-spell': 'SPELLVAL', 'cast-spell-invalid': 'SPELLINV',
          'take-bag': 'TAKEBAG', 'exchange': 'EXCHANGE', 'visit': 'VISIT', 'drop-bag': 'DROPBAG', 'create-rod': 'DROPRODVAL',
          'create-rod-invalid': 'DROPRODINV', 'destroy-rod': 'BREAKROD', 'drag-invalid': 'NODRAGDROP', 'dismiss': 'DISMISSVAL',
          'dismiss-invalid': 'DISMISSINV', 'heal': 'HEALVAL', 'heal-invalid': 'HEALINV', 'revive': 'REVIVEVAL', 'revive-invalid': 'REVIVEINV',
          'dynamic-level': 'DYNLEVELVAL', 'dynamic-level-invalid': 'DYNLEVELINV', 'battle-default': 'CUDEFAULT', 'battle-attack': 'CUATTACK',
          'battle-attack-invalid': 'CUATTACKN', 'battle-heal': 'CUHEAL', 'battle-heal-invalid': 'CUHEALN', 'battle-transform': 'CUTRANSF',
          'battle-transform-invalid': 'CUTRANSFN'}
SPELL_LAYER = {'ground': 'G', 'under': 'U', 'front': 'F', 'over': 'O', 'top': 'T'}
CAPLETTER = {'human': 'H', 'undead': 'U', 'heretic': 'E', 'dwarf': 'D', 'elf': 'F'}


def names(i):
    t = i.split('/')
    c = t[0]
    a = t[1:]
    U = lambda s: s.upper()
    if c in ('battle-unit', 'battle-unit-shadow', 'battle-unit-aura'):
        lay = {'battle-unit': 'A1', 'battle-unit-shadow': 'S1', 'battle-unit-aura': 'A2'}[c]
        return [U(a[0]) + POSE[a[1]] + lay + SIDE[a[2]] + '00']
    if c in ('stack-unit', 'stack-unit-shadow', 'stack-unit-aura'):
        p = {'stack-unit': ('MOVE', 'STOP'), 'stack-unit-shadow': ('SMOV', 'SSTO'), 'stack-unit-aura': ('MOV2', 'STO2')}[c]
        return [U(a[0]) + (p[0] if a[1] == 'move' else p[1]) + a[2]]
    if c in ('stack-boat', 'stack-boat-shadow'):
        p = ('BTMV', 'BOAT') if c == 'stack-boat' else ('SBTM', 'SBOA')
        return ['G000RR000%d%s%s' % (v, p[0] if a[1] == 'move' else p[1], a[2]) for v in range(10)]
    if c == 'battle-effect-attack':
        return [U(a[0]) + {'hit': 'HEFF', 'touch': 'TUCH'}[a[1]] + LAYER[a[2]] + ASIDE[a[3]] + '%02d' % int(a[4])]
    if c == 'battle-effect-heal':
        return ['HEALTUCHA1B00']
    if c == 'battle-effect-blister':
        return ['BLISTUCHA1B00']
    if c == 'battle-effect-death':
        return [DEATH.get((a[0], a[1]), '?')]
    if c == 'battle-effect-item':
        return [ITEM[a[0]] + 'HMOVA1B00']
    if c == 'battle-effect-level-up':
        return ['UPGRADE' + RACE[a[0]] + ('L' if a[1] == 'large' else 'S')]
    if c == 'battle-effect-status':
        return [STATUS.get(a[0], '?') + ('L' if a[1] == 'large' else 'S')]
    if c == 'battle-corpse':
        return ['DEAD_HUMAN_%sA%02d' % ('L' if a[0] == 'large' else 'S', int(a[1]))]
    if c == 'stack-create-effect':
        return ['G000UU9999CREA8']
    if c == 'stack-destroy-effect':
        return ['G000UU9999DYIN8']
    if c == 'stack-summon-destroy-effect':
        return ['G000UU9998DYIN8']
    if c == 'stack-owner-change-effect':
        return ['G000UU9999COWNE']
    if c == 'stack-battle-effect':
        return ['ABATTLE']
    if c == 'cursor':
        n = CURSOR.get(a[0], '?')
        return [n, n + 'A']
    if c in ('landmark', 'water-landmark'):
        return [U(a[0]), U(a[0]) + 'W']
    if c == 'capital':
        r = a[-1]
        return ['G000FT0000%s%s0' % ('HU' if r == 'neutral' else RACE[r], 'C' if a[0] == 'captured' else '')]
    if c == 'city':
        return ['G000FT0000NE' + a[0]]
    if c == 'city-shadow':
        return ['G000FT0000NE' + a[0] + 'S']
    if c == 'crystal':
        return ['G000CR0000' + CRYSTAL[a[0]]]
    if c == 'ruin':
        return ['G000RU0000%d%02d' % (a[0] == 'looted', int(a[1]))]
    if c == 'site':
        return ['G000SI0000%s%02d' % (SITE[a[0]], int(a[1]))]
    if c == 'site-shadow':
        return ['G000SI0000%s%02dS' % (SITE[a[0]], int(a[1]))]
    if c == 'site-banner':
        return ['SITE_ICON_' + SITE[a[0]]]
    if c == 'ruin-banner':
        return ['RUIN_ICON_%d' % (a[0] == 'looted')]
    if c == 'stack-banner':
        return ['STACK_BANNER_%04d' % (int(a[0]) * 100)]
    if c == 'bag':
        return ['G000BG0000%03d' % (int(a[1]) if a[0] == 'water' else 100 + int(a[1]))]
    if c == 'tombstone':
        return ['G000TB0000' + ('W' if a[0] == 'water' else 'G')]
    if c == 'rod':
        return ['G000RR%sRROD8' % RACEID[a[0]]]
    if c == 'city-shield':
        return ['G000RR%s%s8' % (RACEID[a[1]], 'SHLC' if a[0] == 'capital' else 'SHLV')]
    if c == 'cloud':
        return ['CLOUDI%02d' % (4 + int(a[0]))]
    if c == 'mountain':
        return ['MOM%s%02d%02d' % ('DW' if a[0] == 'dwarf' else 'NE', int(a[1]), int(a[2]))]
    if c == 'mountain-remove-effect':
        return ['RMVMOUNTAIN' + a[0]]
    if c == 'water':
        return ['WATERBKGND1']
    if c == 'item-icon':
        return ['SCROLL' + RACE[a[1]]] if a[0] == 'scroll' else [U(a[0])]
    if c == 'spell-icon':
        return [U(a[0])]
    if c == 'building-icon':
        return [U(a[0]) + '.PNG']
    if c == 'unit-face':
        return [U(a[0]) + 'FACE.PNG']
    if c == 'unit-portrait':
        return [U(a[0]) + 'FACEB.PNG']
    if c == 'unit-event-portrait':
        return [U(a[0])]
    if c == 'lord-face':
        return ['LORD%s%s%02d' % (RACE[a[0]], 'L' if a[2] == 'large' else 'S', int(a[1]) + 1)]
    if c == 'world-spell-effect':
        pre = U(a[0]) + SPELL_LAYER[a[1]] + str(int(a[2]) + 1)
        return [k for k in anims if k.startswith(pre) and len(k) == 15] or [pre + '???']
    if c == 'city-image':
        return ['ALN' + a[0]]
    if c == 'capital-image':
        return ['AL%s0' % CAPLETTER[a[0]]]
    return None


cats = collections.OrderedDict()
for sec in ('Animations', 'Sprites'):
    for e in man[sec]:
        i = e['Id']
        c = i.split('/')[0]
        frames = len(e['Frames']) if sec == 'Animations' else 1
        st = cats.setdefault(c, {'n': 0, 'ok': 0, 'kind': collections.Counter(), 'miss': [], 'rule': True})
        st['n'] += 1
        cand = names(i)
        if cand is None:
            st['rule'] = False
            continue
        hit = None
        for n in cand:
            if n in anims:
                hit = ('anim', anims[n][0], n, len(anims[n][1]))
                break
            if n in stills:
                hit = ('still', stills[n][0], n, 1)
                break
            if n in raws:
                hit = ('raw', raws[n], n, 1)
                break
        if hit:
            st['ok'] += 1
            st['kind'][hit[0] + ':' + hit[1]] += 1
            if hit[3] != frames:
                st['kind']['framecount-mismatch'] += 1
        else:
            st['miss'].append(i + ' -> ' + cand[0])

for c, st in cats.items():
    if not st['rule']:
        print('%-36s %5d  NO RULE' % (c, st['n']))
        continue
    print('%-36s %5d  mapped %5d  %s' % (c, st['n'], st['ok'], dict(st['kind'])))
    for m in st['miss'][:4]:
        print('      miss', m)
