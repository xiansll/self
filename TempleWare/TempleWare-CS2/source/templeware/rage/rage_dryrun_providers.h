#pragma once

#include "rage_dryrun.h"

#include <cstdint>
#include <cstdio>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace RageDryRun
{
    // Dedicated, low-volume P6 diagnostic sink. Meaningful transitions only,
    // never per-frame spam. Writes to the explicit dry-run log path so P6
    // events are visible independent of the main TempleWare log.
    inline void p6_log(const char* text) noexcept
    {
        if (!text)
            return;

#ifdef _WIN32
        OutputDebugStringA(text);
        OutputDebugStringA("\n");
#endif

        std::FILE* f = nullptr;
#ifdef _WIN32
        if (fopen_s(&f, "C:\\CS\\TempleWare\\P6_runtime.log", "a") != 0)
            f = nullptr;
#else
        f = std::fopen("C:\\CS\\TempleWare\\P6_runtime.log", "a");
#endif
        if (f)
        {
            std::fprintf(f, "%s\n", text);
            std::fclose(f);
        }
    }

    class ICombatFrameProvider
    {
    public:
        virtual ~ICombatFrameProvider() = default;
        virtual bool ready() const noexcept = 0;
        virtual std::uint64_t generation() const noexcept = 0;
        virtual bool snapshot(CombatFrameSnapshot& out) const noexcept = 0;
    };

    class IWeaponProvider
    {
    public:
        virtual ~IWeaponProvider() = default;
        virtual bool ready() const noexcept = 0;
        virtual std::uint64_t generation() const noexcept = 0;
        virtual bool snapshot(WeaponSnapshot& out) const noexcept = 0;
    };

    class IPredictionProvider
    {
    public:
        virtual ~IPredictionProvider() = default;
        virtual bool ready() const noexcept = 0;
        virtual std::uint64_t generation() const noexcept = 0;
        virtual bool snapshot(PredictionSnapshot& out) const noexcept = 0;
    };

    class IEntityCandidateProvider
    {
    public:
        virtual ~IEntityCandidateProvider() = default;
        virtual bool ready() const noexcept = 0;
        virtual std::uint64_t generation() const noexcept = 0;
        virtual bool snapshot(std::vector<CandidateSnapshot>& out) const noexcept = 0;
    };

    class IBoneSnapshotProvider
    {
    public:
        virtual ~IBoneSnapshotProvider() = default;
        virtual bool ready() const noexcept = 0;
        virtual std::uint64_t generation() const noexcept = 0;
    };

    class IHitboxSnapshotProvider
    {
    public:
        virtual ~IHitboxSnapshotProvider() = default;
        virtual bool ready() const noexcept = 0;
        virtual std::uint64_t generation() const noexcept = 0;
    };

    class ILagRecordProvider
    {
    public:
        virtual ~ILagRecordProvider() = default;
        virtual bool ready() const noexcept = 0;
        virtual std::uint64_t generation() const noexcept = 0;
        virtual bool snapshot(std::vector<LagRecordSnapshot>& out) const noexcept = 0;
    };

    class IShootHistoryProvider
    {
    public:
        virtual ~IShootHistoryProvider() = default;
        virtual bool ready() const noexcept = 0;
        virtual std::uint64_t generation() const noexcept = 0;
        virtual bool snapshot(std::vector<ShootHistorySnapshot>& out) const noexcept = 0;
    };

    class IRichTraceProvider
    {
    public:
        virtual ~IRichTraceProvider() = default;
        virtual bool ready() const noexcept = 0;
        virtual std::uint64_t generation() const noexcept = 0;
    };

    struct ProviderHub
    {
        ICombatFrameProvider* combat_frame = nullptr;
        IWeaponProvider* weapon = nullptr;
        IPredictionProvider* prediction = nullptr;
        IEntityCandidateProvider* entities = nullptr;
        IBoneSnapshotProvider* bones = nullptr;
        IHitboxSnapshotProvider* hitboxes = nullptr;
        ILagRecordProvider* lagcomp = nullptr;
        IShootHistoryProvider* shoot_history = nullptr;
        IRichTraceProvider* trace = nullptr;

        void clear() noexcept
        {
            combat_frame = nullptr;
            weapon = nullptr;
            prediction = nullptr;
            entities = nullptr;
            bones = nullptr;
            hitboxes = nullptr;
            lagcomp = nullptr;
            shoot_history = nullptr;
            trace = nullptr;
        }
    };

    inline ProviderHub g_providers{};

    inline Readiness gate_penetration_to_trace(Readiness trace_readiness) noexcept
    {
        return (trace_readiness == Readiness::Ready)
            ? Readiness::Ready : Readiness::Blocked;
    }

    inline void refresh_provider_readiness() noexcept
    {
        auto& r = g_state.readiness;

        r.combat_frame =
            (g_providers.combat_frame && g_providers.combat_frame->ready())
                ? Readiness::Ready : Readiness::Unavailable;

        r.weapon =
            (g_providers.weapon && g_providers.weapon->ready())
                ? Readiness::Ready : Readiness::Unavailable;

        r.prediction =
            (g_providers.prediction && g_providers.prediction->ready())
                ? Readiness::Ready : Readiness::Unavailable;

        r.entities =
            (g_providers.entities && g_providers.entities->ready())
                ? Readiness::Ready : Readiness::Unavailable;

        r.bones =
            (g_providers.bones && g_providers.bones->ready())
                ? Readiness::Ready : Readiness::Unavailable;

        r.hitboxes =
            (g_providers.hitboxes && g_providers.hitboxes->ready())
                ? Readiness::Ready : Readiness::Unavailable;

        r.lagcomp =
            (g_providers.lagcomp && g_providers.lagcomp->ready())
                ? Readiness::Ready : Readiness::Unavailable;

        r.shoot_history =
            (g_providers.shoot_history && g_providers.shoot_history->ready())
                ? Readiness::Ready : Readiness::Unavailable;

        r.trace =
            (g_providers.trace && g_providers.trace->ready())
                ? Readiness::Ready : Readiness::Blocked;

        r.penetration = gate_penetration_to_trace(r.trace);

        r.command =
            g_state.command.available
                ? Readiness::Ready : Readiness::Unavailable;
    }

    inline void publish_bound_snapshots() noexcept
    {
        refresh_provider_readiness();

        if (g_providers.combat_frame &&
            g_state.readiness.combat_frame == Readiness::Ready)
        {
            CombatFrameSnapshot tmp{};
            if (g_providers.combat_frame->snapshot(tmp))
                g_state.frame = tmp;
        }

        if (g_providers.weapon &&
            g_state.readiness.weapon == Readiness::Ready)
        {
            WeaponSnapshot tmp{};
            if (g_providers.weapon->snapshot(tmp))
                g_state.weapon = tmp;
        }

        if (g_providers.prediction &&
            g_state.readiness.prediction == Readiness::Ready)
        {
            PredictionSnapshot tmp{};
            if (g_providers.prediction->snapshot(tmp))
                g_state.prediction = tmp;
        }

        if (g_providers.entities &&
            g_state.readiness.entities == Readiness::Ready)
        {
            std::vector<CandidateSnapshot> tmp;
            if (g_providers.entities->snapshot(tmp))
                g_state.candidates = std::move(tmp);
        }

        if (g_providers.lagcomp &&
            g_state.readiness.lagcomp == Readiness::Ready)
        {
            std::vector<LagRecordSnapshot> tmp;
            if (g_providers.lagcomp->snapshot(tmp))
                g_state.lag_records = std::move(tmp);
        }

        if (g_providers.shoot_history &&
            g_state.readiness.shoot_history == Readiness::Ready)
        {
            std::vector<ShootHistorySnapshot> tmp;
            if (g_providers.shoot_history->snapshot(tmp))
                g_state.shoot_history = std::move(tmp);
        }

        g_state.evaluate();
    }

    inline void load_synthetic_demo() noexcept
    {
        p6_log("[P6] FULL SYNTH BEGIN");
        g_state.reset_volatile();
        g_state.config.enabled = true;
        g_state.config.max_fov = 10.f;
        g_state.config.require_visibility = true;

        g_state.frame.generation = 1;
        g_state.frame.eye_position = {0.f, 0.f, 64.f};
        g_state.frame.origin = {0.f, 0.f, 0.f};
        g_state.frame.velocity = {120.f, 0.f, 0.f};
        g_state.frame.tick = 1000;
        g_state.frame.time = 15.625f;
        g_state.frame.on_ground = true;
        g_state.frame.ready = true;

        g_state.weapon.generation = 1;
        g_state.weapon.ammo = 30;
        g_state.weapon.range = 8192.f;
        g_state.weapon.max_speed = 225.f;
        g_state.weapon.spread = 0.010f;
        g_state.weapon.inaccuracy = 0.020f;
        g_state.weapon.ready = true;

        g_state.prediction.generation = 1;
        g_state.prediction.origin = g_state.frame.origin;
        g_state.prediction.velocity = g_state.frame.velocity;
        g_state.prediction.speed_2d = 120.f;
        g_state.prediction.on_ground = true;
        g_state.prediction.ready = true;

        CandidateSnapshot a{};
        a.candidate_id = 1;
        a.health = 100;
        a.team = 2;
        a.fov = 4.2f;
        a.distance = 300.f;
        a.valid = a.alive = a.enemy = true;
        a.visibility_known = true;
        a.visible = true;
        a.bones_ready = true;
        a.hitboxes_ready = true;
        g_state.candidates.push_back(a);

        CandidateSnapshot b{};
        b.candidate_id = 2;
        b.health = 70;
        b.team = 2;
        b.fov = 1.8f;
        b.distance = 800.f;
        b.valid = b.alive = b.enemy = true;
        b.visibility_known = true;
        b.visible = true;
        b.bones_ready = true;
        b.hitboxes_ready = true;
        g_state.candidates.push_back(b);

        CandidateSnapshot c{};
        c.candidate_id = 3;
        c.health = 40;
        c.team = 2;
        c.fov = 0.9f;
        c.distance = 250.f;
        c.valid = c.alive = c.enemy = true;
        c.visibility_known = true;
        c.visible = false;
        c.bones_ready = true;
        c.hitboxes_ready = true;
        g_state.candidates.push_back(c);

        LagRecordSnapshot lr{};
        lr.candidate_id = 2;
        lr.tick = 998;
        lr.simulation_time = 15.59375f;
        lr.origin = {800.f, 0.f, 0.f};
        lr.valid = true;
        g_state.lag_records.push_back(lr);

        ShootHistorySnapshot sh{};
        sh.client_tick = 1000;
        sh.server_tick = 1000;
        sh.fraction = 0.f;
        sh.valid = true;
        g_state.shoot_history.push_back(sh);

        g_state.hitchance.state = GateState::Pass;
        g_state.hitchance.samples = 256;
        g_state.hitchance.hits = 201;
        g_state.hitchance.chance = 78.5f;
        g_state.hitchance.required = 75.f;

        g_state.damage.state = GateState::Pass;
        g_state.damage.predicted_damage = 44.f;
        g_state.damage.minimum_damage = 30.f;

        g_state.stop_prediction.state = GateState::Pass;
        g_state.stop_prediction.current_speed = 120.f;
        g_state.stop_prediction.desired_speed = 70.f;
        g_state.stop_prediction.would_stop = true;

        g_state.doubletap.state = GateState::Pass;
        g_state.doubletap.weapon_allowed = true;
        g_state.doubletap.target_available = true;
        g_state.doubletap.cooldown_ready = true;

        auto& r = g_state.readiness;
        r.local = Readiness::Ready;
        r.entities = Readiness::Ready;
        r.combat_frame = Readiness::Ready;
        r.weapon = Readiness::Ready;
        r.bones = Readiness::Ready;
        r.hitboxes = Readiness::Ready;
        r.prediction = Readiness::Ready;
        r.trace = Readiness::Blocked;
        r.penetration = Readiness::Blocked;
        r.lagcomp = Readiness::Ready;
        r.shoot_history = Readiness::Ready;
        r.command = Readiness::Unavailable;

        g_state.source = SourceMode::Synthetic;
        g_state.live_entity_count = static_cast<int>(g_state.candidates.size());
        // Synthetic bundle provides a validated tick source (illustrative), so
        // the doubletap gate can be exercised end-to-end offline.
        g_state.has_tick_state = true;
        g_state.evaluate();
        g_state.self_test = run_builtin_self_test();
        const EvaluatorTestReport et = run_evaluator_tests();

        char line[96]{};
        std::snprintf(line, sizeof(line), "[P6] candidates=%d",
                      static_cast<int>(g_state.candidates.size()));
        p6_log(line);
        std::snprintf(line, sizeof(line), "[P6] selected=%d",
                      g_state.action.target_id);
        p6_log(line);

        p6_log("[P6] trace=BLOCKED");
        p6_log("[P6] command=UNAVAILABLE");
        p6_log("[P6] FULL SYNTH COMPLETE");
        p6_log(
            (g_state.self_test.fov_selection_pass &&
             g_state.self_test.distance_selection_pass &&
             g_state.self_test.invisible_rejection_pass)
                ? "[P6] SELF TEST PASS"
                : "[P6] SELF TEST FAIL");
        p6_log(et.all() ? "[P6] EVAL TESTS PASS" : "[P6] EVAL TESTS FAIL");
        p6_log("[P6] EXECUTION=DISABLED");
    }

    // Volatile-state reset routed through the existing TempleWare lifecycle.
    // Resets volatile runtime state only. Provider bindings and dry-run config
    // persist so validated process-lifetime providers need no re-registration.
    inline void reset_state() noexcept
    {
        g_state.reset_volatile();
        g_state.self_test = {};
        p6_log("[P6] RESET");
    }
}
