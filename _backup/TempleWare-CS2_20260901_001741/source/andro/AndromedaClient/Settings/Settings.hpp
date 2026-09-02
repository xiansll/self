#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include <Common/Common.hpp>
#ifndef IMGUI_VERSION
#include <ImGui/imgui.h>   // skipped when a host ImGui (TempleWare's) is already included
#endif

namespace Settings
{
	namespace Inventory
	{
		struct StickerSlot_t
		{
			int   m_iKitID = 0;
			float m_flWear = 0.f;
		};

		// One entry per (team, slot) selection the user has configured.
		// Music kits use m_iTeam = 0 (server-wide); weapon entries use 2 = TT, 3 = CT.
		struct ItemSelection_t
		{
			int           m_iTeam     = 0;
			int           m_iSlot     = -1;
			uint16_t      m_DefIdx    = 0;
			int           m_iPaintKit = 0;
			float         m_flWear    = 0.f;
			int           m_iSeed     = 0;
			int           m_iStatTrak = -1;
			bool          m_bEquipped = true;
			StickerSlot_t m_Stickers[5] = {};
		};

		inline std::vector<ItemSelection_t> m_vecSelections;
		inline std::string                  m_sLastLoadedConfig = "default.json";
	}

	namespace Menu
	{
		inline auto MenuAlpha = 200;
		inline auto MenuStyle = 0;
	}

	// Compatibility stubs — kept so CSettingsJson can silently read/ignore
	// old config files that contain a "Visual" or "Colors" section.
	namespace Visual
	{
		inline bool Active        = false;
		inline bool Team          = false;
		inline bool Enemy         = false;
		inline bool OnlyVisible   = false;
		inline bool PlayerBox     = false;
		inline bool BoneESP       = false;
		inline bool BoneESPTeam   = false;
		inline bool BoneESPEnemy  = false;
		inline bool Glow          = false;
		inline bool GlowTeam      = false;
		inline bool GlowEnemy     = false;
		inline int  PlayerBoxType = 0;
	}

	namespace Colors
	{
		namespace Visual
		{
			inline ImVec4 PlayerBoxTT          = { 1.f , 1.f , 1.f , 1.f };
			inline ImVec4 PlayerBoxTT_Visible  = { 1.f , 1.f , 1.f , 1.f };
			inline ImVec4 PlayerBoxCT          = { 1.f , 1.f , 1.f , 1.f };
			inline ImVec4 PlayerBoxCT_Visible  = { 1.f , 1.f , 1.f , 1.f };
			inline ImVec4 BoneESPTT            = { 1.f , 1.f , 1.f , 1.f };
			inline ImVec4 BoneESPTT_Visible    = { 1.f , 1.f , 1.f , 1.f };
			inline ImVec4 BoneESPCT            = { 1.f , 1.f , 1.f , 1.f };
			inline ImVec4 BoneESPCT_Visible    = { 1.f , 1.f , 1.f , 1.f };
			inline ImVec4 GlowTT               = { 1.f , 1.f , 1.f , 1.f };
			inline ImVec4 GlowTT_Visible       = { 1.f , 1.f , 1.f , 1.f };
			inline ImVec4 GlowCT               = { 1.f , 1.f , 1.f , 1.f };
			inline ImVec4 GlowCT_Visible       = { 1.f , 1.f , 1.f , 1.f };
		}
	}
}
