#include "chams.h"
#include "../esp/esp.h"

#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cctype>

#include "../../external/kiero/minhook/include/MinHook.h"
#include "../../external/imgui/imgui.h"

namespace
{
    // ---------------------------------------------------------------------
    // Logging (shared TempleWare.log)
    // ---------------------------------------------------------------------
    void LogLine(const char* msg)
    {
        wchar_t path[MAX_PATH] = {};
        GetTempPathW(MAX_PATH, path);
        wcscat_s(path, MAX_PATH, L"TempleWare.log");
        FILE* f = nullptr;
        if (_wfopen_s(&f, path, L"a") == 0 && f)
        {
            fprintf(f, "[chams] %s\n", msg);
            fclose(f);
        }
    }

    // ---------------------------------------------------------------------
    // Pattern scanner with velocity's resolve DSL:
    //   '>' rel_call (E8/E9 rel32)   '*' rip-relative (lea disp32)
    //   '^' absolute ptr             '~' dereference final result
    //   '+N' / '-N' add signed offset before the optional deref
    // ---------------------------------------------------------------------
    int HexVal(char c)
    {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }

    bool ModuleRange(const char* name, uintptr_t& base, size_t& size)
    {
        HMODULE mod = GetModuleHandleA(name);
        if (!mod) return false;
        const auto dos = reinterpret_cast<PIMAGE_DOS_HEADER>(mod);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
        const auto nt = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<uint8_t*>(mod) + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
        base = reinterpret_cast<uintptr_t>(mod);
        size = nt->OptionalHeader.SizeOfImage;
        return true;
    }

    // pattern is the part after "module:".
    uintptr_t ResolvePattern(const char* moduleName, const char* pat)
    {
        uintptr_t base = 0; size_t size = 0;
        if (!ModuleRange(moduleName, base, size)) return 0;

        struct PB { uint8_t b; bool wild; };
        PB bytes[256];
        int n = 0;
        int op = 0;              // 0 direct, 1 rel_call, 2 rip, 3 abs
        size_t opOff = 0;
        long long post = 0;
        bool deref = false;

        for (const char* p = pat; *p && n < 256; )
        {
            const char c = *p;
            if (c == ' ' || c == '\t') { ++p; continue; }
            if (c == '>') { op = 1; opOff = n; ++p; continue; }
            if (c == '*') { op = 2; opOff = n; ++p; continue; }
            if (c == '^') { op = 3; opOff = n; ++p; continue; }
            if (c == '~') { deref = true; ++p; continue; }
            if (c == '+' || c == '-')
            {
                const bool neg = (c == '-'); ++p;
                long long v = 0;
                while (HexVal(*p) >= 0) { v = (v << 4) | HexVal(*p); ++p; }
                post = neg ? -v : v;
                continue;
            }
            if (c == '?')
            {
                bytes[n++] = { 0, true }; ++p;
                if (*p == '?') ++p;
                continue;
            }
            const int hi = HexVal(c); ++p;
            const int lo = HexVal(*p);
            if (lo >= 0) { bytes[n++] = { static_cast<uint8_t>((hi << 4) | lo), false }; ++p; }
            else         { bytes[n++] = { static_cast<uint8_t>(hi), false }; }
        }
        if (n == 0) return 0;

        // first concrete byte
        int first = -1;
        for (int i = 0; i < n; ++i) if (!bytes[i].wild) { first = i; break; }
        if (first < 0) return 0;

        const uint8_t* b = reinterpret_cast<const uint8_t*>(base);
        uintptr_t match = 0;
        for (size_t off = 0; off + n <= size; ++off)
        {
            if (b[off + first] != bytes[first].b) continue;
            bool ok = true;
            for (int j = 0; j < n; ++j)
            {
                if (bytes[j].wild) continue;
                if (b[off + j] != bytes[j].b) { ok = false; break; }
            }
            if (ok) { match = base + off; break; }
        }
        if (!match) return 0;

        uintptr_t result = match;
        switch (op)
        {
        case 1: { // rel_call: operand at match+opOff+1
            const uintptr_t operand = match + opOff + 1;
            const int32_t rel = *reinterpret_cast<const int32_t*>(operand);
            result = operand + 4 + rel;
            break;
        }
        case 2: { // rip_relative: operand at match+opOff
            const uintptr_t operand = match + opOff;
            const int32_t rel = *reinterpret_cast<const int32_t*>(operand);
            result = operand + 4 + rel;
            break;
        }
        case 3: { // absolute ptr
            result = *reinterpret_cast<const uintptr_t*>(match + opOff);
            break;
        }
        default: break;
        }

        result += post;
        if (deref)
        {
            if (result < base || result + sizeof(uintptr_t) > base + size) return 0;
            result = *reinterpret_cast<const uintptr_t*>(result);
        }
        return result;
    }

