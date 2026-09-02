#include "C_CSPlayerPawn.h"

#include "../../../templeware/offsets/offsets.h"
#include "../../../templeware/interfaces/interfaces.h"

Vector_t C_CSPlayerPawn::getPosition() const {
	return m_vOldOrigin();
}

Vector_t C_CSPlayerPawn::getEyePosition() const {
	auto scene = m_pGameSceneNode();
	if (!scene) return {};
	return scene->GetAbsOrigin() + m_vecViewOffset();
}

C_CSWeaponBase* C_CSPlayerPawn::GetActiveWeapon() {
	CCSWeaponBaseVData* pWeaponData = nullptr;
	C_CSWeaponBase* pWeapon = nullptr;
	if (auto pWeaponServices = m_pWeaponServices()) {
		CBaseHandle hWeapon = pWeaponServices->m_hActiveWeapon();
		if (hWeapon.valid()) {
			if (pWeapon = I::GameEntity->Instance->Get<C_CSWeaponBase>(hWeapon)) {
				pWeaponData = pWeapon->m_pWeaponData();
			}
		}
	}

	return pWeapon;
}

CCSPlayer_WeaponServices* C_CSPlayerPawn::GetWeaponServices() const {
	return m_pWeaponServices();
}

uintptr_t C_CSPlayerPawn::getAddress() const {
	return address;
}

int C_CSPlayerPawn::getHealth() const {
	return m_iHealth();
}

uint8_t C_CSPlayerPawn::getTeam() const {
	return m_iTeamNum();
}

Vector_t C_CSPlayerPawn::getViewOffset() const {
	return m_vecViewOffset();
}

bool C_CSPlayerPawn::is_alive() const {
	return m_iHealth() > 0;
}