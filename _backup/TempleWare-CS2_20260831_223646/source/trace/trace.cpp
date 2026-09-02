#include "trace.h"
#include <windows.h>
#include <cstring>
#include <cstdio>

namespace {
void LogLine(const char* m){wchar_t p[MAX_PATH]={};GetTempPathW(MAX_PATH,p);wcscat_s(p,MAX_PATH,L"TempleWare.log");FILE* f=nullptr;if(_wfopen_s(&f,p,L"a")==0&&f){fprintf(f,"[trace] %s\n",m);fclose(f);}}
int HV(char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;}

uintptr_t Resolve(const char* mod,const char* pat){
    HMODULE h=GetModuleHandleA(mod); if(!h) return 0;
    auto dos=(PIMAGE_DOS_HEADER)h; if(dos->e_magic!=IMAGE_DOS_SIGNATURE) return 0;
    auto nt=(PIMAGE_NT_HEADERS)((uint8_t*)h+dos->e_lfanew); if(nt->Signature!=IMAGE_NT_SIGNATURE) return 0;
    uintptr_t base=(uintptr_t)h; size_t size=nt->OptionalHeader.SizeOfImage;
    struct PB{uint8_t b;bool w;}; PB bytes[256]; int n=0; int op=0; size_t opOff=0; long long post=0; bool deref=false;
    for(const char* p=pat;*p&&n<256;){
        char c=*p;
        if(c==' '||c=='\t'){++p;continue;}
        if(c=='>'){op=1;opOff=n;++p;continue;}
        if(c=='*'){op=2;opOff=n;++p;continue;}
        if(c=='^'){op=3;opOff=n;++p;continue;}
        if(c=='~'){deref=true;++p;continue;}
        if(c=='+'||c=='-'){bool ng=(c=='-');++p;long long v=0;while(HV(*p)>=0){v=(v<<4)|HV(*p);++p;}post=ng?-v:v;continue;}
        if(c=='?'){bytes[n++]={0,true};++p;if(*p=='?')++p;continue;}
        int hi=HV(c);++p;int lo=HV(*p);
        if(lo>=0){bytes[n++]={(uint8_t)((hi<<4)|lo),false};++p;} else bytes[n++]={(uint8_t)hi,false};
    }
    if(!n) return 0;
    int first=-1; for(int i=0;i<n;++i) if(!bytes[i].w){first=i;break;} if(first<0) return 0;
    const uint8_t* b=(const uint8_t*)base; uintptr_t match=0;
    for(size_t off=0;off+n<=size;++off){
        if(b[off+first]!=bytes[first].b) continue;
        bool ok=true; for(int j=0;j<n;++j){ if(bytes[j].w) continue; if(b[off+j]!=bytes[j].b){ok=false;break;} }
        if(ok){match=base+off;break;}
    }
    if(!match) return 0;
    uintptr_t r=match;
    if(op==1){uintptr_t o=match+opOff+1;int32_t rel=*(int32_t*)o;r=o+4+rel;}
    else if(op==2){uintptr_t o=match+opOff;int32_t rel=*(int32_t*)o;r=o+4+rel;}
    else if(op==3){r=*(uintptr_t*)(match+opOff);}
    r+=post;
    if(deref){ if(r<base||r+sizeof(uintptr_t)>base+size) return 0; r=*(uintptr_t*)r; }
    return r;
}

#pragma pack(push,1)
struct Ray { float mins[3]; float maxs[3]; uint8_t pad0[0x10]; uint8_t type; uint8_t pad1[7]; }; // 0x30
struct Filter { uintptr_t vtable; uintptr_t mask; uint8_t v1[16]; uint8_t skip[16]; uint8_t coll[4]; int16_t v2; uint8_t layer; uint8_t flags; uint8_t v5; uint8_t v6; uint8_t pad0[6]; char v7; }; // 0x48
struct Result { void* surface; uintptr_t hit_entity; void* hitbox; uint8_t pad0[0x38]; uint32_t contents; uint8_t pad1[0x24]; float start_pos[3]; float end_pos[3]; float normal[3]; float position[3]; uint8_t pad2[4]; float fraction; uint8_t pad3[6]; bool all_solid; uint8_t pad4[0x4d]; }; // 0x104
#pragma pack(pop)

using fnTraceRay = bool(__fastcall*)(void*, Ray*, const float*, const float*, Filter*, Result*);
using fnFilterInit = void(__fastcall*)(Filter*, uintptr_t, uintptr_t, uint8_t, int);

uintptr_t g_mgr = 0;
fnTraceRay g_traceRay = nullptr;
fnFilterInit g_filterInit = nullptr;
bool g_ready = false;

__declspec(noinline) bool DoTrace(const float* start, const float* end, uintptr_t target, uintptr_t skip)
{
    __try
    {
        Filter f{};
        g_filterInit(&f, skip, 0x1c3003, 4, 7);
        Ray r{}; Result res{};
        g_traceRay((void*)g_mgr, &r, start, end, &f, &res);
        if (res.hit_entity == target || res.fraction > 0.97f) return true;
        return false;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return true; }
}

__declspec(noinline) bool DoTraceLine(const float* start, const float* end, uintptr_t skip,
                                      float* oe, float* on, float* of)
{
    __try
    {
        Filter f{};
        g_filterInit(&f, skip, 0x1c3003, 4, 7);
        Ray r{}; Result res{};
        g_traceRay((void*)g_mgr, &r, start, end, &f, &res);
        if (oe) { oe[0] = res.end_pos[0]; oe[1] = res.end_pos[1]; oe[2] = res.end_pos[2]; }
        if (on) { on[0] = res.normal[0];  on[1] = res.normal[1];  on[2] = res.normal[2]; }
        if (of) *of = res.fraction;
        return res.fraction < 0.999f;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { if (of) *of = 1.f; return false; }
}
} // namespace

namespace Trace
{
    bool Initialize()
    {
        if (g_ready) return true;
        g_mgr        = Resolve("client.dll", "488B0D*????????488D3452~");
        g_traceRay   = (fnTraceRay)Resolve("client.dll", "20488D95E0000000>E8????????488B3D????????");
        g_filterInit = (fnFilterInit)Resolve("client.dll", "7D78F3440F58457C>E8????????F30F1005????????");
        char b[128];
        std::snprintf(b, sizeof(b), "mgr=%p ray=%p filt=%p", (void*)g_mgr, (void*)g_traceRay, (void*)g_filterInit);
        LogLine(b);
        g_ready = g_mgr && g_traceRay && g_filterInit;
        return g_ready;
    }

    bool Ready() { return g_ready; }

    bool IsVisible(const float start[3], const float end[3], uintptr_t target, uintptr_t skip)
    {
        if (!g_ready) return true;
        return DoTrace(start, end, target, skip);
    }

    bool Line(const float start[3], const float end[3], uintptr_t skip,
              float outEnd[3], float outNormal[3], float* outFrac)
    {
        if (!g_ready)
        {
            if (outEnd) { outEnd[0] = end[0]; outEnd[1] = end[1]; outEnd[2] = end[2]; }
            if (outNormal) { outNormal[0] = 0; outNormal[1] = 0; outNormal[2] = 1; }
            if (outFrac) *outFrac = 1.f;
            return false;
        }
        return DoTraceLine(start, end, skip, outEnd, outNormal, outFrac);
    }
}