    uintptr_t Export(const char* moduleName, const char* symbol)
    {
        HMODULE mod = GetModuleHandleA(moduleName);
        if (!mod) return 0;
        return reinterpret_cast<uintptr_t>(GetProcAddress(mod, symbol));
    }

    // ---------------------------------------------------------------------
    // Material creation (runtime KV3 compile -> material_create)
    // ---------------------------------------------------------------------
    struct StrongHandle { const void* binding; };
    struct Kv3Id { const char* format; uint64_t guidLow; uint64_t guidHigh; };

    // Material variants. Each type has a visible (z-tested) and an XQZ (ignorez,
    // through-wall) template. Textures are limited to default/debug resources so
    // creation stays robust across builds; a failed one falls back to flat.
    enum MatType {
        MAT_FLAT = 0, MAT_ILLUM = 1, MAT_GLOW = 2, MAT_MATTE = 3, MAT_OUTLINE = 4, MAT_HOLO = 5,
        MAT_METALLIC = 6, MAT_LIQUID = 7, MAT_BLOOM = 8, MAT_DISTORT = 9, MAT_PEARL = 10,
        MAT_COUNT = 11
    };

    const char* kVmat[MAT_COUNT][2] =
    {
        // ---- FLAT ----
        {
            R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_unlitgeneric.vfx"
    F_PAINT_VERTEX_COLORS = 1
    F_TRANSLUCENT = 1
    F_BLEND_MODE = 1
    g_vColorTint = [1.0, 1.0, 1.0, 1.0]
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
})#",
            R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_unlitgeneric.vfx"
    F_PAINT_VERTEX_COLORS = 1
    F_TRANSLUCENT = 1
    F_BLEND_MODE = 1
    F_DISABLE_Z_BUFFERING = 1
    F_DISABLE_Z_WRITE = 1
    g_vColorTint = [1.0, 1.0, 1.0, 1.0]
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
})#"
        },
        // ---- ILLUMINATE (self-illum) ----
        {
            R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_complex.vfx"
    F_SELF_ILLUM = 1
    F_PAINT_VERTEX_COLORS = 1
    F_TRANSLUCENT = 1
    g_vColorTint = [1.0, 1.0, 1.0, 1.0]
    g_flSelfIllumScale = [3.0, 3.0, 3.0, 3.0]
    g_flSelfIllumBrightness = [3.0, 3.0, 3.0, 3.0]
    g_vSelfIllumTint = [10.0, 10.0, 10.0, 10.0]
    g_tColor = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tNormal = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tSelfIllumMask = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tAmbientOcclusion = resource:"materials/debug/particleerror.vtex"
})#",
            R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_complex.vfx"
    F_SELF_ILLUM = 1
    F_PAINT_VERTEX_COLORS = 1
    F_TRANSLUCENT = 1
    F_DISABLE_Z_BUFFERING = 1
    g_vColorTint = [1.0, 1.0, 1.0, 1.0]
    g_flSelfIllumScale = [3.0, 3.0, 3.0, 3.0]
    g_flSelfIllumBrightness = [3.0, 3.0, 3.0, 3.0]
    g_vSelfIllumTint = [10.0, 10.0, 10.0, 10.0]
    g_tColor = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tNormal = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tSelfIllumMask = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tAmbientOcclusion = resource:"materials/debug/particleerror.vtex"
})#"
        },
        // ---- GLOW (fresnel effects) ----
        {
            R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_effects.vfx"
    F_TRANSLUCENT = 1
    g_flFresnelExponent = 7.0
    g_flFresnelFalloff = 10.0
    g_flFresnelMax = 0.1
    g_flFresnelMin = 1.0
    g_vColorTint = [1.0, 1.0, 1.0, 0.0]
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tMask1 = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tMask2 = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tMask3 = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tSceneDepth = resource:"materials/default/default_mask_tga_fde710a5.vtex"
})#",
            R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_effects.vfx"
    F_TRANSLUCENT = 1
    F_DISABLE_Z_BUFFERING = 1
    F_DISABLE_Z_WRITE = 1
    g_flFresnelExponent = 7.0
    g_flFresnelFalloff = 10.0
    g_flFresnelMax = 0.1
    g_flFresnelMin = 1.0
    g_vColorTint = [1.0, 1.0, 1.0, 0.0]
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tMask1 = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tMask2 = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tMask3 = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tSceneDepth = resource:"materials/default/default_mask_tga_fde710a5.vtex"
})#"
        },
        // ---- MATTE (solid, opaque) ----
        {
            R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_unlitgeneric.vfx"
    F_PAINT_VERTEX_COLORS = 1
    g_vColorTint = [1.0, 1.0, 1.0, 1.0]
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
})#",
            R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_unlitgeneric.vfx"
    F_PAINT_VERTEX_COLORS = 1
    F_DISABLE_Z_BUFFERING = 1
    F_DISABLE_Z_WRITE = 1
    g_vColorTint = [1.0, 1.0, 1.0, 1.0]
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
})#"
        },
        // ---- OUTLINE (fresnel edge) ----
        {
            R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_effects.vfx"
    F_ADDITIVE_BLEND = 1
    F_BLEND_MODE = 1
    F_TRANSLUCENT = 1
    g_vColorTint = [1.0, 1.0, 1.0, 0.0]
    g_flOpacityScale = 0.45
    g_flFresnelExponent = 0.75
    g_flFresnelFalloff = 1.0
    g_flFresnelMax = 0.0
    g_flFresnelMin = 1.0
    g_flColorBoost = 2.25
    g_tColor = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tMask1 = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tMask2 = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tMask3 = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tSceneDepth = resource:"materials/default/default_mask_tga_fde710a5.vtex"
})#",
            R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_effects.vfx"
    F_ADDITIVE_BLEND = 1
    F_BLEND_MODE = 1
    F_TRANSLUCENT = 1
    F_DISABLE_Z_BUFFERING = 1
    F_DISABLE_Z_WRITE = 1
    g_vColorTint = [1.0, 1.0, 1.0, 0.0]
    g_flOpacityScale = 0.45
    g_flFresnelExponent = 0.75
    g_flFresnelFalloff = 1.0
    g_flFresnelMax = 0.0
    g_flFresnelMin = 1.0
    g_flColorBoost = 2.25
    g_tColor = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tMask1 = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tMask2 = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tMask3 = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tSceneDepth = resource:"materials/default/default_mask_tga_fde710a5.vtex"
})#"
        },
        // ---- HOLOGRAM (self-illum, scrolling) ----
        {
            R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_complex.vfx"
    F_SELF_ILLUM = 1
    F_RENDER_BACKFACES = 1
    F_TRANSLUCENT = 1
    F_PAINT_VERTEX_COLORS = 1
    g_vColorTint = [0.0, 0.0, 0.0]
    g_flOpacityScale = 0.6
    g_flSelfIllumBrightness = 4.5
    g_flSelfIllumScale = 2.0
    g_vSelfIllumTint = [0.45, 0.85, 1.0]
    g_vSelfIllumScrollSpeed = [0.0, 0.35]
    g_tColor = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
    g_tSelfIllumMask = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tAmbientOcclusion = resource:"materials/debug/particleerror.vtex"
})#",
            R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_complex.vfx"
    F_SELF_ILLUM = 1
    F_RENDER_BACKFACES = 1
    F_TRANSLUCENT = 1
    F_PAINT_VERTEX_COLORS = 1
    F_DISABLE_Z_BUFFERING = 1
    g_vColorTint = [0.0, 0.0, 0.0]
    g_flOpacityScale = 0.6
    g_flSelfIllumBrightness = 4.5
    g_flSelfIllumScale = 2.0
    g_vSelfIllumTint = [0.45, 0.85, 1.0]
    g_vSelfIllumScrollSpeed = [0.0, 0.35]
    g_tColor = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
    g_tSelfIllumMask = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tAmbientOcclusion = resource:"materials/debug/particleerror.vtex"
})#"
        },
        // ---- METALLIC (iridescent character) ----
        {
            R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_character.vfx"
    F_IRIDESCENCE = 1
    F_CLOTH_SHADING = 1
    F_RENDER_BACKFACES = 1
    F_PAINT_VERTEX_COLORS = 1
    g_vColorTint = [1.0, 1.0, 1.0, 1.0]
    g_flIridescentStrength = 2.0
    g_flSheenScale = 10.0
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
    g_tMetalness = resource:"materials/default/default_mask_tga_fde710a5.vtex"
})#",
            R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_character.vfx"
    F_IRIDESCENCE = 1
    F_CLOTH_SHADING = 1
    F_RENDER_BACKFACES = 1
    F_PAINT_VERTEX_COLORS = 1
    F_DISABLE_Z_PREPASS = 1
    g_vColorTint = [1.0, 1.0, 1.0, 1.0]
    g_flIridescentStrength = 2.0
    g_flSheenScale = 10.0
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
    g_tMetalness = resource:"materials/default/default_mask_tga_fde710a5.vtex"
})#"
        },
        // ---- LIQUID (self-illum scrolling) ----
        {
            R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_complex.vfx"
    F_SELF_ILLUM = 1
    F_RENDER_BACKFACES = 1
    F_TRANSLUCENT = 1
    F_PAINT_VERTEX_COLORS = 1
    g_vColorTint = [0.0, 0.0, 0.0]
    g_flOpacityScale = 0.8
    g_flSelfIllumBrightness = 3.0
    g_flSelfIllumScale = 1.5
    g_vSelfIllumScrollSpeed = [0.05, 0.03]
    g_vTexCoordScrollSpeed = [0.01, 0.005]
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
    g_tSelfIllumMask = resource:"materials/default/default_mask_tga_fde710a5.vtex"
})#",
            R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_complex.vfx"
    F_SELF_ILLUM = 1
    F_RENDER_BACKFACES = 1
    F_TRANSLUCENT = 1
    F_PAINT_VERTEX_COLORS = 1
    F_DISABLE_Z_BUFFERING = 1
    g_vColorTint = [1.0, 1.0, 1.0]
    g_flOpacityScale = 0.8
    g_flSelfIllumBrightness = 3.0
    g_flSelfIllumScale = 1.5
    g_vSelfIllumScrollSpeed = [0.05, 0.03]
    g_vTexCoordScrollSpeed = [0.01, 0.005]
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
    g_tSelfIllumMask = resource:"materials/default/default_mask_tga_fde710a5.vtex"
})#"
        },
        // ---- BLOOM (solid color, over-bright) ----
        {
            R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "solidcolor.vfx"
    g_vColorTint = [8.0, 8.0, 8.0]
})#",
            R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "solidcolor.vfx"
    F_IGNOREZ = 1
    F_DISABLE_Z_WRITE = 1
    g_vColorTint = [5.0, 5.0, 5.0]
})#"
        },
        // ---- DISTORTION (fresnel effects) ----
        {
            R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_effects.vfx"
    F_ADDITIVE_BLEND = 1
    F_BLEND_MODE = 1
    F_TRANSLUCENT = 1
    F_RENDER_BACKFACES = 1
    g_vColorTint = [1.0, 1.0, 1.0, 1.0]
    g_flOpacityScale = 0.85
    g_flFresnelExponent = 1.25
    g_flFresnelFalloff = 2.25
    g_flColorBoost = 14.0
    g_vTexCoordScrollSpeed = [0.24, 0.17]
    g_tColor = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tMask1 = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tMask2 = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tMask3 = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tSceneDepth = resource:"materials/default/default_mask_tga_fde710a5.vtex"
})#",
            R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_effects.vfx"
    F_ADDITIVE_BLEND = 1
    F_BLEND_MODE = 1
    F_TRANSLUCENT = 1
    F_RENDER_BACKFACES = 1
    F_DISABLE_Z_BUFFERING = 1
    F_DISABLE_Z_WRITE = 1
    g_vColorTint = [1.0, 1.0, 1.0, 1.0]
    g_flOpacityScale = 0.85
    g_flFresnelExponent = 1.25
    g_flFresnelFalloff = 2.25
    g_flColorBoost = 14.0
    g_vTexCoordScrollSpeed = [0.24, 0.17]
    g_tColor = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tMask1 = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tMask2 = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tMask3 = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tSceneDepth = resource:"materials/default/default_mask_tga_fde710a5.vtex"
})#"
        },
        // ---- PEARL (iridescent, matte sheen) ----
        {
            R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_character.vfx"
    F_IRIDESCENCE = 1
    F_CLOTH_SHADING = 1
    F_RENDER_BACKFACES = 1
    F_PAINT_VERTEX_COLORS = 1
    g_vColorTint = [1.0, 1.0, 1.0]
    g_flIridescentStrength = 3.5
    g_flSheenScale = 6.0
    g_fContrast = 0.35
    g_fSaturation = 1.6
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
    g_tMetalness = resource:"materials/default/default_mask_tga_fde710a5.vtex"
})#",
            R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_character.vfx"
    F_IRIDESCENCE = 1
    F_CLOTH_SHADING = 1
    F_RENDER_BACKFACES = 1
    F_PAINT_VERTEX_COLORS = 1
    F_DISABLE_Z_PREPASS = 1
    g_vColorTint = [1.0, 1.0, 1.0]
    g_flIridescentStrength = 3.5
    g_flSheenScale = 6.0
    g_fContrast = 0.35
    g_fSaturation = 1.6
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
    g_tMetalness = resource:"materials/default/default_mask_tga_fde710a5.vtex"
})#"
        },
    };

    using fnKv3Alloc     = void* (__fastcall*)(void*, int, int);
    using fnKv3Load      = bool  (__fastcall*)(void*, void*, void*, const void*, const char*, int);
    using fnMaterialCreate = void* (__fastcall*)(void*, void*, const char*, void*, int, int);
    using fnUtlBufCtor   = void  (__fastcall*)(void*, int, int, int);
    using fnUtlPutString = void  (__fastcall*)(void*, const char*);

    fnKv3Alloc       g_kv3Alloc = nullptr;
    fnKv3Load        g_kv3Load = nullptr;
    fnMaterialCreate g_materialCreate = nullptr;
    fnUtlBufCtor     g_utlCtor = nullptr;
    fnUtlPutString   g_utlPutString = nullptr;

    uintptr_t CreateMaterial(const char* vmat, const char* name)
    {
        if (!g_kv3Alloc || !g_kv3Load || !g_materialCreate || !g_utlCtor || !g_utlPutString)
            return 0;

        const size_t vmatLen = std::strlen(vmat);
        if (vmatLen > 4096 - 96) return 0;

        char kv3Buffer[5000] = {};
        char utlBuffer[4096] = {};

        void* kv3 = g_kv3Alloc(kv3Buffer, 1, 6);
        if (!kv3) return 0;

        g_utlCtor(utlBuffer, 0, static_cast<int>(vmatLen + 10), 1);
        g_utlPutString(utlBuffer, vmat);

        Kv3Id kvId{ "generic", 0x41B818518343427Eull, 0xB5F447C23C0CDF8Cull };
        if (!g_kv3Load(kv3, nullptr, utlBuffer, &kvId, "", 0))
            return 0;

        StrongHandle handle{};
        g_materialCreate(nullptr, &handle, name, kv3, 0, 1);
        if (!handle.binding) return 0;

        return *reinterpret_cast<const uintptr_t*>(handle.binding);
    }

    // ---------------------------------------------------------------------
    // Hook state — scenesystem "DrawArray" (per-mesh material override).
    // Build-matched layout from the project's own C_Material.h:
    //   CMeshData +0x18 pSceneAnimatableObject
    //             +0x20 pMaterial   (CMaterial2*)
    //             +0x50 color       (Color_tr r,g,b,a)
    //   CSceneAnimatableObject +0xC0 owner CBaseHandle
    // ---------------------------------------------------------------------
    using fnDrawArray = void(__fastcall*)(void*, void*, uintptr_t, int, void*, void*, void*, void*);
    fnDrawArray oDrawArray = nullptr;
    void* g_drawArrayAddr = nullptr;

    uintptr_t g_mat[MAT_COUNT][2] = {};   // [type][0=visible, 1=xqz/ignorez]
    bool g_installed = false;

    // Resolve a material for a type/layer, falling back to flat then to any.
    uintptr_t PickMat(int type, int layer)
    {
        if (type < 0 || type >= MAT_COUNT) type = MAT_FLAT;
        if (g_mat[type][layer]) return g_mat[type][layer];
        if (g_mat[MAT_FLAT][layer]) return g_mat[MAT_FLAT][layer];
        return g_mat[MAT_FLAT][0];
    }

    // Target sets per scope (updated each frame; single render thread -> no lock).
    enum Scope { SC_NONE = 0, SC_ENEMY, SC_TEAM, SC_LOCAL, SC_ITEM, SC_RAGDOLL };
    uintptr_t g_enemy[64] = {}; int g_enemyN = 0;
    uintptr_t g_team[64]  = {}; int g_teamN = 0;
    uintptr_t g_local = 0;
    uintptr_t g_items[128] = {}; int g_itemsN = 0;
    uintptr_t g_ragdolls[64] = {}; int g_ragdollN = 0;

    int ScopeOf(uintptr_t e)
    {
        if (e == g_local && g_local) return SC_LOCAL;
        for (int i = 0; i < g_enemyN;   ++i) if (g_enemy[i]   == e) return SC_ENEMY;
        for (int i = 0; i < g_teamN;    ++i) if (g_team[i]    == e) return SC_TEAM;
        for (int i = 0; i < g_itemsN;   ++i) if (g_items[i]   == e) return SC_ITEM;
        for (int i = 0; i < g_ragdollN; ++i) if (g_ragdolls[i]== e) return SC_RAGDOLL;
        return SC_NONE;
    }

    constexpr uintptr_t kMesh_sceneObject  = 0x18;
    constexpr uintptr_t kMesh_material     = 0x20;
    constexpr uintptr_t kMesh_color        = 0x50;
    constexpr uintptr_t kScene_ownerHandle = 0xC0;

    inline uint8_t To8(float f) { if (f < 0.f) f = 0.f; if (f > 1.f) f = 1.f; return static_cast<uint8_t>(f * 255.f + 0.5f); }

    void SetMeshMat(uintptr_t mesh, uintptr_t material, const float col[4])
    {
        *reinterpret_cast<uintptr_t*>(mesh + kMesh_material) = material;
        uint8_t* c = reinterpret_cast<uint8_t*>(mesh + kMesh_color);
        c[0] = To8(col[0]); c[1] = To8(col[1]); c[2] = To8(col[2]); c[3] = To8(col[3]);
    }

    void __fastcall hkDrawArray(void* a1, void* a2, uintptr_t pMeshScene, int nMeshCount,
                                void* pSceneView, void* pSceneLayer, void* pUnk, void* pUnk2)
    {
        const auto& cfg = Esp::g_config;
        int scope = SC_NONE;

        if (pMeshScene && nMeshCount > 0)
        {
            const uintptr_t sceneObj = *reinterpret_cast<const uintptr_t*>(pMeshScene + kMesh_sceneObject);
            if (sceneObj >= 0x10000)
            {
                const uint32_t ownerHandle = *reinterpret_cast<const uint32_t*>(sceneObj + kScene_ownerHandle);
                if (ownerHandle)
                {
                    const uintptr_t owner = Esp::LookupEntity(static_cast<int>(ownerHandle & 0x7FFF));
                    if (owner) scope = ScopeOf(owner);
                }
            }
        }

        // Build the layer list for this scope: {visibleMat/color, xqzMat/color}.
        bool doVis = false, doXqz = false;
        const float* visCol = nullptr; const float* xqzCol = nullptr;
        int mat = cfg.chamsType;

        switch (scope)
        {
        case SC_ENEMY: doVis = cfg.chams;     doXqz = cfg.chamsXqz; visCol = cfg.chamsColor;     xqzCol = cfg.chamsXqzColor; break;
        case SC_TEAM:    doVis = cfg.chamsTeam;    visCol = cfg.chamsTeamColor;    break;
        case SC_LOCAL:   doVis = cfg.chamsLocal;   visCol = cfg.chamsLocalColor;   break;
        case SC_ITEM:    doVis = cfg.itemChams;    visCol = cfg.itemChamsColor;    break;
        case SC_RAGDOLL: doVis = cfg.ragdollChams; visCol = cfg.ragdollChamsColor; break;
        default: break;
        }

        if (!g_installed || (!doVis && !doXqz) || !g_mat[MAT_FLAT][0])
        {
            oDrawArray(a1, a2, pMeshScene, nMeshCount, pSceneView, pSceneLayer, pUnk, pUnk2);
            return;
        }

        bool drew = false;
        if (doXqz && xqzCol)
        {
            SetMeshMat(pMeshScene, PickMat(mat, 1), xqzCol);
            oDrawArray(a1, a2, pMeshScene, nMeshCount, pSceneView, pSceneLayer, pUnk, pUnk2);
            drew = true;
        }
        if (doVis && visCol)
        {
            SetMeshMat(pMeshScene, PickMat(mat, 0), visCol);
            oDrawArray(a1, a2, pMeshScene, nMeshCount, pSceneView, pSceneLayer, pUnk, pUnk2);
            drew = true;
        }
        if (!drew)
            oDrawArray(a1, a2, pMeshScene, nMeshCount, pSceneView, pSceneLayer, pUnk, pUnk2);
    }
}

