#pragma once
#include <unordered_map>
#include <string>
#include "../../../external/imgui/imgui.h"

namespace Config {

	struct visual_esp_t {
		bool esp = false;
		bool showHealth = false;
		bool teamCheck = false;
		bool espFill = false;
		float espThickness = 1.0f;
		float espFillOpacity = 0.5f;
		ImVec4 espColor = ImVec4(1, 0, 0, 1);
		bool showNameTags = false;
		bool showFlashed = false;
		bool showScoped = false;

		bool espSkeleton = false;
		ImVec4 skeletonColor = ImVec4(1, 0, 0, 1);
		float skeletonThickness = 1.0f;
	};
	extern visual_esp_t visual_esp;

	struct visual_world_t {
		bool Night = false;
		ImVec4 NightColor = ImVec4(0.1, 0.1, 0.1, 1);

		bool fovEnabled = false;
		float fov = 90.0f;

		bool spectatorList = false;

		bool antiflash = false;
	};
	extern visual_world_t visual_world;

	struct visuals_chams_t {
		bool enemyChamsInvisible = false;
		bool enemyChams = false;
		bool teamChams = false;
		bool teamChamsInvisible = false;
		bool localChams = false;
		int chamsMaterial = 0;


		bool armChams = false;
		bool viewmodelChams = false;
		ImVec4 colViewmodelChams = ImVec4(1, 0, 0, 1);
		ImVec4 colArmChams = ImVec4(1, 0, 0, 1);

		ImVec4 colVisualChams = ImVec4(1, 0, 0, 1);
		ImVec4 colVisualChamsIgnoreZ = ImVec4(1, 0, 0, 1);
		ImVec4 teamcolVisualChamsIgnoreZ = ImVec4(1, 0, 0, 1);
		ImVec4 teamcolVisualChams = ImVec4(1, 0, 0, 1);
		ImVec4 localcolChams = ImVec4(1, 0, 0, 1);
	};
	extern visuals_chams_t visual_chams;

	struct legit_aim_t {
		bool aimbot = 0;
		float aimbot_fov = 0;
		bool team_check = false;
		bool rcs = 0;
		bool fov_circle = 0;
		ImVec4 fovCircleColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
	};
	extern legit_aim_t legit_aim;

	struct anti_aim_t
	{
		bool enabled;
		bool overridePitch;
		bool overrideYaw;
		int pitchAmount = 0;
		int yawAmount = 0;
		int jitterAmount = 0;

	};
	extern anti_aim_t anti_aim;

	struct movement_t {
		bool bhop = false;
	};
	extern movement_t movement;

	struct skin_changer_t {
		bool enabled = false;
		bool guns = false;
		bool knives = false;
		bool gloves = false;
		bool agents = false;

		struct applied_skin {
			int paint_kit_id = 0;
			int seed = 0;
			float wear = 0.0001f;
			int stattrak = -1;
			std::string name = "";
			applied_skin() = default;
			applied_skin(int pk, int s, float w, int st, const std::string& n) 
				: paint_kit_id(pk), seed(s), wear(w), stattrak(st), name(n) {}
		};

		std::unordered_map<std::int16_t, applied_skin> skins{};
		std::int16_t ct_agent = 0;
		std::int16_t t_agent = 0;
	};
	extern skin_changer_t skin_changer;
}
