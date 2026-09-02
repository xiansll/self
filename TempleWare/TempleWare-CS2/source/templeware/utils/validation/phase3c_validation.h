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

// Keep SEH in tiny leaf helpers that contain no C++ objects requiring unwind.
// The normal cached getters remain untouched. Fresh-candidate helpers below are
// diagnostic-only and use the exact call signatures/arguments already present
// in IEntitySystem.h; they do not publish or dereference returned objects.
using DiagnosticPawnFn = C_CSPlayerPawn* (__fastcall*)(int);
using DiagnosticControllerFn = void* (__fastcall*)(int);

inline C_CSPlayerPawn* SehGetLocalPawn(I_EntitySystem* entitySystem, DWORD* exceptionCode) {
    C_CSPlayerPawn* result = nullptr;
    if (exceptionCode)
        *exceptionCode = 0;

    __try {
        result = entitySystem ? entitySystem->get_local_pawn() : nullptr;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        if (exceptionCode)
            *exceptionCode = GetExceptionCode();
        result = nullptr;
    }

    return result;
}

inline CCSPlayerController* SehGetLocalController(I_EntitySystem* entitySystem, DWORD* exceptionCode) {
    CCSPlayerController* result = nullptr;
    if (exceptionCode)
        *exceptionCode = 0;

    __try {
        result = entitySystem
            ? reinterpret_cast<CCSPlayerController*>(entitySystem->get_local_controller())
            : nullptr;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        if (exceptionCode)
            *exceptionCode = GetExceptionCode();
        result = nullptr;
    }

    return result;
}

inline C_CSPlayerPawn* SehCallPawnCandidate(void* candidate, DWORD* exceptionCode) {
    C_CSPlayerPawn* result = nullptr;
    if (exceptionCode)
        *exceptionCode = 0;

    __try {
        const auto fn = reinterpret_cast<DiagnosticPawnFn>(candidate);
        result = fn ? fn(0) : nullptr;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        if (exceptionCode)
            *exceptionCode = GetExceptionCode();
        result = nullptr;
    }

    return result;
}

