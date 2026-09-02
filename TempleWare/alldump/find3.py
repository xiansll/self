import pefile, capstone, struct
DLL=r"C:/Program Files (x86)/Steam/steamapps/common/Counter-Strike Global Offensive/game/csgo/bin/win64/client.dll"
pe=pefile.PE(DLL,fast_load=True); IB=pe.OPTIONAL_HEADER.ImageBase; img=pe.get_memory_mapped_image()
def sec(n):
  for s in pe.sections:
    if s.Name.rstrip(b'\x00')==n: return s.VirtualAddress,s.VirtualAddress+s.Misc_VirtualSize
tl,th=sec(b'.text')
VT=0x1bcfb70
t=img[tl:th]
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64); md.detail=True

# find store sites: mov [rip+X], reg  that occur right after a lea VT block.
# We detect the store by scanning each lea-VT site forward for first 'mov [rip+X], r64'.
sites=[]
for op in (b'\x48\x8d\x05',):
  s=0
  while True:
    j=t.find(op,s)
    if j<0: break
    s=j+1; rva=tl+j
    rel=struct.unpack('<i',img[rva+3:rva+7])[0]
    if rva+7+rel==VT: sites.append(rva)

globals_found=set()
for st in sites:
  for ins in md.disasm(img[st:st+0x30], IB+st):
    if ins.mnemonic=="mov" and ins.op_str.startswith("qword ptr [rip") and ins.operands[1].type==capstone.x86.X86_OP_REG:
      disp=ins.operands[0].mem.disp
      g=(ins.address+ins.size+disp)-IB
      globals_found.add(g); break
print("candidate item-system globals:",[hex(g) for g in sorted(globals_found)])

# count read xrefs: mov rax/rcx/..,[rip+g]  -> 48 8b 05/0d/15.. with rel to g
def count_reads(g):
  cnt=0; s=0
  while True:
    j=t.find(b'\x48\x8b',s)
    if j<0: break
    s=j+1; rva=tl+j
    modrm=img[rva+2]
    # [rip+disp32] has mod=00, rm=101 -> modrm low3=101, high2=xxx(reg), mod=00
    if (modrm & 0xC7)==0x05:
      rel=struct.unpack('<i',img[rva+3:rva+7])[0]
      if rva+7+rel==g: cnt+=1
  return cnt
for g in sorted(globals_found):
  print("  global RVA=%#x  reads=%d"%(g,count_reads(g)))
