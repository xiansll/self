#include "econ_item_system.h"
#include <thread>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <algorithm>
#include <functional>

#undef max
#undef min

#include "../../interfaces/interfaces.h"
#include "../../utils/memory/patternscan/patternscan.h"
#include "../../utils/memory/vfunc/vfunc.h"
#include "../../globals/d3d11_globals.h"

using namespace features::skinchanger;

// Guarded pointer read: a stale/invalid offset returns 0 instead of faulting
// (avoids crashing the game when the hardcoded offset no longer matches the
// current client.dll build). Kept in its own function so SEH does not collide
// with C++ object unwinding in the callers.
static std::uintptr_t SafeDeref(std::uintptr_t addr) {
    if (!addr) return 0;
    __try {
        return *reinterpret_cast<std::uintptr_t*>(addr);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

// Safe string read: copies a C-string out of game memory, returning empty on
// any fault or null. Only POD ops inside the SEH block (no C++ unwinding).
static void SafeCopyStr(const char* p, char* buf, int cap) {
    __try {
        int i = 0;
        for (; i < cap - 1 && p[i]; ++i) buf[i] = p[i];
        buf[i] = 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { buf[0] = 0; }
}
static std::string SafeStr(const char* p) {
    if (!p) return std::string();
    char buf[256]; buf[0] = 0;
    SafeCopyStr(p, buf, 256);
    return std::string(buf);
}

static void EisLog(const char* fmt, ...) {
    wchar_t path[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, path);
    wcscat_s(path, MAX_PATH, L"TempleWare.log");
    FILE* f = nullptr;
    if (_wfopen_s(&f, path, L"a") == 0 && f) {
        char buf[512];
        va_list ap; va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        fprintf(f, "[skins] %s\n", buf);
        fclose(f);
    }
}

bool EconItemSystem::Initialize() {
    EisLog("Initialize() begin");

    // Resolve the item-system singleton pointer via signature instead of a
    // hardcoded offset, so it survives client.dll updates.
    //   48 8B 05 ? ? ? ?   mov rax, [rip+g_pItemSystem]
    //   48 85 C0           test rax, rax
    //   0F 85 87           jne  ...
    const auto sig = M::FindPattern("client.dll", "48 8B 05 ? ? ? ? 48 85 C0 0F 85 87");
    EisLog("sig = %p", (void*)sig);
    if (!sig) { EisLog("FAIL: pattern not found"); return false; }

    const auto global_addr = M::ResolveRelativeAddress(sig, 0x3, 0x7);
    EisLog("global_addr = %p", (void*)global_addr);
    if (!global_addr) { EisLog("FAIL: global_addr null"); return false; }

    const auto item_system_ptr = SafeDeref(reinterpret_cast<std::uintptr_t>(global_addr));
    EisLog("item_system_ptr = %p", (void*)item_system_ptr);
    if (!item_system_ptr) { EisLog("FAIL: item_system_ptr null (schema not loaded yet? enter a match)"); return false; }

    const auto schema = SafeDeref(item_system_ptr + 0x8);
    EisLog("schema = %p", (void*)schema);
    if (!schema) { EisLog("FAIL: schema null"); return false; }

    if (!ParseItemDefs(schema)) { EisLog("FAIL: ParseItemDefs"); return false; }
    EisLog("ParseItemDefs ok, defs=%zu", m_item_defs.size());
    if (!ParsePaintKits(schema)) { EisLog("FAIL: ParsePaintKits"); return false; }
    EisLog("ParsePaintKits ok, kits=%zu", m_paint_kits.size());

    BuildIndices();
    EisLog("BuildIndices ok");
    ResolveLocalizedNames();
    EisLog("ResolveLocalizedNames ok");

    if (!BuildVpkIndex()) { EisLog("FAIL: BuildVpkIndex"); return false; }
    EisLog("BuildVpkIndex ok");

    BuildSkinIndex();
    EisLog("Initialize() complete OK");

    return true;
}

const EconItemSystem::ItemDef* EconItemSystem::FindDef(std::int16_t def_index) const {
    const auto it = m_def_index_map.find(def_index);
    if (it == m_def_index_map.end()) return nullptr;
    return &m_item_defs[it->second];
}

const EconItemSystem::PaintKit* EconItemSystem::FindPaintKit(int id) const {
    const auto it = m_paint_kit_map.find(id);
    if (it == m_paint_kit_map.end()) return nullptr;
    return &m_paint_kits[it->second];
}

const EconItemSystem::SkinImage* EconItemSystem::GetSkinImage(const std::string& image_inventory) {
    if (image_inventory.empty()) return nullptr;

    std::lock_guard lock(m_image_mutex);

    auto it = m_image_cache.find(image_inventory);
    if (it == m_image_cache.end()) {
        auto entry = std::make_unique<ImageEntry>();
        it = m_image_cache.emplace(image_inventory, std::move(entry)).first;
        RequestDecode(image_inventory);
        return nullptr;
    }

    const auto state = it->second->state.load(std::memory_order_acquire);
    if (state == ImageState::Ready) {
        return &it->second->image;
    }

    if (state == ImageState::Decoded) {
        if (FinalizeTexture(*it->second)) {
            return &it->second->image;
        }
    }

    return nullptr;
}

const EconItemSystem::SkinImage* EconItemSystem::GetSkinImage(std::int16_t def_index, int paint_kit_id) {
    const auto def = FindDef(def_index);
    if (!def) return nullptr;

    const auto pk = (paint_kit_id != 0) ? FindPaintKit(paint_kit_id) : nullptr;
    const auto path = BuildSkinImagePath(def, pk);

    return GetSkinImage(path);
}

int EconItemSystem::CombinedRarity(std::int16_t def_index, int paint_kit_id) const {
    auto weapon_rarity = 0;
    auto paint_rarity = 0;
    auto category = ItemCategory::Other;

    if (const auto def = FindDef(def_index)) {
        weapon_rarity = def->rarity;
        category = def->category;
    }

    if (const auto pk = FindPaintKit(paint_kit_id)) {
        paint_rarity = pk->rarity;
    }

    if (category == ItemCategory::Knife) {
        return 5;
    }

    return std::clamp(weapon_rarity + paint_rarity - 2, 0, 7);
}

void EconItemSystem::FlushSkinImages() {
    {
        std::lock_guard lock(m_image_mutex);
        m_image_cache.clear();
    }

    {
        std::lock_guard lock(m_vpk_mutex);
        m_archive_handles.clear();
    }
}

bool EconItemSystem::ParseItemDefs(std::uintptr_t schema) {
    const auto count = *reinterpret_cast<int*>(schema + 0xF8);
    const auto array = *reinterpret_cast<std::uintptr_t*>(schema + 0x100);

    if (!array || count <= 0) return false;

    m_item_defs.reserve(count);

    for (auto i = 0; i < count; i++) {
        const auto entry_base = array + static_cast<std::uintptr_t>(24 * i);
        const auto def_index = *reinterpret_cast<int*>(entry_base + 16);

        if (def_index < -1) continue;

        const auto def_ptr = *reinterpret_cast<std::uintptr_t*>(entry_base + 8);
        if (!def_ptr) continue;

        ItemDef item{};
        item.def_index = *reinterpret_cast<std::int16_t*>(def_ptr + 0x10);
        item.loadout_slot = *reinterpret_cast<int*>(def_ptr + 0x338);
        item.used_by_classes = *reinterpret_cast<std::uint32_t*>(def_ptr + 0x368);
        item.rarity = *reinterpret_cast<std::uint8_t*>(def_ptr + 0x42);

        // Name/type/model/image are DIRECT char* fields in the current schema
        // (single deref). The old double-deref treated string bytes as a
        // pointer and faulted. Offsets confirmed against a working build.
        item.item_class      = SafeStr(*reinterpret_cast<const char**>(def_ptr + 0x248)); // m_item_type/weapon name
        item.name            = SafeStr(*reinterpret_cast<const char**>(def_ptr + 0x260)); // item name
        item.model_player    = SafeStr(*reinterpret_cast<const char**>(def_ptr + 0x148)); // model
        item.image_inventory = SafeStr(*reinterpret_cast<const char**>(def_ptr + 0xA8));  // inventory image

        const auto token = *reinterpret_cast<const char**>(def_ptr + 0x70); // m_item_base_name (loc token)
        const std::string safe_token = SafeStr(token);
        item.localized_name = item.name;
        if (!safe_token.empty()) {
            if (const auto localize = I::Localize) {
                using Fn = const char*(__fastcall*)(void*, int, const char*);
                const auto localized = M::GetVFunc<Fn>(localize, 17)(localize, 17, safe_token.c_str());
                if (localized && *localized && std::strcmp(localized, safe_token.c_str()) != 0)
                    item.localized_name = localized;
            }
        }

        item.category = Classify(item.item_class.c_str(), item.loadout_slot);
        m_item_defs.push_back(std::move(item));
    }

    return true;
}

bool EconItemSystem::ParsePaintKits(std::uintptr_t schema) {
    const auto map_base = schema + 0x2E8;
    const auto tree_base = map_base + 0x08;

    const auto count = *reinterpret_cast<int*>(tree_base + 0x00);
    const auto nodes = *reinterpret_cast<std::uintptr_t*>(tree_base + 0x08);

    if (!nodes || count <= 0) return false;

    m_paint_kits.reserve(count);

    for (auto i = 0; i < count; i++) {
        const auto node_base = nodes + static_cast<std::uintptr_t>(32 * i);
        const auto pk_ptr = *reinterpret_cast<std::uintptr_t*>(node_base + 24);

        if (!pk_ptr) continue;

        PaintKit pk{};
        pk.id = *reinterpret_cast<int*>(pk_ptr + 0x00);
        pk.wear_min = *reinterpret_cast<float*>(pk_ptr + 0x6C);
        pk.wear_max = *reinterpret_cast<float*>(pk_ptr + 0x70);
        pk.legacy_model = *reinterpret_cast<bool*>(pk_ptr + 0xAE);
        pk.rarity = static_cast<std::uint8_t>(*reinterpret_cast<int*>(pk_ptr + 0x44) & 0xFF);

        // Direct char* fields (single deref), matching the current schema.
        pk.name       = SafeStr(*reinterpret_cast<const char**>(pk_ptr + 0x08));
        pk.desc_token = SafeStr(*reinterpret_cast<const char**>(pk_ptr + 0x10));
        pk.name_token = SafeStr(*reinterpret_cast<const char**>(pk_ptr + 0x18));

        m_paint_kits.push_back(std::move(pk));
    }

    return true;
}

void EconItemSystem::BuildIndices() {
    for (auto i = 0ull; i < m_item_defs.size(); i++) {
        const auto& def = m_item_defs[i];
        m_def_index_map[def.def_index] = i;

        switch (def.category) {
        case ItemCategory::Knife:   m_knives.push_back(&def); break;
        case ItemCategory::Glove:   m_gloves.push_back(&def); break;
        case ItemCategory::Agent:   m_agents.push_back(&def); break;
        case ItemCategory::Gun:     m_guns.push_back(&def); break;
        default: break;
        }
    }

    for (auto i = 0ull; i < m_paint_kits.size(); i++) {
        m_paint_kit_map[m_paint_kits[i].id] = i;
    }
}

void EconItemSystem::ResolveLocalizedNames() {
    for (auto& pk : m_paint_kits) {
        if (!pk.name_token.empty()) {
            const auto localize = I::Localize;
            if (localize) {
                using Fn = const char*(__fastcall*)(void*, int, const char*);
                const auto localized = M::GetVFunc<Fn>(localize, 17)(localize, 17, pk.name_token.c_str());
                if (localized && *localized && std::strcmp(localized, pk.name_token.c_str()) != 0) {
                    pk.localized_name = localized;
                    continue;
                }
            }
        }
        pk.localized_name = pk.name;
    }
}

bool EconItemSystem::BuildVpkIndex() {
    if (m_vpk_indexed) return !m_vpk_index.empty();

    m_vpk_indexed = true;

    static const char* search_paths[] = {
        "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Counter-Strike Global Offensive\\game\\csgo\\pak01_dir.vpk",
        "C:\\Program Files\\Steam\\steamapps\\common\\Counter-Strike Global Offensive\\game\\csgo\\pak01_dir.vpk"
    };

    std::ifstream file;
    for (const auto& path : search_paths) {
        file.open(path, std::ios::binary);
        if (file.is_open()) break;
    }

    if (!file.is_open()) return false;

#pragma pack(push, 1)
    struct VpkHeader {
        std::uint32_t signature;
        std::uint32_t version;
        std::uint32_t tree_size;
        std::uint32_t file_data_section_size;
        std::uint32_t archive_md5_section_size;
        std::uint32_t other_md5_section_size;
        std::uint32_t signature_section_size;
    };

    struct VpkEntry {
        std::uint32_t crc;
        std::uint16_t preload_bytes;
        std::uint16_t archive_index;
        std::uint32_t entry_offset;
        std::uint32_t entry_length;
        std::uint16_t terminator;
    };
#pragma pack(pop)

    VpkHeader header{};
    file.read(reinterpret_cast<char*>(&header), sizeof(header));

    if (header.signature != 0x55AA1234 || header.version != 2) return false;

    const auto tree_end = static_cast<std::streamoff>(sizeof(VpkHeader)) + static_cast<std::streamoff>(header.tree_size);

    while (file.tellg() < tree_end) {
        std::string extension;
        std::getline(file, extension, '\0');

        if (extension.empty()) break;

        const auto is_vtex = extension == "vtex_c";

        while (true) {
            std::string dir_path;
            std::getline(file, dir_path, '\0');

            if (dir_path.empty()) break;

            const auto is_econ = is_vtex && dir_path.find("panorama/images/econ") != std::string::npos;

            while (true) {
                std::string filename;
                std::getline(file, filename, '\0');

                if (filename.empty()) break;

                VpkEntry entry{};
                file.read(reinterpret_cast<char*>(&entry), sizeof(entry));

                if (entry.preload_bytes > 0) {
                    file.seekg(entry.preload_bytes, std::ios::cur);
                }

                if (!is_econ) continue;

                constexpr auto prefix_len = std::string_view("panorama/images/").size();
                auto key = dir_path.substr(prefix_len) + "/" + filename;

                m_vpk_index[std::move(key)] = VpkFileEntry{
                    entry.archive_index,
                    entry.entry_offset,
                    entry.entry_length
                };
            }
        }
    }

    return !m_vpk_index.empty();
}

void EconItemSystem::BuildSkinIndex() {
    std::unordered_map<std::string, int> pk_by_name;
    pk_by_name.reserve(m_paint_kits.size());

    for (const auto& pk : m_paint_kits) {
        pk_by_name.emplace(pk.name, pk.id);
    }

    std::unordered_map<std::string, std::int16_t> def_by_name;
    def_by_name.reserve(m_item_defs.size());

    for (const auto& d : m_item_defs) {
        if (!d.name.empty()) {
            def_by_name.emplace(d.name, d.def_index);
        }
    }

    constexpr std::string_view prefix{ "econ/default_generated/" };
    constexpr std::string_view suffix{ "_light_png" };

    for (const auto& [path, _] : m_vpk_index) {
        if (!path.starts_with(prefix) || !path.ends_with(suffix)) continue;

        std::string_view stem(path);
        stem.remove_prefix(prefix.size());
        stem.remove_suffix(suffix.size());

        std::int16_t def_idx{ -1 };
        std::string_view pk_name;

        for (auto i = stem.find('_', 1); i != std::string_view::npos; i = stem.find('_', i + 1)) {
            const auto candidate = std::string(stem.substr(0, i));
            const auto it = def_by_name.find(candidate);

            if (it == def_by_name.end()) continue;

            def_idx = it->second;
            pk_name = stem.substr(i + 1);
        }

        if (def_idx == -1 || pk_name.empty()) continue;

        const auto pk_it = pk_by_name.find(std::string(pk_name));
        if (pk_it == pk_by_name.end()) continue;

        m_skins.push_back({ def_idx, pk_it->second });
    }
}

void EconItemSystem::RequestDecode(const std::string& image_inventory) {
    const auto key = image_inventory + "_png";

    std::vector<std::byte> data;
    {
        std::lock_guard lock(m_vpk_mutex);
        data = ReadVpk(key);
    }

    if (data.empty()) {
        m_image_cache[image_inventory]->state.store(ImageState::Failed, std::memory_order_release);
        return;
    }

    m_image_cache[image_inventory]->state.store(ImageState::Loading, std::memory_order_release);

    std::thread([this, inv = image_inventory, buf = std::move(data)]() {
        std::lock_guard lock(m_image_mutex);

        const auto it = m_image_cache.find(inv);
        if (it == m_image_cache.end()) return;

        if (DecodeVtex(std::span<const std::byte>(buf.data(), buf.size()), *it->second)) {
            it->second->state.store(ImageState::Decoded, std::memory_order_release);
        } else {
            it->second->state.store(ImageState::Failed, std::memory_order_release);
        }
    }).detach();
}

bool EconItemSystem::FinalizeTexture(ImageEntry& entry) {
    ID3D11Device* device = D3D11::g_pDevice;
    if (!device) {
        entry.state.store(ImageState::Failed, std::memory_order_release);
        return false;
    }

    if (entry.mip_buffers.empty()) {
        entry.state.store(ImageState::Failed, std::memory_order_release);
        return false;
    }

    auto upload_format = entry.format;
    std::vector<std::uint8_t> rgba_pixels;

    if (entry.format == DXGI_FORMAT_BC7_UNORM) {
        rgba_pixels.resize(static_cast<std::size_t>(entry.width) * entry.height * 4);
        upload_format = DXGI_FORMAT_R8G8B8A8_UNORM;
    }

    const auto upload_data = upload_format == DXGI_FORMAT_R8G8B8A8_UNORM && !rgba_pixels.empty() ? rgba_pixels.data() : entry.mip_buffers[0].data();
    const auto upload_pitch = entry.width * 4;

    D3D11_TEXTURE2D_DESC td{};
    td.Width = entry.width;
    td.Height = entry.height;
    td.MipLevels = 0;
    td.ArraySize = 1;
    td.Format = upload_format;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    td.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
    if (FAILED(device->CreateTexture2D(&td, nullptr, &tex))) {
        entry.state.store(ImageState::Failed, std::memory_order_release);
        return false;
    }

    Microsoft::WRL::ComPtr<ID3D11DeviceContext> ctx;
    device->GetImmediateContext(&ctx);

    ctx->UpdateSubresource(tex.Get(), 0, nullptr, upload_data, upload_pitch, 0);

    D3D11_SHADER_RESOURCE_VIEW_DESC sv{};
    sv.Format = upload_format;
    sv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    sv.Texture2D.MipLevels = static_cast<UINT>(-1);

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    if (FAILED(device->CreateShaderResourceView(tex.Get(), &sv, &srv))) {
        entry.state.store(ImageState::Failed, std::memory_order_release);
        return false;
    }

    ctx->GenerateMips(srv.Get());

    entry.image.srv = std::move(srv);
    entry.image.width = static_cast<int>(entry.width);
    entry.image.height = static_cast<int>(entry.height);
    entry.mip_buffers.clear();
    entry.mip_buffers.shrink_to_fit();
    entry.state.store(ImageState::Ready, std::memory_order_release);

    return true;
}

EconItemSystem::ItemCategory EconItemSystem::Classify(const char* item_class, int loadout_slot) {
    if (loadout_slot == 38) return ItemCategory::Agent;
    if (loadout_slot == 41) return ItemCategory::Glove;

    if (std::strncmp(item_class, "weapon_knife", 12) == 0) return ItemCategory::Knife;

    if (std::strncmp(item_class, "weapon_", 7) == 0) {
        if (std::strcmp(item_class, "weapon_flashbang") == 0 ||
            std::strcmp(item_class, "weapon_hegrenade") == 0 ||
            std::strcmp(item_class, "weapon_smokegrenade") == 0 ||
            std::strcmp(item_class, "weapon_molotov") == 0 ||
            std::strcmp(item_class, "weapon_decoy") == 0 ||
            std::strcmp(item_class, "weapon_incgrenade") == 0 ||
            std::strcmp(item_class, "weapon_c4") == 0 ||
            std::strcmp(item_class, "weapon_healthshot") == 0 ||
            std::strcmp(item_class, "weapon_taser") == 0 ||
            std::strcmp(item_class, "weapon_knifegg") == 0) {
            return ItemCategory::Other;
        }
        return ItemCategory::Gun;
    }

    return ItemCategory::Other;
}

std::vector<std::byte> EconItemSystem::ReadVpk(const std::string& path) {
    const auto it = m_vpk_index.find(path);
    if (it == m_vpk_index.end()) return {};

    const auto& entry = it->second;
    auto& stream = m_archive_handles[entry.archive_index];

    if (!stream.is_open()) {
        char archive_name[256];
        std::snprintf(archive_name, sizeof(archive_name), "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Counter-Strike Global Offensive\\game\\csgo\\pak01_%03d.vpk", entry.archive_index);

        stream.open(archive_name, std::ios::binary);
        if (!stream.is_open()) return {};
    }

    stream.seekg(entry.offset);

    std::vector<std::byte> data(entry.length);
    stream.read(reinterpret_cast<char*>(data.data()), entry.length);

    return data;
}

bool EconItemSystem::DecodeVtex(std::span<const std::byte> data, ImageEntry& out) {
    const auto raw = reinterpret_cast<const std::uint8_t*>(data.data());
    const auto size = data.size();

    if (size < 28) return false;

    const auto file_size = *reinterpret_cast<const std::uint32_t*>(raw + 0x00);
    const auto header_version = *reinterpret_cast<const std::uint16_t*>(raw + 0x04);
    const auto block_count = *reinterpret_cast<const std::uint32_t*>(raw + 0x0C);

    if (header_version != 12 || block_count == 0 || block_count > 64) return false;

    constexpr auto block_header_size = 16u;
    constexpr auto block_entry_size = 12u;
    constexpr auto data_fourcc = 'D' | ('A' << 8) | ('T' << 16) | ('A' << 24);

    const std::uint8_t* data_block = nullptr;
    auto data_block_offset = 0ull;

    for (auto i = 0u; i < block_count; i++) {
        const auto entry_pos = block_header_size + i * block_entry_size;
        if (static_cast<std::size_t>(entry_pos) + block_entry_size > size) break;

        const auto type = *reinterpret_cast<const std::uint32_t*>(raw + entry_pos);
        const auto offset = *reinterpret_cast<const std::uint32_t*>(raw + entry_pos + 4);

        if (type != data_fourcc) continue;

        const auto data_start = entry_pos + 4 + offset;
        if (static_cast<std::size_t>(data_start) + 0x28 > size) return false;

        data_block = raw + data_start;
        data_block_offset = data_start;
        break;
    }

    if (!data_block) return false;

    const auto width = static_cast<std::uint32_t>(*reinterpret_cast<const std::uint16_t*>(data_block + 0x14));
    const auto height = static_cast<std::uint32_t>(*reinterpret_cast<const std::uint16_t*>(data_block + 0x16));
    const auto format = *reinterpret_cast<const std::uint8_t*>(data_block + 0x1A);
    const auto mip_count = static_cast<std::uint32_t>(*reinterpret_cast<const std::uint8_t*>(data_block + 0x1B));
    const auto extra_data_offset = *reinterpret_cast<const std::uint32_t*>(data_block + 0x20);
    const auto extra_data_count = *reinterpret_cast<const std::uint32_t*>(data_block + 0x24);

    if (width == 0 || height == 0 || mip_count == 0) return false;

    auto dxgi_format = DXGI_FORMAT_UNKNOWN;
    auto block_bytes = 0u;
    auto bytes_per_pixel = 0u;

    switch (format) {
    case 1:  dxgi_format = DXGI_FORMAT_BC1_UNORM; block_bytes = 8; break;
    case 2:  dxgi_format = DXGI_FORMAT_BC3_UNORM; block_bytes = 16; break;
    case 4:  dxgi_format = DXGI_FORMAT_R8G8B8A8_UNORM; bytes_per_pixel = 4; break;
    case 19: dxgi_format = DXGI_FORMAT_BC6H_UF16; block_bytes = 16; break;
    case 20: dxgi_format = DXGI_FORMAT_BC7_UNORM; block_bytes = 16; break;
    case 27: dxgi_format = DXGI_FORMAT_BC4_UNORM; block_bytes = 8; break;
    case 28: dxgi_format = DXGI_FORMAT_B8G8R8A8_UNORM; bytes_per_pixel = 4; break;
    default: return false;
    }

    constexpr auto extra_compressed_mip_size = 4u;
    auto is_compressed = false;
    const std::uint32_t* compressed_sizes = nullptr;
    auto compressed_sizes_count = 0u;

    if (extra_data_count > 0) {
        const auto table_pos = static_cast<std::size_t>(0x20) + extra_data_offset;

        for (auto i = 0u; i < extra_data_count; i++) {
            const auto entry_pos = table_pos + static_cast<std::size_t>(i) * 12;
            if (data_block_offset + entry_pos + 12 > size) return false;

            const auto etype = *reinterpret_cast<const std::uint32_t*>(data_block + entry_pos);
            const auto eoff = *reinterpret_cast<const std::uint32_t*>(data_block + entry_pos + 4);
            const auto esize = *reinterpret_cast<const std::uint32_t*>(data_block + entry_pos + 8);

            if (etype != extra_compressed_mip_size) continue;

            const auto body_pos = entry_pos + 4 + eoff;
            if (data_block_offset + body_pos + 12 > size || esize < 12) return false;

            const auto int1 = *reinterpret_cast<const std::uint32_t*>(data_block + body_pos);
            const auto mips_offset = *reinterpret_cast<const std::uint32_t*>(data_block + body_pos + 4);
            const auto mips_count_in_table = *reinterpret_cast<const std::uint32_t*>(data_block + body_pos + 8);

            if (int1 > 1 || mips_count_in_table != mip_count) return false;

            const auto array_pos = body_pos + 4 + mips_offset;
            if (data_block_offset + array_pos + mips_count_in_table * 4u > size) return false;

            is_compressed = (int1 == 1);
            compressed_sizes = reinterpret_cast<const std::uint32_t*>(data_block + array_pos);
            compressed_sizes_count = mips_count_in_table;
            break;
        }
    }

    const auto pixel_start = static_cast<std::size_t>(file_size);
    if (pixel_start >= size) return false;

    struct MipSizeCalculator {
        std::uint32_t width;
        std::uint32_t height;
        bool is_compressed;
        const std::uint32_t* compressed_sizes;
        std::uint32_t compressed_sizes_count;
        std::uint32_t block_bytes;
        std::uint32_t bytes_per_pixel;

        std::uint32_t calc_mip_size(std::uint32_t w, std::uint32_t h) const {
            if (block_bytes > 0) {
                const auto bw = std::max(4u, (w + 3u) & ~3u);
                const auto bh = std::max(4u, (h + 3u) & ~3u);
                return (bw / 4) * (bh / 4) * block_bytes;
            }
            return w * h * bytes_per_pixel;
        }

        std::uint32_t on_disk_size_for(std::uint32_t mip_level) const {
            const auto mw = std::max(1u, width >> mip_level);
            const auto mh = std::max(1u, height >> mip_level);
            const auto uncompressed = calc_mip_size(mw, mh);

            if (!is_compressed || compressed_sizes == nullptr || mip_level >= compressed_sizes_count) {
                return uncompressed;
            }

            const auto compressed = compressed_sizes[mip_level];
            return (compressed >= uncompressed) ? uncompressed : compressed;
        }
    };

    MipSizeCalculator calc {
        width, height, is_compressed, compressed_sizes, compressed_sizes_count, block_bytes, bytes_per_pixel
    };

    std::vector<std::vector<std::uint8_t>> mip_buffers(1);
    auto cursor = pixel_start;

    for (auto j = mip_count; j-- > 0u;) {
        const auto on_disk = calc.on_disk_size_for(j);

        if (cursor + on_disk > size) return false;

        if (j == 0) {
            const auto uncompressed = calc.calc_mip_size(width, height);
            mip_buffers[0].resize(uncompressed);

            if (!is_compressed || on_disk >= uncompressed) {
                if (on_disk != uncompressed) return false;
                std::memcpy(mip_buffers[0].data(), raw + cursor, uncompressed);
            } else {
                return false;
            }
        }

        cursor += on_disk;
    }

    out.mip_buffers = std::move(mip_buffers);
    out.width = width;
    out.height = height;
    out.format = dxgi_format;

    return true;
}

std::string EconItemSystem::BuildSkinImagePath(const ItemDef* def, const PaintKit* pk) const {
    if (!pk || pk->id == 0) {
        return def->image_inventory;
    }

    std::string_view base = def->image_inventory;
    if (base.empty()) {
        base = def->name;
    }

    auto slash = base.find_last_of('/');
    std::string_view stem = (slash == std::string_view::npos) ? base : base.substr(slash + 1);

    return std::string("econ/default_generated/") + std::string(stem) + "_" + pk->name + "_light";
}