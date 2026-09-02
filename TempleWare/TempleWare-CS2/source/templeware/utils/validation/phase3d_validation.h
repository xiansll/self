#pragma once

// Phase 3D validates only a deliberately small, read-only wrapper surface after
// P3C has already proven local resolver provenance. It does not traverse scene
// nodes, entity identity graphs, skeletons, hitboxes, commands, or controller
// fields that are absent from TempleWare's current static schema table.

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
    std::uint8_t team = 0;
    bool local_controller = false;
    bool controller_alive = false;
    DWORD exception_code = 0;
};

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
    static bool s_pairBlocked = false;
    static bool s_offsetsLoggedForPair = false;

    if (s_lastPawn != snapshot.pawn || s_lastController != snapshot.controller) {
        s_lastPawn = snapshot.pawn;
        s_lastController = snapshot.controller;
        s_consecutivePasses = 0;
        s_pairBlocked = false;
        s_offsetsLoggedForPair = false;
    }

    // Do not hammer a resolver-proven pair every Present once the same wrapper
    // interpretation has already failed semantically. A new pair gets one fresh
    // validation attempt automatically.
    if (s_pairBlocked)
        return;

    const std::uint32_t healthOffset =
        SchemaFinder::Get(hash_32_fnv1a_const("C_BaseEntity->m_iHealth"));
    const std::uint32_t teamOffset =
        SchemaFinder::Get(hash_32_fnv1a_const("C_BaseEntity->m_iTeamNum"));
    const std::uint32_t localControllerOffset =
        SchemaFinder::Get(hash_32_fnv1a_const("CBasePlayerController->m_bIsLocalPlayerController"));
    const std::uint32_t pawnAliveOffset =
        SchemaFinder::Get(hash_32_fnv1a_const("CCSPlayerController->m_bPawnIsAlive"));

    if (!s_offsetsLoggedForPair) {
        char buf[448];
        std::snprintf(buf, sizeof(buf),
            "[P3D][SCHEMA] basic offsets health=0x%X team=0x%X local=0x%X alive=0x%X pawn=%p controller=%p",
            healthOffset,
            teamOffset,
            localControllerOffset,
            pawnAliveOffset,
            reinterpret_cast<void*>(snapshot.pawn),
            reinterpret_cast<void*>(snapshot.controller));
        FileLog::Log(buf);
        s_offsetsLoggedForPair = true;
    }

    const bool offsetsReady = healthOffset && teamOffset &&
        localControllerOffset && pawnAliveOffset;

    if (!offsetsReady) {
        char buf[448];
        std::snprintf(buf, sizeof(buf),
            "[P3D][WRAPPER] BLOCKED - schema coverage incomplete health=0x%X team=0x%X local=0x%X alive=0x%X; sdk_safe remains 0",
            healthOffset, teamOffset, localControllerOffset, pawnAliveOffset);
        FileLog::Log(buf);
        s_pairBlocked = true;
        return;
    }

    BasicWrapperProbe probe{};
    const bool callOk = ProbeBasicWrappers(
        reinterpret_cast<C_CSPlayerPawn*>(snapshot.pawn),
        reinterpret_cast<CCSPlayerController*>(snapshot.controller),
        &probe);

    const bool healthSane = probe.health >= 0 && probe.health <= 500;
    const bool teamSane = probe.team <= 5;
    const bool localFlagSane = probe.local_controller;
    const bool aliveConsistent = probe.controller_alive == (probe.health > 0);

    const bool semanticPass = callOk && probe.exception_code == 0 &&
        healthSane && teamSane && localFlagSane && aliveConsistent;

    if (!semanticPass) {
        char buf[768];
        std::snprintf(buf, sizeof(buf),
            "[P3D][WRAPPER] BLOCKED - resolver-proven pair is not layout-compatible with current basic schema wrappers health=%d team=%u local=%d ctrl_alive=%d health_ok=%d team_ok=%d local_ok=%d alive_consistent=%d exception=0x%08lX; sdk_safe remains 0",
            probe.health,
            static_cast<unsigned int>(probe.team),
            probe.local_controller ? 1 : 0,
            probe.controller_alive ? 1 : 0,
            healthSane ? 1 : 0,
            teamSane ? 1 : 0,
            localFlagSane ? 1 : 0,
            aliveConsistent ? 1 : 0,
            probe.exception_code);
        FileLog::Log(buf);
        s_consecutivePasses = 0;
        s_pairBlocked = true;
        return;
    }

    ++s_consecutivePasses;
    if (s_consecutivePasses == 1) {
        char buf[512];
        std::snprintf(buf, sizeof(buf),
            "[P3D][WRAPPER] PROBE PASS - health=%d team=%u local=%d ctrl_alive=%d exception=0x%08lX",
            probe.health,
            static_cast<unsigned int>(probe.team),
            probe.local_controller ? 1 : 0,
            probe.controller_alive ? 1 : 0,
            probe.exception_code);
        FileLog::Log(buf);
    }

    if (s_consecutivePasses < 3)
        return;

    LocalPlayerTrust::publish_basic_wrapper_semantics(true);

    char buf[512];
    std::snprintf(buf, sizeof(buf),
        "[P3D][WRAPPER] PASS - schema-backed basic wrapper semantics proven after %u consecutive samples; view-team and deep graph remain closed",
        s_consecutivePasses);
    FileLog::Log(buf);
}

} // namespace Phase3D
