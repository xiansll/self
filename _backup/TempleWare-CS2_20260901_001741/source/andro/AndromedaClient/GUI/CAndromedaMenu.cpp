#define NOMINMAX
#include "CAndromedaMenu.hpp"

#include <algorithm>
#include <cstring>
#include <cctype>
#include <cstdio>

#include <ImGui/imgui.h>
#include <ImGui/imgui_internal.h>

#include <AndromedaClient/Settings/Settings.hpp>
#include <AndromedaClient/Settings/CSettingsJson.hpp>
#include <AndromedaClient/CAndromedaGUI.hpp>
#include <AndromedaClient/Features/CInventoryChanger/CInventoryChanger.hpp>
#include <AndromedaClient/Features/CInventoryChanger/CInventoryItemsManager.hpp>

static CAndromedaMenu g_CAndromedaMenu{};

static constexpr ImU32 kColBg        = IM_COL32( 12,  12,  12, 255);
static constexpr ImU32 kColSurface   = IM_COL32( 20,  20,  20, 255);
static constexpr ImU32 kColRaised    = IM_COL32( 28,  28,  28, 255);
static constexpr ImU32 kColHover     = IM_COL32( 36,  36,  36, 255);
static constexpr ImU32 kColBorder    = IM_COL32( 42,  42,  42, 255);
static constexpr ImU32 kColAccent    = IM_COL32(255, 255, 255, 255);
static constexpr ImU32 kColAccentDim = IM_COL32(255, 255, 255,  40);
static constexpr ImU32 kColText      = IM_COL32(218, 218, 218, 255);
static constexpr ImU32 kColTextDim   = IM_COL32( 78,  78,  78, 255);
static constexpr ImU32 kColSelected  = IM_COL32( 26,  26,  26, 255);
static constexpr ImU32 kColTabBar    = IM_COL32(  8,   8,   8, 255);

auto CAndromedaMenu::GetWearTierName(float wear) -> const char*
{
    if (wear < 0.07f) return "Factory New";
    if (wear < 0.15f) return "Minimal Wear";
    if (wear < 0.38f) return "Field-Tested";
    if (wear < 0.45f) return "Well-Worn";
    return "Battle-Scarred";
}

auto CAndromedaMenu::ToLower(const std::string& s) -> std::string
{
    std::string out = s;
    for (auto& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

static int ItemTypeFor(int cat)
{
    switch (cat)
    {
    case 0: return CInventoryItemsManager::DUMPED_ITEM_TYPE_WEAPON;
    case 1: return CInventoryItemsManager::DUMPED_ITEM_TYPE_KNIFE;
    case 2: return CInventoryItemsManager::DUMPED_ITEM_TYPE_GLOVE;
    case 3: return CInventoryItemsManager::DUMPED_ITEM_TYPE_AGENT;
    case 4: return CInventoryItemsManager::DUMPED_ITEM_TYPE_MUSIC;
    default: return CInventoryItemsManager::DUMPED_ITEM_TYPE_NONE;
    }
}

static int SlotFor(int cat, int itemLoadoutSlot)
{
    switch (cat)
    {
    case 0: return itemLoadoutSlot;
    case 1: return LOADOUT_SLOT_MELEE;
    case 2: return LOADOUT_SLOT_CLOTHING_HANDS;
    case 3: return LOADOUT_SLOT_CLOTHING_CUSTOMPLAYER;
    case 4: return LOADOUT_SLOT_MUSICKIT;
    default: return -1;
    }
}

static int TeamFor(int cat, int selectedTeam)
{
    return (cat == 4) ? 0 : selectedTeam;
}

static void RenderSectionLabel(const char* label)
{
    ImDrawList* dl  = ImGui::GetWindowDrawList();
    ImVec2      p   = ImGui::GetCursorScreenPos();
    const float lw  = ImGui::CalcTextSize(label).x;
    const float th  = ImGui::GetTextLineHeight();
    const float ly  = p.y + th * 0.5f;
    const float avW = ImGui::GetContentRegionAvail().x;
    dl->AddText(p, kColTextDim, label);
    if (lw + 10.f < avW)
        dl->AddLine(ImVec2(p.x + lw + 8.f, ly),
                    ImVec2(p.x + avW, ly),
                    kColBorder, 1.f);
    ImGui::Dummy(ImVec2(avW, th));
}

auto CAndromedaMenu::OnRenderMenu() -> void
{
    const float alpha = 200.f / 255.f;
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);                            // [v1]

    ImGui::PushStyleColor(ImGuiCol_WindowBg,              ImVec4(12/255.f, 12/255.f, 12/255.f, 1.f)); // [c1]
    ImGui::PushStyleColor(ImGuiCol_Border,                ImVec4(42/255.f, 42/255.f, 42/255.f, 1.f)); // [c2]
    ImGui::PushStyleColor(ImGuiCol_FrameBg,               ImVec4(20/255.f, 20/255.f, 20/255.f, 1.f)); // [c3]
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,        ImVec4(28/255.f, 28/255.f, 28/255.f, 1.f)); // [c4]
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,         ImVec4(36/255.f, 36/255.f, 36/255.f, 1.f)); // [c5]
    ImGui::PushStyleColor(ImGuiCol_SliderGrab,            ImVec4(1.f, 1.f, 1.f, 0.50f));              // [c6]
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,      ImVec4(1.f, 1.f, 1.f, 0.80f));              // [c7]
    ImGui::PushStyleColor(ImGuiCol_CheckMark,             ImVec4(1.f, 1.f, 1.f, 0.80f));              // [c8]
    ImGui::PushStyleColor(ImGuiCol_Button,                ImVec4(20/255.f, 20/255.f, 20/255.f, 1.f)); // [c9]
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,         ImVec4(28/255.f, 28/255.f, 28/255.f, 1.f)); // [c10]
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,          ImVec4(38/255.f, 38/255.f, 38/255.f, 1.f)); // [c11]
    ImGui::PushStyleColor(ImGuiCol_Header,                ImVec4(26/255.f, 26/255.f, 26/255.f, 1.f)); // [c12]
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered,         ImVec4(34/255.f, 34/255.f, 34/255.f, 1.f)); // [c13]
    ImGui::PushStyleColor(ImGuiCol_HeaderActive,          ImVec4(44/255.f, 44/255.f, 44/255.f, 1.f)); // [c14]
    ImGui::PushStyleColor(ImGuiCol_Separator,             ImVec4(42/255.f, 42/255.f, 42/255.f, 1.f)); // [c15]
    ImGui::PushStyleColor(ImGuiCol_SeparatorHovered,      ImVec4(60/255.f, 60/255.f, 60/255.f, 1.f)); // [c16]
    ImGui::PushStyleColor(ImGuiCol_SeparatorActive,       ImVec4(80/255.f, 80/255.f, 80/255.f, 1.f)); // [c17]
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg,           ImVec4(0.f, 0.f, 0.f, 0.f));                // [c18]
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab,         ImVec4(44/255.f, 44/255.f, 44/255.f, 1.f)); // [c19]
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered,  ImVec4(60/255.f, 60/255.f, 60/255.f, 1.f)); // [c20]
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive,   ImVec4(75/255.f, 75/255.f, 75/255.f, 1.f)); // [c21]
    ImGui::PushStyleColor(ImGuiCol_PopupBg,               ImVec4(14/255.f, 14/255.f, 14/255.f, 1.f)); // [c22]
    ImGui::PushStyleColor(ImGuiCol_TableHeaderBg,         ImVec4(14/255.f, 14/255.f, 14/255.f, 1.f)); // [c23]
    ImGui::PushStyleColor(ImGuiCol_TableBorderLight,      ImVec4(42/255.f, 42/255.f, 42/255.f, 1.f)); // [c24]
    ImGui::PushStyleColor(ImGuiCol_TableBorderStrong,     ImVec4(50/255.f, 50/255.f, 50/255.f, 1.f)); // [c25]
    ImGui::PushStyleColor(ImGuiCol_TableRowBg,            ImVec4(0.f, 0.f, 0.f, 0.f));                // [c26]
    ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt,         ImVec4(1.f, 1.f, 1.f, 0.012f));             // [c27]
    ImGui::PushStyleColor(ImGuiCol_Text,                  ImVec4(218/255.f, 218/255.f, 218/255.f, 1.f)); // [c28]
    ImGui::PushStyleColor(ImGuiCol_TextDisabled,          ImVec4(78/255.f, 78/255.f, 78/255.f, 1.f)); // [c29]
    ImGui::PushStyleColor(ImGuiCol_ResizeGrip,            ImVec4(0.f, 0.f, 0.f, 0.f));                // [c30]
    ImGui::PushStyleColor(ImGuiCol_ResizeGripHovered,     ImVec4(1.f, 1.f, 1.f, 0.08f));              // [c31]
    ImGui::PushStyleColor(ImGuiCol_ResizeGripActive,      ImVec4(1.f, 1.f, 1.f, 0.16f));              // [c32]
    ImGui::PushStyleColor(ImGuiCol_ChildBg,              ImVec4(0.f, 0.f, 0.f, 0.f));                // [c33]
    ImGui::PushStyleColor(ImGuiCol_NavWindowingHighlight,ImVec4(0.f, 0.f, 0.f, 0.f));               // [c34]
    ImGui::PushStyleColor(ImGuiCol_NavWindowingDimBg,    ImVec4(0.f, 0.f, 0.f, 0.f));               // [c35]
    ImGui::PushStyleColor(ImGuiCol_WindowShadow,         ImVec4(0.f, 0.f, 0.f, 0.f));               // [c36] kills glow
    static constexpr int kNCol = 36;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,    8.f);   // [v2]
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,     4.f);   // [v3]
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 4.f);   // [v4]
    ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding,      4.f);   // [v5]
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding,     6.f);   // [v6]
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,  1.f);   // [v7]
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,   0.f);   // [v8]
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,       ImVec2(10.f, 10.f)); // [v9]
    static constexpr int kNVar = 9; // v1..v9

    ImGui::SetNextWindowSize(ImVec2(880.f, 560.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(700.f, 440.f), ImVec2(1400.f, 900.f));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(0.f, 0.f));

    const bool bOpen = ImGui::Begin(XorStr(CHEAT_NAME), nullptr,
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize);

    ImGui::PopStyleVar(2);

    if (bOpen)
    {
        RenderTabBar();

        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2       p = ImGui::GetCursorScreenPos();
            float        w = ImGui::GetContentRegionAvail().x;
            dl->AddLine(p, ImVec2(p.x + w, p.y), kColBorder, 1.f);
        }

        static constexpr float kPadX = 20.f;
        static constexpr float kPadY = 14.f;
        ImGui::SetCursorPos(ImVec2(kPadX, ImGui::GetCursorPosY() + 1.f + kPadY));

        const float avW = ImGui::GetContentRegionAvail().x - kPadX;
        const float avH = ImGui::GetContentRegionAvail().y - kPadY;

        if (ImGui::BeginChild(XorStr("##Content"), ImVec2(avW, avH), false))
            RenderContent();
        ImGui::EndChild();
    }

    ImGui::End();
    ImGui::PopStyleColor(kNCol);
    ImGui::PopStyleVar(kNVar);
}

