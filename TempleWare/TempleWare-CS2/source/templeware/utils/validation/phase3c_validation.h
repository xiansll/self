#pragma once

// Phase 3C is diagnostic-only. It does not install hooks, change signatures,
// write game state, or enable gameplay features. Every potentially unsafe
// TempleWare wrapper call is isolated behind a stage marker and SEH guard so
// the log identifies the exact failing call instead of taking the process down.

#include <Windows.h>
#include <cstdint>
#include <cstdio>

#include "validation.h"
#include "../filelog/filelog.h"
#include "../localplayer/localplayer.h"
#include "../../interfaces/interfaces.h"
#include "../../interfaces/IEntitySystem/IEntitySystem.h"
#include "../../../cs2/entity/C_CSPlayerPawn/C_CSPlayerPawn.h"
#include "../../../cs2/entity/CCSPlayerController/CCSPlayerController.h"
#include "../../../cs2/entity/C_EntityInstance/C_EntityInstance.h"

namespace Phase3C {

struct RunStats {
    int logical_failures = 0;
    int exceptions = 0;
    int skips = 0;
};

inline void LogSimple(const char* stage, const char* status, const char* detail = nullptr) {
    char buf[640];
    if (detail && *detail) {
        std::snprintf(buf, sizeof(buf), "[P3C][%s] %s - %s", stage, status, detail);
    } else {
        std::snprintf(buf, sizeof(buf), "[P3C][%s] %s", stage, status);
    }
    FileLog::Log(buf);
}

inline void LogException(const char* stage, unsigned long code, RunStats& stats) {
    char buf[256];
    std::snprintf(buf, sizeof(buf), "exception=0x%08lX", code);
    LogSimple(stage, "FAIL", buf);
    ++stats.exceptions;
}

inline bool ProbePawnBasics(C_CSPlayerPawn* pawn, RunStats& stats) {
    if (!pawn) {
        LogSimple("S3.PAWN", "SKIP", "reference pawn is null");
        ++stats.skips;
        return false;
    }

    bool safe = true;

    LogSimple("S3.1 pawn.m_iTeamNum", "ENTER");
    __try {
        const int team = static_cast<int>(pawn->m_iTeamNum());
        char buf[128];
        std::snprintf(buf, sizeof(buf), "value=%d", team);
        LogSimple("S3.1 pawn.m_iTeamNum", "PASS", buf);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        LogException("S3.1 pawn.m_iTeamNum", GetExceptionCode(), stats);
        safe = false;
    }

    LogSimple("S3.2 pawn.m_iHealth", "ENTER");
    __try {
        const int health = pawn->m_iHealth();
        char buf[128];
        std::snprintf(buf, sizeof(buf), "value=%d", health);
        LogSimple("S3.2 pawn.m_iHealth", "PASS", buf);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        LogException("S3.2 pawn.m_iHealth", GetExceptionCode(), stats);
        safe = false;
    }

    LogSimple("S3.3 pawn.is_alive", "ENTER");
    __try {
        const bool alive = pawn->is_alive();
        LogSimple("S3.3 pawn.is_alive", "PASS", alive ? "value=1" : "value=0");
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        LogException("S3.3 pawn.is_alive", GetExceptionCode(), stats);
        safe = false;
    }

    return safe;
}

inline bool ProbeControllerBasics(CCSPlayerController* controller, CBaseHandle& observerHandle, RunStats& stats) {
    observerHandle = CBaseHandle();
    if (!controller) {
        LogSimple("S4.CONTROLLER", "SKIP", "reference controller is null");
        ++stats.skips;
        return false;
    }

    bool safe = true;

    LogSimple("S4.1 controller.m_iDesiredTeam", "ENTER");
    __try {
        const int desiredTeam = controller->m_iDesiredTeam();
        char buf[128];
        std::snprintf(buf, sizeof(buf), "value=%d", desiredTeam);
        LogSimple("S4.1 controller.m_iDesiredTeam", "PASS", buf);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        LogException("S4.1 controller.m_iDesiredTeam", GetExceptionCode(), stats);
        safe = false;
    }

    LogSimple("S4.2 controller.m_hObserverPawn", "ENTER");
    __try {
        observerHandle = controller->m_hObserverPawn();
        char buf[192];
        std::snprintf(buf, sizeof(buf), "valid=%d index=%d serial=%d",
            observerHandle.valid() ? 1 : 0,
            observerHandle.index(),
            observerHandle.serial_number());
        LogSimple("S4.2 controller.m_hObserverPawn", "PASS", buf);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        LogException("S4.2 controller.m_hObserverPawn", GetExceptionCode(), stats);
        safe = false;
    }

    return safe;
}

inline bool ProbeIdentity(const char* stage, CEntityInstance* entity, RunStats& stats) {
    if (!entity) {
        LogSimple(stage, "SKIP", "entity is null");
        ++stats.skips;
        return false;
    }

    const auto beforeChecks = Validation::g_counters.entity_identity_checks.load(std::memory_order_relaxed);
    const auto beforeMismatch = Validation::g_counters.entity_identity_mismatches.load(std::memory_order_relaxed);

    LogSimple(stage, "ENTER");
    __try {
        Validation::OnEntityIdentityCheck(entity);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        LogException(stage, GetExceptionCode(), stats);
        return false;
    }

    const auto afterChecks = Validation::g_counters.entity_identity_checks.load(std::memory_order_relaxed);
    const auto afterMismatch = Validation::g_counters.entity_identity_mismatches.load(std::memory_order_relaxed);
    if (afterChecks <= beforeChecks) {
        LogSimple(stage, "FAIL", "identity check did not execute");
        ++stats.logical_failures;
        return false;
    }
    if (afterMismatch > beforeMismatch) {
        LogSimple(stage, "FAIL", "identity mismatch counter increased");
        ++stats.logical_failures;
        return false;
    }

    LogSimple(stage, "PASS");
    return true;
}

inline bool ProbeScene(C_CSPlayerPawn* pawn, RunStats& stats) {
    if (!pawn) {
        LogSimple("S6 scene-chain", "SKIP", "pawn is null");
        ++stats.skips;
        return false;
    }

    const auto beforeChecks = Validation::g_counters.scene_node_chain_checks.load(std::memory_order_relaxed);
    const auto beforeFailures = Validation::g_counters.scene_node_chain_failures.load(std::memory_order_relaxed);

    LogSimple("S6 Validation::OnSceneNodeChainCheck", "ENTER");
    __try {
        Validation::OnSceneNodeChainCheck(pawn);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        LogException("S6 Validation::OnSceneNodeChainCheck", GetExceptionCode(), stats);
        return false;
    }

    const auto afterChecks = Validation::g_counters.scene_node_chain_checks.load(std::memory_order_relaxed);
    const auto afterFailures = Validation::g_counters.scene_node_chain_failures.load(std::memory_order_relaxed);
    if (afterChecks <= beforeChecks) {
        LogSimple("S6 Validation::OnSceneNodeChainCheck", "FAIL", "scene check did not execute");
        ++stats.logical_failures;
        return false;
    }
    if (afterFailures > beforeFailures) {
        LogSimple("S6 Validation::OnSceneNodeChainCheck", "FAIL", "scene failure counter increased");
        ++stats.logical_failures;
        return false;
    }

    LogSimple("S6 Validation::OnSceneNodeChainCheck", "PASS");
    return true;
}

inline void ProbeObserverHandle(const CBaseHandle& observerHandle, RunStats& stats) {
    if (!observerHandle.valid()) {
        LogSimple("S7 observer-handle resolve", "SKIP", "observer handle is not active in this state");
        ++stats.skips;
        return;
    }
    if (!I::EntitySystem) {
        LogSimple("S7 observer-handle resolve", "FAIL", "I::EntitySystem is null");
        ++stats.logical_failures;
        return;
    }

    C_CSPlayerPawn* resolved = nullptr;
    LogSimple("S7.1 EntitySystem::get_base_entity(observer)", "ENTER");
    __try {
        resolved = I::EntitySystem->get_base_entity<C_CSPlayerPawn>(observerHandle.index());
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        LogException("S7.1 EntitySystem::get_base_entity(observer)", GetExceptionCode(), stats);
        return;
    }

    if (!resolved) {
        LogSimple("S7.1 EntitySystem::get_base_entity(observer)", "FAIL", "resolved=null");
        ++stats.logical_failures;
        return;
    }

    char ptrBuf[128];
    std::snprintf(ptrBuf, sizeof(ptrBuf), "resolved=%p", static_cast<void*>(resolved));
    LogSimple("S7.1 EntitySystem::get_base_entity(observer)", "PASS", ptrBuf);

    CBaseHandle entityHandle;
    LogSimple("S7.2 resolved->handle", "ENTER");
    __try {
        entityHandle = resolved->handle();
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        LogException("S7.2 resolved->handle", GetExceptionCode(), stats);
        return;
    }

    LogSimple("S7.2 resolved->handle", "PASS");
    Validation::OnEntityHandleLookup(observerHandle, resolved, entityHandle);
}

inline void Run(const LocalPlayerSnapshot& snapshot) {
    // Re-run the complete staged suite only when the local pair changes. This
    // gives one concise trace at spawn/respawn without per-frame diagnostic spam.
    static std::uintptr_t s_lastPawn = 0;
    static std::uintptr_t s_lastController = 0;
    static unsigned int s_run = 0;

    if (!snapshot.pawn && !snapshot.controller)
        return;
    if (snapshot.pawn == s_lastPawn && snapshot.controller == s_lastController)
        return;

    s_lastPawn = snapshot.pawn;
    s_lastController = snapshot.controller;
    ++s_run;

    RunStats stats{};
    char beginBuf[320];
    std::snprintf(beginBuf, sizeof(beginBuf),
        "run=%u pawn=%p controller=%p cache_sdk_safe=%d",
        s_run,
        reinterpret_cast<void*>(snapshot.pawn),
        reinterpret_cast<void*>(snapshot.controller),
        snapshot.sdk_deref_safe ? 1 : 0);
    LogSimple("BEGIN", "FULL SUITE", beginBuf);

    C_CSPlayerPawn* sdkPawn = nullptr;
    CCSPlayerController* sdkController = nullptr;

    // S1: resolver address health. Address existence is separate from call semantics.
    if (!I::EntitySystem) {
        LogSimple("S1 resolver-addresses", "FAIL", "I::EntitySystem is null");
        ++stats.logical_failures;
    } else {
        void* pawnResolver = I::EntitySystem->diagnostic_local_pawn_resolver();
        void* controllerResolver = I::EntitySystem->diagnostic_local_controller_resolver();
        char resolverBuf[256];
        std::snprintf(resolverBuf, sizeof(resolverBuf), "pawn=%p controller=%p", pawnResolver, controllerResolver);
        if (pawnResolver && controllerResolver) {
            LogSimple("S1 resolver-addresses", "PASS", resolverBuf);
        } else {
            LogSimple("S1 resolver-addresses", "FAIL", resolverBuf);
            ++stats.logical_failures;
        }
    }

    // S2: call the existing local getters under isolated guards and compare the
    // returned pointers with the already-proven pointer-only local references.
    if (I::EntitySystem) {
        LogSimple("S2.1 EntitySystem::get_local_pawn", "ENTER");
        __try {
            sdkPawn = I::EntitySystem->get_local_pawn();
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            LogException("S2.1 EntitySystem::get_local_pawn", GetExceptionCode(), stats);
            sdkPawn = nullptr;
        }
        if (sdkPawn) {
            char buf[192];
            std::snprintf(buf, sizeof(buf), "result=%p reference=%p match=%d",
                static_cast<void*>(sdkPawn), reinterpret_cast<void*>(snapshot.pawn),
                reinterpret_cast<std::uintptr_t>(sdkPawn) == snapshot.pawn ? 1 : 0);
            LogSimple("S2.1 EntitySystem::get_local_pawn", "PASS", buf);
        } else {
            LogSimple("S2.1 EntitySystem::get_local_pawn", "FAIL", "result=null");
            ++stats.logical_failures;
        }

        LogSimple("S2.2 EntitySystem::get_local_controller", "ENTER");
        __try {
            sdkController = reinterpret_cast<CCSPlayerController*>(I::EntitySystem->get_local_controller());
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            LogException("S2.2 EntitySystem::get_local_controller", GetExceptionCode(), stats);
            sdkController = nullptr;
        }
        if (sdkController) {
            char buf[192];
            std::snprintf(buf, sizeof(buf), "result=%p reference=%p match=%d",
                static_cast<void*>(sdkController), reinterpret_cast<void*>(snapshot.controller),
                reinterpret_cast<std::uintptr_t>(sdkController) == snapshot.controller ? 1 : 0);
            LogSimple("S2.2 EntitySystem::get_local_controller", "PASS", buf);
        } else {
            LogSimple("S2.2 EntitySystem::get_local_controller", "FAIL", "result=null");
            ++stats.logical_failures;
        }
    } else {
        LogSimple("S2 local-getter calls", "SKIP", "I::EntitySystem is null");
        ++stats.skips;
    }

    const bool providerAlias = I::GameEntity &&
        reinterpret_cast<std::uintptr_t>(I::GameEntity->Instance) ==
        reinterpret_cast<std::uintptr_t>(I::EntitySystem);
    LogSimple("S2.3 provider-alias", providerAlias ? "PASS" : "FAIL",
        providerAlias ? "GameEntity.Instance == I::EntitySystem" : "providers differ");
    if (!providerAlias)
        ++stats.logical_failures;

    // S3/S4 deliberately use the proven reference pointers even when S2 returns
    // null. This separates resolver-call failure from wrapper/layout failure.
    auto* referencePawn = reinterpret_cast<C_CSPlayerPawn*>(snapshot.pawn);
    auto* referenceController = reinterpret_cast<CCSPlayerController*>(snapshot.controller);
    const bool pawnBasicSafe = ProbePawnBasics(referencePawn, stats);

    CBaseHandle observerHandle;
    const bool controllerBasicSafe = ProbeControllerBasics(referenceController, observerHandle, stats);

    // S5: entity identity is only attempted when the corresponding basic wrapper
    // reads did not raise an access violation.
    if (pawnBasicSafe)
        ProbeIdentity("S5.1 Validation::OnEntityIdentityCheck(pawn)", reinterpret_cast<CEntityInstance*>(referencePawn), stats);
    else {
        LogSimple("S5.1 Validation::OnEntityIdentityCheck(pawn)", "SKIP", "pawn basic wrapper probe failed");
        ++stats.skips;
    }

    if (controllerBasicSafe)
        ProbeIdentity("S5.2 Validation::OnEntityIdentityCheck(controller)", reinterpret_cast<CEntityInstance*>(referenceController), stats);
    else {
        LogSimple("S5.2 Validation::OnEntityIdentityCheck(controller)", "SKIP", "controller basic wrapper probe failed");
        ++stats.skips;
    }

    // S6: scene/skeleton/bone-cache chain.
    if (pawnBasicSafe)
        ProbeScene(referencePawn, stats);
    else {
        LogSimple("S6 Validation::OnSceneNodeChainCheck", "SKIP", "pawn basic wrapper probe failed");
        ++stats.skips;
    }

    // S7 is state-dependent: alive local players commonly have no observer pawn.
    if (controllerBasicSafe)
        ProbeObserverHandle(observerHandle, stats);
    else {
        LogSimple("S7 observer-handle resolve", "SKIP", "controller basic wrapper probe failed");
        ++stats.skips;
    }

    char endBuf[256];
    const char* overall = (stats.exceptions == 0 && stats.logical_failures == 0) ? "PASS" : "FAIL";
    std::snprintf(endBuf, sizeof(endBuf), "run=%u result=%s logical_failures=%d exceptions=%d skips=%d",
        s_run, overall, stats.logical_failures, stats.exceptions, stats.skips);
    LogSimple("COMPLETE", overall, endBuf);
}

} // namespace Phase3C
