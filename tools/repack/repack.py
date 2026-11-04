import io, json, os, re, struct, sys, time, collections
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
src = open(os.path.join(HERE, 'ffscan.py')).read().split('root = sys.argv[1]')[0]
ns = {}
exec(src, ns)
load_ff = ns['load']

RACES = {'human': 'HU', 'undead': 'UN', 'heretic': 'HE', 'dwarf': 'DW', 'neutral': 'NE', 'elf': 'EL'}
RACE_IDS = {'human': '0000', 'undead': '0003', 'heretic': '0002', 'dwarf': '0001', 'neutral': '8888', 'elf': '0005'}
BOATS = {'human': '0', 'undead': '1', 'heretic': '2', 'dwarf': '3', 'neutral': '4', 'elf': '5'}
CAP_LETTERS = {'human': 'H', 'undead': 'U', 'heretic': 'E', 'dwarf': 'D', 'elf': 'F'}
SITES = {'mage': 'MAGE', 'merchant': 'MERH', 'mercenary': 'MERC', 'trainer': 'TRAI'}
CRYSTALS = {'gold': 'GL', 'death-mana': 'RG', 'rune-mana': 'WH', 'life-mana': 'YE', 'infernal-mana': 'RD', 'grove-mana': 'GR'}
POSES = {'idle': 'IDLE', 'attack': 'HMOV', 'hit': 'HHIT'}
SIDES = {'attacker': 'A', 'defender': 'D'}
LAYERS = {'shadow': 'S1', 'body': 'A1', 'aura': 'A2'}
ATTACK_SIDES = {'attack': 'A', 'defend': 'D', 'both': 'B'}
ATTACK_TYPES = {'hit': 'HEFF', 'touch': 'TUCH'}
ITEMS = {'potion': 'G000IG1111', 'orb': 'G000IG2222', 'talisman': 'G000IG3333'}
SPELL_LAYERS = {'ground': 'G', 'under': 'U', 'front': 'F', 'over': 'O', 'top': 'T'}
STATUSES = {'poison': 'POISONANIM', 'frostbite': 'FROSTBITEANIM', 'blister': 'BLISTERANIM', 'revive': 'REVIVEANIM',
            'reduce-level': 'DRAINLVL', 'transform': 'TRANSF', 'doppelganger': 'DOPPEL', 'unsummon': 'USUMMONANIM',
            'restore-level': 'UDRAINLVL', 'untransform': 'UTRANSF', 'undoppelganger': 'UDOPPEL'}
DEATHS = {'human/small': 'DEATH_HUMAN_S13', 'heretic/small': 'DEATH_HERETIC_S13', 'dwarf/small': 'DEATH_DWARF_S15',
          'undead/small': 'DEATH_UNDEAD_S15', 'neutral/small': 'DEATH_NEUTRAL_S10', 'dragon/small': 'DEATH_DRAGON_S15',
          'ghost/small': 'DEATH_GHOST_S15', 'elf/small': 'DEATH_ELF_S15', 'elf/large': 'DEATH_ELF_L15'}
CURSORS = {'default': 'DEFAULT', 'wait': 'WAIT', 'alliance': 'ALLIANCE', 'attack': 'BATTLE', 'negotiate': 'NEGO',
           'enter-city': 'ENTERCITY', 'move': 'MOVE', 'select-unit': 'SELECTUNIT', 'select-city': 'SELECTCITY',
           'view-city': 'VIEWCITY', 'manage-stack': 'MANAGESTK', 'use-scroll': 'SCROLLVAL', 'use-scroll-invalid': 'SCROLLINV',
           'cast-spell': 'SPELLVAL', 'cast-spell-invalid': 'SPELLINV', 'take-bag': 'TAKEBAG', 'exchange': 'EXCHANGE',
           'visit': 'VISIT', 'drop-bag': 'DROPBAG', 'create-rod': 'DROPRODVAL', 'create-rod-invalid': 'DROPRODINV',
           'destroy-rod': 'BREAKROD', 'drag-invalid': 'NODRAGDROP', 'dismiss': 'DISMISSVAL', 'dismiss-invalid': 'DISMISSINV',
           'heal': 'HEALVAL', 'heal-invalid': 'HEALINV', 'revive': 'REVIVEVAL', 'revive-invalid': 'REVIVEINV',
           'dynamic-level': 'DYNLEVELVAL', 'dynamic-level-invalid': 'DYNLEVELINV', 'battle-default': 'CUDEFAULTA',
           'battle-attack': 'CUATTACKA', 'battle-attack-invalid': 'CUATTACKNA', 'battle-heal': 'CUHEALA',
           'battle-heal-invalid': 'CUHEALNA', 'battle-transform': 'CUTRANSFA', 'battle-transform-invalid': 'CUTRANSFNA'}
