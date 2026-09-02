#include "icons.h"

#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include <fstream>
#include <unordered_map>
#include <unordered_set>

#define NANOSVG_IMPLEMENTATION
#define NANOSVGRAST_IMPLEMENTATION
#include "../../external/nanosvg/nanosvg.h"
#include "../../external/nanosvg/nanosvgrast.h"

namespace
{
    void LogLine(const char* m)
    {
        wchar_t p[MAX_PATH] = {}; GetTempPathW(MAX_PATH, p); wcscat_s(p, MAX_PATH, L"TempleWare.log");
        FILE* f = nullptr; if (_wfopen_s(&f, p, L"a") == 0 && f) { fprintf(f, "[icons] %s\n", m); fclose(f); }
    }

    ID3D11Device*        g_device = nullptr;
    ID3D11DeviceContext* g_context = nullptr;

    std::unordered_map<std::string, std::vector<uint8_t>> g_pending;   // name -> raw svg
    struct Ico { ID3D11ShaderResourceView* srv; int w, h; };
    std::unordered_map<std::string, Ico> g_cache;                      // name+scale -> texture
    bool g_ready = false;

    // Weapon short names to pull from the equipment folder.
    const char* kNames[] = {
        "deagle","elite","fiveseven","glock","hkp2000","usp_silencer","p250","cz75a","tec9","revolver",
        "mac10","mp5sd","mp7","mp9","bizon","p90","ump45","ak47","m4a1","m4a1_silencer","aug","famas",
        "galilar","sg556","nova","sawedoff","xm1014","mag7","awp","g3sg1","scar20","ssg08","m249","negev",
        "hegrenade","flashbang","smokegrenade","molotov","incgrenade","decoy","taser","knife","c4","healthshot",
    };

    ID3D11ShaderResourceView* CreateSRV(const uint8_t* px, int w, int h)
    {
        D3D11_TEXTURE2D_DESC td{};
        td.Width = (UINT)w; td.Height = (UINT)h; td.MipLevels = 1; td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_IMMUTABLE; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA sd{}; sd.pSysMem = px; sd.SysMemPitch = (UINT)(w * 4);
        ID3D11Texture2D* tex = nullptr;
        if (FAILED(g_device->CreateTexture2D(&td, &sd, &tex)) || !tex) return nullptr;
        D3D11_SHADER_RESOURCE_VIEW_DESC sv{}; sv.Format = td.Format; sv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; sv.Texture2D.MipLevels = 1;
        ID3D11ShaderResourceView* srv = nullptr;
        HRESULT hr = g_device->CreateShaderResourceView(tex, &sv, &srv);
        tex->Release();
        return SUCCEEDED(hr) ? srv : nullptr;
    }

    ID3D11ShaderResourceView* RasterSVG(const std::vector<uint8_t>& data, float scale, int* ow, int* oh)
    {
        std::string str(reinterpret_cast<const char*>(data.data()), data.size());
        NSVGimage* img = nsvgParse(str.data(), "px", 96.0f);
        if (!img) return nullptr;
        int w = (int)(img->width * scale), h = (int)(img->height * scale);
        if (w <= 0 || h <= 0) { nsvgDelete(img); return nullptr; }
        NSVGrasterizer* rast = nsvgCreateRasterizer();
        if (!rast) { nsvgDelete(img); return nullptr; }
        std::vector<unsigned char> px((size_t)w * h * 4);
        nsvgRasterize(rast, img, 0, 0, scale, px.data(), w, h, w * 4);
        nsvgDeleteRasterizer(rast); nsvgDelete(img);
        if (ow) *ow = w; if (oh) *oh = h;
        return CreateSRV(px.data(), w, h);
    }

