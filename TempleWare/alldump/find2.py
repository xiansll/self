import pefile, capstone, struct
DLL=r"C:/Program Files (x86)/Steam/steamapps/common/Counter-Strike Global Offensive/game/csgo/bin/win64/client.dll"
pe=pefile.PE(DLL,fast_load=True); IB=pe.OPTIONAL_HEADER.ImageBase; img=pe.get_memory_mapped_image()
def sec(n):
  for s in pe.sections:
    if s.Name.rstrip(b'\x00')==n: return s.VirtualAddress,s.VirtualAddress+s.Misc_VirtualSize
tl,th=sec(b'.text'); rl,rh=sec(b'.rdata')
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64); md.detail=True

def vtable(name):
  i=img.find(name); td=i-0x10
  tdb=struct.pack("<I",td); cols=[]; p=rl
  while True:
    i=img.find(tdb,p,rh)
    if i<0: break
    cols.append(i-0xC); p=i+1
  for col in cols:
    vb=struct.pack("<Q",IB+col); p=rl
    while True:
      i=img.find(vb,p,rh)
      if i<0: break
      return i+8  # first vtable
VT=vtable(b".?AVCCStrike15ItemSystem@@")
print("vtable RVA=%#x"%VT)

# true ctor: cc-padded function start; within first 0x20 bytes: lea rax,[VT]; mov [rcx],rax
t=img[tl:th]
def func_starts():
  # positions right after cc padding run
  i=0; starts=[]
  while i<len(t):
    if t[i]==0xCC:
      j=i
      while j<len(t) and t[j]==0xCC: j+=1
      if j<len(t): starts.append(tl+j)
      i=j
    else: i+=1
  return starts
starts=func_starts()
print("func starts",len(starts))
ctors=[]
for fs in starts:
  head=img[fs:fs+0x28]
  # find lea rax,[rip+VT]
  for k in range(0,len(head)-7):
    if head[k]==0x48 and head[k+1]==0x8d and head[k+2]==0x05:
      rel=struct.unpack("<i",head[k+3:k+7])[0]
      if (fs+k+7+rel)==VT:
        # next: mov [rcx],rax = 48 89 01
        if head[k+7:k+10]==b"\x48\x89\x01":
          ctors.append(fs)
print("ctors:",[hex(c) for c in ctors])

def callers(func):
  s=0;res=[]
  while True:
    j=t.find(b"\xe8",s)
    if j<0: break
    s=j+1; rva=tl+j
    rel=struct.unpack("<i",img[rva+1:rva+5])[0]
    if rva+5+rel==func: res.append(rva)
  return res
for ct in ctors:
  cs=callers(ct)
  print("ctor %#x callers=%d"%(ct,len(cs)))
  for c in cs:
    print("  caller site %#x:"%c)
    for ins in md.disasm(img[c:c+0x50], IB+c):
      print("    %#x %s %s"%(ins.address,ins.mnemonic,ins.op_str))
      if ins.mnemonic=="ret": break
