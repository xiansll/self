#pragma once

// Split runtime readiness gates.
//
// Render gate:
//   - refreshed when Present asks whether rendering is ready
//   - reads fresh entity-system pawn + controller pointers
//   - requires stable pawn + controller pointers
//   - never depends on CUserCmd/CreateMove
//
// Command gate:
//   - updated only from CreateMove
//   - additionally requires a valid CUserCmd and mutable_base
//
// This keeps rendering features alive even if the CreateMove hook is temporarily
// unavailable, while command-mutating features remain protected.

#include "interfaces/interfaces.h"
#include "globals/globals.h"
#include "utils/filelog/filelog.h"

#include <atomic>
#include <cstdint>
#include <cstdio>

namespace RuntimeGate
{
    inline constexpr int kRequiredStableTicks = 3;

    inline std::atomic<bool> g_renderReady{false};
    inline int g_renderStableCount = 0;
    inline uintptr_t g_renderLastPawn = 0;
    inline uintptr_t g_renderLastCtrl = 0;
    inline std::atomic<ULONGLONG> g_renderLastEvalMs{~0ull};

    inline std::atomic<bool> g_commandReady{false};
    inline int g_commandStableCount = 0;
    inline uintptr_t g_commandLastPawn = 0;
    inline uintptr_t g_commandLastCtrl = 0;

    // --- SEH-safe SDK wrappers (separate functions to avoid C2712) ---

