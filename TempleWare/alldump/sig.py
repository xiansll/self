import pefile, struct
DLL=r"C:/Program Files (x86)/Steam/steamapps/common/Counter-Strike Global Offensive/game/csgo/bin/win64/client.dll"
pe=pefile.PE(DLL,fast_load=True); IB=pe.OPTIONAL_HEADER.ImageBase; img=pe.get_memory_mapped_image()
def sec(n):
  for s in pe.sections:
    if s.Name.rstrip(b'\x00')==n: return s.VirtualAddress,s.VirtualAddress+s.Misc_VirtualSize
tl,th=sec(b'.text')
G=0x2453fc0
t=img[tl:th]
# find read sites: 48 8b 05 rel -> G
reads=[]
s=0
while True:
  j=t.find(b'\x48\x8b\x05',s)
  if j<0: break
  s=j+1; rva=tl+j
  rel=struct.unpack('<i',img[rva+3:rva+7])[0]
  if rva+7+rel==G: reads.append(rva)
print("read sites (48 8b 05):",len(reads))

def to_sig(rva, ctx_after):
  # 48 8B 05 ?? ?? ?? ?? then ctx_after literal bytes
  b=img[rva:rva+7+ctx_after]
  parts=["48","8B","05","?","?","?","?"]
  for k in range(7,7+ctx_after):
    parts.append("%02X"%b[k])
  return " ".join(parts)

def count(sig):
  # count occurrences in whole .text of the concrete (non-?) pattern
  toks=sig.split()
  # build regex-free scan
  n=len(toks); cnt=0; i=0
  # simple: iterate
  arr=t
  L=len(arr)
  for p in range(L-n):
    ok=True
    for q,tok in enumerate(toks):
      if tok=="?": continue
      if arr[p+q]!=int(tok,16): ok=False;break
    if ok: cnt+=1
  return cnt

for rva in reads:
  for ctx in (6,9,12):
    sig=to_sig(rva,ctx)
    c=count(sig)
    if c==1:
      print("UNIQUE @%#x ctx=%d  sig= %s"%(rva,ctx,sig))
      break
  else:
    continue
  break
