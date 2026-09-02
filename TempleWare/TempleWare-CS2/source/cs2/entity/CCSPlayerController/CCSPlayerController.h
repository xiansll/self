#pragma once
#include <cstdint>
#include "../../../templeware/utils/memory/memorycommon.h"
#include "../../../templeware/utils/math/vector/vector.h"
#include "../../../templeware/utils/schema/schema.h"
#include "../C_BasePlayerController/C_BasePlayerController.h"

class CCSPlayerController : public C_BasePlayerController {
public:
    CCSPlayerController(uintptr_t address);
    const char* getName() const;
    uintptr_t getAddress() const;

    schema(int, m_iCompetitiveRanking, "CCSPlayerController->m_iCompetitiveRanking");
    schema(int, m_iCompetitiveWins, "CCSPlayerController->m_iCompetitiveWins");
    schema(int, m_iAccount, "CCSPlayerController->m_iAccount");
    schema(int, m_iMusicID, "CCSPlayerController->m_iMusicID");
    schema(bool, m_bHasCustomProfileParams, "CCSPlayerController->m_bHasCustomProfileParams");
    schema(bool, m_bHasDisconnectReason, "CCSPlayerController->m_bHasDisconnectReason");
    schema(int, m_iDisconnectReason, "CCSPlayerController->m_iDisconnectReason");
    schema(int, m_iConnected, "CCSPlayerController->m_iConnected");
    schema(int, m_iDesiredTeam, "CCSPlayerController->m_iDesiredTeam");
    schema(int, m_iPendingTeamNum, "CCSPlayerController->m_iPendingTeamNum");
    schema(CBaseHandle, m_hObserverPawn, "CCSPlayerController->m_hObserverPawn");
    schema(int, m_iObserverMode, "CCSPlayerController->m_iObserverMode");

    // Compat aliases for existing code
    [[nodiscard]] inline bool IsLocalPlayer() { return m_bIsLocalPlayerController(); }
    [[nodiscard]] inline bool m_bPawnIsAlive() { return C_BasePlayerController::m_bPawnIsAlive(); }
    [[nodiscard]] inline const char* m_sSanitizedPlayerName() { return m_iszPlayerName(); }
    [[nodiscard]] inline int m_nTickBase() { return C_BasePlayerController::m_nTickBase(); }

private:
    uintptr_t address;
};