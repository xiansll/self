import re, os, json, sys

SRC = r'C:\Dev\CS2\velocity\cs2\velocity-cs2\project\protection\patterns.cpp'
BASE = r'C:\Program Files (x86)\Steam\steamapps\common\Counter-Strike Global Offensive\game'

MODULE_DIRS = {
    'client.dll': os.path.join(BASE, 'csgo', 'bin', 'win64'),
    'server.dll': os.path.join(BASE, 'csgo', 'bin', 'win64'),
    'engine2.dll': os.path.join(BASE, 'bin', 'win64'),
    'scenesystem.dll': os.path.join(BASE, 'bin', 'win64'),
    'materialsystem2.dll': os.path.join(BASE, 'bin', 'win64'),
    'rendersystemdx11.dll': os.path.join(BASE, 'bin', 'win64'),
    'schemasystem.dll': os.path.join(BASE, 'bin', 'win64'),
    'soundsystem.dll': os.path.join(BASE, 'bin', 'win64'),
    'particles.dll': os.path.join(BASE, 'bin', 'win64'),
    'networksystem.dll': os.path.join(BASE, 'bin', 'win64'),
    'tier0.dll': os.path.join(BASE, 'bin', 'win64'),
    'worldrenderer.dll': os.path.join(BASE, 'bin', 'win64'),
    'vphysics2.dll': os.path.join(BASE, 'bin', 'win64'),
    'animationsystem.dll': os.path.join(BASE, 'bin', 'win64'),
    'meshsystem.dll': os.path.join(BASE, 'bin', 'win64'),
    'resourcesystem.dll': os.path.join(BASE, 'bin', 'win64'),
    'filesystem_stdio.dll': os.path.join(BASE, 'bin', 'win64'),
    'panorama.dll': os.path.join(BASE, 'bin', 'win64'),
    'pulse_system.dll': os.path.join(BASE, 'bin', 'win64'),
    'host.dll': os.path.join(BASE, 'bin', 'win64'),
    'steamaudio.dll': os.path.join(BASE, 'bin', 'win64'),
}

def read_module(mod):
    d = MODULE_DIRS.get(mod)
    if not d:
        return None
    p = os.path.join(d, mod)
    if os.path.exists(p):
        return open(p, 'rb').read()
    return None

def hex_val(c):
    if '0' <= c <= '9': return ord(c)-48
    if 'a' <= c <= 'f': return ord(c)-87
    if 'A' <= c <= 'F': return ord(c)-55
    return -1

def parse_pattern(pat):
    # returns (bytes list, wildcard list, op, op_off, post_offset, deref)
    bs = []
    wild = []
    op = 'direct'
    op_off = 0
    post = 0
    deref = False
    i = 0
    n = len(pat)
    while i < n:
        c = pat[i]
        if c in ' \t':
            i += 1; continue
        if c == '>':
            op = 'rel_call'; op_off = len(bs); i += 1; continue
        if c == '*':
            op = 'rip_relative'; op_off = len(bs); i += 1; continue
        if c == '^':
            op = 'absolute_ptr'; op_off = len(bs); i += 1; continue
        if c == '~':
            deref = True; i += 1; continue
        if c in '+-':
            neg = (c == '-'); i += 1; val = 0
            while i < n:
                h = hex_val(pat[i])
                if h < 0: break
                val = (val << 4) | h; i += 1
            post = -val if neg else val
            continue
        if c == '?':
            bs.append(0); wild.append(True); i += 1
            if i < n and pat[i] == '?': i += 1
            continue
        high = hex_val(pat[i]); i += 1
        if i < n:
            low = hex_val(pat[i])
            if low != -1:
                bs.append((high << 4) | low); wild.append(False); i += 1
            else:
                bs.append(high); wild.append(False)
        else:
            bs.append(high); wild.append(False)
    return bs, wild, op, op_off, post, deref

def find_match(data, bs, wild):
    n = len(bs)
    if n == 0: return -1
    # find first non-wildcard byte for speed
    first = -1
    for i in range(n):
        if not wild[i]: first = i; break
    if first == -1: return 0
    fb = bs[first]
    results = []
    idx = 0
    while True:
        p = data.find(bytes([fb]), idx)
        if p < 0: break
        start = p - first
        if start < 0:
            idx = p + 1; continue
        if start + n > len(data):
            break
        ok = True
        for j in range(n):
            if wild[j]: continue
            if data[start + j] != bs[j]:
                ok = False; break
        if ok:
            results.append(start)
        idx = p + 1
    return results[0] if results else -1

def resolve(data, match, bs, wild, op, op_off, post, deref):
    import struct
    base_off = match
    # op resolve: read a 4-byte relative/absolute at the op position
    target = base_off
    if op == 'rel_call':
        # '>' marks a E8 relative call; the relative offset is typically at op_off+1
        # In this codebase, '>' is placed right before the E8, so the imm32 follows.
        # We approximate: relative offset is the dword right after op_off.
        p = base_off + op_off
        # skip the E8 opcode byte if present
        # read dword at p+1
        rel = struct.unpack_from('<i', data, p + 1)[0]
        target = (p + 1 + 4) + rel  # after the E8 + imm32
    elif op == 'rip_relative':
        p = base_off + op_off
        rel = struct.unpack_from('<i', data, p)[0]
        target = (p + 4) + rel
    elif op == 'absolute_ptr':
        p = base_off + op_off
        target = struct.unpack_from('<Q', data, p)[0]
    target += post
    # convert file offset -> RVA is complex; just return the file offset for match verification
    return target

def main():
    src = open(SRC, encoding='utf-8').read()
    # extract ADDRESS_IMPL blocks
    blocks = re.findall(r'const ::protection::addresses::address_t& (\w+) = ADDRESS_IMPL\(\s*::protection::addresses::hash\("([^"]+)"\),\s*::protection::addresses::address_type::pattern,\s*"([^"]+)"\);', src)
    print(f'total patterns parsed: {len(blocks)}')
    mods_loaded = {}
    matches = 0
    no_match = []
    for name, hashv, pat in blocks:
        if ':' not in pat:
            no_match.append((name, pat, 'no module prefix')); continue
        mod, body = pat.split(':', 1)
        data = mods_loaded.get(mod)
        if data is None:
            data = read_module(mod)
            mods_loaded[mod] = data
        if data is None:
            no_match.append((name, mod, 'module file not found')); continue
        bs, wild, op, op_off, post, deref = parse_pattern(body)
        m = find_match(data, bs, wild)
        if m < 0:
            no_match.append((name, mod, body[:60]))
        else:
            matches += 1
    print(f'MATCH: {matches}')
    print(f'NO MATCH: {len(no_match)}')
    print()
    for name, mod, body in no_match:
        print(f'  {name}  [{mod}]  {body}')

if __name__ == '__main__':
    main()
