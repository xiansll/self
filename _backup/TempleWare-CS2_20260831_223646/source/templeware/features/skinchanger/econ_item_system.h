#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <fstream>
#include <memory>
#include <span>
#include <d3d11.h>
#include <wrl/client.h>

#include "../../utils/fnv1a/fnv1a.h"
#include "../../utils/memory/memorycommon.h"
#include "../../utils/memory/patternscan/patternscan.h"
#include "../../utils/schema/schema.h"
#include "../../interfaces/interfaces.h"
#include "../../config/config.h"

namespace features::skinchanger {

class EconItemSystem {
public:
    enum class ItemCategory : std::uint8_t {
        Gun,
        Knife,
        Glove,
        Agent,
        Other
    };

    struct PaintKit {
        int id{};
        std::string name{};
        std::string desc_token{};
        std::string name_token{};
        std::string localized_name{};
        float wear_min{};
        float wear_max{};
        bool legacy_model{};
        std::uint8_t rarity{};
    };

    struct ItemDef {
        std::int16_t def_index{};
        std::string item_class{};
        std::string name{};
        std::string localized_name{};
        std::string model_player{};
        std::string image_inventory{};
        int loadout_slot{};
        std::uint32_t used_by_classes{};
        ItemCategory category{};
        std::uint8_t rarity{};

        [[nodiscard]] int Team() const {
            if ((used_by_classes & 0xC) == 0xC) return 0;
            if (used_by_classes & 4) return 2;
            if (used_by_classes & 8) return 3;
            return 0;
        }
    };

    struct SkinEntry {
        std::int16_t def_index{};
        int paint_kit_id{};
    };

    struct SkinImage {
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv{};
        int width{};
        int height{};
    };

    [[nodiscard]] bool Initialize();

    [[nodiscard]] const std::vector<PaintKit>& PaintKits() const { return m_paint_kits; }
    [[nodiscard]] const std::vector<ItemDef>& ItemDefs() const { return m_item_defs; }

    [[nodiscard]] const std::vector<const ItemDef*>& Knives() const { return m_knives; }
    [[nodiscard]] const std::vector<const ItemDef*>& Gloves() const { return m_gloves; }
    [[nodiscard]] const std::vector<const ItemDef*>& Agents() const { return m_agents; }
    [[nodiscard]] const std::vector<const ItemDef*>& Guns() const { return m_guns; }
    [[nodiscard]] const std::vector<SkinEntry>& Skins() const { return m_skins; }

    [[nodiscard]] const ItemDef* FindDef(std::int16_t def_index) const;
    [[nodiscard]] const PaintKit* FindPaintKit(int id) const;
    [[nodiscard]] const SkinImage* GetSkinImage(const std::string& image_inventory);
    [[nodiscard]] const SkinImage* GetSkinImage(std::int16_t def_index, int paint_kit_id);

    [[nodiscard]] int CombinedRarity(std::int16_t def_index, int paint_kit_id) const;

    void FlushSkinImages();

private:
    enum class ImageState : std::uint8_t {
        Idle,
        Loading,
        Decoded,
        Ready,
        Failed
    };

    struct ImageEntry {
        SkinImage image{};
        std::atomic<ImageState> state{ ImageState::Idle };
        std::vector<std::vector<std::uint8_t>> mip_buffers{};
        std::uint32_t width{};
        std::uint32_t height{};
        DXGI_FORMAT format{ DXGI_FORMAT_UNKNOWN };
    };

    bool ParseItemDefs(std::uintptr_t schema);
    bool ParsePaintKits(std::uintptr_t schema);
    void BuildIndices();
    void ResolveLocalizedNames();
    bool BuildVpkIndex();
    void BuildSkinIndex();

    void RequestDecode(const std::string& image_inventory);
    bool FinalizeTexture(ImageEntry& entry);

    [[nodiscard]] ItemCategory Classify(const char* item_class, int loadout_slot);
    [[nodiscard]] std::vector<std::byte> ReadVpk(const std::string& path);
    [[nodiscard]] bool DecodeVtex(std::span<const std::byte> data, ImageEntry& out);
    [[nodiscard]] std::string BuildSkinImagePath(const ItemDef* def, const PaintKit* pk) const;

    std::vector<PaintKit> m_paint_kits{};
    std::vector<ItemDef> m_item_defs{};

    std::vector<const ItemDef*> m_knives{};
    std::vector<const ItemDef*> m_gloves{};
    std::vector<const ItemDef*> m_agents{};
    std::vector<const ItemDef*> m_guns{};
    std::vector<SkinEntry> m_skins{};

    std::unordered_map<std::int16_t, std::size_t> m_def_index_map{};
    std::unordered_map<int, std::size_t> m_paint_kit_map{};

    struct VpkFileEntry {
        std::uint16_t archive_index{};
        std::uint32_t offset{};
        std::uint32_t length{};
    };

    std::unordered_map<std::string, VpkFileEntry> m_vpk_index{};
    bool m_vpk_indexed{};

    std::unordered_map<std::string, std::unique_ptr<ImageEntry>> m_image_cache{};
    std::mutex m_image_mutex{};

    std::unordered_map<std::uint16_t, std::ifstream> m_archive_handles{};
    std::mutex m_vpk_mutex{};
};

inline EconItemSystem g_econ_item_system;

} // namespace features::skinchanger