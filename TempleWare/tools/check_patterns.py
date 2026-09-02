import struct, os, sys

def read_file(path):
    with open(path, 'rb') as f:
        return f.read()

def rva_to_offset(data, rva):
    e_lfanew = struct.unpack_from('<I', data, 0x3C)[0]
    num_sections = struct.unpack_from('<H', data, e_lfanew + 6)[0]
    opt_size = struct.unpack_from('<H', data, e_lfanew + 20)[0]
    sec_off = e_lfanew + 24 + opt_size
    for i in range(num_sections):
        s = sec_off + i * 40
        va = struct.unpack_from('<I', data, s + 12)[0]
        vsize = struct.unpack_from('<I', data, s + 8)[0]
        raw = struct.unpack_from('<I', data, s + 20)[0]
        rsize = struct.unpack_from('<I', data, s + 16)[0]
        if va <= rva < va + max(vsize, rsize):
            return raw + (rva - va)
    return None

def offset_to_rva(data, off):
    e_lfanew = struct.unpack_from('<I', data, 0x3C)[0]
    num_sections = struct.unpack_from('<H', data, e_lfanew + 6)[0]
    opt_size = struct.unpack_from('<H', data, e_lfanew + 20)[0]
    sec_off = e_lfanew + 24 + opt_size
    for i in range(num_sections):
        s = sec_off + i * 40
        va = struct.unpack_from('<I', data, s + 12)[0]
        vsize = struct.unpack_from('<I', data, s + 8)[0]
        raw = struct.unpack_from('<I', data, s + 20)[0]
        rsize = struct.unpack_from('<I', data, s + 16)[0]
        if raw <= off < raw + rsize:
            return va + (off - raw)
    return None

def find_pattern(data, pattern):
    toks = pattern.split()
    pat = []
    mask = []
    for t in toks:
        if t == '?':
            pat.append(0); mask.append(0)
        else:
            pat.append(int(t, 16)); mask.append(1)
    n = len(pat)
    matches = []
    for i in range(len(data) - n + 1):
        ok = True
        for j in range(n):
            if mask[j] and data[i+j] != pat[j]:
                ok = False
                break
        if ok:
            matches.append(i)
    return matches

base = r"C:\Program Files (x86)\Steam\steamapps\common\Counter-Strike Global Offensive\game"
modules = {
    "client.dll": os.path.join(base, "csgo", "bin", "win64", "client.dll"),
    "engine2.dll": os.path.join(base, "bin", "win64", "engine2.dll"),
    "scenesystem.dll": os.path.join(base, "bin", "win64", "scenesystem.dll"),
    "materialsystem2.dll": os.path.join(base, "bin", "win64", "materialsystem2.dll"),
}

patterns = {
    "client.dll": [
        ("Input (CCSGOInput)", "48 8B 0D ? ? ? ? 8B D3 E8 ? ? ? ? ? ? ? ? F2 0F 11 45"),
        ("EntitySystem", "48 8B 0D ? ? ? ? 48 89 7C 24 ? 8B FA C1 EB"),
        ("oGetWeaponData", "48 8B 81 ? ? ? ? 85 D2 78 ? 48 83 FA ? 73 ? F3 0F 10 84 90 ? ? ? ? C3 F3 0F 10 80 ? ? ? ? C3 CC CC CC CC"),
        ("ogGetBaseEntity", "4C 8D 49 10 81 FA FE 7F 00 00 ? ? 8B CA C1 F9 09 83 F9 3F ? ? 48 63 C1 4D"),
        ("FrameStageNotify", "48 89 5C 24 ? 48 89 6C 24 ? 57 48 83 EC 40 48 8B F9 33 ED"),
        ("GetRenderFov", "40 53 48 83 EC ? 48 8B D9 E8 ? ? ? ? 48 85 C0 74 ? 48 8B C8 48 83 C4"),
        ("LevelInit", "48 89 74 24 ? 57 48 83 EC ? 48 8B 0D ? ? ? ? 48 8B FA"),
        ("LevelShutdown", "48 83 EC ? 48 8B 0D ? ? ? ? 48 8D 15 ? ? ? ? 45 33 C9 45 33 C0 48 8B 01 FF 50 ? 48 85 C0 74 ? 48 8B 0D ? ? ? ? 48 8B D0 4C 8B 01 41 FF 50 ? 48 83 C4"),
        ("RenderFlashBangOverlay", "85 D2 0F 88 ? ? ? ? 48 89 4C 24 ? 55 56"),
        ("OnAddEntity", "48 89 74 24 ? 57 48 83 EC ? 41 B9 ? ? ? ? 41 8B C0 41 23 C1 48 8B F2 41 83 F8 ? 48 8B F9 44 0F 45 C8 41 81 F9 ? ? ? ? 73 ? FF 81"),
        ("OnRemoveEntity", "48 89 74 24 ? 57 48 83 EC ? 41 B9 ? ? ? ? 41 8B C0 41 23 C1 48 8B F2 41 83 F8 ? 48 8B F9 44 0F 45 C8 41 81 F9 ? ? ? ? 73 ? FF 89"),
        ("GlobalVars", "48 8B 0D ? ? ? ? 4C 8D 05 ? ? ? ? 48 85 D2"),
    ],
    "scenesystem.dll": [
        ("DrawArray", "48 8B C4 53 57 41 54 48 81 EC D0 00 00 00 49 63 F9 49"),
        ("UpdateAggregateSceneObject", "48 8B C4 48 89 50 ? 48 89 48 ? 55 53 56 57 41 54 41 55 41 56 41 57 48 8D A8 ? ? ? ? 48 81 EC ? ? ? ? 0F 29 70"),
        ("UpdateLightObject", "48 89 54 24 ? 55 57 41 56 48 83 EC"),
    ],
    "engine2.dll": [
        ("g_pPVS", "48 8D 0D ? ? ? ? 33 D2 FF 50"),
    ],
    "materialsystem2.dll": [
        ("CreateMaterial", "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 48 89 7C 24 ? 41 56 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 8B F2"),
    ],
}

for mod, plist in patterns.items():
    path = modules[mod]
    data = read_file(path)
    print(f"=== {mod} ({len(data)} bytes) ===")
    for name, pat in plist:
        m = find_pattern(data, pat)
        if m:
            off = m[0]
            rva = offset_to_rva(data, off)
            rva_s = ("0x%X" % rva) if rva is not None else "?"
            print(f"  MATCH ({len(m)}x) {name}: file=0x{off:X} rva={rva_s}")
        else:
            print(f"  NO MATCH {name}")
    print()
