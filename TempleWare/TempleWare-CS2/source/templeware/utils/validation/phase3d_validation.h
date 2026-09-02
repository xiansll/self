#pragma once

// Phase 3D validates only a deliberately small, read-only wrapper surface after
// P3C has already proven local resolver provenance. It does not traverse scene
// nodes, entity identity graphs, skeletons, hitboxes, or commands.

#include <Windows.h>
#include <cstdint>
#include <cstdio>

#include "../filelog/filelog.h"
#include "../localplayer/localplayer.h"
#include "../schema/schema.h"
#include "../../../cs2/entity/C_CSPlayerPawn/C_CSPlayerPawn.h"
#include "../../../cs2/entity/CCSPlayerController/CCSPlayerController.h"

namespace Phase3D {

struct BasicWrapperProbe {
    int health = 0;
    int desired_team = 0;
    std::uint8_t team = 0;
    bool local_controller = false;
    bool controller_alive = false;
    DWORD exception_code = 0;
};

// Keep SEH inside a tiny POD-only leaf helper to avoid MSVC C2712 object-unwind
// restrictions. The probe calls only existing TempleWare schema accessors.
inline bool ProbeBasicWrappers(C_CSPlayerPawn* pawn,
                               CCSPlayerController* controller,
                               BasicWrapperProbe* out) {
    if (!out)
        return false;

    out->exception_code = 0;
    __try {
        if (!pawn || !controller)
            return false;

        out->health = pawn->m_iHealth();
        out->team = pawn->m_iTeamNum();
        out->desired_team = controller->m_iDesiredTeam();
        out->local_controller = controller->m_bIsLocalPlayerController();
        out->controller_alive = controller->m_bPawnIsAlive();
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        out->exception_code = GetExceptionCode();
        return false;
    }
}

inline void Run(const LocalPlayerSnapshot& snapshot) {
    if (LocalPlayerTrust::basic_wrapper_semantics_proven())
        return;

    if (!snapshot.sdk_resolver_pair_proven || !snapshot.pawn || !snapshot.controller)
        return;

    static std::uintptr_t s_lastPawn = 0;
    static std::uintptr_t s_lastController = 0;
    static unsigned int s_consecutivePasses = 0;
    static bool s_firstProbeLogged = false;

    if (s_lastPawn != snapshot.pawn || s_lastController != snapshot.controller) {
        s_lastPawn = snapshot.pawn;
        s_lastController = snapshot.controller;
        s_consecutivePasses = 0;
        s_firstProbeLogged = false;
    }

    // A zero schema offset means the static schema table does not contain the
    // field. Refuse to interpret object memory in that case instead of treating
    // offset zero as valid data.
    const std::uint32_t healthOffset =
        SchemaFinder::Get(hash_32_fnv1a_const("C_BaseEntity->m_iHealth"));
    const std::uint32_t teamOffset =
        SchemaFinder::Get(hash_32_fnv1a_const("C_BaseEntity->m_iTeamNum"));
    const std::uint32_t desiredTeamOffset =
        SchemaFinder::Get(hash_32_fnv1a_const("CCSPlayerController->m_iDesiredTeam"));
    const std::uint32_t localControllerOffset =
        SchemaFinder::Get(hash_32_fnv1a_const("CBasePlayerController->m_bIsLocalPlayerController"));
    const std::uint32_t pawnAliveOffset =
        SchemaFinder::Get(hash_32_fnv1a_const("CCSPlayerController->m_bPawnIsAlive"));

    const bool offsetsReady = healthOffset && teamOffset && desiredTeamOffset &&
        localControllerOffset && pawnAliveOffset;

    if (!offsetsReady) {
        char buf[384];
        std::snprintf(buf, sizeof(buf),
            "[P3D][WRAPPER] BLOCKED - schema offsets health=0x%X team=0x%X desired=0x%X local=0x%X alive=0x%X",
            healthOffset, teamOffset, desiredTeamOffset, localControllerOffset, pawnAliveOffset);
        FileLog::Log(buf);
        return;
    }

    BasicWrapperProbe probe{};
    const bool callOk = ProbeBasicWrappers(
        reinterpret_cast<C_CSPlayerPawn*>(snapshot.pawn),
        reinterpret_cast<CCSPlayerController*>(snapshot.controller),
        &probe);

    // These are intentionally broad semantic bounds. They are not gameplay
    // decisions; they only reject obviously nonsensical wrapper interpretation.
    const bool healthSane = probe.health >= 0 && probe.health <= 500;
    const bool teamSane = probe.team <= 5;
    const bool desiredTeamSane = probe.desired_team >= 0 && probe.desired_team <= 5;
    const bool localFlagSane = probe.local_controller;
    const bool aliveConsistent = probe.controller_alive == (probe.health > 0);

    const bool semanticPass = callOk && probe.exception_code == 0 &&
        healthSane && teamSane && desiredTeamSane && localFlagSane && aliveConsistent;

    if (!s_firstProbeLogged || !semanticPass) {
        char buf[640];
        std::snprintf(buf, sizeof(buf),
            "[P3D][WRAPPER] %s - health=%d team=%u desired=%d local=%d ctrl_alive=%d health_alive=%d exception=0x%08lX offsets_ready=%d",
            semanticPass ? "PROBE PASS" : "PROBE FAIL",
            probe.health,
            static_cast<unsigned int>(probe.team),
            probe.desired_team,
            probe.local_controller ? 1 : 0,
            probe.controller_alive ? 1 : 0,
            probe.health > 0 ? 1 : 0,
            probe.exception_code,
            offsetsReady ? 1 : 0);
        FileLog::Log(buf);
        s_firstProbeLogged = true;
    }

    if (!semanticPass) {
        s_consecutivePasses = 0;
        return;
    }

    ++s_consecutivePasses;
    if (s_consecutivePasses < 3)
        return;

    LocalPlayerTrust::publish_basic_wrapper_semantics(true);

    char buf[512];
    std::snprintf(buf, sizeof(buf),
        "[P3D][WRAPPER] PASS - basic wrapper semantics proven after %u consecutive samples; deep graph remains closed",
        s_consecutivePasses);
    FileLog::Log(buf);
}

} // namespace Phase3D