CONSTANTS = {'battle-effect-heal': 'HEALTUCHA1B00', 'battle-effect-blister': 'BLISTUCHA1B00',
             'stack-create-effect': 'G000UU9999CREA8', 'stack-destroy-effect': 'G000UU9999DYIN8',
             'stack-summon-destroy-effect': 'G000UU9998DYIN8', 'stack-owner-change-effect': 'G000UU9999COWNE',
             'stack-battle-effect': 'ABATTLE', 'water': 'WATERBKGND1'}

CANVAS, CENTRE, SPRITE = 'canvas', 'centre', 'sprite'


def num(s):
    n = ''
    for ch in s:
        if not ch.isdigit():
            break
        n += ch
    return int(n or 0)


def game_name(pid):
    t = pid.split('/') + [''] * 8
    c = t[0]
    unit = t[1].upper() if len(t[1]) < 16 else ''
    if c.startswith('battle-unit'):
        layer = {'battle-unit': 'A1', 'battle-unit-shadow': 'S1', 'battle-unit-aura': 'A2'}.get(c)
        if layer and t[2] in POSES and t[3] in SIDES:
            return unit + POSES[t[2]] + layer + SIDES[t[3]] + '00', CANVAS
        return None, CANVAS
    if c == 'battle-effect-attack':
        if t[2] in ATTACK_TYPES and t[3] in LAYERS and t[4] in ATTACK_SIDES:
            return '%s%s%s%s%02d' % (unit, ATTACK_TYPES[t[2]], LAYERS[t[3]], ATTACK_SIDES[t[4]], num(t[5])), CANVAS
        return None, CANVAS
    if c == 'battle-effect-death':
        return DEATHS.get(t[1] + '/' + t[2]), CANVAS
    if c == 'battle-effect-item':
        return (ITEMS[t[1]] + 'HMOVA1B00') if t[1] in ITEMS else None, CANVAS
    if c == 'battle-effect-level-up':
        return ('UPGRADE' + RACES[t[1]] + ('L' if t[2] == 'large' else 'S')) if t[1] in RACES else None, CANVAS
    if c == 'battle-effect-status':
        return (STATUSES[t[1]] + ('L' if t[2] == 'large' else 'S')) if t[1] in STATUSES else None, CANVAS
    if c in CONSTANTS:
        return CONSTANTS[c], CANVAS if c.startswith('battle-') or c == 'water' else CENTRE
    if c.startswith('stack-unit'):
        move = t[2] == 'move'
        pose = {'stack-unit': ('MOVE', 'STOP'), 'stack-unit-shadow': ('SMOV', 'SSTO'), 'stack-unit-aura': ('MOV2', 'STO2')}.get(c)
        return (unit + pose[0 if move else 1] + t[3]) if pose else None, CENTRE
    if c in ('stack-boat', 'stack-boat-shadow'):
        move = t[2] == 'move'
        pose = ('BTMV', 'BOAT') if c == 'stack-boat' else ('SBTM', 'SBOA')
        return ('G000RR000' + BOATS[t[1]] + pose[0 if move else 1] + t[3]) if t[1] in BOATS else None, CENTRE
    if c == 'battle-corpse':
        return 'DEAD_HUMAN_%sA%02d' % ('L' if t[1] == 'large' else 'S', num(t[2])), CENTRE
    if c == 'cursor':
        return CURSORS.get(t[1]), CENTRE
    if c in ('landmark', 'water-landmark'):
        return unit or None, CENTRE
    if c == 'capital':
        captured = t[1] == 'captured'
        race = t[2] if captured else t[1]
        r = 'HU' if race == 'neutral' else RACES.get(race)
        return ('G000FT0000%s%s0' % (r, 'C' if captured else '')) if r else None, CENTRE
    if c == 'city':
        return 'G000FT0000NE' + t[1], CENTRE
    if c == 'city-shadow':
        return 'G000FT0000NE' + t[1] + 'S', CENTRE
    if c == 'crystal':
        return ('G000CR0000' + CRYSTALS[t[1]]) if t[1] in CRYSTALS else None, CENTRE
    if c == 'ruin':
        return 'G000RU0000%d%02d' % (1 if t[1] == 'looted' else 0, num(t[2])), CENTRE
    if c in ('site', 'site-shadow'):
        return ('G000SI0000%s%02d%s' % (SITES[t[1]], num(t[2]), 'S' if c == 'site-shadow' else '')) if t[1] in SITES else None, CENTRE
    if c == 'site-banner':
        return ('SITE_ICON_' + SITES[t[1]]) if t[1] in SITES else None, CENTRE
    if c == 'ruin-banner':
        return 'RUIN_ICON_%d' % (1 if t[1] == 'looted' else 0), CENTRE
    if c == 'stack-banner':
        return 'STACK_BANNER_%04d' % (num(t[1]) * 100), CENTRE
    if c == 'bag':
        return 'G000BG0000%03d' % (num(t[2]) if t[1] == 'water' else 100 + num(t[2])), CENTRE
    if c == 'tombstone':
        return 'G000TB0000' + ('W' if t[1] == 'water' else 'G'), CENTRE
    if c == 'rod':
        return ('G000RR%sRROD8' % RACE_IDS[t[1]]) if t[1] in RACE_IDS else None, CENTRE
    if c == 'city-shield':
        return ('G000RR%s%s8' % (RACE_IDS[t[2]], 'SHLC' if t[1] == 'capital' else 'SHLV')) if t[2] in RACE_IDS else None, CENTRE
    if c == 'cloud':
        return 'CLOUDI%02d' % (4 + num(t[1])), CENTRE
    if c == 'mountain':
        return 'MOM%s%02d%02d' % ('DW' if t[1] == 'dwarf' else 'NE', num(t[2]), num(t[3])), CENTRE
    if c == 'mountain-remove-effect':
        return 'RMVMOUNTAIN' + t[1], CENTRE
    if c == 'world-spell-effect':
        return ('%s%s%d' % (unit, SPELL_LAYERS[t[2]], num(t[3]) + 1)) if t[2] in SPELL_LAYERS else None, CENTRE
    if c == 'item-icon':
        if t[1] == 'scroll':
            return ('SCROLL' + RACES[t[2]]) if t[2] in RACES else None, SPRITE
        return unit or None, SPRITE
    if c in ('spell-icon', 'unit-event-portrait'):
        return unit or None, SPRITE
    if c == 'building-icon':
        return unit + '.PNG', SPRITE
    if c == 'unit-face':
        return unit + 'FACE.PNG', SPRITE
    if c == 'unit-portrait':
        return unit + 'FACEB.PNG', SPRITE
    if c == 'lord-face':
        return ('LORD%s%s%02d' % (RACES[t[1]], 'L' if t[3] == 'large' else 'S', num(t[2]) + 1)) if t[1] in RACES else None, SPRITE
    if c == 'city-image':
        return 'ALN' + t[1], SPRITE
    if c == 'capital-image':
        return ('AL%s0' % CAP_LETTERS[t[1]]) if t[1] in CAP_LETTERS else None, SPRITE
    return None, None


