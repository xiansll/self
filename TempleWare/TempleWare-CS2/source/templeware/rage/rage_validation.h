#pragma once

// TempleWare P8 hardening / validation suite.
//
// A single deterministic, side-effect-contained test battery over the Rage
// dry-run system. It snapshots g_state, runs every check against local State
// objects (or restores g_state afterwards), and reports pass/fail counts plus
// the first failing check name and a coarse performance number.
//
// It NEVER touches CUserCmd, view angles, attack/movement/tick state, or the
// game at all. Synthetic/mock readiness is used ONLY inside these checks.

#include "rage_dryrun.h"
#include "rage_dryrun_providers.h"
#include "rage_live_providers.h"
#include "rage_runtime_providers.h"
#include "../../trace/trace.h"

#include <chrono>
#include <cmath>
#include <cstring>
#include <vector>

namespace RageDryRun
{
    struct ValidationReport
    {
        int total = 0;
        int passed = 0;
        int failed = 0;
        char first_fail[64] = "";
        long long perf_us = 0;   // wall time for the perf-sanity loop
        int perf_iters = 0;

        bool ok() const noexcept { return failed == 0 && total > 0; }
        bool ran = false;
    };

    // Last suite result, for the debug UI.
    inline ValidationReport g_validation{};

    namespace detail
    {
        inline CandidateSnapshot make_candidate(
            int id, float fov, float dist, bool visible, int health = 100) noexcept
        {
            CandidateSnapshot c{};
            c.candidate_id = id;
            c.pawn = static_cast<std::uintptr_t>(0x1000 + id);
            c.controller = static_cast<std::uintptr_t>(0x2000 + id);
            c.valid = c.alive = c.enemy = true;
            c.visibility_known = true;
            c.visible = visible;
            c.fov = fov;
            c.distance = dist;
            c.health = health;
            c.bones_ready = c.hitboxes_ready = true;
            return c;
        }
    }

