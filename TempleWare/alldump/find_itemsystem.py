import pefile, capstone, struct

DLL = r"C:/Program Files (x86)/Steam/steamapps/common/Counter-Strike Global Offensive/game/csgo/bin/win64/client.dll"
pe = pefile.PE(DLL, fast_load=True)
IB = pe.OPTIONAL_HEADER.ImageBase
img = pe.get_memory_mapped_image()

def sec(name):
    for s in pe.sections:
        if s.Name.rstrip(b'\x00') == name:
            return s.VirtualAddress, s.VirtualAddress + s.Misc_VirtualSize
text_lo, text_hi = sec(b'.text')
rdata_lo, rdata_hi = sec(b'.rdata')

md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64); md.detail = True

def typedesc(name):
    i = img.find(name); return None if i < 0 else i - 0x10

def vtables_for(name):
    td = typedesc(name)
    if td is None: return []
    cols = []
    tdb = struct.pack("<I", td); p = rdata_lo
    while True:
        i = img.find(tdb, p, rdata_hi)
        if i < 0: break
        cols.append(i - 0xC); p = i + 1
    vts = []
    for col in cols:
        vb = struct.pack("<Q", IB + col); p = rdata_lo
        while True:
            i = img.find(vb, p, rdata_hi)
            if i < 0: break
            vts.append(i + 8); p = i + 1
    return vts

def find_ctor_start(vt_rva):
    # locate: lea reg,[rip+vt] ; mov [reg], reg  (this->vtable = vt), then walk
    # back over int3 padding to the function start.
    tcode = img[text_lo:text_hi]
    for op in (b"\x48\x8d\x05", b"\x4c\x8d\x05"):
        s = 0
        while True:
            j = tcode.find(op, s)
            if j < 0: break
            s = j + 1
            rva = text_lo + j
            rel = struct.unpack("<i", img[rva+3:rva+7])[0]
            if rva + 7 + rel == vt_rva:
                nxt = img[rva+7:rva+11]
                # 48 89 01 (mov [rcx],rax) or 48 89 03 etc.
                if nxt[0] == 0x48 and nxt[1] == 0x89 and (nxt[2] & 0xC0) == 0x00:
                    # walk back to prologue: nearest 0xCC padding boundary
                    k = rva
                    while k > text_lo and img[k-1] != 0xCC:
                        k -= 1
                    yield k
def rel_targets_calls(func_rva):
    # find e8 rel32 calls whose target == func_rva
    tcode = img[text_lo:text_hi]; s = 0; res = []
    while True:
        j = tcode.find(b"\xe8", s)
        if j < 0: break
        s = j + 1
        rva = text_lo + j
        rel = struct.unpack("<i", img[rva+1:rva+5])[0]
        if rva + 5 + rel == func_rva:
            res.append(rva)
    return res

for cname in [b".?AVCCStrike15ItemSystem@@", b".?AVCEconItemSystem@@"]:
    print("===", cname.decode())
    for vt in vtables_for(cname):
        print(" vtable RVA=%#x" % vt)
        for ctor in set(find_ctor_start(vt)):
            calls = rel_targets_calls(ctor)
            print("  ctor@%#x  callers=%d" % (ctor, len(calls)))
            for c in calls:
                win = img[c: c+0x40]
                for ins in md.disasm(win, IB + c):
                    if ins.mnemonic == "mov" and ins.op_str.startswith("qword ptr [rip"):
                        # store of object ptr into a global
                        # compute target rva
                        disp = ins.operands[0].mem.disp
                        gl = (ins.address + ins.size + disp) - IB
                        print("     store global RVA=%#x  (%s)  at %#x" % (gl, ins.op_str, ins.address))
                        break
    print()
