#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <unordered_map>

#include "config.h"
#include "../../../external/json/json.hpp"

namespace internal_config
{
    class ConfigManager
    {
    private:

        static std::filesystem::path GetConfigFolder()
        {
            char* userProfile = nullptr;
            size_t len = 0;
            errno_t err = _dupenv_s(&userProfile, &len, "USERPROFILE");

            std::filesystem::path folder;
            if (err != 0 || userProfile == nullptr || len == 0)
            {
                folder = ".templeware";
            }
            else
            {
                folder = userProfile;
                free(userProfile);
                folder /= ".templeware";
            }
            folder /= "internal";

            std::error_code ec;
            std::filesystem::create_directories(folder, ec);

            return folder;
        }

        static std::filesystem::path GetConfigPath(const std::string& configName)
        {
            auto folder = GetConfigFolder();
            return folder / (configName + ".json");
        }

    public:

        static std::vector<std::string> ListConfigs()
        {
            std::vector<std::string> list;
            auto folder = GetConfigFolder();
            if (!std::filesystem::exists(folder))
                return list;

            for (const auto& entry : std::filesystem::directory_iterator(folder))
            {
                if (entry.is_regular_file())
                {
                    auto path = entry.path();
                    if (path.extension() == ".json")
                    {
                        list.push_back(path.stem().string());
                    }
                }
            }
            return list;
        }

