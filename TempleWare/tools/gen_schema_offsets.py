import json, re, os, glob

SRC = r"C:\Dev\CS2\TempleWare-CS2-1.1.5\TempleWare-CS2\source"
DUMP = r"C:\Dev\CS2\Antigravity\output"

def fnv1a(s):
    h = 0x811c9dc5
    for b in s.encode('utf-8'):
        h = ((h ^ b) * 0x1000193) & 0xFFFFFFFF
    return h

# load client + server schema
def load_schema(path):
    d = json.load(open(path, encoding='utf-8'))
    out = {}
    for mod, v in d.items():
        if isinstance(v, dict) and 'classes' in v:
            out.update(v['classes'])
    return out

client_classes = load_schema(os.path.join(DUMP, 'client_dll.json'))
server_classes = load_schema(os.path.join(DUMP, 'server_dll.json'))

def resolve_in(classes, cn, fn):
    seen = set()
    while cn and cn not in seen:
        seen.add(cn)
        c = classes.get(cn)
        if not c:
            return None
        if fn in c.get('fields', {}):
            return c['fields'][fn]
        cn = c.get('parent')
    return None

def resolve(cn, fn):
    r = resolve_in(client_classes, cn, fn)
    if r is not None:
        return r
    return resolve_in(server_classes, cn, fn)

# extract "Class->field" strings from schema() macro usages
field_strings = set()
for path in glob.glob(os.path.join(SRC, '**', '*.h'), recursive=True) + glob.glob(os.path.join(SRC, '**', '*.cpp'), recursive=True):
    try:
        txt = open(path, encoding='utf-8', errors='ignore').read()
    except:
        continue
    for m in re.finditer(r'schema\s*\(\s*[^,]+,\s*[^,]+,\s*"([^"]+->[^"]+)"', txt):
        field_strings.add(m.group(1))

print(f"found {len(field_strings)} unique field strings")

rows = []
missing = []
for fs in sorted(field_strings):
    if '->' not in fs:
        continue
    cn, fn = fs.split('->', 1)
    off = resolve(cn, fn)
    if off is None:
        missing.append(fs)
        continue
    h = fnv1a(fs)
    rows.append((h, off, fs))

print(f"resolved {len(rows)}, missing {len(missing)}")
for m in missing:
    print("  MISSING:", m)

# generate header
rows.sort(key=lambda r: r[0])
out = []
out.append("#pragma once")
out.append("#include <cstdint>")
out.append("")
out.append("// Auto-generated from Antigravity schema dump (build 14178). Do not edit by hand.")
out.append("struct SchemaOffsetEntry { std::uint32_t hash; std::uint32_t offset; };")
out.append("static const SchemaOffsetEntry g_schemaOffsets[] = {")
for h, off, fs in rows:
    out.append(f"    {{ 0x{h:08X}u, 0x{off:X}u }}, // {fs}")
out.append("};")
out.append(f"static constexpr std::size_t g_schemaOffsetCount = {len(rows)};")
out.append("")

dst = os.path.join(SRC, "templeware", "utils", "schema", "schema_offsets_table.h")
with open(dst, 'w', encoding='utf-8') as f:
    f.write("\n".join(out) + "\n")

print(f"wrote {dst} with {len(rows)} entries")
