#pragma once
#include <d3d11.h>

// Weapon icons ripped live from CS2's own equipment SVGs: parse pak01_dir.vpk,
// pull the compiled .vsvg_c blobs, decompile to raw SVG, rasterize with nanosvg
// into D3D11 textures. Fully self-contained; nothing shipped, keyed by weapon
// short name (e.g. "ak47", "awp", "hegrenade", "knife", "c4").
namespace Icons
{
    // Locates the game VPK (via client.dll path) and caches the equipment SVGs.
    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context);

    bool Ready();

    // Returns an ImTextureID (ID3D11ShaderResourceView*) for `name` rasterized at
    // `scale`, or nullptr if unavailable. Fills w/h with the pixel size.
    void* Get(const char* name, float scale, int* w, int* h);

    // Rasterize an SVG file from disk to a texture at a target pixel height.
    // Cached by path+height. Returns SRV (ImTextureID) or nullptr.
    void* LoadSvgFile(const char* path, int targetHeight, int* w, int* h);

    // Rasterize SVG markup from memory. `cacheKey` uniquely names the result.
    void* RasterSvgData(const char* cacheKey, const char* svg, int targetHeight, int* w, int* h);
}