        static void Save(const std::string& configName)
        {
            nlohmann::json j;

            j["esp"] = Config::visual_esp.esp;
            j["showHealth"] = Config::visual_esp.showHealth;
            j["teamCheck"] = Config::visual_esp.teamCheck;
            j["espFill"] = Config::visual_esp.espFill;
            j["espThickness"] = Config::visual_esp.espThickness;
            j["espFillOpacity"] = Config::visual_esp.espFillOpacity;
            j["espColor"] = {
                Config::visual_esp.espColor.x,
                Config::visual_esp.espColor.y,
                Config::visual_esp.espColor.z,
                Config::visual_esp.espColor.w
            };
            j["showNameTags"] = Config::visual_esp.showNameTags;
            j["showFlashed"] = Config::visual_esp.showFlashed;
            j["showScoped"] = Config::visual_esp.showScoped;
            j["espSkeleton"] = Config::visual_esp.espSkeleton;
            j["skeletonColor"] = {
                Config::visual_esp.skeletonColor.x,
                Config::visual_esp.skeletonColor.y,
                Config::visual_esp.skeletonColor.z,
                Config::visual_esp.skeletonColor.w
            };
            j["skeletonThickness"] = Config::visual_esp.skeletonThickness;

            j["Night"] = Config::visual_world.Night;
            j["NightColor"] = {
                Config::visual_world.NightColor.x,
                Config::visual_world.NightColor.y,
                Config::visual_world.NightColor.z,
                Config::visual_world.NightColor.w
            };
            j["fovEnabled"] = Config::visual_world.fovEnabled;
            j["fov"] = Config::visual_world.fov;
            j["spectatorList"] = Config::visual_world.spectatorList;
            j["antiflash"] = Config::visual_world.antiflash;

            j["enemyChamsInvisible"] = Config::visual_chams.enemyChamsInvisible;
            j["enemyChams"] = Config::visual_chams.enemyChams;
            j["teamChams"] = Config::visual_chams.teamChams;
            j["teamChamsInvisible"] = Config::visual_chams.teamChamsInvisible;
            j["localChams"] = Config::visual_chams.localChams;
            j["chamsMaterial"] = Config::visual_chams.chamsMaterial;
            j["armChams"] = Config::visual_chams.armChams;
            j["viewmodelChams"] = Config::visual_chams.viewmodelChams;
            j["colViewmodelChams"] = {
                Config::visual_chams.colViewmodelChams.x,
                Config::visual_chams.colViewmodelChams.y,
                Config::visual_chams.colViewmodelChams.z,
                Config::visual_chams.colViewmodelChams.w
            };
            j["colArmChams"] = {
                Config::visual_chams.colArmChams.x,
                Config::visual_chams.colArmChams.y,
                Config::visual_chams.colArmChams.z,
                Config::visual_chams.colArmChams.w
            };
            j["colVisualChams"] = {
                Config::visual_chams.colVisualChams.x,
                Config::visual_chams.colVisualChams.y,
                Config::visual_chams.colVisualChams.z,
                Config::visual_chams.colVisualChams.w
            };
            j["colVisualChamsIgnoreZ"] = {
                Config::visual_chams.colVisualChamsIgnoreZ.x,
                Config::visual_chams.colVisualChamsIgnoreZ.y,
                Config::visual_chams.colVisualChamsIgnoreZ.z,
                Config::visual_chams.colVisualChamsIgnoreZ.w
            };
            j["teamcolVisualChamsIgnoreZ"] = {
                Config::visual_chams.teamcolVisualChamsIgnoreZ.x,
                Config::visual_chams.teamcolVisualChamsIgnoreZ.y,
                Config::visual_chams.teamcolVisualChamsIgnoreZ.z,
                Config::visual_chams.teamcolVisualChamsIgnoreZ.w
            };
            j["teamcolVisualChams"] = {
                Config::visual_chams.teamcolVisualChams.x,
                Config::visual_chams.teamcolVisualChams.y,
                Config::visual_chams.teamcolVisualChams.z,
                Config::visual_chams.teamcolVisualChams.w
            };
            j["localcolChams"] = {
                Config::visual_chams.localcolChams.x,
                Config::visual_chams.localcolChams.y,
                Config::visual_chams.localcolChams.z,
                Config::visual_chams.localcolChams.w
            };

            j["aimbot"] = Config::legit_aim.aimbot;
            j["aimbot_fov"] = Config::legit_aim.aimbot_fov;
            j["team_check"] = Config::legit_aim.team_check;
            j["rcs"] = Config::legit_aim.rcs;
            j["fov_circle"] = Config::legit_aim.fov_circle;
            j["fovCircleColor"] = {
                Config::legit_aim.fovCircleColor.x,
                Config::legit_aim.fovCircleColor.y,
                Config::legit_aim.fovCircleColor.z,
                Config::legit_aim.fovCircleColor.w
            };

            j["anti_aim_enabled"] = Config::anti_aim.enabled;
            j["anti_aim_overridePitch"] = Config::anti_aim.overridePitch;
            j["anti_aim_overrideYaw"] = Config::anti_aim.overrideYaw;
            j["anti_aim_pitchAmount"] = Config::anti_aim.pitchAmount;
            j["anti_aim_yawAmount"] = Config::anti_aim.yawAmount;
            j["anti_aim_jitterAmount"] = Config::anti_aim.jitterAmount;

            j["bhop"] = Config::movement.bhop;

            // Skin Changer
            j["skin_changer_enabled"] = Config::skin_changer.enabled;
            j["skin_changer_guns"] = Config::skin_changer.guns;
            j["skin_changer_knives"] = Config::skin_changer.knives;
            j["skin_changer_gloves"] = Config::skin_changer.gloves;
            j["skin_changer_agents"] = Config::skin_changer.agents;
            j["skin_changer_ct_agent"] = Config::skin_changer.ct_agent;
            j["skin_changer_t_agent"] = Config::skin_changer.t_agent;

            nlohmann::json skins_json = nlohmann::json::array();
            for (const auto& [def_idx, skin] : Config::skin_changer.skins) {
                nlohmann::json skin_entry;
                skin_entry["def_index"] = def_idx;
                skin_entry["paint_kit_id"] = skin.paint_kit_id;
                skin_entry["seed"] = skin.seed;
                skin_entry["wear"] = skin.wear;
                skin_entry["stattrak"] = skin.stattrak;
                skin_entry["name"] = skin.name;
                skins_json.push_back(skin_entry);
            }
            j["skin_changer_skins"] = skins_json;

            auto filePath = GetConfigPath(configName);
            std::ofstream ofs(filePath);
            if (ofs.is_open())
            {
                ofs << j.dump(4);
                ofs.close();
            }
        }