auto CAndromedaMenu::RenderTabBar() -> void
{
    const float tabH   = 46.f;
    const float totalW = ImGui::GetContentRegionAvail().x;
    const float tabW   = totalW / 3.f;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(8/255.f, 8/255.f, 8/255.f, 1.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(0.f, 0.f));

    bool childOpen = ImGui::BeginChild(XorStr("##TabBar"), ImVec2(totalW, tabH), false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    if (childOpen)
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2      wp = ImGui::GetWindowPos();
        dl->AddRectFilled(wp, ImVec2(wp.x + totalW, wp.y + tabH), kColTabBar);

        static const char* kLabels[] = { "ADD", "LOADOUT", "SETTINGS" };

        for (int i = 0; i < 3; ++i)
        {
            if (i > 0) ImGui::SameLine(0.f, 0.f);

            const bool active = (m_eTab == i);

            ImGui::PushID(i);
            ImGui::PushStyleColor(ImGuiCol_Button,
                active ? ImVec4(16/255.f, 16/255.f, 16/255.f, 1.f)
                       : ImVec4(0.f, 0.f, 0.f, 0.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                active ? ImVec4(16/255.f, 16/255.f, 16/255.f, 1.f)
                       : ImVec4(1.f, 1.f, 1.f, 0.02f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                ImVec4(20/255.f, 20/255.f, 20/255.f, 1.f));
            ImGui::PushStyleColor(ImGuiCol_Text,
                active ? ImVec4(0.90f, 0.90f, 0.90f, 1.f)
                       : ImVec4(0.32f, 0.32f, 0.32f, 1.f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(0.f, 0.f));

            if (ImGui::Button(kLabels[i], ImVec2(tabW, tabH)))
                m_eTab = static_cast<ETab_t>(i);

            const ImVec2 bMin = ImGui::GetItemRectMin();
            const ImVec2 bMax = ImGui::GetItemRectMax();

            if (i < 2)
                dl->AddLine(ImVec2(bMax.x, bMin.y + 14.f),
                            ImVec2(bMax.x, bMax.y - 14.f),
                            kColBorder, 1.f);

            if (active)
                dl->AddRectFilled(
                    ImVec2(bMin.x + 28.f, bMax.y - 2.f),
                    ImVec2(bMax.x - 28.f, bMax.y),
                    kColAccent);

            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(4);
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
}

auto CAndromedaMenu::RenderContent() -> void
{
    switch (m_eTab)
    {
    case TAB_ADD:      RenderTab_Add();      break;
    case TAB_LOADOUT:  RenderTab_Loadout();  break;
    case TAB_SETTINGS: RenderTab_Settings(); break;
    default: break;
    }
}

auto CAndromedaMenu::RenderTab_Add() -> void
{
    auto* pMgr = GetInventoryItemsManager();

    if (pMgr->GetDumpedItems().empty())
    {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.40f, 0.40f, 0.40f, 1.f));
        ImGui::TextUnformatted(XorStr("Item schema not loaded yet."));
        ImGui::TextUnformatted(XorStr("Launch CS2 and reach the main menu or a match, then:"));
        ImGui::PopStyleColor();
        ImGui::Spacing();

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(14.f, 7.f));
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(20/255.f, 20/255.f, 20/255.f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(30/255.f, 30/255.f, 30/255.f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.80f, 0.80f, 0.80f, 1.f));
        if (ImGui::Button(XorStr("Force Refresh Schema")))
            pMgr->ForceRescanCatalogue();
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(2);
        return;
    }

    switch (m_eAddStep)
    {
    case ADD_STEP_WEAPON:  RenderAdd_WeaponPicker();  break;
    case ADD_STEP_SKIN:    RenderAdd_SkinPicker();    break;
    case ADD_STEP_CONFIG:  RenderAdd_ItemConfig();    break;
    case ADD_STEP_STICKER: RenderAdd_StickerPicker(); break;
    default: break;
    }
}

auto CAndromedaMenu::RenderAdd_WeaponPicker() -> void
{
    auto* pMgr    = GetInventoryItemsManager();
    auto& vecItems = pMgr->GetDumpedItems();

    {
        static const char* kCatLabels[] =
            { "Weapons", "Knives", "Gloves", "Agents", "Music Kits" };

        const float availW  = ImGui::GetContentRegionAvail().x;
        const bool  showTeam = (m_eWeaponCategory != WCAT_MUSIC);
        const float rightW  = showTeam ? 220.f : 130.f;

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(12.f, 5.f));

        for (int i = 0; i < 5; ++i)
        {
            if (i > 0) ImGui::SameLine(0.f, 4.f);
            const bool active = (m_eWeaponCategory == i);

            ImGui::PushID(100 + i);
            ImGui::PushStyleColor(ImGuiCol_Button,
                active ? ImVec4(28/255.f, 28/255.f, 28/255.f, 1.f)
                       : ImVec4(0.f, 0.f, 0.f, 0.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                ImVec4(28/255.f, 28/255.f, 28/255.f, 1.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                ImVec4(36/255.f, 36/255.f, 36/255.f, 1.f));
            ImGui::PushStyleColor(ImGuiCol_Text,
                active ? ImVec4(0.85f, 0.85f, 0.85f, 1.f)
                       : ImVec4(0.38f, 0.38f, 0.38f, 1.f));

            if (ImGui::Button(kCatLabels[i]))
            {
                m_eWeaponCategory = static_cast<EWeaponCategory_t>(i);
                m_szWeaponSearch[0] = '\0';
            }

            if (active)
            {
                ImDrawList* dl  = ImGui::GetWindowDrawList();
                ImVec2 bMin     = ImGui::GetItemRectMin();
                ImVec2 bMax     = ImGui::GetItemRectMax();
                dl->AddRectFilled(ImVec2(bMin.x + 6.f, bMax.y - 1.f),
                                  ImVec2(bMax.x - 6.f, bMax.y),
                                  kColAccent);
            }

            ImGui::PopStyleColor(4);
            ImGui::PopID();
        }

        ImGui::PopStyleVar(2);
        ImGui::SameLine(availW - rightW, 0.f);

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(10.f, 4.f));

        if (showTeam)
        {
            ImGui::PushStyleColor(ImGuiCol_Button,
                m_iSelectedTeam == 2
                ? ImVec4(0.18f, 0.14f, 0.05f, 1.f)
                : ImVec4(0.10f, 0.10f, 0.10f, 1.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                ImVec4(0.20f, 0.16f, 0.07f, 1.f));
            ImGui::PushStyleColor(ImGuiCol_Text,
                m_iSelectedTeam == 2
                ? ImVec4(0.85f, 0.68f, 0.30f, 1.f)
                : ImVec4(0.40f, 0.40f, 0.40f, 1.f));
            if (ImGui::Button(XorStr(" T ##T"))) m_iSelectedTeam = 2;
            ImGui::PopStyleColor(3);

            ImGui::SameLine(0.f, 4.f);

            ImGui::PushStyleColor(ImGuiCol_Button,
                m_iSelectedTeam == 3
                ? ImVec4(0.05f, 0.10f, 0.20f, 1.f)
                : ImVec4(0.10f, 0.10f, 0.10f, 1.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                ImVec4(0.07f, 0.12f, 0.22f, 1.f));
            ImGui::PushStyleColor(ImGuiCol_Text,
                m_iSelectedTeam == 3
                ? ImVec4(0.40f, 0.60f, 0.90f, 1.f)
                : ImVec4(0.40f, 0.40f, 0.40f, 1.f));
            if (ImGui::Button(XorStr("CT##CT"))) m_iSelectedTeam = 3;
            ImGui::PopStyleColor(3);

            ImGui::SameLine(0.f, 8.f);
        }

        ImGui::SetNextItemWidth(showTeam ? 110.f : rightW);
        ImGui::InputText(XorStr("##WSearch"), m_szWeaponSearch, sizeof(m_szWeaponSearch));
        ImGui::PopStyleVar(2);
    }

    ImGui::Spacing();

    std::vector<const CInventoryItemsManager::DumpedItem_t*> filtered;
    filtered.reserve(vecItems.size());
    const int typeFilter = ItemTypeFor(m_eWeaponCategory);
    const std::string sLow = ToLower(m_szWeaponSearch);

    for (const auto& item : vecItems)
    {
        if (item.m_ItemType != typeFilter) continue;
        if (item.m_DumpedSkins.empty() && m_eWeaponCategory == WCAT_WEAPONS) continue;
        if (!sLow.empty() && ToLower(item.m_DisplayName).find(sLow) == std::string::npos) continue;
        filtered.push_back(&item);
    }

    const float avW     = ImGui::GetContentRegionAvail().x;
    const float cardH   = 68.f;
    const float cardGap = 8.f;
    const int   cols    = std::max(1, (int)((avW + cardGap) / (160.f + cardGap)));
    const float cardW   = (avW - (cols - 1) * cardGap) / static_cast<float>(cols);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(0.f, cardGap));

    if (ImGui::BeginChild(XorStr("##WeaponGrid"), ImVec2(0.f, 0.f), false))
    {
        ImGui::PopStyleVar(2);
        const int team = TeamFor(m_eWeaponCategory, m_iSelectedTeam);

        for (int i = 0; i < static_cast<int>(filtered.size()); ++i)
        {
            const auto* item = filtered[i];
            const int   slot = SlotFor(m_eWeaponCategory, item->m_LoadoutSlot);

            bool highlighted = false;
            if (slot >= 0)
            {
                auto* sel = pMgr->GetSelection(team, slot);
                if (sel)
                {
                    highlighted = (m_eWeaponCategory == WCAT_MUSIC)
                        ? (!item->m_DumpedSkins.empty() &&
                           sel->m_iPaintKit == item->m_DumpedSkins[0].m_ID)
                        : (sel->m_DefIdx == item->m_DefIdx);
                }
            }

            if (i % cols != 0) ImGui::SameLine(0.f, cardGap);

            if (RenderItemCard(i, item->m_DisplayName.c_str(),
                               item->m_Rarity, highlighted, cardW, cardH))
            {
                m_PendingSelection = {};
                m_PendingSelection.m_iTeam    = team;
                m_PendingSelection.m_iSlot    = slot;
                m_PendingSelection.m_DefIdx   = item->m_DefIdx;
                m_PendingSelection.m_bEquipped = true;

                if (slot >= 0)
                    if (auto* existing = pMgr->GetSelection(team, slot))
                        if (m_eWeaponCategory == WCAT_MUSIC ||
                            existing->m_DefIdx == item->m_DefIdx)
                            m_PendingSelection = *existing;

                if (m_eWeaponCategory == WCAT_MUSIC && !item->m_DumpedSkins.empty())
                    m_PendingSelection.m_iPaintKit = item->m_DumpedSkins[0].m_ID;

                const bool skipSkins = (m_eWeaponCategory == WCAT_MUSIC ||
                                        m_eWeaponCategory == WCAT_AGENTS ||
                                        item->m_DumpedSkins.empty());
                if (skipSkins)
                {
                    m_bSkippedSkinStep = true;
                    m_eAddStep = ADD_STEP_CONFIG;
                }
                else
                {
                    m_bSkippedSkinStep = false;
                    m_szSkinSearch[0] = '\0';
                    m_eAddStep = ADD_STEP_SKIN;
                }
            }
        }

        if (filtered.empty())
        {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.35f, 0.35f, 1.f));
            ImGui::TextUnformatted(XorStr("No items match the current filter."));
            ImGui::PopStyleColor();
        }
    }
    else { ImGui::PopStyleVar(2); }
    ImGui::EndChild();
}

auto CAndromedaMenu::RenderAdd_SkinPicker() -> void
{
    auto* pMgr    = GetInventoryItemsManager();
    auto& vecItems = pMgr->GetDumpedItems();

    const CInventoryItemsManager::DumpedItem_t* pSelItem = nullptr;
    for (const auto& item : vecItems)
        if (item.m_DefIdx == m_PendingSelection.m_DefIdx) { pSelItem = &item; break; }

    {
        if (RenderBackButton()) { m_eAddStep = ADD_STEP_WEAPON; return; }

        ImGui::SameLine(0.f, 12.f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.70f, 0.70f, 0.70f, 1.f));
        const std::string header = std::string(pSelItem ? pSelItem->m_DisplayName : "Unknown")
            + XorStr("  —  Choose Skin");
        ImGui::TextUnformatted(header.c_str());
        ImGui::PopStyleColor();

        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 210.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.f);
        ImGui::SetNextItemWidth(190.f);
        ImGui::InputText(XorStr("##SkinSearch"), m_szSkinSearch, sizeof(m_szSkinSearch));
        ImGui::PopStyleVar();
    }

    ImGui::Spacing();
    if (!pSelItem) return;

    struct SkinEntry { int id; int rarity; const char* name; };
    std::vector<SkinEntry> skins;
    skins.reserve(pSelItem->m_DumpedSkins.size() + 1);

    const std::string sLow = ToLower(m_szSkinSearch);
    if (sLow.empty() || std::string("default").find(sLow) != std::string::npos)
        skins.push_back({ 0, pSelItem->m_Rarity, "Default" });

    for (const auto& sk : pSelItem->m_DumpedSkins)
    {
        if (!sLow.empty() && ToLower(sk.m_DisplayName).find(sLow) == std::string::npos)
            continue;
        skins.push_back({ sk.m_ID,
                          sk.m_Rarity > 0 ? sk.m_Rarity : pSelItem->m_Rarity,
                          sk.m_DisplayName.c_str() });
    }

    const float avW     = ImGui::GetContentRegionAvail().x;
    const float cardH   = 68.f;
    const float cardGap = 8.f;
    const int   cols    = std::max(1, (int)((avW + cardGap) / (160.f + cardGap)));
    const float cardW   = (avW - (cols - 1) * cardGap) / static_cast<float>(cols);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(0.f, cardGap));

    if (ImGui::BeginChild(XorStr("##SkinGrid"), ImVec2(0.f, 0.f), false))
    {
        ImGui::PopStyleVar(2);
        for (int i = 0; i < static_cast<int>(skins.size()); ++i)
        {
            const bool highlighted = (skins[i].id == m_PendingSelection.m_iPaintKit);
            if (i % cols != 0) ImGui::SameLine(0.f, cardGap);
            if (RenderItemCard(i, skins[i].name, skins[i].rarity,
                               highlighted, cardW, cardH))
            {
                m_PendingSelection.m_iPaintKit = skins[i].id;
                m_eAddStep = ADD_STEP_CONFIG;
            }
        }
        if (skins.empty())
        {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.35f, 0.35f, 1.f));
            ImGui::TextUnformatted(XorStr("No skins match the current filter."));
            ImGui::PopStyleColor();
        }
    }
    else { ImGui::PopStyleVar(2); }
    ImGui::EndChild();
}