    inline bool TryEngineReady() noexcept
    {
        if (!I::EngineClient)
            return false;

        __try {
            return I::EngineClient->connected() && I::EngineClient->in_game();
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    inline bool TryGetLocalPawn(C_CSPlayerPawn** out) noexcept
    {
        *out = nullptr;
        if (!I::EntitySystem)
            return false;

        __try { *out = I::EntitySystem->get_local_pawn(); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        return *out != nullptr;
    }

    inline bool TryGetLocalController(CCSPlayerController** out) noexcept
    {
        *out = nullptr;
        if (!I::EntitySystem)
            return false;

        __try {
            *out = reinterpret_cast<CCSPlayerController*>(
                I::EntitySystem->get_local_controller());
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
        return *out != nullptr;
    }

    inline bool TryGetHealth(C_CSPlayerPawn* pawn, int* out) noexcept
    {
        *out = 0;
        if (!pawn)
            return false;

        __try { *out = pawn->m_iHealth(); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        return true;
    }

    inline bool TryGetController(C_CSPlayerPawn* pawn, CCSPlayerController** out) noexcept
    {
        *out = nullptr;
        if (!pawn || !I::GameEntity || !I::GameEntity->Instance)
            return false;

        __try {
            auto handle = pawn->m_hController();
            *out = I::GameEntity->Instance->Get<CCSPlayerController>(handle.index());
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
        return *out != nullptr;
    }

    inline bool TryGetUserCmd(CCSGOInput* input, CCSPlayerController* controller,
                              CUserCmd** out) noexcept
    {
        *out = nullptr;
        if (!input || !controller)
            return false;

        __try { *out = input->get_user_cmd(controller); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        return *out != nullptr;
    }

    inline bool TryHasMutableBase(CUserCmd* cmd) noexcept
    {
        if (!cmd)
            return false;

        __try { return cmd->csgoUserCmd.mutable_base() != nullptr; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    inline bool IsPlausibleAddress(uintptr_t address) noexcept
    {
        return address >= 0x10000ull && address < 0x0000800000000000ull;
    }

    // --- Rate-limited diagnostics ---

    inline void LogRenderBlocked(const char* literal) noexcept
    {
        static const char* s_last = nullptr;
        static ULONGLONG s_t = 0;
        const ULONGLONG now = GetTickCount64();
        if (literal != s_last || now - s_t > 1000) {
            s_last = literal;
            s_t = now;
            FileLog::Log(literal);
        }
    }

    inline void LogCommandBlocked(const char* literal) noexcept
    {
        static const char* s_last = nullptr;
        static ULONGLONG s_t = 0;
        const ULONGLONG now = GetTickCount64();
        if (literal != s_last || now - s_t > 1000) {
            s_last = literal;
            s_t = now;
            FileLog::Log(literal);
        }
    }

    // --- State management ---

    inline void ResetRender(const char* reason) noexcept
    {
        const bool wasReady = g_renderReady.exchange(false);
        g_renderStableCount = 0;
        g_renderLastPawn = 0;
        g_renderLastCtrl = 0;

        if (g_ctx) {
            g_ctx->local_pawn = nullptr;
            g_ctx->local_controller = nullptr;
        }

        if (wasReady) {
            char buf[128];
            std::snprintf(buf, sizeof(buf), "[RENDER_GATE] RESET reason=%s", reason);
            FileLog::Log(buf);
        }
    }

    inline void ResetCommand(const char* reason) noexcept
    {
        const bool wasReady = g_commandReady.exchange(false);
        g_commandStableCount = 0;
        g_commandLastPawn = 0;
        g_commandLastCtrl = 0;

        if (g_ctx)
            g_ctx->user_cmd = nullptr;

        if (wasReady) {
            char buf[128];
            std::snprintf(buf, sizeof(buf), "[CMD_GATE] RESET reason=%s", reason);
            FileLog::Log(buf);
        }
    }

    inline void Reset(const char* reason) noexcept
    {
        ResetRender(reason);
        ResetCommand(reason);
    }

    // Called from Present after LocalPlayerCache publishes its validated snapshot.
    // Rendering features intentionally do not depend on user_cmd/CreateMove.
    inline bool EvaluateRender(uintptr_t pawnAddress, uintptr_t ctrlAddress,
                               bool sourceTrusted) noexcept
    {
        if (!TryEngineReady()) {
            ResetRender("engine");
            LogRenderBlocked("[RENDER_GATE] engine");
            return false;
        }

        if (!sourceTrusted) {
            ResetRender("snapshot_untrusted");
            LogRenderBlocked("[RENDER_GATE] snapshot_untrusted");
            return false;
        }

        if (!IsPlausibleAddress(pawnAddress) || !IsPlausibleAddress(ctrlAddress)) {
            ResetRender("local_pointer");
            LogRenderBlocked("[RENDER_GATE] local_pointer");
            return false;
        }

        if (pawnAddress != g_renderLastPawn || ctrlAddress != g_renderLastCtrl) {
            if (g_renderReady.exchange(false)) {
                char buf[160];
                std::snprintf(buf, sizeof(buf),
                    "[RENDER_GATE] RESET reason=ptr_change pawn=%p ctrl=%p",
                    reinterpret_cast<void*>(pawnAddress),
                    reinterpret_cast<void*>(ctrlAddress));
                FileLog::Log(buf);
            }

            g_renderLastPawn = pawnAddress;
            g_renderLastCtrl = ctrlAddress;
            g_renderStableCount = 1;
        }
        else if (g_renderStableCount < kRequiredStableTicks) {
            ++g_renderStableCount;
        }

        if (g_ctx) {
            g_ctx->local_pawn = reinterpret_cast<C_CSPlayerPawn*>(pawnAddress);
            g_ctx->local_controller =
                reinterpret_cast<CCSPlayerController*>(ctrlAddress);
        }

        if (g_renderStableCount >= kRequiredStableTicks) {
            if (!g_renderReady.exchange(true)) {
                char buf[160];
                std::snprintf(buf, sizeof(buf),
                    "[RENDER_GATE] READY pawn=%p ctrl=%p",
                    reinterpret_cast<void*>(pawnAddress),
                    reinterpret_cast<void*>(ctrlAddress));
                FileLog::Log(buf);
            }
            return true;
        }

        static ULONGLONG s_warmingTimer = 0;
        const ULONGLONG now = GetTickCount64();
        if (now - s_warmingTimer > 1000) {
            s_warmingTimer = now;
            char buf[80];
            std::snprintf(buf, sizeof(buf), "[RENDER_GATE] warming %d/%d",
                g_renderStableCount, kRequiredStableTicks);
            FileLog::Log(buf);
        }
        return false;
    }

    // Present calls IsReady() twice in the current render path. The millisecond
    // stamp prevents both checks from counting as two separate stable frames.
    inline bool EvaluateRenderFresh() noexcept
    {
        const ULONGLONG now = GetTickCount64();
        const ULONGLONG previous =
            g_renderLastEvalMs.exchange(now, std::memory_order_relaxed);
        if (previous == now)
            return g_renderReady.load(std::memory_order_relaxed);

        if (!TryEngineReady()) {
            ResetRender("engine");
            LogRenderBlocked("[RENDER_GATE] engine");
            return false;
        }

        C_CSPlayerPawn* pawn = nullptr;
        if (!TryGetLocalPawn(&pawn)) {
            ResetRender("pawn");
            LogRenderBlocked("[RENDER_GATE] pawn");
            return false;
        }

        CCSPlayerController* ctrl = nullptr;
        if (!TryGetLocalController(&ctrl)) {
            ResetRender("ctrl");
            LogRenderBlocked("[RENDER_GATE] ctrl");
            return false;
        }

        // A successful, exception-free schema read proves the pawn wrapper is
        // usable. Zero health is allowed so ESP can remain available while dead.
        int health = 0;
        if (!TryGetHealth(pawn, &health) || health < 0 || health > 200) {
            ResetRender("pawn_semantics");
            LogRenderBlocked("[RENDER_GATE] pawn_semantics");
            return false;
        }

        return EvaluateRender(
            reinterpret_cast<uintptr_t>(pawn),
            reinterpret_cast<uintptr_t>(ctrl),
            true);
    }

    struct TickResult {
        C_CSPlayerPawn* pawn = nullptr;
        CCSPlayerController* ctrl = nullptr;
        CUserCmd* cmd = nullptr;
        bool valid = false;
    };

    // Called from CreateMove after original. Only command-mutating features depend
    // on this result.
    inline TickResult EvaluateCommand(CCSGOInput* input) noexcept
    {
        TickResult result{};

        if (!TryEngineReady()) {
            ResetCommand("engine");
            LogCommandBlocked("[CMD_GATE] engine");
            return result;
        }

        if (!I::EntitySystem || !I::GameEntity || !I::GameEntity->Instance) {
            ResetCommand("entity_sys");
            LogCommandBlocked("[CMD_GATE] entity_sys");
            return result;
        }

        C_CSPlayerPawn* pawn = nullptr;
        if (!TryGetLocalPawn(&pawn)) {
            ResetCommand("pawn");
            LogCommandBlocked("[CMD_GATE] pawn");
            return result;
        }

        int health = 0;
        if (!TryGetHealth(pawn, &health) || health <= 0) {
            ResetCommand("dead");
            LogCommandBlocked("[CMD_GATE] dead");
            return result;
        }

        CCSPlayerController* ctrl = nullptr;
        if (!TryGetController(pawn, &ctrl)) {
            ResetCommand("ctrl");
            LogCommandBlocked("[CMD_GATE] ctrl");
            return result;
        }

        CUserCmd* cmd = nullptr;
        if (!TryGetUserCmd(input, ctrl, &cmd)) {
            ResetCommand("cmd");
            LogCommandBlocked("[CMD_GATE] cmd");
            return result;
        }

        if (!TryHasMutableBase(cmd)) {
            ResetCommand("base");
            LogCommandBlocked("[CMD_GATE] base");
            return result;
        }

        const uintptr_t pawnAddress = reinterpret_cast<uintptr_t>(pawn);
        const uintptr_t ctrlAddress = reinterpret_cast<uintptr_t>(ctrl);

        if (pawnAddress != g_commandLastPawn || ctrlAddress != g_commandLastCtrl) {
            if (g_commandReady.exchange(false)) {
                char buf[160];
                std::snprintf(buf, sizeof(buf),
                    "[CMD_GATE] RESET reason=ptr_change pawn=%p ctrl=%p",
                    reinterpret_cast<void*>(pawnAddress),
                    reinterpret_cast<void*>(ctrlAddress));
                FileLog::Log(buf);
            }

            g_commandLastPawn = pawnAddress;
            g_commandLastCtrl = ctrlAddress;
            g_commandStableCount = 1;
        }
        else if (g_commandStableCount < kRequiredStableTicks) {
            ++g_commandStableCount;
        }

        if (g_ctx) {
            g_ctx->local_pawn = pawn;
            g_ctx->local_controller = ctrl;
            g_ctx->user_cmd = cmd;
        }

        if (g_commandStableCount >= kRequiredStableTicks) {
            if (!g_commandReady.exchange(true)) {
                char buf[176];
                std::snprintf(buf, sizeof(buf),
                    "[CMD_GATE] READY pawn=%p ctrl=%p hp=%d",
                    reinterpret_cast<void*>(pawnAddress),
                    reinterpret_cast<void*>(ctrlAddress), health);
                FileLog::Log(buf);
            }

            result.pawn = pawn;
            result.ctrl = ctrl;
            result.cmd = cmd;
            result.valid = true;
        }
        else {
            static ULONGLONG s_warmingTimer = 0;
            const ULONGLONG now = GetTickCount64();
            if (now - s_warmingTimer > 1000) {
                s_warmingTimer = now;
                char buf[80];
                std::snprintf(buf, sizeof(buf), "[CMD_GATE] warming %d/%d",
                    g_commandStableCount, kRequiredStableTicks);
                FileLog::Log(buf);
            }
        }

        return result;
    }

    // Backward-compatible name used by the current CreateMove hook.
    inline TickResult Evaluate(CCSGOInput* input) noexcept
    {
        return EvaluateCommand(input);
    }

    inline bool IsRenderReady() noexcept
    {
        return g_renderReady.load(std::memory_order_relaxed);
    }

    inline bool IsCommandReady() noexcept
    {
        return g_commandReady.load(std::memory_order_relaxed);
    }

    // Backward-compatible name used by the current Present render path.
    // It refreshes only the render lane; command readiness stays independent.
    inline bool IsReady() noexcept
    {
        return EvaluateRenderFresh();
    }

    inline void LogOnce(ULONGLONG& timer, const char* msg) noexcept
    {
        const ULONGLONG now = GetTickCount64();
        if (now - timer > 1000) {
            timer = now;
            FileLog::Log(msg);
        }
    }

} // namespace RuntimeGate
