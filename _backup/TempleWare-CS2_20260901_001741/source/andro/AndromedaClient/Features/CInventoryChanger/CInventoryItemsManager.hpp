#pragma once

#include <vector>
#include <Common/Common.hpp>
#include <unordered_map>
#include <string>

// Do NOT drag a full ImGui into consumers (andro_bridge uses TempleWare's ImGui,
// an ABI-different build). Only ImU32 is needed in this header's interface.
#ifndef IMGUI_VERSION
typedef unsigned int ImU32;
#endif

#include <AndromedaClient/Settings/Settings.hpp>

class CCSPlayerInventory;
class CEconItem;
struct ID3D11ShaderResourceView;

class CInventoryItemsManager final
{
public:
    enum EInventoryPrefab_t : uint32_t
    {
        INVENTORY_PREFAB_MUSIC_KIT = 1314 ,
        INVENTORY_PREFAB_STICKER_KIT = 1209 ,
    };

    enum EDumpedItemType_t
    {
        DUMPED_ITEM_TYPE_NONE ,
        DUMPED_ITEM_TYPE_WEAPON ,
        DUMPED_ITEM_TYPE_KNIFE ,
        DUMPED_ITEM_TYPE_GLOVE ,
        DUMPED_ITEM_TYPE_AGENT ,
        DUMPED_ITEM_TYPE_MUSIC,
    };

    struct DumpedSkin_t
    {
        int m_ID = 0;
        int m_Rarity = 0;
        std::string m_PaintKitName = "";
        std::string m_DisplayName = ""; 
		std::string m_IconPath = "";
        bool m_bLegacyModel = false;
    };

    struct DumpedItem_t
    {
        uint16_t m_DefIdx = 0;
        int m_Rarity = 0;
        int m_LoadoutSlot = -1;
        bool m_UnusualItem = false;
        std::vector<DumpedSkin_t> m_DumpedSkins = {};
        EDumpedItemType_t m_ItemType = DUMPED_ITEM_TYPE_NONE;
        std::string m_PaintKitName = "";
        std::string m_DisplayName = ""; 
        std::string m_IconPath = "";
    };

    struct AddedItem_t
    {
        uint16_t m_DefIdx = 0;
        int m_PaintKit = 0;
        int m_Rarity = 0;
        float m_Wear = 0.0f;
        bool m_UnusualItem = false;
        bool m_bLegacyModel = false;
        std::string m_PaintKitName = "";
        std::string m_DisplayName = "";
        EDumpedItemType_t m_ItemType = DUMPED_ITEM_TYPE_NONE;
        CEconItem* m_pEconItem = nullptr;
	};

    struct DumpedStickerKit_t
    {
        int         m_ID          = 0;
        int         m_Rarity      = 0;
        std::string m_DisplayName = "";
    };

private:
    std::vector<DumpedItem_t>                  m_vecDumpedItems;
    std::vector<DumpedStickerKit_t>            m_vecDumpedStickerKits;
    std::unordered_map<uint64_t , AddedItem_t> m_AddedItems;

public:
    auto ScanAllItems() -> void;

    auto ForceRescanCatalogue() -> bool;

    auto OnAddAllItems() -> void;

    auto ApplyLoadoutFromConfig() -> void;

    auto AddSelectedSkinToInventory( uint16_t defIdx , int paintKit , float wear = 0.0f , bool equip = false , int seed = 0 , int statTrak = -1 ) -> uint64_t;

    auto AddMusicToInventory( uint16_t defIdx , int MusicId , bool equip = false ) -> uint64_t;

    auto RemoveItemFromInventoryByID( uint64_t ID ) -> bool;

    auto FindOrCreateSelection( int iTeam , int iSlot ) -> Settings::Inventory::ItemSelection_t&;

    auto RemoveSelection( int iTeam , int iSlot ) -> void;

    auto GetSelection( int iTeam , int iSlot ) -> Settings::Inventory::ItemSelection_t*;

    inline auto IsItemIDAdded( uint64_t ID ) -> bool
    {
        if ( m_AddedItems.find( ID ) == m_AddedItems.end() )
            return false;

        return true;
    }

    inline auto GetDumpedItems() -> std::vector<DumpedItem_t>&
    {
        return m_vecDumpedItems;
    }

    inline auto GetDumpedStickerKits() -> std::vector<DumpedStickerKit_t>&
    {
        return m_vecDumpedStickerKits;
    }

    inline auto GetAddedItems() -> std::unordered_map<uint64_t, AddedItem_t>&
    {
        return m_AddedItems;
    }

    auto GetSkinName( int paintKitId ) -> std::string;

    auto GetWeaponName( uint16_t defIdx ) -> std::string;

    auto GetStickerName( int stickerKitId ) -> std::string;

    auto GetRarityName( int rarity ) -> std::string;

    auto GetRarityColor( int rarity ) -> ImU32;
};

auto GetInventoryItemsManager() -> CInventoryItemsManager*;