auto CAndromedaMenu::RenderAdd_ItemConfig() -> void
{
    auto* pMgr    = GetInventoryItemsManager();
    auto& vecItems = pMgr->GetDumpedItems();
    auto& sel      = m_PendingSelection;

    const CInventoryItemsManager::DumpedItem_t* pSelItem = nullptr;
    for (const auto& item : vecItems)
        if (item.m_DefIdx == sel.m_DefIdx) { pSelItem = &item; break; }

    std::string skinName;
    if (sel.m_iPaintKit == 0)
        skinName = "Default";
    else if (pSelItem)
    {
        for (const auto& sk : pSelItem->m_DumpedSkins)
            if (sk.m_ID == sel.m_iPaintKit) { skinName = sk.m_DisplayName; break; }
        if (skinName.empty())
            skinName = pMgr->GetSkinName(sel.m_iPaintKit);
    }

    const bool bIsMusic    = (m_eWeaponCategory == WCAT_MUSIC);
    const bool bIsAgent    = (m_eWeaponCategory == WCAT_AGENTS);
    const bool bHasWear    = (!bIsMusic && !bIsAgent);
    const bool bHasSkins   = (!bIsMusic && !bIsAgent);
    const bool bHasStickers = (m_eWeaponCategory == WCAT_WEAPONS ||
                                m_eWeaponCategory == WCAT_KNIVES);

    {
        if (RenderBackButton())
        {
            m_eAddStep = m_bSkippedSkinStep ? ADD_STEP_WEAPON : ADD_STEP_SKIN;
            return;
        }
        ImGui::SameLine(0.f, 12.f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.70f, 0.70f, 0.70f, 1.f));
        const std::string header = (pSelItem ? pSelItem->m_DisplayName : "Unknown")
            + (bHasSkins ? ("  |  " + skinName) : "");
        ImGui::TextUnformatted(header.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const float fieldW = ImGui::GetContentRegionAvail().x;

    if (bHasWear)
    {
        ImGui::AlignTextToFramePadding();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.55f, 1.f));
        ImGui::TextUnformatted(XorStr("Wear"));
        ImGui::PopStyleColor();
        ImGui::SameLine(80.f);

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.f);
        ImGui::SetNextItemWidth(fieldW - 80.f - 110.f - 10.f);
        ImGui::SliderFloat(XorStr("##Wear"), &sel.m_flWear, 0.f, 1.f, "%.4f");
        ImGui::PopStyleVar();

        ImGui::SameLine(0.f, 10.f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.45f, 0.45f, 1.f));
        ImGui::TextUnformatted(GetWearTierName(sel.m_flWear));
        ImGui::PopStyleColor();

        {
            ImDrawList* dl  = ImGui::GetWindowDrawList();
            const float barH = 5.f;
            const float barX = ImGui::GetCursorScreenPos().x + 80.f;
            const float barY = ImGui::GetCursorScreenPos().y + 4.f;
            const float barW = fieldW - 80.f - 110.f - 10.f;
            RenderWearBar(dl, ImVec2(barX, barY), barW, barH, sel.m_flWear);
            ImGui::Dummy(ImVec2(0.f, barH + 10.f));
        }

        ImGui::AlignTextToFramePadding();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.55f, 1.f));
        ImGui::TextUnformatted(XorStr("Seed"));
        ImGui::PopStyleColor();
        ImGui::SameLine(80.f);
        ImGui::SetNextItemWidth(120.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.f);
        ImGui::InputInt(XorStr("##Seed"), &sel.m_iSeed, 1, 10);
        if (sel.m_iSeed < 0) sel.m_iSeed = 0;
        ImGui::PopStyleVar();

        ImGui::Spacing();

        ImGui::AlignTextToFramePadding();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.55f, 1.f));
        ImGui::TextUnformatted(XorStr("StatTrak"));
        ImGui::PopStyleColor();
        ImGui::SameLine(80.f);
        ImGui::SetNextItemWidth(120.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.f);
        ImGui::InputInt(XorStr("##StatTrak"), &sel.m_iStatTrak, 1, 100);
        if (sel.m_iStatTrak < -1) sel.m_iStatTrak = -1;
        ImGui::PopStyleVar();
        ImGui::SameLine(0.f, 10.f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.32f, 0.32f, 0.32f, 1.f));
        ImGui::TextUnformatted(XorStr("(-1 = disabled)"));
        ImGui::PopStyleColor();

        ImGui::Spacing();
    }

    ImGui::AlignTextToFramePadding();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.55f, 1.f));
    ImGui::TextUnformatted(XorStr("Equipped"));
    ImGui::PopStyleColor();
    ImGui::SameLine(80.f);
    ImGui::Checkbox(XorStr("##Equipped"), &sel.m_bEquipped);

    if (bHasStickers)
    {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        RenderSectionLabel("STICKERS");
        ImGui::Spacing();

        const float slotSz  = 64.f;
        const float slotGap = 8.f;
        auto& stickerKits   = pMgr->GetDumpedStickerKits();

        ImGui::PushID(XorStr("##StickerSlots"));
        for (int s = 0; s < 5; ++s)
        {
            if (s > 0) ImGui::SameLine(0.f, slotGap);
            ImGui::PushID(s);

            const int kitID  = sel.m_Stickers[s].m_iKitID;
            const bool hasKit = (kitID > 0);

            const CInventoryItemsManager::DumpedStickerKit_t* pKit = nullptr;
            if (hasKit)
                for (const auto& k : stickerKits)
                    if (k.m_ID == kitID) { pKit = &k; break; }

            ImGui::PushStyleColor(ImGuiCol_Button,
                hasKit ? ImVec4(22/255.f, 22/255.f, 22/255.f, 1.f)
                       : ImVec4(14/255.f, 14/255.f, 14/255.f, 1.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                hasKit ? ImVec4(30/255.f, 30/255.f, 30/255.f, 1.f)
                       : ImVec4(22/255.f, 22/255.f, 22/255.f, 1.f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f);

            bool clicked = ImGui::Button(XorStr("##slot"), ImVec2(slotSz, slotSz));

            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);

            {
                ImDrawList* dl  = ImGui::GetWindowDrawList();
                ImVec2 bMin     = ImGui::GetItemRectMin();
                ImVec2 bMax     = ImGui::GetItemRectMax();
                const float cX  = (bMin.x + bMax.x) * 0.5f;
                const float cY  = (bMin.y + bMax.y) * 0.5f;
                const float th  = ImGui::GetTextLineHeight();

                char numBuf[4];
                snprintf(numBuf, sizeof(numBuf), "%d", s + 1);
                dl->AddText(ImVec2(bMin.x + 5.f, bMin.y + 4.f), kColTextDim, numBuf);

                dl->AddRect(bMin, bMax, kColBorder, 6.f, 0, 1.f);

                if (hasKit && pKit)
                {
                    dl->AddCircleFilled(ImVec2(cX, cY - th * 0.5f - 4.f),
                                        3.f, pMgr->GetRarityColor(pKit->m_Rarity));
                    std::string nm = pKit->m_DisplayName;
                    while (nm.size() > 2 &&
                           ImGui::CalcTextSize(nm.c_str()).x > slotSz - 8.f)
                        nm.pop_back();
                    if (nm != pKit->m_DisplayName) nm += "\xe2\x80\xa6";
                    const float nW = ImGui::CalcTextSize(nm.c_str()).x;
                    dl->AddText(ImVec2(cX - nW * 0.5f, cY + th * 0.1f),
                                IM_COL32(200, 200, 200, 255), nm.c_str());
                }
                else
                {
                    const float plusW = ImGui::CalcTextSize("+").x;
                    dl->AddText(ImVec2(cX - plusW * 0.5f, cY - th * 0.5f),
                                kColTextDim, "+");
                }
            }

            if (clicked)
            {
                m_iActiveStickerSlot = s;
                m_szStickerSearch[0] = '\0';
                m_eAddStep = ADD_STEP_STICKER;
            }

            if (hasKit)
            {
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.f, 8.f));
                if (ImGui::BeginPopupContextItem(XorStr("##StkCtx")))
                {
                    if (ImGui::MenuItem(XorStr("Remove Sticker")))
                    {
                        sel.m_Stickers[s].m_iKitID = 0;
                        sel.m_Stickers[s].m_flWear  = 0.f;
                    }
                    ImGui::EndPopup();
                }
                ImGui::PopStyleVar();
            }
            ImGui::PopID();
        }
        ImGui::PopID();

        bool anyOccupied = false;
        for (int s = 0; s < 5; ++s)
            if (sel.m_Stickers[s].m_iKitID > 0) { anyOccupied = true; break; }

        if (anyOccupied)
        {
            ImGui::Spacing();
            for (int s = 0; s < 5; ++s)
            {
                if (sel.m_Stickers[s].m_iKitID == 0) continue;

                const CInventoryItemsManager::DumpedStickerKit_t* pKit = nullptr;
                for (const auto& k : stickerKits)
                    if (k.m_ID == sel.m_Stickers[s].m_iKitID) { pKit = &k; break; }

                ImGui::PushID(200 + s);

                char sLabel[64];
                snprintf(sLabel, sizeof(sLabel), "Slot %d — %s", s + 1,
                         pKit ? pKit->m_DisplayName.c_str() : "?");

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.45f, 0.45f, 1.f));
                ImGui::TextUnformatted(sLabel);
                ImGui::PopStyleColor();
                ImGui::SameLine(240.f);
                ImGui::SetNextItemWidth(160.f);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.f);
                ImGui::SliderFloat(XorStr("##StkWear"), &sel.m_Stickers[s].m_flWear,
                                   0.f, 1.f, "Scratch %.2f");
                ImGui::PopStyleVar();
                ImGui::PopID();
            }
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const float btnW = (fieldW - 8.f) * 0.5f;

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,  4.f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
    ImGui::PushStyleColor(ImGuiCol_Border,        ImVec4(58/255.f, 58/255.f, 58/255.f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(22/255.f, 22/255.f, 22/255.f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(32/255.f, 32/255.f, 32/255.f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(42/255.f, 42/255.f, 42/255.f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.82f, 0.82f, 0.82f, 1.f));

    if (ImGui::Button(XorStr("Apply"), ImVec2(btnW, 32.f)))
    {
        auto& stored = pMgr->FindOrCreateSelection(sel.m_iTeam, sel.m_iSlot);
        stored = sel;
        pMgr->ApplyLoadoutFromConfig();
        m_eAddStep = ADD_STEP_WEAPON;
    }

    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar(2);

    ImGui::SameLine(0.f, 8.f);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.f);
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(14/255.f, 14/255.f, 14/255.f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(22/255.f, 22/255.f, 22/255.f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.45f, 0.45f, 0.45f, 1.f));
    if (ImGui::Button(XorStr("Clear Slot"), ImVec2(-1.f, 32.f)))
    {
        pMgr->RemoveSelection(sel.m_iTeam, sel.m_iSlot);
        pMgr->ApplyLoadoutFromConfig();
        m_eAddStep = ADD_STEP_WEAPON;
    }
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();
}

auto CAndromedaMenu::RenderAdd_StickerPicker() -> void
{
    auto* pMgr        = GetInventoryItemsManager();
    auto& stickerKits  = pMgr->GetDumpedStickerKits();

    {
        if (RenderBackButton()) { m_eAddStep = ADD_STEP_CONFIG; return; }

        ImGui::SameLine(0.f, 12.f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.70f, 0.70f, 0.70f, 1.f));
        char hdr[64];
        snprintf(hdr, sizeof(hdr), XorStr("Choose Sticker  —  Slot %d"),
                 m_iActiveStickerSlot + 1);
        ImGui::TextUnformatted(hdr);
        ImGui::PopStyleColor();

        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 210.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.f);
        ImGui::SetNextItemWidth(190.f);
        ImGui::InputText(XorStr("##StkSearch"), m_szStickerSearch, sizeof(m_szStickerSearch));
        ImGui::PopStyleVar();
    }

    ImGui::Spacing();

    const std::string sLow = ToLower(m_szStickerSearch);
    std::vector<const CInventoryItemsManager::DumpedStickerKit_t*> filtered;
    filtered.reserve(stickerKits.size());
    for (const auto& kit : stickerKits)
    {
        if (!sLow.empty() &&
            ToLower(kit.m_DisplayName).find(sLow) == std::string::npos)
            continue;
        filtered.push_back(&kit);
    }

    const float avW     = ImGui::GetContentRegionAvail().x;
    const float cardH   = 64.f;
    const float cardGap = 8.f;
    const int   cols    = std::max(1, (int)((avW + cardGap) / (160.f + cardGap)));
    const float cardW   = (avW - (cols - 1) * cardGap) / static_cast<float>(cols);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(0.f, cardGap));

    if (ImGui::BeginChild(XorStr("##StickerGrid"), ImVec2(0.f, 0.f), false))
    {
        ImGui::PopStyleVar(2);
        const int activeSlot = m_iActiveStickerSlot;

        for (int i = 0; i < static_cast<int>(filtered.size()); ++i)
        {
            const auto* kit = filtered[i];
            const bool highlighted = (activeSlot >= 0 &&
                m_PendingSelection.m_Stickers[activeSlot].m_iKitID == kit->m_ID);

            if (i % cols != 0) ImGui::SameLine(0.f, cardGap);

            if (RenderItemCard(i, kit->m_DisplayName.c_str(),
                               kit->m_Rarity, highlighted, cardW, cardH))
            {
                if (activeSlot >= 0 && activeSlot < 5)
                {
                    m_PendingSelection.m_Stickers[activeSlot].m_iKitID = kit->m_ID;
                    m_PendingSelection.m_Stickers[activeSlot].m_flWear  = 0.f;
                }
                m_eAddStep = ADD_STEP_CONFIG;
            }
        }

        if (filtered.empty())
        {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.35f, 0.35f, 1.f));
            ImGui::TextUnformatted(XorStr("No stickers match the current filter."));
            ImGui::PopStyleColor();
        }
    }
    else { ImGui::PopStyleVar(2); }
    ImGui::EndChild();
}

