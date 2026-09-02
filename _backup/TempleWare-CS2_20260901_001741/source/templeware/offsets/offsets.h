#pragma once
#include <cstddef>

namespace Offset {
	constexpr std::ptrdiff_t dwLocalPlayerPawn = 0x2341698;

	namespace C_BasePlayerPawn {
		constexpr std::ptrdiff_t m_vOldOrigin = 0x1390;
		constexpr std::ptrdiff_t m_iHealth = 0x34C;
		constexpr std::ptrdiff_t m_iTeamNum = 0x3E3;
		constexpr std::ptrdiff_t m_vecViewOffset = 0xE70;
	}
}