inline CCSPlayerController* SehCallControllerCandidate(void* candidate, DWORD* exceptionCode) {
    CCSPlayerController* result = nullptr;
    if (exceptionCode)
        *exceptionCode = 0;

    __try {
        const auto fn = reinterpret_cast<DiagnosticControllerFn>(candidate);
        result = fn ? reinterpret_cast<CCSPlayerController*>(fn(-1)) : nullptr;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        if (exceptionCode)
            *exceptionCode = GetExceptionCode();
        result = nullptr;
    }

    return result;
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
    // already-existing patterns.
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

    // When the strict scan succeeds but the one-shot cached resolver is null,
    // validate the fresh candidates directly under SEH. This gives us the final
    // evidence needed to distinguish a pure null-cache/init-order bug from a
    // callable-semantics problem, without enabling the normal SDK path.
    C_CSPlayerPawn* freshPawn = nullptr;
    CCSPlayerController* freshController = nullptr;
    DWORD freshPawnException = 0;
    DWORD freshControllerException = 0;

    if (pawnCandidate) {
        freshPawn = SehCallPawnCandidate(pawnCandidate, &freshPawnException);
        char buf[320];
        const bool match = freshPawn && snapshot.pawn &&
            reinterpret_cast<std::uintptr_t>(freshPawn) == snapshot.pawn;
        std::snprintf(buf, sizeof(buf),
            "candidate=%p result=%p reference=%p match=%d exception=0x%08lX",
            pawnCandidate,
            static_cast<void*>(freshPawn),
            reinterpret_cast<void*>(snapshot.pawn),
            match ? 1 : 0,
            freshPawnException);
        LogSimple("S1.2 fresh-pawn-call",
            freshPawnException ? "FAIL" : (match ? "PASS" : "FAIL"), buf);
    }
    else {
        LogSimple("S1.2 fresh-pawn-call", "SKIP", "fresh candidate missing");
    }

    if (controllerRaw) {
        freshController = SehCallControllerCandidate(controllerRaw, &freshControllerException);
        char buf[320];
        const bool match = freshController && snapshot.controller &&
            reinterpret_cast<std::uintptr_t>(freshController) == snapshot.controller;
        std::snprintf(buf, sizeof(buf),
            "candidate=%p result=%p reference=%p match=%d exception=0x%08lX",
            controllerRaw,
            static_cast<void*>(freshController),
            reinterpret_cast<void*>(snapshot.controller),
            match ? 1 : 0,
            freshControllerException);
        LogSimple("S1.3 fresh-controller-call",
            freshControllerException ? "FAIL" : (match ? "PASS" : "FAIL"), buf);
    }
    else {
        LogSimple("S1.3 fresh-controller-call", "SKIP", "fresh candidate missing");
    }

    const bool freshPawnMatches = freshPawn && snapshot.pawn &&
        reinterpret_cast<std::uintptr_t>(freshPawn) == snapshot.pawn;
    const bool freshControllerMatches = freshController && snapshot.controller &&
        reinterpret_cast<std::uintptr_t>(freshController) == snapshot.controller;
    const bool freshCallsReady = freshPawnMatches && freshControllerMatches &&
        freshPawnException == 0 && freshControllerException == 0;

    // Produce one useful root-cause classification instead of just another null
    // address. No signatures, offsets, call arguments, or hooks are updated here.
    if (!winClient) {
        LogSimple("DIAGNOSIS", "BLOCKER", "client.dll is not visible through Win32 module lookup");
    }
    else if (!registryClient) {
        LogSimple("DIAGNOSIS", "BLOCKER", "client module is loaded but TempleWare module registry has no 'client' entry");
    }
    else if (!pawnPatternReady || !controllerPatternReady) {
        LogSimple("DIAGNOSIS", "BLOCKER", "one or more existing resolver patterns do not match the currently loaded client module");
    }
    else if ((!pawnResolver || !controllerResolver) && freshCallsReady) {
        LogSimple("DIAGNOSIS", "CACHE BUG CONFIRMED", "fresh resolver candidates return the reference local pair while one-shot cached resolvers remain null");
    }
    else if (freshPawnException || freshControllerException) {
        LogSimple("DIAGNOSIS", "BLOCKER", "fresh resolver candidate raised an exception; callable semantics are not proven");
    }
    else if (!freshCallsReady && (!pawnResolver || !controllerResolver)) {
        LogSimple("DIAGNOSIS", "BLOCKER", "fresh resolver bytes exist but returned local pointers do not match the reference pair");
    }
    else if (!cacheParity) {
        LogSimple("DIAGNOSIS", "BLOCKER", "fresh resolver candidate differs from the cached resolver address");
    }
    else {
        LogSimple("DIAGNOSIS", "SCAN LAYER READY", "module, patterns, cached resolver addresses, and fresh-call parity are coherent");
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
        DWORD exceptionCode = 0;
        sdkPawn = SehGetLocalPawn(I::EntitySystem, &exceptionCode);
        pawnCallException = exceptionCode != 0;

        if (pawnCallException) {
            char buf[128];
            std::snprintf(buf, sizeof(buf), "exception=0x%08lX", exceptionCode);
            LogSimple("S3.1 EntitySystem::get_local_pawn", "FAIL", buf);
        }
        else if (sdkPawn) {
            char buf[224];
            const bool match = reinterpret_cast<std::uintptr_t>(sdkPawn) == snapshot.pawn;
            std::snprintf(buf, sizeof(buf),
                "result=%p reference=%p match=%d",
                static_cast<void*>(sdkPawn),
                reinterpret_cast<void*>(snapshot.pawn),
                match ? 1 : 0);
            LogSimple("S3.1 EntitySystem::get_local_pawn", match ? "PASS" : "FAIL", buf);
        }
        else {
            LogSimple("S3.1 EntitySystem::get_local_pawn", "FAIL", "result=null");
        }
    }
    else {
        LogSimple("S3.1 EntitySystem::get_local_pawn", "SKIP", "cached resolver address missing");
    }

    if (controllerResolver) {
        LogSimple("S3.2 EntitySystem::get_local_controller", "ENTER");
        DWORD exceptionCode = 0;
        sdkController = SehGetLocalController(I::EntitySystem, &exceptionCode);
        controllerCallException = exceptionCode != 0;

        if (controllerCallException) {
            char buf[128];
            std::snprintf(buf, sizeof(buf), "exception=0x%08lX", exceptionCode);
            LogSimple("S3.2 EntitySystem::get_local_controller", "FAIL", buf);
        }
        else if (sdkController) {
            char buf[224];
            const bool match = reinterpret_cast<std::uintptr_t>(sdkController) == snapshot.controller;
            std::snprintf(buf, sizeof(buf),
                "result=%p reference=%p match=%d",
                static_cast<void*>(sdkController),
                reinterpret_cast<void*>(snapshot.controller),
                match ? 1 : 0);
            LogSimple("S3.2 EntitySystem::get_local_controller", match ? "PASS" : "FAIL", buf);
        }
        else {
            LogSimple("S3.2 EntitySystem::get_local_controller", "FAIL", "result=null");
        }
    }
    else {
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

    const bool cachedGateReady = moduleReady && strictPatternsReady && cacheParity &&
        resolverAddressesReady && providerAlias && pawnMatches && controllerMatches &&
        !pawnCallException && !controllerCallException;

    if (cachedGateReady) {
        LogSimple("GATE", "PASS", "cached resolver provenance and local pointer parity are proven; deep wrapper validation remains separately disabled");
    }
    else if (moduleReady && strictPatternsReady && providerAlias && freshCallsReady) {
        LogSimple("GATE", "FRESH RESOLVER PROVEN", "fresh candidates match the local pair; cached resolver repair is the remaining SDK gate");
    }
    else {
        LogSimple("GATE", "BLOCKED", "resolver provenance is not fully proven; wrapper/identity/scene probes remain disabled");
    }
}

} // namespace Phase3C