auto CAndromedaMenu::RenderTab_Loadout() -> void
{
    auto* pMgr = GetInventoryItemsManager();

    {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(12.f, 5.f));

        auto TeamBtn = [&](const char* label, int team, const ImVec4& activeBg, const ImVec4& activeText)
        {
            const bool active = (m_iLoadoutViewTeam == team);
            ImGui::PushStyleColor(ImGuiCol_Button,
                active ? activeBg : ImVec4(0.10f, 0.10f, 0.10f, 1.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, activeBg);
            ImGui::PushStyleColor(ImGuiCol_Text,
                active ? activeText : ImVec4(0.38f, 0.38f, 0.38f, 1.f));
            if (ImGui::Button(label)) m_iLoadoutViewTeam = team;
            ImGui::PopStyleColor(3);
        };

        TeamBtn(XorStr(" T-Side ##LT"), 2,
                ImVec4(0.18f, 0.14f, 0.05f, 1.f),
                ImVec4(0.85f, 0.68f, 0.30f, 1.f));
        ImGui::SameLine(0.f, 4.f);
        TeamBtn(XorStr("CT-Side##LCT"), 3,
                ImVec4(0.05f, 0.10f, 0.20f, 1.f),
                ImVec4(0.40f, 0.60f, 0.90f, 1.f));
        ImGui::SameLine(0.f, 4.f);
        TeamBtn(XorStr("Both##LB"), 0,
                ImVec4(0.16f, 0.16f, 0.16f, 1.f),
                ImVec4(0.80f, 0.80f, 0.80f, 1.f));

        const float rightBtnX = ImGui::GetContentRegionAvail().x
            + ImGui::GetCursorPosX() - 240.f;
        ImGui::SameLine(rightBtnX, 0.f);

        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
        ImGui::PushStyleColor(ImGuiCol_Border,        ImVec4(55/255.f, 55/255.f, 55/255.f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(22/255.f, 22/255.f, 22/255.f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(32/255.f, 32/255.f, 32/255.f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.80f, 0.80f, 0.80f, 1.f));
        if (ImGui::Button(XorStr("Apply All")))
            pMgr->ApplyLoadoutFromConfig();
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar();

        ImGui::SameLine(0.f, 6.f);

        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(14/255.f, 14/255.f, 14/255.f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(22/255.f, 22/255.f, 22/255.f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.40f, 0.40f, 0.40f, 1.f));
        if (ImGui::Button(XorStr("Refresh Schema")))
            pMgr->ForceRescanCatalogue();
        ImGui::PopStyleColor(3);

        ImGui::PopStyleVar(2);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(10.f, 7.f));

    if (ImGui::BeginTable(XorStr("##LoadoutTable"), 6,
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersInnerH |
        ImGuiTableFlags_SizingFixedFit |
        ImGuiTableFlags_ScrollY,
        ImVec2(0.f, 0.f)))
    {
        ImGui::TableSetupColumn(XorStr(""),        ImGuiTableColumnFlags_WidthFixed,   6.f);
        ImGui::TableSetupColumn(XorStr("Weapon"),  ImGuiTableColumnFlags_WidthFixed, 140.f);
        ImGui::TableSetupColumn(XorStr("Skin"),    ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn(XorStr("Wear"),    ImGuiTableColumnFlags_WidthFixed, 100.f);
        ImGui::TableSetupColumn(XorStr("Stickers"),ImGuiTableColumnFlags_WidthFixed,  70.f);
        ImGui::TableSetupColumn(XorStr(""),        ImGuiTableColumnFlags_WidthFixed,  32.f);
        ImGui::TableHeadersRow();

        auto& vecSel = Settings::Inventory::m_vecSelections;
        int deleteIdx = -1;

        for (int idx = 0; idx < static_cast<int>(vecSel.size()); ++idx)
        {
            const auto& s = vecSel[idx];
            if (m_iLoadoutViewTeam != 0 && s.m_iTeam != 0 &&
                s.m_iTeam != m_iLoadoutViewTeam)
                continue;

            int rarity = 0;
            for (const auto& item : pMgr->GetDumpedItems())
            {
                if (item.m_DefIdx == s.m_DefIdx)
                {
                    rarity = item.m_Rarity;
                    if (s.m_iPaintKit != 0)
                        for (const auto& sk : item.m_DumpedSkins)
                            if (sk.m_ID == s.m_iPaintKit)
                            { rarity = sk.m_Rarity > 0 ? sk.m_Rarity : rarity; break; }
                    break;
                }
            }

            ImGui::PushID(idx);
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            {
                ImDrawList* dl = ImGui::GetWindowDrawList();
                ImVec2 p0  = ImGui::GetCursorScreenPos();
                float  rowH = ImGui::GetTextLineHeightWithSpacing()
                    + ImGui::GetStyle().CellPadding.y * 2.f;
                dl->AddRectFilled(ImVec2(p0.x, p0.y + 3.f),
                                  ImVec2(p0.x + 3.f, p0.y + rowH - 3.f),
                                  pMgr->GetRarityColor(rarity), 2.f);
            }

            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(pMgr->GetWeaponName(s.m_DefIdx).c_str());

            ImGui::TableSetColumnIndex(2);
            if (s.m_iPaintKit == 0)
                ImGui::TextDisabled(XorStr("Default"));
            else
                ImGui::TextUnformatted(pMgr->GetSkinName(s.m_iPaintKit).c_str());

            ImGui::TableSetColumnIndex(3);
            if (s.m_flWear > 0.f)
            {
                const auto wCol = [&]() -> ImU32 {
                    if (s.m_flWear < 0.07f) return IM_COL32(100, 180, 255, 255);
                    if (s.m_flWear < 0.15f) return IM_COL32( 80, 220, 190, 255);
                    if (s.m_flWear < 0.38f) return IM_COL32(200, 200,  50, 255);
                    if (s.m_flWear < 0.45f) return IM_COL32(240, 140,  40, 255);
                    return IM_COL32(220, 60, 60, 255);
                }();
                ImGui::TextColored(ImColor(wCol).Value, "%.4f", s.m_flWear);
            }
            else { ImGui::TextDisabled("\xe2\x80\x94"); }

            ImGui::TableSetColumnIndex(4);
            {
                int cnt = 0;
                for (const auto& stk : s.m_Stickers)
                    if (stk.m_iKitID > 0) ++cnt;
                if (cnt > 0) ImGui::Text("%d/5", cnt);
                else         ImGui::TextDisabled("\xe2\x80\x94");
            }

            ImGui::TableSetColumnIndex(5);
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.f, 0.f, 0.f, 0.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.18f, 0.18f, 0.15f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.95f, 0.18f, 0.18f, 0.30f));
            ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.70f, 0.28f, 0.28f, 1.f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.f);
            if (ImGui::Button(XorStr("\xc3\x97##del"), ImVec2(28.f, 0.f)))
                deleteIdx = idx;
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(4);

            ImGui::PopID();
        }

        if (deleteIdx >= 0)
        {
            const auto& s = vecSel[deleteIdx];
            pMgr->RemoveSelection(s.m_iTeam, s.m_iSlot);
            pMgr->ApplyLoadoutFromConfig();
        }

        if (vecSel.empty())
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(1);
            ImGui::TextDisabled(XorStr("No items configured. Use the Add tab."));
        }

        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}

