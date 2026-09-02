import json, re, os

ROOT = r"C:/CS/TempleWare/TempleWare-CS2/source"
TABLE = ROOT + "/templeware/utils/schema/schema_offsets_table.h"
DUMP_DIR = r"C:/CS/TempleWare/alldump"

def h32(s):
    v = 0x811c9dc5
    for ch in s.encode('ascii'):
        v = ((v ^ ch) * 0x1000193) & 0xFFFFFFFF
    return v

# load dumps: class -> field -> offset
def load_dump(fn):
    d = json.load(open(fn, encoding='utf-8'))
    mod = list(d.keys())[0]
    return d[mod]['classes']
classes = {}
for fn in ["client_dll.json", "animationsystem_dll.json"]:
    p = os.path.join(DUMP_DIR, fn)
    if os.path.exists(p):
        for cn, cv in load_dump(p).items():
            classes.setdefault(cn, {}).update(cv.get('fields', {}))

# existing entries (preserve)
txt = open(TABLE, encoding='utf-8').read()
entries = {}  # hash -> (offset, comment)
for m in re.finditer(r"\{\s*0x([0-9A-Fa-f]+)u,\s*0x([0-9A-Fa-f]+)u\s*\},\s*//\s*(.*)", txt):
    entries[int(m.group(1),16)] = (int(m.group(2),16), m.group(3).strip())

# nerv SCHEMA pairs
pairs = set()
for root,_,files in os.walk(ROOT + "/nerv/valve"):
    for f in files:
        if f.endswith(('.hpp','.h','.cpp')):
            s = open(os.path.join(root,f), encoding='utf-8', errors='ignore').read()
            for m in re.finditer(r'SCHEMA(?:_ARRAY)?\s*\([^;]*?"([A-Za-z_0-9]+)"\s*,\s*"([A-Za-z_0-9]+)"', s):
                pairs.add((m.group(1), m.group(2)))

added=0; missing=[]
for cls, fld in sorted(pairs):
    off = classes.get(cls, {}).get(fld)
    if off is None:
        missing.append((cls,fld)); continue
    key = h32(f"{cls}->{fld}")   # table convention: fnv1a32 of "Class->Field"
    if key in entries:
        continue  # keep existing
    entries[key] = (off, f"{cls}->{fld}")
    added += 1

# emit
lines = ["#pragma once", "#include <cstdint>", "",
    "// Auto-generated schema offsets (fnv1a32(class)^fnv1a32(field) -> offset).",
    "// Regenerated from cs2-dumper dump to cover both TempleWare and nerv fields.",
    "struct SchemaOffsetEntry { std::uint32_t hash; std::uint32_t offset; };",
    "static const SchemaOffsetEntry g_schemaOffsets[] = {"]
for k in sorted(entries):
    off, cm = entries[k]
    lines.append(f"    {{ 0x{k:08X}u, 0x{off:X}u }}, // {cm}")
lines.append("};")
lines.append(f"static constexpr std::size_t g_schemaOffsetCount = {len(entries)};")
open(TABLE,'w',encoding='utf-8',newline='\n').write("\n".join(lines)+"\n")
print(f"total entries={len(entries)} added={added} missing_from_dump={len(missing)}")
for m in missing[:20]: print("  MISSING:", m)
