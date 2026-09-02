#pragma once

// Phase 3D validates only a deliberately small, read-only wrapper surface after
// P3C has already proven local resolver provenance. It does not traverse scene
// nodes, skeletons, hitboxes, commands, or mutate runtime state. Class-info and
// direct-offset probes below reuse only offsets/resolvers already present in the
// project so wrapper mechanics can be separated from object/layout semantics.

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

struct DirectOffsetProbe {
    int max_health = 0;
    int health = 0;
    std::uint8_t team = 0;
    std::uint32_t pawn_handle_raw = 0;
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

struct ClassIdentityProbe {
    char pawn_class[64]{};
    char controller_class[64]{};
    std::uintptr_t pawn_vtable = 0;
    std::uintptr_t controller_vtable = 0;
    bool pawn_info_ok = false;
    bool controller_info_ok = false;
    DWORD exception_code = 0;
};

inline void CopyClassName(char (&dst)[64], const char* src) {
    if (!src)
        return;

    unsigned int i = 0;
    for (; i < 63 && src[i] != '\0'; ++i)
        dst[i] = src[i];
    dst[i] = '\0';
}

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

// Read the exact same already-resolved project offsets without going through
// the accessor-local cache. This is diagnostic-only and lets us distinguish a
// stale/zero wrapper cache from a genuinely incompatible runtime layout.
inline bool ProbeDirectOffsets(std::uintptr_t pawn,
                               std::uintptr_t controller,
                               std::uint32_t maxHealthOffset,
                               std::uint32_t healthOffset,
                               std::uint32_t teamOffset,
                               std::uint32_t controllerPawnOffset,
                               std::uint32_t localControllerOffset,
                               std::uint32_t pawnAliveOffset,
                               DirectOffsetProbe* out) {
    if (!out)
        return false;

    out->exception_code = 0;
    __try {
        if (!pawn || !controller)
            return false;

        out->max_health = *reinterpret_cast<const int*>(pawn + maxHealthOffset);
        out->health = *reinterpret_cast<const int*>(pawn + healthOffset);
        out->team = *reinterpret_cast<const std::uint8_t*>(pawn + teamOffset);
        out->pawn_handle_raw = *reinterpret_cast<const std::uint32_t*>(controller + controllerPawnOffset);
        out->local_controller = *reinterpret_cast<const bool*>(controller + localControllerOffset);
        out->controller_alive = *reinterpret_cast<const bool*>(controller + pawnAliveOffset);
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

// Existing-vfunc diagnostic only. This does not enable the deep graph gate and
// does not trust any field value. If the resolver returns the wrong object type,
// the reported schema class names should expose that directly.
inline bool ProbeClassIdentity(C_CSPlayerPawn* pawn,
                               CCSPlayerController* controller,
                               ClassIdentityProbe* out) {
    if (!out)
        return false;

    out->exception_code = 0;
    __try {
        if (!pawn || !controller)
            return false;

        out->pawn_vtable = *reinterpret_cast<std::uintptr_t*>(pawn);
        out->controller_vtable = *reinterpret_cast<std::uintptr_t*>(controller);

        SchemaClassInfoData_t* pawnInfo = nullptr;
        pawn->dump_class_info(&pawnInfo);
        if (pawnInfo && pawnInfo->szName) {
            CopyClassName(out->pawn_class, pawnInfo->szName);
            out->pawn_info_ok = out->pawn_class[0] != '\0';
        }

        SchemaClassInfoData_t* controllerInfo = nullptr;
        controller->dump_class_info(&controllerInfo);
        if (controllerInfo && controllerInfo->szName) {
            CopyClassName(out->controller_class, controllerInfo->szName);
            out->controller_info_ok = out->controller_class[0] != '\0';
        }

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

    ClassIdentityProbe classProbe{};
    const bool classCallOk = ProbeClassIdentity(
        reinterpret_cast<C_CSPlayerPawn*>(snapshot.pawn),
        reinterpret_cast<CCSPlayerController*>(snapshot.controller),
        &classProbe);

    {
        char buf[640];
        std::snprintf(buf, sizeof(buf),
            "[P3D][CLASS] call_ok=%d pawn_class=%s pawn_info=%d pawn_vtbl=%p ctrl_class=%s ctrl_info=%d ctrl_vtbl=%p exception=0x%08lX",
            classCallOk ? 1 : 0,
            classProbe.pawn_info_ok ? classProbe.pawn_class : "<unresolved>",
            classProbe.pawn_info_ok ? 1 : 0,
            reinterpret_cast<void*>(classProbe.pawn_vtable),
            classProbe.controller_info_ok ? classProbe.controller_class : "<unresolved>",
            classProbe.controller_info_ok ? 1 : 0,
            reinterpret_cast<void*>(classProbe.controller_vtable),
            classProbe.exception_code);
        FileLog::Log(buf);
    }

    DirectOffsetProbe rawProbe{};
    const bool rawCallOk = ProbeDirectOffsets(
        snapshot.pawn,
        snapshot.controller,
        maxHealthOffset,
        healthOffset,
        teamOffset,
        controllerPawnOffset,
        localControllerOffset,
        pawnAliveOffset,
        &rawProbe);

    {
        char buf[640];
        std::snprintf(buf, sizeof(buf),
            "[P3D][RAW] call_ok=%d max_health=%d health=%d team=%u hPawn=0x%08X hPawn_index=%u local=%d ctrl_alive=%d exception=0x%08lX",
            rawCallOk ? 1 : 0,
            rawProbe.max_health,
            rawProbe.health,
            static_cast<unsigned int>(rawProbe.team),
            rawProbe.pawn_handle_raw,
            rawProbe.pawn_handle_raw & ENT_ENTRY_MASK,
            rawProbe.local_controller ? 1 : 0,
            rawProbe.controller_alive ? 1 : 0,
            rawProbe.exception_code);
        FileLog::Log(buf);
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
