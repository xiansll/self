#include "menu.h"
#include "../config/config.h"

#include <iostream>
#include <vector>
#include <string>
#include "../config/configmanager.h"

#include "../keybinds/keybinds.h"

#include "../utils/logging/log.h"
#include "../features/skinchanger/econ_item_system.h"
#include "../features/skinchanger/features.h"
#include "../features/skinchanger/skinchanger.h"

void ApplyImGuiTheme() {
    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    ImVec4 primaryColor = ImVec4(0.44f, 0.23f, 0.78f, 1.0f);
    ImVec4 outlineColor = ImVec4(0.54f, 0.33f, 0.88f, 0.7f);

    colors[ImGuiCol_WindowBg] = ImVec4(0.11f, 0.11f, 0.13f, 1.0f);
    colors[ImGuiCol_Border] = ImVec4(0.30f, 0.30f, 0.30f, 1.0f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.11f, 0.11f, 0.13f, 1.0f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.15f, 0.15f, 0.18f, 1.0f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.15f, 0.15f, 0.18f, 1.0f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.11f, 0.11f, 0.13f, 1.0f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.11f, 0.11f, 0.13f, 1.0f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.11f, 0.11f, 0.13f, 1.0f);

    colors[ImGuiCol_Button] = primaryColor;
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.54f, 0.33f, 0.88f, 1.0f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.34f, 0.13f, 0.68f, 1.0f);

    colors[ImGuiCol_CheckMark] = ImVec4(0.80f, 0.50f, 1.00f, 1.0f);
    colors[ImGuiCol_SliderGrab] = primaryColor;
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.54f, 0.33f, 0.88f, 1.0f);

    colors[ImGuiCol_Header] = primaryColor;
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.54f, 0.33f, 0.88f, 1.0f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.34f, 0.13f, 0.68f, 1.0f);

    colors[ImGuiCol_Separator] = ImVec4(0.34f, 0.13f, 0.68f, 1.0f);
    colors[ImGuiCol_SeparatorHovered] = primaryColor;
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.54f, 0.33f, 0.88f, 1.0f);

    colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);

    colors[ImGuiCol_Tab] = ImVec4(0.17f, 0.17f, 0.21f, 1.0f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.44f, 0.23f, 0.78f, 0.8f);
    colors[ImGuiCol_TabActive] = primaryColor;
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.17f, 0.17f, 0.21f, 1.0f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.34f, 0.13f, 0.68f, 1.0f);

    colors[ImGuiCol_Border] = outlineColor;
    colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    style.WindowRounding = 0.0f;
    style.FrameRounding = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.GrabRounding = 0.0f;
    style.TabRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.PopupRounding = 0.0f;

    style.ItemSpacing = ImVec2(8, 4);
    style.FramePadding = ImVec2(4, 3);
    style.WindowPadding = ImVec2(8, 8);

    style.FrameBorderSize = 1.0f;
    style.TabBorderSize = 1.0f;
    style.WindowBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;

    style.GrabMinSize = 7.0f;
}

Menu::Menu() {
    activeTab = 0;
    showMenu = true;
}

void Menu::init(HWND& window, ID3D11Device* pDevice, ID3D11DeviceContext* pContext, ID3D11RenderTargetView* mainRenderTargetView) {
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;
    ImGui_ImplWin32_Init(window);
    ImGui_ImplDX11_Init(pDevice, pContext);

    ApplyImGuiTheme();

    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\arial.ttf", 16.0f);

    std::cout << "initialized menu\n";
}

