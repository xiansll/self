#pragma once
#include <cstdint>
#include "../C_BaseEntity/C_BaseEntity.h"
#include "../../../templeware/utils/schema/schema.h"
#include "../../../templeware/utils/math/vector/vector.h"
#include "../../../cs2/entity/handle.h"

class C_BasePlayerController : public C_BaseEntity {
public:
    schema(CBaseHandle, m_hPawn, "CBasePlayerController->m_hPawn");
    schema(bool, m_bIsLocalPlayerController, "CBasePlayerController->m_bIsLocalPlayerController");
    schema(int, m_nTickBase, "CBasePlayerController->m_nTickBase");
    schema(uint64_t, m_steamID, "CBasePlayerController->m_steamID");
    schema(const char*, m_iszPlayerName, "CBasePlayerController->m_iszPlayerName");
    schema(int, m_iConnected, "CBasePlayerController->m_iConnected");
    schema(int, m_iDesiredTeam, "CBasePlayerController->m_iDesiredTeam");
    schema(int, m_iPendingTeamNum, "CBasePlayerController->m_iPendingTeamNum");
    schema(bool, m_bHasDisconnectReason, "CBasePlayerController->m_bHasDisconnectReason");
    schema(int, m_iDisconnectReason, "CBasePlayerController->m_iDisconnectReason");
    schema(CBaseHandle, m_hObserverPawn, "CBasePlayerController->m_hObserverPawn");
    schema(int, m_iObserverMode, "CBasePlayerController->m_iObserverMode");
    schema(int, m_iAccount, "CBasePlayerController->m_iAccount");
    schema(bool, m_bHasCustomProfileParams, "CBasePlayerController->m_bHasCustomProfileParams");
    schema(bool, m_bPawnIsAlive, "CCSPlayerController->m_bPawnIsAlive");
};