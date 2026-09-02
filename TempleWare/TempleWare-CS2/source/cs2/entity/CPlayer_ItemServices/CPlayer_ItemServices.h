#pragma once
#include <cstdint>
#include "../../../templeware/utils/schema/schema.h"

class CPlayer_ItemServices {
public:
    schema(bool, m_bHasHelmet, "CPlayer_ItemServices->m_bHasHelmet");
    schema(bool, m_bHasHeavyArmor, "CPlayer_ItemServices->m_bHasHeavyArmor");
    schema(int, m_nArmorValue, "CPlayer_ItemServices->m_nArmorValue");
};

class CCSPlayer_ItemServices : public CPlayer_ItemServices {
public:
    schema(bool, m_bHasDefuser, "CCSPlayer_ItemServices->m_bHasDefuser");
    schema(int, m_nTouchingWeaponCount, "CCSPlayer_ItemServices->m_nTouchingWeaponCount");
    schema(CBaseHandle, m_hTouchingWeapon, "CCSPlayer_ItemServices->m_hTouchingWeapon");
};