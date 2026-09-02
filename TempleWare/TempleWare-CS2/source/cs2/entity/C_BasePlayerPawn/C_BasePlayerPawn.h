#pragma once
#include <cstdint>
#include "../C_BaseEntity/C_BaseEntity.h"
#include "../../../templeware/utils/schema/schema.h"
#include "../../../templeware/utils/math/vector/vector.h"
#include "../../../cs2/entity/handle.h"
#include "../../../templeware/interfaces/CUserCmd/CUserCmd.h"

class CCSPlayer_WeaponServices;
class C_PlayerMovementService;
class CPlayer_ItemServices;
class CPlayer_ObserverServices;
class CPlayer_CameraServices;
class ViewAngleServerChange_t;

class C_BasePlayerPawn : public C_BaseEntity {
public:
    schema(Vector_t, m_vOldOrigin, "C_BasePlayerPawn->m_vOldOrigin");
    schema(CCSPlayer_WeaponServices*, m_pWeaponServices, "C_BasePlayerPawn->m_pWeaponServices");
    schema(C_PlayerMovementService*, m_pMovementServices, "C_BasePlayerPawn->m_pMovementServices");
    schema(CPlayer_ItemServices*, m_pItemServices, "C_BasePlayerPawn->m_pItemServices");
    schema(CPlayer_ObserverServices*, m_pObserverServices, "C_BasePlayerPawn->m_pObserverServices");
    schema(CPlayer_CameraServices*, m_pCameraServices, "C_BasePlayerPawn->m_pCameraServices");
    schema(CBaseHandle, m_hController, "C_BasePlayerPawn->m_hController");
    schema(ViewAngleServerChange_t*, m_ServerViewAngleChanges, "C_BasePlayerPawn->m_ServerViewAngleChanges");
    schema(float, m_flDeathTime, "C_BasePlayerPawn->m_flDeathTime");
    schema(bool, m_bDeadFlag, "C_BasePlayerPawn->m_bDeadFlag");
    schema(int, m_lifeState, "C_BasePlayerPawn->m_lifeState");
    schema(Vector_t, m_vecBaseVelocity, "C_BasePlayerPawn->m_vecBaseVelocity");
    schema(Vector_t, m_vecAbsVelocity, "C_BasePlayerPawn->m_vecAbsVelocity");
    schema(float, m_flMaxspeed, "C_BasePlayerPawn->m_flMaxspeed");
    schema(float, m_flGravity, "C_BasePlayerPawn->m_flGravity");
    schema(int, m_nWaterLevel, "C_BasePlayerPawn->m_nWaterLevel");
    schema(Vector_t, m_vecViewOffset, "C_BaseModelEntity->m_vecViewOffset");
};