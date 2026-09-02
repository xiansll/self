#pragma once

// Phase 3C is diagnostic-only. This checkpoint validates resolver provenance,
// scanner/module health, and pointer parity. It deliberately does NOT
// dereference pointer-only locals through TempleWare entity wrappers.

#include <Windows.h>
#include <cstdint>
#include <cstdio>

#include "../filelog/filelog.h"
#include "../localplayer/localplayer.h"
#include "../module/module.h"
#include "../../interfaces/interfaces.h"
#include "../../interfaces/IEntitySystem/IEntitySystem.h"

namespace Phase3C {

inline void LogSimple(const char* stage, const char* status, const char* detail = nullptr) {
    char buf[768];
    if (detail && *detail)
        std::snprintf(buf, sizeof(buf), "[P3C][%s] %s - %s", stage, status, detail);
    else
        std::snprintf(buf, sizeof(buf), "[P3C][%s] %s", stage, status);
    FileLog::Log(buf);
}

inline void Run(const LocalPlayerSnapshot& snapshot) {
    // Re-run only when the selected local pair changes. This keeps the heavier
    // strict scanner diagnostics out of the per-frame path.
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
    LogSimple("BEGIN", "RESOLVER/PROVENANCE GATE", beginBuf);

    const HMODULE winClient = GetModuleHandleA("client.dll");
    const std::uintptr_t registryClient = modules.getModule("client");
    char moduleBuf[320];
    std::snprintf(moduleBuf, sizeof(moduleBuf),
        "GetModuleHandle(client.dll)=%p registry(client)=%p",
        static_cast<void*>(winClient),
        reinterpret_cast<void*>(registryClient));
    const bool moduleReady = winClient != nullptr && registryClient != 0;
    LogSimple("S0 module-health", moduleReady ? "PASS" : "FAIL", moduleBuf);

    if (!I::EntitySystem) {
        LogSimple("S1 resolver-addresses", "FAIL", "I::EntitySystem is null");
        LogSimple("DIAGNOSIS", "BLOCKER", "entity-system interface missing; resolver scan analysis stopped");
        LogSimple("GATE", "BLOCKED", "deep SDK validation disabled");
        return;
    }

    // Cached addresses are the exact resolver values used by the normal SDK
    // getters. Raw/candidate values are fresh diagnostic scans using the same
    // already-existing patterns. None of these calls invoke a game function.
    void* pawnResolver = I::EntitySystem->diagnostic_local_pawn_resolver();
    void* controllerResolver = I::EntitySystem->diagnostic_local_controller_resolver();
    void* pawnRaw = I::EntitySystem->diagnostic_local_pawn_raw_match();
    void* pawnCandidate = I::EntitySystem->diagnostic_local_pawn_absolute_candidate();
    void* controllerRaw = I::EntitySystem->diagnostic_local_controller_raw_match();

    char scanBuf[640];
    std::snprintf(scanBuf, sizeof(scanBuf),
        "pawn_raw=%p pawn_candidate=%p pawn_cached=%p ctrl_raw=%p ctrl_cached=%p",
        pawnRaw, pawnCandidate, pawnResolver, controllerRaw, controllerResolver);

    const bool pawnPatternReady = pawnRaw != nullptr && pawnCandidate != nullptr;
    const bool controllerPatternReady = controllerRaw != nullptr;
    const bool strictPatternsReady = pawnPatternReady && controllerPatternReady;
    LogSimple("S1 strict-scan", strictPatternsReady ? "PASS" : "FAIL", scanBuf);

    const bool pawnCacheParity = pawnResolver && pawnCandidate && pawnResolver == pawnCandidate;
    const bool controllerCacheParity = controllerResolver && controllerRaw && controllerResolver == controllerRaw;
    const bool cacheParity = pawnCacheParity && controllerCacheParity;

    char parityBuf[256];
    std::snprintf(parityBuf, sizeof(parityBuf),
        "pawn_cache_match=%d controller_cache_match=%d",
        pawnCacheParity ? 1 : 0,
        controllerCacheParity ? 1 : 0);
    LogSimple("S1.1 cache-parity", cacheParity ? "PASS" : "FAIL", parityBuf);

    // Produce one useful root-cause classification instead of just another null
    // address. This is intentionally conservative: it identifies the failing
    // layer but does not update signatures, offsets, call arguments, or hooks.
    if (!winClient) {
        LogSimple("DIAGNOSIS", "BLOCKER", "client.dll is not visible through Win32 module lookup");
    }
    else if (!registryClient) {
        LogSimple("DIAGNOSIS", "BLOCKER", "client module is loaded but TempleWare module registry has no 'client' entry");
    }
    else if (!pawnPatternReady && pawnResolver) {
        LogSimple("DIAGNOSIS", "BLOCKER", "pawn strict scan misses while cached resolver is non-null; scanner false-positive/stale-cache path suspected");
    }
    else if (!pawnPatternReady || !controllerPatternReady) {
        LogSimple("DIAGNOSIS", "BLOCKER", "one or more existing resolver patterns do not match the currently loaded client module");
    }
    else if (!pawnResolver || !controllerResolver) {
        LogSimple("DIAGNOSIS", "BLOCKER", "fresh raw patterns resolve but cached resolver is null; one-shot null cache/init-order path suspected");
    }
    else if (!cacheParity) {
        LogSimple("DIAGNOSIS", "BLOCKER", "fresh resolver candidate differs from the cached resolver address");
    }
    else {
        LogSimple("DIAGNOSIS", "SCAN LAYER READY", "module, strict patterns, and cached resolver addresses are coherent; pointer-call parity is next gate");
    }

    char resolverBuf[256];
    std::snprintf(resolverBuf, sizeof(resolverBuf),
        "pawn=%p controller=%p", pawnResolver, controllerResolver);

    const bool resolverAddressesReady = pawnResolver && controllerResolver;
    LogSimple("S2 resolver-addresses", resolverAddressesReady ? "PASS" : "FAIL", resolverBuf);

    C_CSPlayerPawn* sdkPawn = nullptr;
    CCSPlayerController* sdkController = nullptr;
    bool pawnCallException = false;
    bool controllerCallException = false;

    if (pawnResolver) {
        LogSimple("S3.1 EntitySystem::get_local_pawn", "ENTER");
        __try {
            sdkPawn = I::EntitySystem->get_local_pawn();
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            pawnCallException = true;
            char buf[128];
            std::snprintf(buf, sizeof(buf), "exception=0x%08lX", GetExceptionCode());
            LogSimple("S3.1 EntitySystem::get_local_pawn", "FAIL", buf);
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
                LogSimple("S3.1 EntitySystem::get_local_pawn", match ? "PASS" : "FAIL", buf);
            } else {
                LogSimple("S3.1 EntitySystem::get_local_pawn", "FAIL", "result=null");
            }
        }
    } else {
        LogSimple("S3.1 EntitySystem::get_local_pawn", "SKIP", "cached resolver address missing");
    }