HOLE_EDGE = {'DLG_MAIN_MENU_BG0TRANS'}


def fill_hole_edges(data, steps=8):
    im = np.array(Image.open(io.BytesIO(data)).convert('RGBA'))
    solid = im[:, :, 3] >= 128
    for _ in range(steps):
        grow = ~solid
        src = np.zeros(im.shape, im.dtype)
        got = np.zeros(solid.shape, bool)
        for dy, dx in ((0, 1), (0, -1), (1, 0), (-1, 0)):
            ns = np.roll(solid, (dy, dx), (0, 1))
            ni = np.roll(im, (dy, dx), (0, 1))
            take = grow & ns & ~got
            src[take] = ni[take]
            got |= take
        im[got] = src[got]
        im[got, 3] = 255
        solid |= got
    out = io.BytesIO()
    Image.fromarray(im, 'RGBA').save(out, 'WEBP', lossless=True)
    return out.getvalue()


def read_manifest(path):
    with open(path, 'rb') as f:
        head = f.read(32)
        assert head[:11] == b'GALLEANPACK' and struct.unpack_from('<i', head, 12)[0] == 27, path
        off, ln = struct.unpack_from('<qq', head, 16)
        f.seek(off)
        return head, json.loads(f.read(ln))


def frames_of(entry, anim):
    return entry['Frames'] if anim else [entry]