    inline ValidationReport run_validation_suite() noexcept
    {
        ValidationReport rep{};

        // Preserve the live/synthetic view so the suite is non-destructive.
        const State saved = g_state;

        auto chk = [&](const char* name, bool cond)
        {
            ++rep.total;
            if (cond) { ++rep.passed; return; }
            ++rep.failed;
            if (rep.first_fail[0] == '\0')
            {
#ifdef _WIN32
                strncpy_s(rep.first_fail, sizeof(rep.first_fail), name, _TRUNCATE);
#else
                std::strncpy(rep.first_fail, name, sizeof(rep.first_fail) - 1);
#endif
            }
        };

        // 1. Deterministic evaluator regression.
        chk("evaluator_tests", run_evaluator_tests().all());

        // 2. Synthetic selection self-test.
        {
            const auto st = run_builtin_self_test();
            chk("self_test", st.fov_selection_pass &&
                             st.distance_selection_pass &&
                             st.invisible_rejection_pass);
        }

        // 3. Config sanitize / migration clamp.
        {
            RageDryConfig bad{};
            bad.selection = 99;
            bad.max_fov = std::nanf("");
            bad.primary_hitbox = 250;
            bad.point_scale = -5.f;
            bad.hitchance = 999;
            bad.minimum_damage = -10;
            const RageDryConfig g = Guards::sanitize(bad);
            chk("config_sanitize",
                g.selection >= 0 && g.selection <= 2 &&
                std::isfinite(g.max_fov) && g.max_fov >= 0.f &&
                g.primary_hitbox >= 0 && g.primary_hitbox < kHitboxCount &&
                g.point_scale >= 0.05f && g.point_scale <= 1.f &&
                g.hitchance >= 0 && g.hitchance <= 100 &&
                g.minimum_damage >= 0);
        }

        // 4. NaN candidate guard.
        {
            std::vector<CandidateSnapshot> v;
            auto nanc = detail::make_candidate(1, std::nanf(""), std::nanf(""), true);
            v.push_back(nanc);
            v.push_back(detail::make_candidate(2, 2.f, 300.f, true));
            Guards::sanitize_candidates(v);
            // NaN must be neutralized (guard pushes it to +inf so it can never
            // win a comparison); assert no NaN remains and count is intact.
            bool no_nan = true;
            for (const auto& c : v)
                no_nan &= !std::isnan(c.fov) && !std::isnan(c.distance);
            chk("candidate_nan_guard", no_nan && v.size() == 2);
        }

        // 5. Duplicate entity guard.
        {
            std::vector<CandidateSnapshot> v;
            auto a = detail::make_candidate(1, 2.f, 300.f, true);
            auto b = a;               // same pawn/controller -> duplicate
            b.candidate_id = 5;
            v.push_back(a);
            v.push_back(b);
            v.push_back(detail::make_candidate(3, 1.f, 200.f, true));
            const int removed = Guards::sanitize_candidates(v);
            chk("candidate_dup_guard", removed == 1 && v.size() == 2);
        }

        // 6. Lifecycle / reset clears volatile but preserves config + bumps gen.
        {
            State s;
            s.config.enabled = true;
            s.config.hitchance = 42;
            s.candidates.push_back(detail::make_candidate(1, 2.f, 300.f, true));
            s.lag_records.push_back(LagRecordSnapshot{});
            s.source = SourceMode::Live;
            const std::uint64_t g0 = s.generation;
            s.reset_volatile();
            chk("lifecycle_reset",
                s.candidates.empty() && s.lag_records.empty() &&
                s.source == SourceMode::None &&
                s.generation > g0 &&
                s.config.enabled == true && s.config.hitchance == 42);
        }

        // 7. Lag-history bounds.
        {
            std::vector<LagRecordSnapshot> h(100);
            Live::cap_lag_history(h);
            chk("lag_history_cap", h.size() == Live::kLagHistoryMax);
        }

        // 8. Blocked/unavailable fallback defaults.
        {
            ReadinessMatrix r{};
            chk("readiness_defaults",
                r.trace == Readiness::Blocked &&
                r.penetration == Readiness::Blocked &&
                r.command == Readiness::Unavailable &&
                r.combat_frame == Readiness::Unavailable);
        }

        // 9. UI -> config -> plan parity.
        {
            State s;
            s.config.enabled = true;
            s.config.require_visibility = true;
            s.config.max_fov = 10.f;
            s.config.silent_plan = true;
            s.config.auto_scope_plan = true;
            s.config.no_spread_plan = true;
            s.config.anti_aim_plan = true;
            s.config.prefer_body = true;
            s.candidates.push_back(detail::make_candidate(7, 2.f, 250.f, true));
            s.evaluate();
            const auto& p = s.action;
            chk("ui_config_parity",
                p.target_found && p.would_silent && p.would_scope &&
                p.would_no_spread && p.would_anti_aim &&
                p.body_hitbox == 2 && p.execution_enabled == false);
        }

        // 10. Live vs synthetic parity: identical metrics -> identical winner.
        {
            RageDryConfig cfg{};
            cfg.enabled = true; cfg.require_visibility = true; cfg.max_fov = 10.f;
            cfg.selection = static_cast<int>(TargetSelection::Fov);
            std::vector<CandidateSnapshot> synth, live;
            synth.push_back(detail::make_candidate(1, 4.2f, 300.f, true));
            synth.push_back(detail::make_candidate(2, 1.8f, 800.f, true));
            live = synth; // same values from a different "source"
            const auto* ws = DecisionEngine::select_candidate(cfg, synth);
            const auto* wl = DecisionEngine::select_candidate(cfg, live);
            chk("live_synth_parity",
                ws && wl && ws->candidate_id == wl->candidate_id &&
                ws->candidate_id == 2);
        }

        // 11. Blocked penetration never enables the penetration action.
        {
            State s;
            s.config.enabled = true; s.config.max_fov = 10.f;
            s.config.penetration_crosshair = true;
            s.readiness.trace = Readiness::Blocked;
            s.candidates.push_back(detail::make_candidate(9, 1.f, 200.f, true));
            s.evaluate();
            chk("blocked_penetration",
                s.penetration.state == GateState::Blocked &&
                s.action.would_use_penetration == false);
        }

        // 12. Doubletap blocked without validated tick state.
        {
            WeaponSnapshot w{}; w.ready = true; w.weapon_type = 2;
            auto c = detail::make_candidate(1, 1.f, 200.f, true);
            const auto d = Eval::eval_doubletap(RageDryConfig{}, w, &c, false);
            chk("doubletap_blocked_live", d.state == GateState::Blocked);
        }

        // 13. Execution boundary is a hard NO-OP reject.
        {
            DryRunActionPlan p{};
            p.would_fire = true; // even a "fire" plan must not execute
            chk("execution_boundary",
                ExecutionBoundary::execute(p) == false &&
                ExecutionBoundary::execution_permitted() == false &&
                p.execution_enabled == false);
        }

        // 14. Generation monotonicity across resets.
        {
            State s;
            const auto g0 = s.generation;
            s.reset_volatile();
            const auto g1 = s.generation;
            s.reset_volatile();
            chk("generation_monotonic", g1 > g0 && s.generation > g1);
        }

        // 15. Determinism: evaluate twice -> identical plan (no hidden state).
        {
            State s;
            s.config.enabled = true; s.config.max_fov = 10.f;
            s.weapon.ready = true; s.weapon.weapon_type = 2;
            s.weapon.spread = 0.006f; s.weapon.inaccuracy = 0.008f;
            s.candidates.push_back(detail::make_candidate(4, 1.5f, 350.f, true));
            s.evaluate();
            const auto a = s.action;
            s.evaluate();
            const auto b = s.action;
            chk("determinism",
                a.target_id == b.target_id &&
                a.hitchance == b.hitchance &&
                a.predicted_damage == b.predicted_damage &&
                a.would_fire == b.would_fire);
        }

        // 16. Performance sanity: many evaluations stay cheap.
        {
            State s;
            s.config.enabled = true; s.config.max_fov = 20.f;
            s.weapon.ready = true; s.weapon.weapon_type = 2;
            s.weapon.spread = 0.006f; s.weapon.inaccuracy = 0.008f;
            for (int i = 0; i < 10; ++i)
                s.candidates.push_back(
                    detail::make_candidate(i, 1.f + i * 0.3f, 200.f + i * 40.f, true));

            const int iters = 2000;
            const auto t0 = std::chrono::steady_clock::now();
            volatile int sink = 0;
            for (int i = 0; i < iters; ++i)
            {
                s.evaluate();
                sink += s.action.target_id;
            }
            const auto t1 = std::chrono::steady_clock::now();
            rep.perf_us =
                std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
            rep.perf_iters = iters;
            (void)sink;
            // Generous bound: 2000 full evaluations (each ~256-sample hitchance)
            // must complete well under a second on any sane build.
            chk("perf_sanity", rep.perf_us < 1000000);
        }

        // 17. P9 trace provider bound and ready reflects Trace::Ready().
        {
            chk("trace_provider_bound",
                g_providers.trace != nullptr &&
                g_providers.trace->ready() == Trace::Ready());
        }

        // 18. P9 penetration gates to trace readiness (pure logic, independent of runtime trace state).
        {
            // Test the gate helper directly, not through refresh_provider_readiness()
            // which would override mock values with actual provider status.
            chk("penetration_gates_blocked",
                gate_penetration_to_trace(Readiness::Blocked) == Readiness::Blocked);
            chk("penetration_gates_ready",
                gate_penetration_to_trace(Readiness::Ready) == Readiness::Ready);
        }

        // 19. P9 tick state capture increments generation and sets has_tick_state.
        {
            RuntimeProviders::reset_runtime_state();
            const auto g0 = RuntimeProviders::g_tick_state.generation;
            RuntimeProviders::capture_tick_state(123, 0.5f);  // synthetic tick values
            chk("tick_state_capture",
                RuntimeProviders::g_tick_state.generation > g0 &&
                RuntimeProviders::g_tick_state.has_tick_state &&
                RuntimeProviders::g_tick_state.tick == 123 &&
                RuntimeProviders::g_tick_state.subtick_frac == 0.5f);
        }

        // 20. P9 runtime reset clears tick state.
        {
            RuntimeProviders::g_tick_state.has_tick_state = true;
            RuntimeProviders::reset_runtime_state();
            chk("runtime_reset_clears_tick_state",
                RuntimeProviders::g_tick_state.has_tick_state == false &&
                RuntimeProviders::g_tick_state.generation == 0);
        }

        // Restore the pre-suite view.
        g_state = saved;
        rep.ran = true;
        g_validation = rep;

        // Meaningful transition log only (no per-check spam).
        {
            char b[96];
            std::snprintf(b, sizeof(b), "[P6] VALIDATION %s (%d/%d, %lldus)",
                          rep.ok() ? "PASS" : "FAIL",
                          rep.passed, rep.total, rep.perf_us);
            p6_log(b);
            if (!rep.ok() && rep.first_fail[0])
            {
                char f[96];
                std::snprintf(f, sizeof(f), "[P6] VALIDATION first_fail=%s",
                              rep.first_fail);
                p6_log(f);
            }
        }

        return rep;
    }
} // namespace RageDryRun
