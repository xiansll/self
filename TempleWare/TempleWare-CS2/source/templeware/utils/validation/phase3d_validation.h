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
#include "../../interfaces/IEntitySystem/IEntitySystem.h"
#include "../../../cs2/entity/C_CSPlayerPawn/C_CSPlayerPawn.h"
#include "../../../cs2/entity/CCSPlayerController/CCSPlayerController.h"

namespace Phase3D {

struct BasicWrapperProbe {
    int max_health = 0;
    int health = 0;
    std::uint8_t team = 0;
    bool local_controller = false;
    bool controller_alive = false;
    DWORD exception_code = 0;
};

struct PointerIdentityProbe {
    bool handle_valid = false;
    int handle_index = -1;
    std::uintptr_t resolved_pawn = 0;
    bool resolved_matches = false;
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

        out->max_health = pawn->m_iMaxHealth();
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

// Diagnostic-only pointer-identity check. It reuses TempleWare's already
// existing controller m_hPawn wrapper and EntitySystem::get_base_entity path.
// No new offsets, signatures, call arguments, or hooks are introduced here.
inline bool ProbePointerIdentity(CCSPlayerController* controller,
                                 std::uintptr_t expectedPawn,
                                 PointerIdentityProbe* out) {
    if (!out)
        return false;

    out->exception_code = 0;
    __try {
        if (!controller || !I::EntitySystem)
            return false;

        const CBaseHandle& pawnHandle = controller->m_hPawn();
        out->handle_valid = pawnHandle.valid();
        if (!out->handle_valid)
            return true;

        out->handle_index = pawnHandle.index();
        C_CSPlayerPawn* resolved =
            I::EntitySystem->get_base_entity<C_CSPlayerPawn>(out->handle_index);
        out->resolved_pawn = reinterpret_cast<std::uintptr_t>(resolved);
        out->resolved_matches = resolved && out->resolved_pawn == expectedPawn;
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

    const std::uint32_t maxHealthOffset =
        SchemaFinder::Get(hash_32_fnv1a_const("C_BaseEntity->m_iMaxHealth"));
    const std::uint32_t healthOffset =
        SchemaFinder::Get(hash_32_fnv1a_const("C_BaseEntity->m_iHealth"));
    const std::uint32_t teamOffset =
        SchemaFinder::Get(hash_32_fnv1a_const("C_BaseEntity->m_iTeamNum"));
    const std::uint32_t controllerPawnOffset =
        SchemaFinder::Get(hash_32_fnv1a_const("CBasePlayerController->m_hPawn"));
    const std::uint32_t localControllerOffset =
        SchemaFinder::Get(hash_32_fnv1a_const("CBasePlayerController->m_bIsLocalPlayerController"));
    const std::uint32_t pawnAliveOffset =
        SchemaFinder::Get(hash_32_fnv1a_const("CCSPlayerController->m_bPawnIsAlive"));

    if (!s_offsetsLoggedForPair) {
        char buf[576];
        std::snprintf(buf, sizeof(buf),
            "[P3D][SCHEMA] offsets max_health=0x%X health=0x%X team=0x%X hPawn=0x%X local=0x%X alive=0x%X pawn=%p controller=%p",
            maxHealthOffset,
            healthOffset,
            teamOffset,
            controllerPawnOffset,
            localControllerOffset,
            pawnAliveOffset,
            reinterpret_cast<void*>(snapshot.pawn),
            reinterpret_cast<void*>(snapshot.controller));
        FileLog::Log(buf);
        s_offsetsLoggedForPair = true;
    }

    const bool offsetsReady = maxHealthOffset && healthOffset && teamOffset &&
        controllerPawnOffset && localControllerOffset && pawnAliveOffset;

    if (!offsetsReady) {
        char buf[576];
        std::snprintf(buf, sizeof(buf),
            "[P3D][WRAPPER] BLOCKED - schema coverage incomplete max_health=0x%X health=0x%X team=0x%X hPawn=0x%X local=0x%X alive=0x%X; sdk_safe remains 0",
            maxHealthOffset,
            healthOffset,
            teamOffset,
            controllerPawnOffset,
            localControllerOffset,
            pawnAliveOffset);
        FileLog::Log(buf);
        s_pairBlocked = true;
        return;
    }

    PointerIdentityProbe pointerProbe{};
    const bool pointerCallOk = ProbePointerIdentity(
        reinterpret_cast<CCSPlayerController*>(snapshot.controller),
        snapshot.pawn,
        &pointerProbe);

    {
        char buf[576];
        std::snprintf(buf, sizeof(buf),
            "[P3D][PTR] controller-hPawn call_ok=%d valid=%d index=%d resolved=%p expected=%p match=%d exception=0x%08lX",
            pointerCallOk ? 1 : 0,
            pointerProbe.handle_valid ? 1 : 0,
            pointerProbe.handle_index,
            reinterpret_cast<void*>(pointerProbe.resolved_pawn),
            reinterpret_cast<void*>(snapshot.pawn),
            pointerProbe.resolved_matches ? 1 : 0,
            pointerProbe.exception_code);
        FileLog::Log(buf);
    }

    BasicWrapperProbe probe{};
    const bool callOk = ProbeBasicWrappers(
        reinterpret_cast<C_CSPlayerPawn*>(snapshot.pawn),
        reinterpret_cast<CCSPlayerController*>(snapshot.controller),
        &probe);

    const bool maxHealthSane = probe.max_health > 0 && probe.max_health <= 500;
    const bool healthSane = probe.health >= 0 && probe.health <= 500;
    const bool teamSane = probe.team <= 5;
    const bool localFlagSane = probe.local_controller;
    const bool aliveConsistent = probe.controller_alive == (probe.health > 0);

    // Pointer identity is diagnostic-only because get_base_entity has its own
    // independent resolver path. The semantic gate remains based on the wrapper
    // values themselves and does not silently trust this secondary lookup.
    const bool semanticPass = callOk && probe.exception_code == 0 &&
        maxHealthSane && healthSane && teamSane && localFlagSane &&
        aliveConsistent;

    if (!semanticPass) {
        char buf[896];
        std::snprintf(buf, sizeof(buf),
            "[P3D][WRAPPER] BLOCKED - resolver-proven pair is not layout-compatible with current basic schema wrappers max_health=%d health=%d team=%u local=%d ctrl_alive=%d max_health_ok=%d health_ok=%d team_ok=%d local_ok=%d alive_consistent=%d exception=0x%08lX; sdk_safe remains 0",
            probe.max_health,
            probe.health,
            static_cast<unsigned int>(probe.team),
            probe.local_controller ? 1 : 0,
            probe.controller_alive ? 1 : 0,
            maxHealthSane ? 1 : 0,
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
        char buf[640];
        std::snprintf(buf, sizeof(buf),
            "[P3D][WRAPPER] PROBE PASS - max_health=%d health=%d team=%u local=%d ctrl_alive=%d exception=0x%08lX",
            probe.max_health,
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