        static void Load(const std::string& configName)
        {
            auto filePath = GetConfigPath(configName);
            if (!std::filesystem::exists(filePath))
                return;

            std::ifstream ifs(filePath);
            if (!ifs.is_open())
                return;

            nlohmann::json j;
            ifs >> j;

            Config::visual_esp.esp = j.value("esp", false);
            Config::visual_esp.showHealth = j.value("showHealth", false);
            Config::visual_esp.teamCheck = j.value("teamCheck", false);
            Config::visual_esp.espFill = j.value("espFill", false);
            Config::visual_esp.espThickness = j.value("espThickness", 1.0f);
            Config::visual_esp.espFillOpacity = j.value("espFillOpacity", 0.5f);
            Config::visual_esp.showNameTags = j.value("showNameTags", false);
            Config::visual_esp.showFlashed = j.value("showFlashed", false);
            Config::visual_esp.showScoped = j.value("showScoped", false);
            Config::visual_esp.espSkeleton = j.value("espSkeleton", false);
            Config::visual_esp.skeletonThickness = j.value("skeletonThickness", 1.0f);

            if (j.contains("espColor") && j["espColor"].is_array() && j["espColor"].size() == 4)
            {
                auto arr = j["espColor"];
                Config::visual_esp.espColor.x = arr[0].get<float>();
                Config::visual_esp.espColor.y = arr[1].get<float>();
                Config::visual_esp.espColor.z = arr[2].get<float>();
                Config::visual_esp.espColor.w = arr[3].get<float>();
            }

            if (j.contains("skeletonColor") && j["skeletonColor"].is_array() && j["skeletonColor"].size() == 4)
            {
                auto arr = j["skeletonColor"];
                Config::visual_esp.skeletonColor.x = arr[0].get<float>();
                Config::visual_esp.skeletonColor.y = arr[1].get<float>();
                Config::visual_esp.skeletonColor.z = arr[2].get<float>();
                Config::visual_esp.skeletonColor.w = arr[3].get<float>();
            }

            Config::visual_world.Night = j.value("Night", false);
            Config::visual_world.fovEnabled = j.value("fovEnabled", false);
            Config::visual_world.fov = j.value("fov", 90.0f);
            Config::visual_world.spectatorList = j.value("spectatorList", false);
            Config::visual_world.antiflash = j.value("antiflash", false);

            if (j.contains("NightColor") && j["NightColor"].is_array() && j["NightColor"].size() == 4)
            {
                auto arr = j["NightColor"];
                Config::visual_world.NightColor.x = arr[0].get<float>();
                Config::visual_world.NightColor.y = arr[1].get<float>();
                Config::visual_world.NightColor.z = arr[2].get<float>();
                Config::visual_world.NightColor.w = arr[3].get<float>();
            }

            Config::visual_chams.enemyChamsInvisible = j.value("enemyChamsInvisible", false);
            Config::visual_chams.enemyChams = j.value("enemyChams", false);
            Config::visual_chams.teamChams = j.value("teamChams", false);
            Config::visual_chams.teamChamsInvisible = j.value("teamChamsInvisible", false);
            Config::visual_chams.localChams = j.value("localChams", false);
            Config::visual_chams.chamsMaterial = j.value("chamsMaterial", 0);
            Config::visual_chams.armChams = j.value("armChams", false);
            Config::visual_chams.viewmodelChams = j.value("viewmodelChams", false);

            if (j.contains("colViewmodelChams") && j["colViewmodelChams"].is_array() && j["colViewmodelChams"].size() == 4)
            {
                auto arr = j["colViewmodelChams"];
                Config::visual_chams.colViewmodelChams.x = arr[0].get<float>();
                Config::visual_chams.colViewmodelChams.y = arr[1].get<float>();
                Config::visual_chams.colViewmodelChams.z = arr[2].get<float>();
                Config::visual_chams.colViewmodelChams.w = arr[3].get<float>();
            }

            if (j.contains("colArmChams") && j["colArmChams"].is_array() && j["colArmChams"].size() == 4)
            {
                auto arr = j["colArmChams"];
                Config::visual_chams.colArmChams.x = arr[0].get<float>();
                Config::visual_chams.colArmChams.y = arr[1].get<float>();
                Config::visual_chams.colArmChams.z = arr[2].get<float>();
                Config::visual_chams.colArmChams.w = arr[3].get<float>();
            }

            if (j.contains("colVisualChams") && j["colVisualChams"].is_array() && j["colVisualChams"].size() == 4)
            {
                auto arr = j["colVisualChams"];
                Config::visual_chams.colVisualChams.x = arr[0].get<float>();
                Config::visual_chams.colVisualChams.y = arr[1].get<float>();
                Config::visual_chams.colVisualChams.z = arr[2].get<float>();
                Config::visual_chams.colVisualChams.w = arr[3].get<float>();
            }

            if (j.contains("colVisualChamsIgnoreZ") && j["colVisualChamsIgnoreZ"].is_array() && j["colVisualChamsIgnoreZ"].size() == 4)
            {
                auto arr = j["colVisualChamsIgnoreZ"];
                Config::visual_chams.colVisualChamsIgnoreZ.x = arr[0].get<float>();
                Config::visual_chams.colVisualChamsIgnoreZ.y = arr[1].get<float>();
                Config::visual_chams.colVisualChamsIgnoreZ.z = arr[2].get<float>();
                Config::visual_chams.colVisualChamsIgnoreZ.w = arr[3].get<float>();
            }

            if (j.contains("teamcolVisualChamsIgnoreZ") && j["teamcolVisualChamsIgnoreZ"].is_array() && j["teamcolVisualChamsIgnoreZ"].size() == 4)
            {
                auto arr = j["teamcolVisualChamsIgnoreZ"];
                Config::visual_chams.teamcolVisualChamsIgnoreZ.x = arr[0].get<float>();
                Config::visual_chams.teamcolVisualChamsIgnoreZ.y = arr[1].get<float>();
                Config::visual_chams.teamcolVisualChamsIgnoreZ.z = arr[2].get<float>();
                Config::visual_chams.teamcolVisualChamsIgnoreZ.w = arr[3].get<float>();
            }

            if (j.contains("teamcolVisualChams") && j["teamcolVisualChams"].is_array() && j["teamcolVisualChams"].size() == 4)
            {
                auto arr = j["teamcolVisualChams"];
                Config::visual_chams.teamcolVisualChams.x = arr[0].get<float>();
                Config::visual_chams.teamcolVisualChams.y = arr[1].get<float>();
                Config::visual_chams.teamcolVisualChams.z = arr[2].get<float>();
                Config::visual_chams.teamcolVisualChams.w = arr[3].get<float>();
            }

            if (j.contains("localcolChams") && j["localcolChams"].is_array() && j["localcolChams"].size() == 4)
            {
                auto arr = j["localcolChams"];
                Config::visual_chams.localcolChams.x = arr[0].get<float>();
                Config::visual_chams.localcolChams.y = arr[1].get<float>();
                Config::visual_chams.localcolChams.z = arr[2].get<float>();
                Config::visual_chams.localcolChams.w = arr[3].get<float>();
            }

            Config::legit_aim.aimbot = j.value("aimbot", false);
            Config::legit_aim.aimbot_fov = j.value("aimbot_fov", 0.0f);
            Config::legit_aim.team_check = j.value("team_check", false);
            Config::legit_aim.rcs = j.value("rcs", false);
            Config::legit_aim.fov_circle = j.value("fov_circle", false);

            if (j.contains("fovCircleColor") && j["fovCircleColor"].is_array() && j["fovCircleColor"].size() == 4)
            {
                auto arr = j["fovCircleColor"];
                Config::legit_aim.fovCircleColor.x = arr[0].get<float>();
                Config::legit_aim.fovCircleColor.y = arr[1].get<float>();
                Config::legit_aim.fovCircleColor.z = arr[2].get<float>();
                Config::legit_aim.fovCircleColor.w = arr[3].get<float>();
            }

            Config::anti_aim.enabled = j.value("anti_aim_enabled", false);
            Config::anti_aim.overridePitch = j.value("anti_aim_overridePitch", false);
            Config::anti_aim.overrideYaw = j.value("anti_aim_overrideYaw", false);
            Config::anti_aim.pitchAmount = j.value("anti_aim_pitchAmount", 0);
            Config::anti_aim.yawAmount = j.value("anti_aim_yawAmount", 0);
            Config::anti_aim.jitterAmount = j.value("anti_aim_jitterAmount", 0);

            Config::movement.bhop = j.value("bhop", false);

            // Skin Changer
            Config::skin_changer.enabled = j.value("skin_changer_enabled", false);
            Config::skin_changer.guns = j.value("skin_changer_guns", false);
            Config::skin_changer.knives = j.value("skin_changer_knives", false);
            Config::skin_changer.gloves = j.value("skin_changer_gloves", false);
            Config::skin_changer.agents = j.value("skin_changer_agents", false);
            Config::skin_changer.ct_agent = j.value("skin_changer_ct_agent", static_cast<std::int16_t>(0));
            Config::skin_changer.t_agent = j.value("skin_changer_t_agent", static_cast<std::int16_t>(0));

            Config::skin_changer.skins.clear();
            if (j.contains("skin_changer_skins") && j["skin_changer_skins"].is_array()) {
                for (const auto& skin_entry : j["skin_changer_skins"]) {
                    std::int16_t def_idx = skin_entry.value("def_index", 0);
                    Config::skin_changer_t::applied_skin skin(
                        skin_entry.value("paint_kit_id", 0),
                        skin_entry.value("seed", 0),
                        skin_entry.value("wear", 0.0001f),
                        skin_entry.value("stattrak", -1),
                        skin_entry.value("name", std::string(""))
                    );
                    Config::skin_changer.skins[def_idx] = skin;
                }
            }

            ifs.close();
        }

        static void Remove(const std::string& configName)
        {
            auto filePath = GetConfigPath(configName);
            if (std::filesystem::exists(filePath))
            {
                std::error_code ec;
                std::filesystem::remove(filePath, ec);
            }
        }
    };
}