namespace Chams
{
    __declspec(dllexport) bool Initialize()
    {
        if (g_installed) return true;

        // Resolve material-system functions.
        g_kv3Alloc       = reinterpret_cast<fnKv3Alloc>(ResolvePattern("tier0.dll", ">E8????????4C8BF0EB03"));
        g_kv3Load        = reinterpret_cast<fnKv3Load>(ResolvePattern("tier0.dll", "44242848897C2420>E8????????0FB6D88B4C2444"));
        // Direct CMaterialSystem2 CreateMaterial prologue (verified: unique match on this build).
        g_materialCreate = reinterpret_cast<fnMaterialCreate>(ResolvePattern("materialsystem2.dll",
            "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 48 89 7C 24 ? 41 56 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 8B F2"));
        g_utlCtor        = reinterpret_cast<fnUtlBufCtor>(Export("tier0.dll", "??0CUtlBuffer@@QEAA@HHW4BufferFlags_t@0@@Z"));
        g_utlPutString   = reinterpret_cast<fnUtlPutString>(Export("tier0.dll", "?PutString@CUtlBuffer@@QEAAXPEBD@Z"));

        {
            char b[256];
            std::snprintf(b, sizeof(b),
                "resolve: kv3Alloc=%p kv3Load=%p matCreate=%p utlCtor=%p putStr=%p tier0=%p matsys2=%p scene=%p",
                (void*)g_kv3Alloc, (void*)g_kv3Load, (void*)g_materialCreate,
                (void*)g_utlCtor, (void*)g_utlPutString,
                (void*)GetModuleHandleA("tier0.dll"),
                (void*)GetModuleHandleA("materialsystem2.dll"),
                (void*)GetModuleHandleA("scenesystem.dll"));
            LogLine(b);
        }

        if (!g_kv3Alloc || !g_kv3Load || !g_materialCreate || !g_utlCtor || !g_utlPutString)
        {
            LogLine("material functions unresolved");
            return false;
        }

        const char* matNames[MAT_COUNT][2] = {
            { "materials/dev/tw_flat.vmat",  "materials/dev/tw_flat_i.vmat"  },
            { "materials/dev/tw_illum.vmat", "materials/dev/tw_illum_i.vmat" },
            { "materials/dev/tw_glow.vmat",  "materials/dev/tw_glow_i.vmat"  },
            { "materials/dev/tw_matte.vmat", "materials/dev/tw_matte_i.vmat" },
            { "materials/dev/tw_outl.vmat",  "materials/dev/tw_outl_i.vmat"  },
            { "materials/dev/tw_holo.vmat",  "materials/dev/tw_holo_i.vmat"  },
            { "materials/dev/tw_metal.vmat", "materials/dev/tw_metal_i.vmat" },
            { "materials/dev/tw_liquid.vmat","materials/dev/tw_liquid_i.vmat"},
            { "materials/dev/tw_bloom.vmat", "materials/dev/tw_bloom_i.vmat" },
            { "materials/dev/tw_dist.vmat",  "materials/dev/tw_dist_i.vmat"  },
            { "materials/dev/tw_pearl.vmat", "materials/dev/tw_pearl_i.vmat" },
        };
        for (int t = 0; t < MAT_COUNT; ++t)
            for (int l = 0; l < 2; ++l)
                g_mat[t][l] = CreateMaterial(kVmat[t][l], matNames[t][l]);

        {
            char b[192];
            std::snprintf(b, sizeof(b), "mats flat=%p/%p illum=%p/%p glow=%p/%p",
                (void*)g_mat[0][0], (void*)g_mat[0][1], (void*)g_mat[1][0],
                (void*)g_mat[1][1], (void*)g_mat[2][0], (void*)g_mat[2][1]);
            LogLine(b);
        }
        if (!g_mat[MAT_FLAT][0])
        {
            LogLine("flat material creation failed");
            return false;
        }

        // Resolve + hook scenesystem DrawArray (per-mesh material override).
        g_drawArrayAddr = reinterpret_cast<void*>(ResolvePattern("scenesystem.dll",
            "48 8B C4 53 57 41 54 48 81 EC D0 00 00 00 49 63 F9 49"));
        if (!g_drawArrayAddr)
        {
            LogLine("DrawArray pattern not found");
            return false;
        }

        MH_Initialize(); // no-op if kiero already did it
        if (MH_CreateHook(g_drawArrayAddr, &hkDrawArray, reinterpret_cast<void**>(&oDrawArray)) != MH_OK)
        {
            LogLine("MH_CreateHook failed");
            return false;
        }
        if (MH_EnableHook(g_drawArrayAddr) != MH_OK)
        {
            LogLine("MH_EnableHook failed");
            return false;
        }

        g_installed = true;
        LogLine("chams init ok");
        return true;
    }