void Menu::render() {
    keybind.pollInputs();
    if (showMenu) {
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar;

        ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_Once);
        ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_Once);

        ImGui::Begin("TempleWare | Internal", nullptr, window_flags);

        {
            float windowWidth = ImGui::GetWindowWidth();
            float rightTextWidth = ImGui::CalcTextSize("Updated by PasatAlexDis1").x;

            ImGui::Text("TempleWare - Internal");

            ImGui::SameLine(windowWidth - rightTextWidth - 10);
            ImGui::Text("Updated by PasatAlexDis1");
        }

        ImGui::Separator();

        const char* tabNames[] = { "Aim", "Rage", "Visuals", "Misc", "Skins", "Config" };

        if (ImGui::BeginTabBar("MainTabBar", ImGuiTabBarFlags_NoTooltip)) {
            for (int i = 0; i < 6; i++) {
                if (ImGui::BeginTabItem(tabNames[i])) {
                    activeTab = i;
                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }

        ImGui::BeginChild("ContentRegion", ImVec2(0, 0), false);

        switch (activeTab) {
        case 0:
        {
            ImGui::BeginChild("AimLeft", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f - 5, 0), true);
            ImGui::Text("General");
            ImGui::Separator();

            ImGui::Checkbox("Enable##AimBot", &Config::legit_aim.aimbot);
            ImGui::SameLine();
            ImGui::Text("Key:");
            ImGui::SameLine();
            keybind.menuButton(Config::legit_aim.aimbot);

            ImGui::Checkbox("Team Check", &Config::legit_aim.team_check);
            ImGui::SliderFloat("FOV", &Config::legit_aim.aimbot_fov, 0.f, 90.f);
            ImGui::Checkbox("Draw FOV Circle", &Config::legit_aim.fov_circle);
            if (Config::legit_aim.fov_circle) {
                ImGui::ColorEdit4("Circle Color##FovColor", (float*)&Config::legit_aim.fovCircleColor);
            }
            ImGui::Checkbox("Recoil Control", &Config::legit_aim.rcs);
            ImGui::EndChild();

            ImGui::SameLine();
            ImGui::BeginChild("AimRight", ImVec2(0, 0), true);
            ImGui::Text("TriggerBot");
            ImGui::Separator();
            ImGui::Text("No additional settings");

            ImGui::EndChild();
        }
        break;

        case 1:
        {
            ImGui::BeginChild("RageLeft", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f - 5, 0), true);
            ImGui::Text("RageBot");
            ImGui::Separator();

            

            ImGui::EndChild();

            ImGui::SameLine();
            ImGui::BeginChild("RageRight", ImVec2(0, 0), true);
            ImGui::Text("Anti-Aim");
            ImGui::Separator();

            ImGui::Checkbox("Enabled", &Config::anti_aim.enabled);

            ImGui::Checkbox("Override Pitch", &Config::anti_aim.overridePitch);
            if (Config::anti_aim.overridePitch)
                ImGui::SliderInt("##override_pitch", &Config::anti_aim.pitchAmount, -89, 89);
            ImGui::Checkbox("Override Yaw", &Config::anti_aim.overrideYaw);
            if (Config::anti_aim.overrideYaw) {
                ImGui::SliderInt("##override_yaw", &Config::anti_aim.yawAmount, 0, 180);
                ImGui::SliderInt("Jiter Amount", &Config::anti_aim.jitterAmount, 0, 180);
            }

            ImGui::EndChild();
        }
        break;

        case 2:
        {
            ImGui::BeginChild("VisualsLeft", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f - 5, 0), true);
            ImGui::Text("Player ESP");
            ImGui::Separator();

            ImGui::Checkbox("Box", &Config::visual_esp.esp);
            ImGui::SliderFloat("Thickness", &Config::visual_esp.espThickness, 1.0f, 5.0f);
            ImGui::Checkbox("Box Fill", &Config::visual_esp.espFill);
            if (Config::visual_esp.espFill) {
                ImGui::SliderFloat("Fill Opacity", &Config::visual_esp.espFillOpacity, 0.0f, 1.0f);
            }
            ImGui::ColorEdit4("ESP Color##BoxColor", (float*)&Config::visual_esp.espColor);
            ImGui::Checkbox("Team Check", &Config::visual_esp.teamCheck);
            ImGui::Checkbox("Health Bar", &Config::visual_esp.showHealth);
            ImGui::Checkbox("Name Tags", &Config::visual_esp.showNameTags);
            ImGui::Checkbox("Flashed", &Config::visual_esp.showFlashed);
            ImGui::Checkbox("Scope", &Config::visual_esp.showScoped);

            ImGui::Checkbox("Skeleton ESP", &Config::visual_esp.espSkeleton);
            if (Config::visual_esp.espSkeleton)
            {
                ImGui::SliderFloat("Skeleton Thickness", &Config::visual_esp.skeletonThickness, 1.f, 5.f);
                ImGui::ColorEdit4("Skeleton Color", (float*)&Config::visual_esp.skeletonColor);
            }

            ImGui::Spacing();
            ImGui::Text("World");
            ImGui::Separator();

            ImGui::Checkbox("Night Mode", &Config::visual_world.Night);
            if (Config::visual_world.Night) {
                ImGui::ColorEdit4("Night Color", (float*)&Config::visual_world.NightColor);
            }

            ImGui::Checkbox("Custom FOV", &Config::visual_world.fovEnabled);
            if (Config::visual_world.fovEnabled) {
                ImGui::SliderFloat("FOV Value##FovSlider", &Config::visual_world.fov, 20.0f, 160.0f, "%1.0f");
            }

            ImGui::Checkbox("Spectator List", &Config::visual_world.spectatorList);

            ImGui::EndChild();

            ImGui::SameLine();
            ImGui::BeginChild("VisualsRight", ImVec2(0, 0), true);
            ImGui::Text("Chams");
            ImGui::Separator();

            ImGui::Checkbox("Chams##ChamsCheckbox", &Config::visual_chams.enemyChams);
            const char* chamsMaterials[] = { "Flat", "Illuminate", "Glow" };
            ImGui::Combo("Material", &Config::visual_chams.chamsMaterial, chamsMaterials, IM_ARRAYSIZE(chamsMaterials));
            if (Config::visual_chams.enemyChams) {
                ImGui::ColorEdit4("Chams Color##ChamsColor", (float*)&Config::visual_chams.colVisualChams);
            }
            ImGui::Checkbox("Chams-XQZ", &Config::visual_chams.enemyChamsInvisible);
            if (Config::visual_chams.enemyChamsInvisible) {
                ImGui::ColorEdit4("XQZ Color##ChamsXQZColor", (float*)&Config::visual_chams.colVisualChamsIgnoreZ);
            }

            ImGui::Spacing();
            ImGui::Text("Local Chams");
            ImGui::Separator();

            ImGui::Checkbox("Hand Chams", &Config::visual_chams.armChams);
            if (Config::visual_chams.armChams) {
                ImGui::ColorEdit4("Hand Color##HandChamsColor", (float*)&Config::visual_chams.colArmChams);
            }
            ImGui::Checkbox("Viewmodel Chams", &Config::visual_chams.viewmodelChams);
            if (Config::visual_chams.viewmodelChams) {
                ImGui::ColorEdit4("Viewmodel Color##ViewModelChamsColor", (float*)&Config::visual_chams.colViewmodelChams);
            }

            ImGui::Checkbox("Local Chams", &Config::visual_chams.localChams);
            if (Config::visual_chams.localChams)
            {
                ImGui::ColorEdit4("Local Color", (float*)&Config::visual_chams.localcolChams);
            }

            ImGui::Spacing();
            ImGui::Text("Removals");
            ImGui::Separator();

            ImGui::Checkbox("Anti Flash", &Config::visual_world.antiflash);

            ImGui::EndChild();
        }
        break;

        case 3:
        {
            ImGui::BeginChild("MiscLeft", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f - 5, 0), true);
            ImGui::Text("Movement");
            ImGui::Separator();

            ImGui::Checkbox("Bhop", &Config::movement.bhop);

            ImGui::EndChild();

            ImGui::SameLine();
            ImGui::BeginChild("MiscRight", ImVec2(0, 0), true);
            ImGui::Text("Other");
            ImGui::Separator();

            ImGui::Text("No additional settings");

            ImGui::EndChild();
        }
        break;

        case 4:
        {
            ImGui::BeginChild("SkinsLeft", ImVec2(ImGui::GetContentRegionAvail().x * 0.35f - 5, 0), true);
            ImGui::Text("Skin Changer");
            ImGui::Separator();

            ImGui::Checkbox("Enabled", &Config::skin_changer.enabled);
            ImGui::Spacing();
            
            ImGui::Text("Categories");
            ImGui::Separator();
            ImGui::Checkbox("Guns", &Config::skin_changer.guns);
            ImGui::Checkbox("Knives", &Config::skin_changer.knives);
            ImGui::Checkbox("Gloves", &Config::skin_changer.gloves);
            ImGui::Checkbox("Agents", &Config::skin_changer.agents);

            ImGui::Spacing();
            ImGui::Text("Agents");
            ImGui::Separator();

            static char ctAgentName[128] = "";
            static char tAgentName[128] = "";
            
            const auto& agents = features::skinchanger::g_econ_item_system.Agents();
            static int ctAgentIdx = -1, tAgentIdx = -1;

            ImGui::Text("CT Agent");
            if (ImGui::BeginCombo("##ctagent", ctAgentIdx >= 0 && ctAgentIdx < (int)agents.size() ? agents[ctAgentIdx]->localized_name.c_str() : "Select...")) {
                for (int i = 0; i < (int)agents.size(); i++) {
                    bool is_selected = (ctAgentIdx == i);
                    if (ImGui::Selectable(agents[i]->localized_name.c_str(), is_selected)) {
                        ctAgentIdx = i;
                        Config::skin_changer.ct_agent = agents[i]->def_index;
                    }
                    if (is_selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::Text("T Agent");
            if (ImGui::BeginCombo("##tagent", tAgentIdx >= 0 && tAgentIdx < (int)agents.size() ? agents[tAgentIdx]->localized_name.c_str() : "Select...")) {
                for (int i = 0; i < (int)agents.size(); i++) {
                    bool is_selected = (tAgentIdx == i);
                    if (ImGui::Selectable(agents[i]->localized_name.c_str(), is_selected)) {
                        tAgentIdx = i;
                        Config::skin_changer.t_agent = agents[i]->def_index;
                    }
                    if (is_selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            if (ImGui::Button("Initialize Item System")) {
                features::skinchanger::Initialize();
            }

            ImGui::EndChild();

            ImGui::SameLine();
            ImGui::BeginChild("SkinsRight", ImVec2(0, 0), true);
            
            static int currentCategory = 0; // 0=Guns, 1=Knives, 2=Gloves
            static char searchBuffer[128] = "";
            static int selectedWeaponIdx = -1;
            static int selectedCategoryWeaponIdx = -1;
            
            ImGui::Text("Weapon Skins");
            ImGui::Separator();

            // Category tabs
            if (ImGui::Button("Guns", ImVec2(ImGui::GetContentRegionAvail().x / 3 - 2, 0))) currentCategory = 0;
            ImGui::SameLine();
            if (ImGui::Button("Knives", ImVec2(ImGui::GetContentRegionAvail().x / 2 - 1, 0))) currentCategory = 1;
            ImGui::SameLine();
            if (ImGui::Button("Gloves", ImVec2(ImGui::GetContentRegionAvail().x, 0))) currentCategory = 2;

            ImGui::Spacing();
            ImGui::InputText("Search", searchBuffer, IM_ARRAYSIZE(searchBuffer));
            ImGui::Separator();

            const auto& guns = features::skinchanger::g_econ_item_system.Guns();
            const auto& knives = features::skinchanger::g_econ_item_system.Knives();
            const auto& gloves = features::skinchanger::g_econ_item_system.Gloves();
            
            const std::vector<const features::skinchanger::EconItemSystem::ItemDef*>* currentList = nullptr;
            switch (currentCategory) {
                case 0: currentList = &guns; break;
                case 1: currentList = &knives; break;
                case 2: currentList = &gloves; break;
            }

            if (currentList) {
                if (ImGui::BeginChild("WeaponList", ImVec2(0, ImGui::GetContentRegionAvail().y - 200), true)) {
                    for (int i = 0; i < (int)currentList->size(); i++) {
                        const auto* def = (*currentList)[i];
                        if (searchBuffer[0] && std::string(def->localized_name).find(searchBuffer) == std::string::npos)
                            continue;

                        auto it = Config::skin_changer.skins.find(def->def_index);
                        bool hasSkin = (it != Config::skin_changer.skins.end());
                        
                        std::string label = (hasSkin ? "[✓] " : "[ ] ") + def->localized_name;
                        if (ImGui::Selectable(label.c_str(), selectedWeaponIdx == i)) {
                            selectedWeaponIdx = i;
                            selectedCategoryWeaponIdx = i;
                        }
                    }
                    ImGui::EndChild();
                }
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Text("Selected Weapon Skin Settings");
            ImGui::Separator();

            if (selectedCategoryWeaponIdx >= 0 && currentList && selectedCategoryWeaponIdx < (int)currentList->size()) {
                const auto* def = (*currentList)[selectedCategoryWeaponIdx];
                auto& skin = Config::skin_changer.skins[def->def_index]; // creates if not exists
                
                ImGui::Text("%s", def->localized_name.c_str());
                ImGui::Separator();
                
                // Show skin image if available
                const auto* img = features::skinchanger::g_econ_item_system.GetSkinImage(def->def_index, skin.paint_kit_id);
                if (img && img->srv) {
                    ImGui::Image((ImTextureID)img->srv.Get(), ImVec2(120, 120));
                    ImGui::Spacing();
                }

                ImGui::Checkbox("Enable Skin", (bool*)&skin.paint_kit_id); // 0 = disabled
                if (skin.paint_kit_id != 0) {
                    ImGui::Spacing();
                    
                    // Paint Kit picker
                    const auto& paintKits = features::skinchanger::g_econ_item_system.PaintKits();
                    static char pkSearch[128] = "";
                    static char pkPreview[256] = "";
                    
                    // Update preview text
                    if (skin.paint_kit_id > 0) {
                        const auto* pk = features::skinchanger::g_econ_item_system.FindPaintKit(skin.paint_kit_id);
                        if (pk) {
                            std::snprintf(pkPreview, sizeof(pkPreview), "%s (ID: %d)", pk->localized_name.c_str(), pk->id);
                        } else {
                            std::snprintf(pkPreview, sizeof(pkPreview), "Unknown (ID: %d)", skin.paint_kit_id);
                        }
                    } else {
                        std::snprintf(pkPreview, sizeof(pkPreview), "Select Paint Kit...");
                    }
                    
                    ImGui::InputText("Search Paint Kit", pkSearch, IM_ARRAYSIZE(pkSearch));
                    
                    if (ImGui::BeginCombo("Paint Kit", pkPreview)) {
                        for (const auto& pk : paintKits) {
                            if (pkSearch[0] && std::string(pk.localized_name).find(pkSearch) == std::string::npos)
                                continue;
                            bool is_selected = (skin.paint_kit_id == pk.id);
                            if (ImGui::Selectable(pk.localized_name.c_str(), is_selected)) {
                                skin.paint_kit_id = pk.id;
                            }
                            if (is_selected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }

                    ImGui::DragInt("Seed", &skin.seed, 1, 0, 1000);
                    ImGui::DragFloat("Wear", &skin.wear, 0.001f, 0.0f, 1.0f, "%.4f");
                    ImGui::DragInt("StatTrak", &skin.stattrak, 1, -1, 9999);
                    
                    if (ImGui::Button("Remove Skin")) {
                        skin.paint_kit_id = 0;
                        skin.seed = 0;
                        skin.wear = 0.0001f;
                        skin.stattrak = -1;
                        skin.name = "";
                        Config::skin_changer.skins.erase(def->def_index);
                    }
                } else {
                    ImGui::TextDisabled("No skin applied. Check 'Enable Skin' and select a Paint Kit.");
                }
            } else {
                ImGui::TextDisabled("Select a weapon from the list to configure its skin.");
            }

            ImGui::EndChild();
        }
        break;

        case 5:
        {
            ImGui::BeginChild("ConfigLeft", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f - 5, 0), true);
            ImGui::Text("General");
            ImGui::Separator();

            static char configName[128] = "";
            static std::vector<std::string> configList = internal_config::ConfigManager::ListConfigs();
            static int selectedConfigIndex = -1;

            ImGui::InputText("Config Name", configName, IM_ARRAYSIZE(configName));

            if (ImGui::Button("Refresh")) {
                configList = internal_config::ConfigManager::ListConfigs();
            }
            ImGui::SameLine();
            if (ImGui::Button("Load")) {
                internal_config::ConfigManager::Load(configName);
            }
            ImGui::SameLine();
            if (ImGui::Button("Save")) {
                internal_config::ConfigManager::Save(configName);
                configList = internal_config::ConfigManager::ListConfigs();
            }
            ImGui::SameLine();
            if (ImGui::Button("Delete")) {
                internal_config::ConfigManager::Remove(configName);
                configList = internal_config::ConfigManager::ListConfigs();
            }

            ImGui::EndChild();

            ImGui::SameLine();
            ImGui::BeginChild("ConfigRight", ImVec2(0, 0), true);
            ImGui::Text("Saved Configs");
            ImGui::Separator();

            for (int i = 0; i < static_cast<int>(configList.size()); i++) {
                if (ImGui::Selectable(configList[i].c_str(), selectedConfigIndex == i)) {
                    selectedConfigIndex = i;
                    strncpy_s(configName, sizeof(configName), configList[i].c_str(), _TRUNCATE);
                }
            }

            ImGui::EndChild();
        }
        break;
        }

        ImGui::EndChild();
        ImGui::End();
    }
}

void Menu::toggleMenu() {
    showMenu = !showMenu;
}