    // Source-2 .vsvg_c -> raw <svg>...</svg>
    std::vector<uint8_t> DecompileVsvg(const uint8_t* raw, size_t size)
    {
        if (size < 16) return {};
        const uint16_t ver = *reinterpret_cast<const uint16_t*>(raw + 4);
        const uint32_t blocks = *reinterpret_cast<const uint32_t*>(raw + 12);
        if (ver != 12 || blocks == 0 || blocks > 64) return {};
        const uint32_t DATA = 'D' | ('A' << 8) | ('T' << 16) | ('A' << 24);
        for (uint32_t i = 0; i < blocks; ++i)
        {
            const size_t ep = 16 + (size_t)i * 12;
            if (ep + 12 > size) break;
            const uint32_t type = *reinterpret_cast<const uint32_t*>(raw + ep);
            const uint32_t off = *reinterpret_cast<const uint32_t*>(raw + ep + 4);
            const uint32_t bsz = *reinterpret_cast<const uint32_t*>(raw + ep + 8);
            if (type != DATA) continue;
            const size_t ds = ep + 4 + off;
            if (ds + bsz > size) break;
            const char* bp = reinterpret_cast<const char*>(raw + ds);
            for (uint32_t j = 0; j + 4 < bsz; ++j)
            {
                if (bp[j] != '<') continue;
                if (bp[j + 1] == 's' && bp[j + 2] == 'v' && bp[j + 3] == 'g')
                {
                    for (uint32_t k = bsz; k > j + 5; --k)
                        if (bp[k - 1] == '>' && bp[k - 2] == 'g' && bp[k - 3] == 'v' && bp[k - 4] == 's')
                            return std::vector<uint8_t>(raw + ds + j, raw + ds + k);
                }
            }
            break;
        }
        return {};
    }

    void CacheBytes(const std::string& archive, const std::string& name, uint32_t off, uint32_t len)
    {
        std::ifstream f(archive, std::ios::binary);
        if (!f.is_open()) return;
        f.seekg(off);
        std::vector<uint8_t> raw(len);
        f.read(reinterpret_cast<char*>(raw.data()), len);
        auto svg = DecompileVsvg(raw.data(), raw.size());
        if (!svg.empty()) g_pending[name] = std::move(svg);
    }

    bool LoadVpk(const std::string& path)
    {
        std::ifstream f(path, std::ios::binary);
        if (!f.is_open()) return false;
#pragma pack(push,1)
        struct Hdr { uint32_t sig, ver, tree, fdata, amd5, omd5, ssig; };
        struct Ent { uint32_t crc; uint16_t preload; uint16_t arch; uint32_t off; uint32_t len; uint16_t term; };
#pragma pack(pop)
        Hdr h{}; f.read(reinterpret_cast<char*>(&h), sizeof(h));
        if (h.sig != 0x55AA1234 || h.ver != 2) return false;
        const std::streamoff treeEnd = (std::streamoff)sizeof(Hdr) + (std::streamoff)h.tree;
        const std::string base = path.substr(0, path.find_last_of("/\\") + 1);

        std::unordered_set<std::string> targets;
        for (const char* n : kNames) targets.insert(n);

        while (f.tellg() < treeEnd)
        {
            std::string ext; std::getline(f, ext, '\0');
            if (ext.empty()) break;
            const bool isSvg = (ext == "vsvg_c" || ext == "vsvg");
            while (true)
            {
                std::string dir; std::getline(f, dir, '\0');
                if (dir.empty()) break;
                const bool isEquip = isSvg && dir.find("equipment") != std::string::npos;
                while (true)
                {
                    std::string fn; std::getline(f, fn, '\0');
                    if (fn.empty()) break;
                    Ent e{}; f.read(reinterpret_cast<char*>(&e), sizeof(e));
                    if (e.preload > 0) f.seekg(e.preload, std::ios::cur);
                    if (!isEquip) continue;
                    if (targets.find(fn) == targets.end()) continue;
                    char arch[512];
                    std::snprintf(arch, sizeof(arch), "%spak01_%03d.vpk", base.c_str(), e.arch);
                    CacheBytes(arch, fn, e.off, e.len);
                }
            }
        }
        return !g_pending.empty();
    }

    // <game>\csgo\bin\win64\client.dll -> <game>\csgo\pak01_dir.vpk
    std::string VpkPathFromGame()
    {
        HMODULE cl = GetModuleHandleA("client.dll");
        if (!cl) return {};
        char buf[MAX_PATH] = {};
        if (!GetModuleFileNameA(cl, buf, MAX_PATH)) return {};
        std::string p(buf);
        for (int i = 0; i < 3; ++i) { const size_t s = p.find_last_of("\\/"); if (s == std::string::npos) return {}; p = p.substr(0, s); }
        return p + "\\pak01_dir.vpk";
    }