    __declspec(dllexport) void Shutdown()
    {
        if (!g_installed) return;
        MH_DisableHook(g_drawArrayAddr);
        MH_RemoveHook(g_drawArrayAddr);
        g_installed = false;
    }

    __declspec(dllexport) void UpdateTargets(const uintptr_t* enemies, int nEnemy,
                           const uintptr_t* team, int nTeam,
                           uintptr_t local,
                           const uintptr_t* items, int nItems,
                           const uintptr_t* ragdolls, int nRagdolls)
    {
        if (nEnemy < 0) nEnemy = 0; if (nEnemy > 64) nEnemy = 64;
        if (nTeam  < 0) nTeam  = 0; if (nTeam  > 64) nTeam  = 64;
        if (nItems < 0) nItems = 0; if (nItems > 128) nItems = 128;
        if (nRagdolls < 0) nRagdolls = 0; if (nRagdolls > 64) nRagdolls = 64;
        for (int i = 0; i < nEnemy; ++i) g_enemy[i] = enemies[i];
        for (int i = 0; i < nTeam;  ++i) g_team[i]  = team[i];
        for (int i = 0; i < nItems; ++i) g_items[i] = items[i];
        for (int i = 0; i < nRagdolls; ++i) g_ragdolls[i] = ragdolls[i];
        g_enemyN = nEnemy; g_teamN = nTeam; g_itemsN = nItems; g_ragdollN = nRagdolls; g_local = local;
    }
}
