#pragma once

#include <string>
#include <vector>

#include <Common/Common.hpp>
#include <ImGui/imgui.h>
#include <AndromedaClient/Settings/Settings.hpp>

class CAndromedaMenu final
{
public:
    auto OnRenderMenu() -> void;

private:
    auto RenderCheckBox(const char* szTitle, const char* szStrID, bool& v) -> bool;
    auto RenderComboBox(const char* szTitle, const char* szStrID, int& v,
        const char* Items[], int ItemsCount) -> bool;
    auto RenderSliderInt(const char* szTitle, const char* szStrID,
        int& v, int Min, int Max) -> bool;
    auto RenderSliderFloat(const char* szTitle, const char* szStrID,
        float& v, float Min, float Max) -> bool;
    auto RenderInputInt(const char* szTitle, const char* szStrID, int& v) -> bool;
    auto RenderInputText(const char* szTitle, const char* szStrID, std::string& v) -> bool;
    auto RenderColorEdit(const char* szTitle, const char* szStrID, float* Color) -> bool;

    auto RenderTabBar() -> void;
    auto RenderContent() -> void;

    auto RenderTab_Add() -> void;
    auto RenderAdd_WeaponPicker() -> void;
    auto RenderAdd_SkinPicker() -> void;
    auto RenderAdd_ItemConfig() -> void;
    auto RenderAdd_StickerPicker() -> void;

    auto RenderTab_Loadout() -> void;

    auto RenderTab_Settings() -> void;

    auto RenderBackButton(const char* label = nullptr) -> bool;
    auto RenderWearBar(ImDrawList* dl, ImVec2 pos,
        float width, float height, float wear) -> void;
    auto RenderItemCard(int idx, const char* name, int rarity,
        bool highlighted, float cardW, float cardH) -> bool;

    static auto GetWearTierName(float wear) -> const char*;
    static auto ToLower(const std::string& s) -> std::string;

private:
    enum ETab_t : int
    {
        TAB_ADD = 0,
        TAB_LOADOUT = 1,
        TAB_SETTINGS = 2,
    };

    enum EAddStep_t : int
    {
        ADD_STEP_WEAPON = 0,
        ADD_STEP_SKIN = 1,
        ADD_STEP_CONFIG = 2,
        ADD_STEP_STICKER = 3,
    };

    enum EWeaponCategory_t : int
    {
        WCAT_WEAPONS = 0,
        WCAT_KNIVES = 1,
        WCAT_GLOVES = 2,
        WCAT_AGENTS = 3,
        WCAT_MUSIC = 4,
    };

    ETab_t            m_eTab = TAB_ADD;
    EAddStep_t        m_eAddStep = ADD_STEP_WEAPON;
    EWeaponCategory_t m_eWeaponCategory = WCAT_WEAPONS;

    int  m_iSelectedTeam = 2;
    bool m_bSkippedSkinStep = false;

    char m_szWeaponSearch[128] = {};
    char m_szSkinSearch[128] = {};
    char m_szStickerSearch[128] = {};

    int  m_iActiveStickerSlot = -1;

    Settings::Inventory::ItemSelection_t m_PendingSelection{};

    int m_iLoadoutViewTeam = 2;

    int         m_iSelectedConfigIndex = 0;
    bool        m_bConfigListInit = false;
    std::string m_sNewConfigName = "default.json";
};

auto GetAndromedaMenu() -> CAndromedaMenu*;