auto CAndromedaMenu::RenderTab_Settings() -> void
{
    auto* pSettings = GetSettingsJson();
    const float totalW = ImGui::GetContentRegionAvail().x;
    const float colW   = (totalW - 20.f) * 0.5f;

    ImGui::BeginGroup();
    {
        RenderSectionLabel("CONFIG");
        ImGui::Spacing();

        if (!m_bConfigListInit)
        {
            pSettings->UpdateConfigList();
            m_bConfigListInit = true;
        }

        auto& vecConfigs = pSettings->GetConfigList();

        {
            std::vector<const char*> names;
            names.reserve(vecConfigs.size());
            for (const auto& n : vecConfigs) names.push_back(n.c_str());

            if (!names.empty())
            {
                if (m_iSelectedConfigIndex >= static_cast<int>(names.size()))
                    m_iSelectedConfigIndex = 0;

                ImGui::AlignTextToFramePadding();
                ImGui::TextDisabled(XorStr("File"));
                ImGui::SameLine(60.f);
                ImGui::PushItemWidth(colW - 60.f);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.f);
                ImGui::Combo(XorStr("##ConfigList"), &m_iSelectedConfigIndex,
                             names.data(), static_cast<int>(names.size()));
                ImGui::PopStyleVar();
                ImGui::PopItemWidth();
            }
            else
            {
                ImGui::TextDisabled(XorStr("No saved configs found."));
            }
        }

        {
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled(XorStr("Name"));
            ImGui::SameLine(60.f);
            char buf[260] = {};
            const auto len = std::min(m_sNewConfigName.size(), sizeof(buf) - 1);
            std::memcpy(buf, m_sNewConfigName.data(), len);
            ImGui::PushItemWidth(colW - 60.f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.f);
            if (ImGui::InputText(XorStr("##NewName"), buf, sizeof(buf)))
                m_sNewConfigName = buf;
            ImGui::PopStyleVar();
            ImGui::PopItemWidth();
        }

        ImGui::Spacing();

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.f);
        const float bW = (colW - 18.f) / 4.f;
        auto& vecConfigs2 = pSettings->GetConfigList();

        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
        ImGui::PushStyleColor(ImGuiCol_Border,        ImVec4(55/255.f, 55/255.f, 55/255.f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(22/255.f, 22/255.f, 22/255.f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(32/255.f, 32/255.f, 32/255.f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.80f, 0.80f, 0.80f, 1.f));
        if (ImGui::Button(XorStr("Save"), ImVec2(bW, 30.f)))
        {
            std::string target = m_sNewConfigName;
            if (target.size() < 5 || target.compare(target.size() - 5, 5, ".json") != 0)
                target += ".json";
            pSettings->SaveConfig(target);
            pSettings->UpdateConfigList();
        }
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar();

        ImGui::SameLine(0.f, 6.f);

        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(14/255.f, 14/255.f, 14/255.f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(22/255.f, 22/255.f, 22/255.f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.55f, 0.55f, 0.55f, 1.f));
        if (ImGui::Button(XorStr("Load"), ImVec2(bW, 30.f)) &&
            !vecConfigs2.empty() &&
            m_iSelectedConfigIndex < static_cast<int>(vecConfigs2.size()))
        {
            pSettings->LoadConfig(vecConfigs2[m_iSelectedConfigIndex]);
            GetAndromedaGUI()->UpdateStyle();
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine(0.f, 6.f);

        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.14f, 0.05f, 0.05f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.07f, 0.07f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.70f, 0.28f, 0.28f, 1.f));
        if (ImGui::Button(XorStr("Delete"), ImVec2(bW, 30.f)) &&
            !vecConfigs2.empty() &&
            m_iSelectedConfigIndex < static_cast<int>(vecConfigs2.size()))
        {
            pSettings->DeleteConfig(vecConfigs2[m_iSelectedConfigIndex]);
            pSettings->UpdateConfigList();
            if (m_iSelectedConfigIndex >= static_cast<int>(vecConfigs2.size()))
                m_iSelectedConfigIndex = 0;
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine(0.f, 6.f);

        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(14/255.f, 14/255.f, 14/255.f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(22/255.f, 22/255.f, 22/255.f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.40f, 0.40f, 0.40f, 1.f));
        if (ImGui::Button(XorStr("Refresh"), ImVec2(bW, 30.f)))
            pSettings->UpdateConfigList();
        ImGui::PopStyleColor(3);

        ImGui::PopStyleVar();

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.30f, 0.30f, 0.30f, 1.f));
        ImGui::Text(XorStr("Last loaded: %s"),
                    Settings::Inventory::m_sLastLoadedConfig.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::EndGroup();
}