def main():
    game, gallean, ui, out = sys.argv[1:5]
    t0 = time.time()

    stills, anims, raws = {}, {}, set()
    for sub in ('Imgs', 'Interf'):
        folder = os.path.join(game, sub)
        for f in sorted(os.listdir(folder)):
            if f.lower().endswith('.ff'):
                r = load_ff(os.path.join(folder, f))
                for k, v in r['stills'].items():
                    stills.setdefault(k, v)
                for k, v in r['anims'].items():
                    anims.setdefault(k, v)
                raws.update(r['raw'])
    spell_anims = collections.defaultdict(list)
    for k in anims:
        if len(k) == 15 and k.startswith('G000SS'):
            spell_anims[k[:12]].append(k)

    def dims_of(name):
        if name in anims:
            return anims[name]
        if name in stills:
            return [stills[name]]
        return None

    def resolve(name):
        cands = [name]
        cands.append(name[:-4] if name.endswith('.PNG') else name + '.PNG')
        for n in cands:
            if n in anims or n in stills or n in raws:
                return [n]
        return spell_anims.get(name, [])

    entries = collections.OrderedDict()
    sources = []
    stats = collections.Counter()
    unmapped = collections.Counter()

    def add(src, name, kind, entry):
        if re.fullmatch(r'FOG\d{4}', name):
            stats['excluded'] += 1
            return
        if name in entries:
            stats['duplicate'] += 1
            return
        entries[name] = (src, kind, entry)

    ui_head, ui_man = read_manifest(ui)
    g_head, g_man = read_manifest(gallean)
    sources.append(ui)
    sources.append(gallean)
    for kind in ('Sprites', 'Animations'):
        anim = kind == 'Animations'
        for e in g_man[kind]:
            pid = e['Id']
            name, origin = game_name(pid)
            if not name:
                unmapped[pid.split('/')[0]] += 1
                continue
            targets = resolve(name)
            if not targets:
                stats['not-in-game'] += 1
                continue
            for target in targets:
                dims = dims_of(target)
                if origin == CENTRE and not dims:
                    stats['no-dims'] += 1
                    continue
                ne = json.loads(json.dumps(e))
                ne['Id'] = target
                for k, fr in enumerate(frames_of(ne, anim)):
                    md = fr['Metadata']
                    if origin == CENTRE:
                        w, h = dims[min(k, len(dims) - 1)]
                        md['OffsetX'] += w // 2
                        md['OffsetY'] += h // 2
                    elif origin == SPRITE:
                        md['OffsetX'] = md['OffsetY'] = 0
                before = len(entries)
                add(1, target, kind, ne)
                if len(entries) > before:
                    stats['gallean'] += 1

    for kind in ('Sprites', 'Animations'):
        for e in ui_man[kind]:
            before = len(entries)
            add(0, e['Id'].split('/')[-1].upper(), kind, e)
            if len(entries) > before:
                stats['own'] += 1

    mem = []
    with open(ui, 'rb') as h:
        for name, (src, kind, e) in entries.items():
            if src != 0 or kind != 'Sprites' or name not in HOLE_EDGE:
                continue
            h.seek(32 + e['Offset'])
            data = fill_hole_edges(h.read(e['Length']))
            mem.append(data)
            entries[name] = (2, kind, dict(e, Offset=len(mem) - 1, Length=len(data)))
            print('filled hole edges', name)

    blobs = {}
    order = []
    for name, (src, kind, e) in entries.items():
        for fr in frames_of(e, kind == 'Animations'):
            key = (src, fr['Offset'], fr['Length'])
            if key not in blobs:
                blobs[key] = None
                order.append(key)
    order.sort()

    tmp = out + '.tmp'
    pos = 0
    with open(tmp, 'wb') as o:
        o.write(b'\0' * 32)
        handles = [open(s, 'rb') for s in sources]
        for key in order:
            src, off, ln = key
            if src == 2:
                data = mem[off]
            else:
                h = handles[src]
                h.seek(32 + off)
                data = h.read(ln)
            assert len(data) == ln
            o.write(data)
            blobs[key] = pos
            pos += ln
        for h in handles:
            h.close()

        sprites, animations = [], []
        for name, (src, kind, e) in entries.items():
            for fr in frames_of(e, kind == 'Animations'):
                fr['Offset'] = blobs[(src, fr['Offset'], fr['Length'])]
            e['Id'] = name
            (animations if kind == 'Animations' else sprites).append(e)
        manifest = json.dumps({'CreatedAt': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()), 'CreatedBy': 'repack.py',
                               'Sparse': False, 'Sprites': sprites, 'Animations': animations, 'Sounds': []},
                              separators=(',', ':')).encode('utf8')
        moff = 32 + pos
        o.write(manifest)
        o.seek(0)
        o.write(g_head[:12] + struct.pack('<iqq', 27, moff, len(manifest)))
    os.replace(tmp, out)

    print('entries', len(entries), dict(stats))
    print('blobs', len(order), 'bytes', pos, 'size', os.path.getsize(out))
    print('unmapped by category', dict(unmapped))
    print('time %.1fs' % (time.time() - t0))


main()
