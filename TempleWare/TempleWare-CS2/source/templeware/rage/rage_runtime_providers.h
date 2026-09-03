#pragma once

// TempleWare P9 RUNTIME DEPENDENCY INTEGRATION
//
// This layer formalizes the three runtime dependencies:
// 1. RuntimeTraceProvider  - Wraps Trace::* system from source/trace/trace.h
// 2. TickStateProvider     - Exposes tick/subtick from IEngineClient
// 3. PenetrationBackend    - Gates penetration to trace readiness
//
// These providers are read-only, deterministic, and formalize adapters between
// the existing TempleWare runtime and the P6 evaluation pipeline.

#include "rage_dryrun_providers.h"
#include "../../trace/trace.h"

#include <cstdint>
#include <vector>

namespace RageDryRun
{
    namespace RuntimeProviders
    {
        // ===== Tick / Subtick State Provider =====
        // Exposes player tick and subtick fraction from IEngineClient::get_networked_client_info().
        struct TickState
        {
            std::uint32_t tick = 0;
            float subtick_frac = 0.0f;  // 0..1 within current tick
            bool has_tick_state = false;
            std::uint64_t generation = 0;
        };

        inline TickState g_tick_state{};

        // Capture current tick/subtick from CGlobalVars. Called once per frame
        // during live provider capture (PublishP6Live).
        // Implementation in esp.cpp to avoid CGlobalVarsBase include-order issues.
        inline void capture_tick_state(std::uint32_t tick, float subtick_frac) noexcept
        {
            __try
            {
                g_tick_state.tick = tick;
                g_tick_state.subtick_frac = subtick_frac;
                g_tick_state.has_tick_state = true;
                g_tick_state.generation++;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                g_tick_state.has_tick_state = false;
            }
        }

        // ===== Runtime Trace Provider =====
        // Wraps the existing Trace:: interface and adapts it to IRichTraceProvider.
        class RuntimeTraceProvider final : public IRichTraceProvider
        {
        public:
            bool initialize() noexcept
            {
                return Trace::Initialize();
            }

            bool ready() const noexcept override
            {
                return Trace::Ready();
            }

            std::uint64_t generation() const noexcept override
            {
                static std::uint64_t gen = 0;
                if (Trace::Ready())
                    return ++gen;
                return gen;
            }

            bool is_visible(const float start[3], const float end[3],
                           std::uintptr_t target, std::uintptr_t skip) const noexcept
            {
                if (!Trace::Ready()) return true;
                return Trace::IsVisible(start, end, target, skip);
            }

            bool line_trace(const float start[3], const float end[3],
                           std::uintptr_t skip,
                           float out_end[3], float out_normal[3],
                           float* out_frac) const noexcept
            {
                if (!Trace::Ready()) {
                    if (out_end) { out_end[0] = end[0]; out_end[1] = end[1]; out_end[2] = end[2]; }
                    if (out_normal) { out_normal[0] = 0; out_normal[1] = 0; out_normal[2] = 1; }
                    if (out_frac) *out_frac = 1.0f;
                    return false;
                }
                return Trace::Line(start, end, skip, out_end, out_normal, out_frac);
            }

            Trace::ResolverDiagnostics get_diagnostics() const noexcept
            {
                return Trace::GetResolverDiagnostics();
            }

            bool diagnose_and_retry() noexcept
            {
                return Trace::DiagnoseAndRetryExistingResolvers();
            }
        };

        inline RuntimeTraceProvider g_trace_provider{};

        // ===== Penetration Backend =====
        // Gates to trace readiness. When trace is READY, penetration is READY.
        class PenetrationBackend
        {
        public:
            bool is_active() const noexcept
            {
                return Trace::Ready();
            }

            bool eval_penetration(const float origin[3], const float target_pos[3],
                                 std::uintptr_t skip) const noexcept
            {
                if (!Trace::Ready()) return false;

                float end[3] = {target_pos[0], target_pos[1], target_pos[2]};
                float dummy_normal[3] = {};
                float dummy_frac = 0.0f;

                return Trace::Line(origin, end, skip, end, dummy_normal, &dummy_frac);
            }
        };

        inline PenetrationBackend g_penetration_backend{};

        // ===== Lifecycle / Initialization =====

        inline void initialize_runtime_providers() noexcept
        {
            if (!g_trace_provider.initialize()) {
                p6_log("[P9] Trace initialization failed");
            } else {
                p6_log("[P9] Trace provider initialized");
            }

            // Attempt to retry failed resolvers
            if (!g_trace_provider.ready()) {
                if (g_trace_provider.diagnose_and_retry()) {
                    p6_log("[P9] Trace provider recovered after retry");
                }
            }
        }

        inline void bind_runtime_providers() noexcept
        {
            // Bind the runtime trace provider into the hub
            g_providers.trace = &g_trace_provider;

            p6_log("[P9] Runtime providers bound to hub");
        }

        inline void unbind_runtime_providers() noexcept
        {
            g_providers.trace = nullptr;
            p6_log("[P9] Runtime providers unbound from hub");
        }

        inline void reset_runtime_state() noexcept
        {
            g_tick_state = {};
            p6_log("[P9] Runtime state reset");
        }

        // Diagnostic output for UI/debug panel
        struct RuntimeDiagnostics
        {
            bool trace_ready = false;
            bool trace_initialized = false;
            bool tick_state_valid = false;
            std::uint32_t current_tick = 0;
            float current_subtick_frac = 0.0f;
            bool penetration_active = false;
        };

        inline RuntimeDiagnostics get_runtime_diagnostics() noexcept
        {
            return {
                g_trace_provider.ready(),
                Trace::Ready(),
                g_tick_state.has_tick_state,
                g_tick_state.tick,
                g_tick_state.subtick_frac,
                g_penetration_backend.is_active()
            };
        }

    } // namespace RuntimeProviders
} // namespace RageDryRun
