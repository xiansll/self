#pragma once
#include <cstdint>
#include "../C_CSWeaponBase/C_CSWeaponBase.h"
#include "../../../templeware/utils/schema/schema.h"
#include "../../../templeware/utils/math/vector/vector.h"
#include "../../../cs2/entity/handle.h"

class C_BasePlayerWeapon : public C_BaseEntity {
public:
    schema(int32_t, m_iClip1, "C_BasePlayerWeapon->m_iClip1");
    schema(int32_t, m_iClip2, "C_BasePlayerWeapon->m_iClip2");
    schema(int32_t, m_iPrimaryAmmoType, "C_BasePlayerWeapon->m_iPrimaryAmmoType");
    schema(int32_t, m_iSecondaryAmmoType, "C_BasePlayerWeapon->m_iSecondaryAmmoType");
    schema(int32_t, m_nNextPrimaryAttackTick, "C_BasePlayerWeapon->m_nNextPrimaryAttackTick");
    schema(float, m_flNextPrimaryAttackTickRatio, "C_BasePlayerWeapon->m_flNextPrimaryAttackTickRatio");
    schema(int32_t, m_nNextSecondaryAttackTick, "C_BasePlayerWeapon->m_nNextSecondaryAttackTick");
    schema(float, m_flNextSecondaryAttackTickRatio, "C_BasePlayerWeapon->m_flNextSecondaryAttackTickRatio");
    schema(float, m_flTimeWeaponIdle, "C_BasePlayerWeapon->m_flTimeWeaponIdle");
    schema(bool, m_bInReload, "C_BasePlayerWeapon->m_bInReload");
    schema(int32_t, m_iState, "C_BasePlayerWeapon->m_iState");
    schema(int32_t, m_iViewModelIndex, "C_BasePlayerWeapon->m_iViewModelIndex");
    schema(int32_t, m_iWorldModelIndex, "C_BasePlayerWeapon->m_iWorldModelIndex");
    schema(bool, m_bLowered, "C_BasePlayerWeapon->m_bLowered");
    schema(int32_t, m_iBurstShotsRemaining, "C_BasePlayerWeapon->m_iBurstShotsRemaining");
    schema(int32_t, m_iBurstShotsRemainingOld, "C_BasePlayerWeapon->m_iBurstShotsRemainingOld");
    schema(bool, m_bBurstMode, "C_BasePlayerWeapon->m_bBurstMode");
};