    if (controllerResolver) {
        LogSimple("S3.2 EntitySystem::get_local_controller", "ENTER");
        __try {
            sdkController = reinterpret_cast<CCSPlayerController*>(I::EntitySystem->get_local_controller());
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            controllerCallException = true;
            char buf[128];
            std::snprintf(buf, sizeof(buf), "exception=0x%08lX", GetExceptionCode());
            LogSimple("S3.2 EntitySystem::get_local_controller", "FAIL", buf);
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
                LogSimple("S3.2 EntitySystem::get_local_controller", match ? "PASS" : "FAIL", buf);
            } else {
                LogSimple("S3.2 EntitySystem::get_local_controller", "FAIL", "result=null");
            }
        }
    } else {
        LogSimple("S3.2 EntitySystem::get_local_controller", "SKIP", "cached resolver address missing");
    }

    const bool providerAlias = I::GameEntity &&
        reinterpret_cast<std::uintptr_t>(I::GameEntity->Instance) ==
        reinterpret_cast<std::uintptr_t>(I::EntitySystem);
    LogSimple("S3.3 provider-alias", providerAlias ? "PASS" : "FAIL",
        providerAlias ? "GameEntity.Instance == I::EntitySystem" : "providers differ");

    const bool pawnMatches = sdkPawn &&
        reinterpret_cast<std::uintptr_t>(sdkPawn) == snapshot.pawn;
    const bool controllerMatches = sdkController &&
        reinterpret_cast<std::uintptr_t>(sdkController) == snapshot.controller;

    const bool gateReady = moduleReady && strictPatternsReady && cacheParity &&
        resolverAddressesReady && providerAlias && pawnMatches && controllerMatches &&
        !pawnCallException && !controllerCallException;

    if (gateReady) {
        LogSimple("GATE", "PASS", "resolver provenance and local pointer parity are proven; deep wrapper validation remains separately disabled");
    } else {
        LogSimple("GATE", "BLOCKED", "resolver provenance is not fully proven; wrapper/identity/scene probes remain disabled");
    }
}

} // namespace Phase3C