auto CAndromedaMenu::RenderBackButton(const char* label) -> bool
{
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.f, 1.f, 1.f, 0.04f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1.f, 1.f, 1.f, 0.08f));
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.55f, 0.55f, 0.55f, 1.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(10.f, 5.f));

    const char* btnLabel = label ? label : "\xe2\x86\x90  Back";
    bool clicked = ImGui::Button(btnLabel);

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
    return clicked;
}

auto CAndromedaMenu::RenderItemCard(
    int idx, const char* name, int rarity,
    bool highlighted, float cardW, float cardH) -> bool
{
    auto* pMgr = GetInventoryItemsManager();

    ImGui::PushID(idx);
    bool clicked = ImGui::InvisibleButton(XorStr("##card"), ImVec2(cardW, cardH));
    const bool hovered = ImGui::IsItemHovered();
    ImGui::PopID();

    ImDrawList* dl   = ImGui::GetWindowDrawList();
    ImVec2      pMin = ImGui::GetItemRectMin();
    ImVec2      pMax = ImGui::GetItemRectMax();

    const ImU32 bgCol = highlighted ? kColRaised
                      : hovered     ? IM_COL32(24, 24, 24, 255)
                                    : kColSurface;
    dl->AddRectFilled(pMin, pMax, bgCol, 4.f);

    const ImU32 borderCol = highlighted ? kColAccent
                          : hovered     ? IM_COL32(65, 65, 65, 255)
                                        : kColBorder;
    dl->AddRect(pMin, pMax, borderCol, 4.f, 0, highlighted ? 1.2f : 1.f);

    dl->AddRectFilled(pMin,
                      ImVec2(pMax.x, pMin.y + 2.f),
                      pMgr->GetRarityColor(rarity),
                      4.f, ImDrawFlags_RoundCornersTop);

    {
        const float pad  = 10.f;
        const float maxW = cardW - pad * 2.f;
        std::string display = name;
        while (display.size() > 2 &&
               ImGui::CalcTextSize(display.c_str()).x > maxW)
            display.pop_back();
        if (display != std::string(name)) display += "\xe2\x80\xa6";

        const float tw = ImGui::CalcTextSize(display.c_str()).x;
        const float th = ImGui::GetTextLineHeight();
        const float tx = pMin.x + (cardW - tw) * 0.5f;
        const float ty = pMin.y + 2.f + (cardH - 2.f - th) * 0.5f;

        const ImU32 textCol = highlighted ? IM_COL32(225, 225, 225, 255)
                            : hovered     ? IM_COL32(175, 175, 175, 255)
                                          : IM_COL32(110, 110, 110, 255);
        dl->AddText(ImVec2(tx, ty), textCol, display.c_str());
    }

    return clicked;
}