    std::string Normalize(const char* name)
    {
        if (!name) return {};
        if (std::strstr(name, "knife") || std::strstr(name, "bayonet") || std::strstr(name, "karambit")) return "knife";
        return name;
    }
}

namespace Icons
{
    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
    {
        if (g_ready) return true;
        g_device = device; g_context = context;
        if (!g_device) return false;

        std::string vpk = VpkPathFromGame();
        bool ok = false;
        if (!vpk.empty()) ok = LoadVpk(vpk);
        if (!ok)
        {
            // fallback: common Steam path
            ok = LoadVpk("C:\\Program Files (x86)\\Steam\\steamapps\\common\\Counter-Strike Global Offensive\\game\\csgo\\pak01_dir.vpk");
        }
        char b[128]; std::snprintf(b, sizeof(b), "vpk=%s loaded=%d svgs=%zu", vpk.c_str(), (int)ok, g_pending.size());
        LogLine(b);
        g_ready = ok && !g_pending.empty();
        return g_ready;
    }

    bool Ready() { return g_ready; }

    // Rasterize SVG markup (in `mutableSvg`, which nsvgParse edits in place) to a
    // texture of target pixel height, cached under `key`.
    void* RasterSvgToTex(const std::string& key, std::string mutableSvg, int targetHeight, int* ow, int* oh)
    {
        if (!g_device) return nullptr;
        auto it = g_cache.find(key);
        if (it != g_cache.end()) { if (ow) *ow = it->second.w; if (oh) *oh = it->second.h; return it->second.srv; }
        if (mutableSvg.empty()) return nullptr;

        NSVGimage* img = nsvgParse(mutableSvg.data(), "px", 96.0f);
        if (!img) return nullptr;
        const float scale = (img->height > 0.f) ? (float)targetHeight / img->height : 1.f;
        const int w = (int)(img->width * scale), h = (int)(img->height * scale);
        if (w <= 0 || h <= 0) { nsvgDelete(img); return nullptr; }
        NSVGrasterizer* rast = nsvgCreateRasterizer();
        if (!rast) { nsvgDelete(img); return nullptr; }
        std::vector<unsigned char> px((size_t)w * h * 4);
        nsvgRasterize(rast, img, 0, 0, scale, px.data(), w, h, w * 4);
        nsvgDeleteRasterizer(rast); nsvgDelete(img);

        ID3D11ShaderResourceView* srv = CreateSRV(px.data(), w, h);
        if (!srv) return nullptr;
        g_cache[key] = { srv, w, h };
        if (ow) *ow = w; if (oh) *oh = h;
        return srv;
    }

    void* LoadSvgFile(const char* path, int targetHeight, int* ow, int* oh)
    {
        if (!path) return nullptr;
        std::ifstream f(path, std::ios::binary);
        if (!f.is_open()) return nullptr;
        std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        char key[600]; std::snprintf(key, sizeof(key), "file:%s#%d", path, targetHeight);
        return RasterSvgToTex(key, std::move(s), targetHeight, ow, oh);
    }

    void* RasterSvgData(const char* cacheKey, const char* svg, int targetHeight, int* ow, int* oh)
    {
        if (!svg) return nullptr;
        char key[128]; std::snprintf(key, sizeof(key), "data:%s#%d", cacheKey ? cacheKey : "?", targetHeight);
        return RasterSvgToTex(key, std::string(svg), targetHeight, ow, oh);
    }

    void* Get(const char* rawName, float scale, int* w, int* h)
    {
        if (!g_ready || !rawName) return nullptr;
        const std::string name = Normalize(rawName);

        char key[96]; std::snprintf(key, sizeof(key), "%s#%d", name.c_str(), (int)(scale * 100.f));
        auto it = g_cache.find(key);
        if (it != g_cache.end()) { if (w) *w = it->second.w; if (h) *h = it->second.h; return it->second.srv; }

        auto pit = g_pending.find(name);
        if (pit == g_pending.end()) return nullptr;

        int iw = 0, ih = 0;
        ID3D11ShaderResourceView* srv = RasterSVG(pit->second, scale, &iw, &ih);
        if (!srv) return nullptr;

        g_cache[key] = { srv, iw, ih };
        if (w) *w = iw; if (h) *h = ih;
        return srv;
    }
}
