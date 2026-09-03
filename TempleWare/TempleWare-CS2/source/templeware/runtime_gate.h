#pragma once

// Crash-safe runtime readiness gate.
//
// Features run ONLY after the gate is READY: entity system, pawn,
// controller and user-cmd have been valid for kRequiredStableTicks
// consecutive CreateMove calls with stable pointer values.
//
// The gate takes a FRESH snapshot each tick from the SDK (independent
// of FrameStageNotify). On success it publishes validated pointers to
// g_ctx so every consumer reads the same proven state.

#include "interfaces/interfaces.h"
#include "globals/globals.h"
#include "utils/filelog/filelog.h"

#include <atomic>
#include <cstdint>
#include <cstdio>

namespace RuntimeGate
{
    inline constexpr int kRequiredStableTicks = 3;

    inline std::atomic<bool> g_ready{false};
    inline int  g_stableCount   = 0;
    inline uintptr_t g_lastPawn = 0;
    inline uintptr_t g_lastCtrl = 0;

    // --- SEH-safe SDK wrappers (separate functions to avoid C2712) ---

    inline bool TryGetLocalPawn(C_CSPlayerPawn** out) noexcept
    {
        *out = nullptr;
        if (!I::EntitySystem) return false;
        __try { *out = I::EntitySystem->get_local_pawn(); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        return *out != nullptr;
    }

    inline bool TryGetHealth(C_CSPlayerPawn* p, int* out) noexcept
    {
        *out = 0;
        __try { *out = p->m_iHealth(); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        return true;
    }

    inline bool TryGetController(C_CSPlayerPawn* p, CCSPlayerController** out) noexcept
    {
        *out = nullptr;
        __try {
            auto h = p->m_hController();
            *out = I::GameEntity->Instance->Get<CCSPlayerController>(h.index());
        } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        return *out != nullptr;
    }

    inline bool TryGetUserCmd(CCSPlayerController* c, CUserCmd** out) noexcept
    {
        *out = nullptr;
        __try { *out = I::Input->get_user_cmd(c); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        return *out != nullptr;
    }

    // --- Rate-limited diagnostics (string-literal tags only) ---

    inline void LogBlocked(const char* literal) noexcept
    {
        static const char* s_last = nullptr;
        static ULONGLONG   s_t    = 0;
        ULONGLONG now = GetTickCount64();
        if (literal != s_last || now - s_t > 1000) {
            s_last = literal;
            s_t    = now;
            FileLog::Log(literal);
        }
    }

    // --- State management ---

    inline void Reset(const char* reason) noexcept
    {
        bool was = g_ready.exchange(false);
        g_stableCount = 0;
        g_lastPawn = 0;
        g_lastCtrl = 0;
        g_ctx->local_pawn       = nullptr;
        g_ctx->local_controller = nullptr;
        g_ctx->user_cmd         = nullptr;
        if (was) {
            char buf[128];
            std::snprintf(buf, sizeof(buf), "[GATE] RESET reason=%s", reason);
            FileLog::Log(buf);
        }
    }

    struct TickResult {
        C_CSPlayerPawn*      pawn = nullptr;
        CCSPlayerController* ctrl = nullptr;
        CUserCmd*            cmd  = nullptr;
        bool valid = false;
    };

    // Called from CreateMove AFTER original. Returns a valid result
    // only when the gate has been READY for kRequiredStableTicks.
    inline TickResult Evaluate(CCSGOInput* input) noexcept
    {
        TickResult r{};

        // 1. Engine
        if (!I::EngineClient || !I::EngineClient->connected()
            || !I::EngineClient->in_game())
        { Reset("engine"); LogBlocked("[GATE] engine"); return r; }

        // 2. Entity system
        if (!I::EntitySystem || !I::GameEntity || !I::GameEntity->Instance)
        { Reset("entity_sys"); LogBlocked("[GATE] entity_sys"); return r; }

        // 3. Fresh pawn
        C_CSPlayerPawn* pawn = nullptr;
        if (!TryGetLocalPawn(&pawn))
        { Reset("pawn"); LogBlocked("[GATE] pawn"); return r; }

        // 4. Health
        int hp = 0;
        if (!TryGetHealth(pawn, &hp) || hp <= 0)
        { Reset("dead"); LogBlocked("[GATE] dead"); return r; }

        // 5. Controller
        CCSPlayerController* ctrl = nullptr;
        if (!TryGetController(pawn, &ctrl))
        { Reset("ctrl"); LogBlocked("[GATE] ctrl"); return r; }

        // 6. Input / user-cmd
        if (!I::Input)
        { Reset("input"); LogBlocked("[GATE] input"); return r; }

        CUserCmd* cmd = nullptr;
        if (!TryGetUserCmd(ctrl, &cmd))
        { Reset("cmd"); LogBlocked("[GATE] cmd"); return r; }

        // 7. mutable_base
        auto* base = cmd->csgoUserCmd.mutable_base();
        if (!base)
        { Reset("base"); LogBlocked("[GATE] base"); return r; }

        // 8. Pointer stability
        uintptr_t pa = reinterpret_cast<uintptr_t>(pawn);
        uintptr_t ca = reinterpret_cast<uintptr_t>(ctrl);

        if (pa != g_lastPawn || ca != g_lastCtrl) {
            if (g_ready.exchange(false)) {
                char buf[128];
                std::snprintf(buf, sizeof(buf),
                    "[GATE] RESET reason=ptr_change pawn=%p ctrl=%p",
                    reinterpret_cast<void*>(pa), reinterpret_cast<void*>(ca));
                FileLog::Log(buf);
            }
            g_lastPawn = pa;
            g_lastCtrl = ca;
            g_stableCount = 1;
        } else {
            g_stableCount++;
        }

        // Publish fresh snapshot to central context every valid tick
        g_ctx->local_pawn       = pawn;
        g_ctx->local_controller = ctrl;

        // 9. Readiness
        if (g_stableCount >= kRequiredStableTicks) {
            if (!g_ready.exchange(true)) {
                char buf[128];
                std::snprintf(buf, sizeof(buf),
                    "[GATE] READY pawn=%p ctrl=%p hp=%d",
                    reinterpret_cast<void*>(pa), reinterpret_cast<void*>(ca), hp);
                FileLog::Log(buf);
            }
            r.pawn = pawn;
            r.ctrl = ctrl;
            r.cmd  = cmd;
            r.valid = true;
        } else {
            static ULONGLONG s_wt = 0;
            ULONGLONG now = GetTickCount64();
            if (now - s_wt > 1000) {
                s_wt = now;
                char buf[64];
                std::snprintf(buf, sizeof(buf),
                    "[GATE] warming %d/%d", g_stableCount, kRequiredStableTicks);
                FileLog::Log(buf);
            }
        }

        return r;
    }

    // Convenience: is the gate ready? (for Present-side gating)
    inline bool IsReady() noexcept { return g_ready.load(std::memory_order_relaxed); }

    // Rate-limited checkpoint log helper
    inline void LogOnce(ULONGLONG& timer, const char* msg) noexcept
    {
        ULONGLONG now = GetTickCount64();
        if (now - timer > 1000) { timer = now; FileLog::Log(msg); }
    }

} // namespace RuntimeGate