auto CAndromedaMenu::RenderWearBar(
    ImDrawList* dl, ImVec2 pos, float width, float height, float wear) -> void
{
    struct Seg { float t0, t1; ImU32 c0, c1; };
    static const Seg kSegs[] =
    {
        { 0.00f, 0.07f, IM_COL32(100,180,255,255), IM_COL32( 80,220,190,255) },
        { 0.07f, 0.15f, IM_COL32( 80,220,190,255), IM_COL32(120,220, 80,255) },
        { 0.15f, 0.38f, IM_COL32(120,220, 80,255), IM_COL32(240,200, 40,255) },
        { 0.38f, 0.45f, IM_COL32(240,200, 40,255), IM_COL32(240,120, 40,255) },
        { 0.45f, 1.00f, IM_COL32(240,120, 40,255), IM_COL32(220, 50, 50,255) },
    };

    dl->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height),
                      IM_COL32(18, 18, 18, 255), height * 0.5f);

    for (const auto& s : kSegs)
        dl->AddRectFilledMultiColor(
            ImVec2(pos.x + s.t0 * width, pos.y),
            ImVec2(pos.x + s.t1 * width, pos.y + height),
            s.c0, s.c1, s.c1, s.c0);

    const float mx = pos.x + std::clamp(wear, 0.f, 1.f) * width;
    dl->AddRectFilled(ImVec2(mx - 1.5f, pos.y - 3.f),
                      ImVec2(mx + 1.5f, pos.y + height + 3.f),
                      IM_COL32(240, 240, 240, 220), 1.f);
}

