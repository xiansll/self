#pragma once

// Phase 3C is diagnostic-only. This checkpoint validates only resolver health
// and pointer parity. It deliberately does NOT dereference pointer-only locals
// through TempleWare entity wrappers. Deeper wrapper/identity/scene validation
// is enabled only in a later checkpoint after the SDK resolvers are proven.

#include <Windows.h>
#include <cstdint>
#include <cstdio>

#include "../filelog/filelog.h"
#include "../localplayer/localplayer.h"
#include "../../interfaces/interfaces.h"
#include "../../interfaces/IEntitySystem/IEntitySystem.h"

namespace Phase3C {

inline void LogSimple(const char* stage, const char* status, const char* detail = nullptr) {
    char buf[640];
    if (detail && *detail)
        std::snprintf(buf, sizeof(buf), "[P3C][%s] %s - %s", stage, status, detail);
    else
        std::snprintf(buf, sizeof(buf), "[P3C][%s] %s", stage, status);
    FileLog::Log(buf);
}

inline void Run(const LocalPlayerSnapshot& snapshot) {
    // Re-run only when the selected local pair changes. This gives one concise
    // checkpoint trace at spawn/respawn without per-frame diagnostic spam.
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

    char beginBuf[320];
    std::snprintf(beginBuf, sizeof(beginBuf),
        "run=%u pawn=%p controller=%p cache_sdk_safe=%d",
        s_run,
        reinterpret_cast<void*>(snapshot.pawn),
        reinterpret_cast<void*>(snapshot.controller),
        snapshot.sdk_deref_safe ? 1 : 0);
    LogSimple("BEGIN", "RESOLVER GATE", beginBuf);

    if (!I::EntitySystem) {
        LogSimple("S1 resolver-addresses", "FAIL", "I::EntitySystem is null");
        LogSimple("GATE", "BLOCKED", "deep SDK validation disabled");
        return;
    }

    void* pawnResolver = I::EntitySystem->diagnostic_local_pawn_resolver();
    void* controllerResolver = I::EntitySystem->diagnostic_local_controller_resolver();

    char resolverBuf[256];
    std::snprintf(resolverBuf, sizeof(resolverBuf),
        "pawn=%p controller=%p", pawnResolver, controllerResolver);

    const bool resolverAddressesReady = pawnResolver && controllerResolver;
    LogSimple("S1 resolver-addresses", resolverAddressesReady ? "PASS" : "FAIL", resolverBuf);

    C_CSPlayerPawn* sdkPawn = nullptr;
    CCSPlayerController* sdkController = nullptr;
    bool pawnCallException = false;
    bool controllerCallException = false;

    if (pawnResolver) {
        LogSimple("S2.1 EntitySystem::get_local_pawn", "ENTER");
        __try {
            sdkPawn = I::EntitySystem->get_local_pawn();
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            pawnCallException = true;
            char buf[128];
            std::snprintf(buf, sizeof(buf), "exception=0x%08lX", GetExceptionCode());
            LogSimple("S2.1 EntitySystem::get_local_pawn", "FAIL", buf);
        }

        if (!pawnCallException) {
            if (sdkPawn) {
                char buf[224];
                const bool match = reinterpret_cast<std::uintptr_t>(sdkPawn) == snapshot.pawn;
                std::snprintf(buf, sizeof(buf),
                    "result=%p reference=%p match=%d",
                    static_cast<void*>(sdkPawn),
                    reinterpret_cast<void*>(snapshot.pawn),
                    match ? 1 : 0);
                LogSimple("S2.1 EntitySystem::get_local_pawn", match ? "PASS" : "FAIL", buf);
            } else {
                LogSimple("S2.1 EntitySystem::get_local_pawn", "FAIL", "result=null");
            }
        }
    } else {
        LogSimple("S2.1 EntitySystem::get_local_pawn", "SKIP", "resolver address missing");
    }

    if (controllerResolver) {
        LogSimple("S2.2 EntitySystem::get_local_controller", "ENTER");
        __try {
            sdkController = reinterpret_cast<CCSPlayerController*>(I::EntitySystem->get_local_controller());
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            controllerCallException = true;
            char buf[128];
            std::snprintf(buf, sizeof(buf), "exception=0x%08lX", GetExceptionCode());
            LogSimple("S2.2 EntitySystem::get_local_controller", "FAIL", buf);
        }

        if (!controllerCallException) {
            if (sdkController) {
                char buf[224];
                const bool match = reinterpret_cast<std::uintptr_t>(sdkController) == snapshot.controller;
                std::snprintf(buf, sizeof(buf),
                    "result=%p reference=%p match=%d",
                    static_cast<void*>(sdkController),
                    reinterpret_cast<void*>(snapshot.controller),
                    match ? 1 : 0);
                LogSimple("S2.2 EntitySystem::get_local_controller", match ? "PASS" : "FAIL", buf);
            } else {
                LogSimple("S2.2 EntitySystem::get_local_controller", "FAIL", "result=null");
            }
        }
    } else {
        LogSimple("S2.2 EntitySystem::get_local_controller", "SKIP", "resolver address missing");
    }

    const bool providerAlias = I::GameEntity &&
        reinterpret_cast<std::uintptr_t>(I::GameEntity->Instance) ==
        reinterpret_cast<std::uintptr_t>(I::EntitySystem);
    LogSimple("S2.3 provider-alias", providerAlias ? "PASS" : "FAIL",
        providerAlias ? "GameEntity.Instance == I::EntitySystem" : "providers differ");

    const bool pawnMatches = sdkPawn &&
        reinterpret_cast<std::uintptr_t>(sdkPawn) == snapshot.pawn;
    const bool controllerMatches = sdkController &&
        reinterpret_cast<std::uintptr_t>(sdkController) == snapshot.controller;

    const bool gateReady = resolverAddressesReady && providerAlias &&
        pawnMatches && controllerMatches &&
        !pawnCallException && !controllerCallException;

    if (gateReady) {
        LogSimple("GATE", "PASS", "SDK resolver pair matches reference locals; deeper validation remains disabled until next checkpoint");
    } else {
        LogSimple("GATE", "BLOCKED", "SDK resolver pair is not proven; S3-S7 wrapper/identity/scene probes disabled");
    }
}

} // namespace Phase3C