auto CAndromedaMenu::RenderCheckBox(
    const char* szTitle, const char* szStrID, bool& v) -> bool
{
    if (szTitle)
    {
        ImGui::AlignTextToFramePadding();
        ImGui::Text(szTitle);
        ImGui::SameLine(ImGui::CalcTextSize(szTitle).x + 10.f);
    }
    ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x - 27.f -
        ImGui::GetStyle().FramePadding.x, 0.f));
    ImGui::SameLine();
    return ImGui::Checkbox(szStrID, &v);
}

auto CAndromedaMenu::RenderComboBox(
    const char* szTitle, const char* szStrID, int& v,
    const char* Items[], int ItemsCount) -> bool
{
    if (szTitle) { ImGui::AlignTextToFramePadding(); ImGui::Text(szTitle); ImGui::SameLine(); }
    ImGui::PushItemWidth(-1.f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.f);
    const bool r = ImGui::Combo(szStrID, &v, Items, ItemsCount);
    ImGui::PopStyleVar();
    ImGui::PopItemWidth();
    return r;
}

auto CAndromedaMenu::RenderColorEdit(
    const char* szTitle, const char* szStrID, float* Color) -> bool
{
    if (szTitle) { ImGui::AlignTextToFramePadding(); ImGui::Text(szTitle); ImGui::SameLine(); }
    return ImGui::ColorEdit4(szStrID, Color);
}

auto CAndromedaMenu::RenderSliderInt(
    const char* szTitle, const char* szStrID,
    int& v, int Min, int Max) -> bool
{
    if (szTitle) { ImGui::AlignTextToFramePadding(); ImGui::Text(szTitle); ImGui::SameLine(); }
    ImGui::PushItemWidth(-1.f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.f);
    const bool r = ImGui::SliderInt(szStrID, &v, Min, Max);
    ImGui::PopStyleVar();
    ImGui::PopItemWidth();
    return r;
}

auto CAndromedaMenu::RenderSliderFloat(
    const char* szTitle, const char* szStrID,
    float& v, float Min, float Max) -> bool
{
    if (szTitle) { ImGui::AlignTextToFramePadding(); ImGui::Text(szTitle); ImGui::SameLine(); }
    ImGui::PushItemWidth(-1.f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.f);
    const bool r = ImGui::SliderFloat(szStrID, &v, Min, Max, "%.3f");
    ImGui::PopStyleVar();
    ImGui::PopItemWidth();
    return r;
}

auto CAndromedaMenu::RenderInputInt(
    const char* szTitle, const char* szStrID, int& v) -> bool
{
    if (szTitle) { ImGui::AlignTextToFramePadding(); ImGui::Text(szTitle); ImGui::SameLine(); }
    ImGui::PushItemWidth(-1.f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.f);
    const bool r = ImGui::InputInt(szStrID, &v, 1, 100);
    ImGui::PopStyleVar();
    ImGui::PopItemWidth();
    return r;
}

auto CAndromedaMenu::RenderInputText(
    const char* szTitle, const char* szStrID, std::string& v) -> bool
{
    if (szTitle) { ImGui::AlignTextToFramePadding(); ImGui::Text(szTitle); ImGui::SameLine(); }
    char buf[260] = {};
    const auto len = std::min(v.size(), sizeof(buf) - 1);
    std::memcpy(buf, v.data(), len);
    ImGui::PushItemWidth(-1.f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.f);
    const bool r = ImGui::InputText(szStrID, buf, sizeof(buf));
    ImGui::PopStyleVar();
    ImGui::PopItemWidth();
    if (r) v = buf;
    return r;
}

auto GetAndromedaMenu() -> CAndromedaMenu*
{
    return &g_CAndromedaMenu;
}
