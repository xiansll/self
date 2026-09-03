#pragma once

// TempleWare P6 dry-run Rage foundation.
//
// IMPORTANT:
// - This layer does not acquire CUserCmd.
// - This layer does not mutate CUserCmd.
// - This layer does not write view angles, attack buttons, movement, or tick state.
// - UI controls publish only dry-run/planner configuration.
// - Live providers can be attached later by the owner without changing the DTO/evaluator surface.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

class CUserCmd;

namespace RageDryRun
{
    inline constexpr int kHitboxCount = 9;

    enum class Readiness : std::uint8_t
    {
        Unavailable = 0,
        Ready,
        Blocked
    };

    enum class GateState : std::uint8_t
    {
        Unknown = 0,
        Pass,
        Fail,
        Blocked
    };

    // Where the currently published snapshot bundle came from.
    enum class SourceMode : std::uint8_t
    {
        None = 0,
        Synthetic,
        Live
    };

    enum class TargetSelection : int
    {
        Fov = 0,
        Distance = 1,
        Health = 2
    };

    struct Vec3
    {
        float x = 0.f;
        float y = 0.f;
        float z = 0.f;
    };

    // Trivially-copyable on purpose: gui_config.h snapshots it directly.
    struct RageDryConfig
    {
        bool enabled = false;

        // Targeting / scan policy.
        int selection = static_cast<int>(TargetSelection::Fov);
        float max_fov = 5.0f;
        bool require_visibility = true;
        bool prefer_body = false;

        // Hitbox / scan geometry policy.
        int primary_hitbox = 0;
        bool hitboxes[kHitboxCount] = {
            true,   // head
            false,  // neck
            true,   // chest
            true,   // stomach
            true,   // pelvis
            false,  // left arm
            false,  // right arm
            false,  // left leg
            false   // right leg
        };
        bool multipoint = true;
        float point_scale = 0.80f;
        bool safe_points = false;

        // Accuracy / damage evaluation configuration.
        int hitchance = 75;
        int minimum_damage = 30;
        bool damage_override = false;
        int override_damage = 50;
        bool penetration_crosshair = false;

        // Planner-only features. These NEVER execute from this layer.
        bool silent_plan = false;
        bool auto_fire_plan = false;
        bool auto_scope_plan = false;
        bool auto_stop_plan = false;
        bool force_shot_plan = false;
        bool no_spread_plan = false;
        bool doubletap_plan = false;

        // Snapshot/simulation lanes.
        bool lagcomp_snapshots = true;
        bool extrapolation_plan = false;
        bool shoot_history = true;
        bool quick_peek_plan = false;
        bool duck_peek_plan = false;

        // Anti-aim is represented only as a planner/debug request.
        bool anti_aim_plan = false;
        int anti_aim_pitch = 0;
        int anti_aim_yaw = 0;
        int anti_aim_body_yaw = 0;
        bool anti_aim_freestanding = false;
    };

    struct CommandContext
    {
        // Owner-supplied later. This foundation never dereferences or mutates it.
        CUserCmd* command = nullptr;
        std::uint64_t sequence = 0;
        bool available = false;

        void reset() noexcept
        {
            command = nullptr;
            sequence = 0;
            available = false;
        }
    };

    struct CombatFrameSnapshot
    {
        std::uint64_t generation = 0;

        std::uintptr_t local_pawn = 0;
        std::uintptr_t local_controller = 0;

        Vec3 eye_position{};
        Vec3 origin{};
        Vec3 velocity{};

        float view_pitch = 0.f;
        float view_yaw = 0.f;

        int tick = 0;
        float time = 0.f;

        bool on_ground = false;
        bool scoped = false;
        bool ready = false;
    };

    struct WeaponSnapshot
    {
        std::uint64_t generation = 0;
        std::uintptr_t weapon = 0;

        int weapon_type = 0;
        int item_definition = 0;
        int ammo = 0;

        float range = 0.f;
        float max_speed = 0.f;
        float spread = 0.f;
        float inaccuracy = 0.f;
        float recoil_index = 0.f;

        bool can_scope = false;
        bool ready = false;
    };

    struct PredictionSnapshot
    {
        std::uint64_t generation = 0;
        Vec3 origin{};
        Vec3 velocity{};

        float speed_2d = 0.f;
        bool on_ground = false;
        bool ready = false;
    };

    struct CandidateSnapshot
    {
        int candidate_id = -1;

        std::uintptr_t controller = 0;
        std::uintptr_t pawn = 0;

        int health = 0;
        int team = 0;

        float fov = std::numeric_limits<float>::infinity();
        float distance = std::numeric_limits<float>::infinity();

        bool valid = false;
        bool alive = false;
        bool enemy = false;

        bool visibility_known = false;
        bool visible = false;

        bool bones_ready = false;
        bool hitboxes_ready = false;
    };

    struct BonePose
    {
        Vec3 position{};
        float qx = 0.f;
        float qy = 0.f;
        float qz = 0.f;
        float qw = 1.f;
        bool valid = false;
    };

    struct LagRecordSnapshot
    {
        int candidate_id = -1;
        int tick = 0;
        float simulation_time = 0.f;

        Vec3 origin{};
        std::array<BonePose, 128> bones{};

        bool valid = false;
    };

    struct ShootHistorySnapshot
    {
        int client_tick = 0;
        int server_tick = 0;
        float fraction = 0.f;
        bool valid = false;
    };

    struct HitchanceResult
    {
        GateState state = GateState::Unknown;
        int samples = 0;
        int hits = 0;
        float chance = 0.f;
        float required = 0.f;
    };

    struct PenetrationResult
    {
        GateState state = GateState::Blocked;
        float predicted_damage = 0.f;
        bool penetrated = false;
        int surfaces = 0;
    };

    struct DamageResult
    {
        GateState state = GateState::Unknown;
        float predicted_damage = 0.f;
        float minimum_damage = 0.f;
    };

    struct StopPredictionResult
    {
        GateState state = GateState::Unknown;
        float current_speed = 0.f;
        float desired_speed = 0.f;
        bool would_stop = false;
    };

    struct ExtrapolationResult
    {
        GateState state = GateState::Unknown;
        int ticks_ahead = 0;
        Vec3 before{};
        Vec3 projected{};
    };

    struct DoubletapEligibility
    {
        GateState state = GateState::Unknown;
        bool weapon_allowed = false;
        bool target_available = false;
        bool cooldown_ready = false;
        int requested_ticks = 0;
    };

    struct AntiAimPlan
    {
        GateState state = GateState::Unknown;
        float planned_pitch = 0.f;
        float planned_yaw = 0.f;
        float planned_body_yaw = 0.f;
        int freestanding_side = 0;
    };

    struct QuickPeekPlan
    {
        GateState state = GateState::Unknown;
        Vec3 anchor{};
        Vec3 planned_return{};
    };

    struct DryRunActionPlan
    {
        bool target_found = false;
        int target_id = -1;
        int selected_hitbox = -1;
        int selected_record = -1;

        float fov = 0.f;
        float distance = 0.f;
        float hitchance = 0.f;
        float predicted_damage = 0.f;

        bool would_aim = false;
        bool would_silent = false;
        bool would_fire = false;
        bool would_scope = false;
        bool would_stop = false;
        bool would_no_spread = false;
        bool would_use_penetration = false;
        bool would_use_backtrack = false;
        bool would_extrapolate = false;
        bool would_doubletap = false;
        bool would_quick_peek = false;
        bool would_duck_peek = false;
        bool would_anti_aim = false;

        // Gate provenance (WHY the plan resolved as it did).
        bool hitchance_pass = false;
        bool damage_pass = false;
        bool visibility_pass = false;
        bool fov_pass = false;
        int  body_hitbox = -1;      // hitbox the body-preference logic settled on
        int  backtrack_records = 0; // lag records available for this target

        // Deliberately immutable policy for P6/P7.
        bool execution_enabled = false;
    };

    struct ReadinessMatrix
    {
        Readiness local = Readiness::Unavailable;
        Readiness entities = Readiness::Unavailable;
        Readiness combat_frame = Readiness::Unavailable;
        Readiness weapon = Readiness::Unavailable;
        Readiness bones = Readiness::Unavailable;
        Readiness hitboxes = Readiness::Unavailable;
        Readiness prediction = Readiness::Unavailable;
        Readiness trace = Readiness::Blocked;
        Readiness penetration = Readiness::Blocked;
        Readiness lagcomp = Readiness::Unavailable;
        Readiness shoot_history = Readiness::Unavailable;
        Readiness command = Readiness::Unavailable;
    };

    struct SelfTestReport
    {
        bool ran = false;
        bool fov_selection_pass = false;
        bool distance_selection_pass = false;
        bool invisible_rejection_pass = false;

        int fov_winner = -1;
        int distance_winner = -1;
    };

    struct EvaluatorTestReport
    {
        bool ran = false;
        bool hitchance_pass = false;   // near passes, far fails
        bool damage_pass = false;      // override passes, weak fails
        bool stop_pass = false;        // moving stops, still does not
        bool doubletap_pass = false;   // blocked w/o tick, eligible with tick
        bool extrapolation_pass = false; // projects forward from velocity
        bool penetration_pass = false; // stays BLOCKED
        bool antiaim_pass = false;     // planner pass when enabled
        bool quickpeek_pass = false;   // planner pass when enabled + frame ready

        bool all() const noexcept
        {
            return hitchance_pass && damage_pass && stop_pass && doubletap_pass &&
                   extrapolation_pass && penetration_pass && antiaim_pass &&
                   quickpeek_pass;
        }
    };

    class DecisionEngine
    {
    public:
        static const CandidateSnapshot* select_candidate(
            const RageDryConfig& cfg,
            const std::vector<CandidateSnapshot>& candidates) noexcept
        {
            const CandidateSnapshot* best = nullptr;

            for (const auto& c : candidates)
            {
                if (!c.valid || !c.alive || !c.enemy)
                    continue;

                if (cfg.require_visibility)
                {
                    if (!c.visibility_known || !c.visible)
                        continue;
                }

                if (c.fov > cfg.max_fov)
                    continue;

                if (!best)
                {
                    best = &c;
                    continue;
                }

                switch (static_cast<TargetSelection>(cfg.selection))
                {
                case TargetSelection::Distance:
                    if (c.distance < best->distance)
                        best = &c;
                    break;
                case TargetSelection::Health:
                    if (c.health < best->health)
                        best = &c;
                    break;
                case TargetSelection::Fov:
                default:
                    if (c.fov < best->fov)
                        best = &c;
                    break;
                }
            }

            return best;
        }

        static DryRunActionPlan build_plan(
            const RageDryConfig& cfg,
            const CandidateSnapshot* target,
            const HitchanceResult& hitchance,
            const DamageResult& damage,
            const StopPredictionResult& stop,
            const DoubletapEligibility& doubletap) noexcept
        {
            DryRunActionPlan out{};
            out.execution_enabled = false;

            if (!cfg.enabled || !target)
                return out;

            out.target_found = true;
            out.target_id = target->candidate_id;
            out.selected_hitbox = cfg.primary_hitbox;
            out.fov = target->fov;
            out.distance = target->distance;
            out.hitchance = hitchance.chance;
            out.predicted_damage = damage.predicted_damage;

            const bool hc_pass =
                hitchance.state == GateState::Pass ||
                cfg.hitchance <= 0;

            const bool damage_pass =
                damage.state == GateState::Pass ||
                cfg.minimum_damage <= 0;

            out.would_aim = true;
            out.would_silent = cfg.silent_plan;
            out.would_scope = cfg.auto_scope_plan;
            out.would_stop = cfg.auto_stop_plan && stop.would_stop;
            out.would_use_penetration =
                cfg.penetration_crosshair && damage.state != GateState::Blocked;
            out.would_use_backtrack = cfg.lagcomp_snapshots;
            out.would_extrapolate = cfg.extrapolation_plan;
            out.would_doubletap =
                cfg.doubletap_plan && doubletap.state == GateState::Pass;
            out.would_quick_peek = cfg.quick_peek_plan;
            out.would_duck_peek = cfg.duck_peek_plan;
            out.would_anti_aim = cfg.anti_aim_plan;

            // Planner output only. No command/button mutation occurs here.
            out.would_fire =
                cfg.auto_fire_plan &&
                hc_pass &&
                damage_pass;

            return out;
        }
    };

    // -------------------------------------------------------------------
    // Offline / deterministic evaluators.
    //
    // Every function here is pure: it reads only the config + read-only
    // snapshots and returns a result DTO. No game calls, no randomness that
    // is not seeded, no CUserCmd, no memory writes. The same evaluators run
    // for LIVE and SYNTHETIC bundles, so menu settings deterministically
    // shape the plan in both modes.
    // -------------------------------------------------------------------
    namespace Eval
    {
        // Nominal per-group base damage table (weapon group 0..5). This is an
        // illustrative model constant, NOT a game memory read, used only when a
        // verified live base-damage field is unavailable.
        inline float group_base_damage(int group) noexcept
        {
            switch (group)
            {
            case 1: return 26.f;  // smg
            case 2: return 32.f;  // rifle
            case 3: return 26.f;  // shotgun (per pellet-ish, illustrative)
            case 4: return 90.f;  // sniper
            case 5: return 40.f;  // utility/knife/misc
            default: return 24.f; // pistol and the rest
            }
        }

        inline bool weapon_supports_doubletap(int group) noexcept
        {
            // Rifles/pistols/smg conceptually eligible; utility/knife not.
            return group != 5;
        }

        // Deterministic offline hitchance sampler. Models the aim cone against
        // an angular hitbox tolerance derived from distance, point scale and
        // multipoint. Seeded LCG -> reproducible for identical inputs.
        inline HitchanceResult eval_hitchance(
            const RageDryConfig& cfg,
            const WeaponSnapshot& weapon,
            const CandidateSnapshot* target) noexcept
        {
            HitchanceResult r{};
            r.required = static_cast<float>(cfg.hitchance);

            if (!target)
            {
                r.state = GateState::Unknown;
                return r;
            }
            if (cfg.hitchance <= 0)
            {
                r.state = GateState::Pass;
                r.chance = 100.f;
                r.samples = 0;
                r.hits = 0;
                return r;
            }

            const float dist = target->distance > 1.f ? target->distance : 1.f;
            float hitbox_radius = (cfg.prefer_body ? 18.f : 8.f) * cfg.point_scale;
            if (cfg.multipoint)
                hitbox_radius *= 1.25f;
            if (cfg.safe_points)
                hitbox_radius *= 0.85f; // safe points trade coverage for safety

            const float tol = std::atan2(hitbox_radius, dist); // radians tolerance
            const float spread = weapon.spread + weapon.inaccuracy;

            const int samples = 256;
            int hits = 0;

            if (spread <= 0.f)
            {
                // No verified spread (live weapon): closed-form estimate from the
                // angular tolerance vs a nominal cone. Deterministic.
                const float nominal_cone = 0.02f;
                const float ratio = tol / (tol + nominal_cone);
                r.chance = 100.f * ratio;
                r.samples = samples;
                r.hits = static_cast<int>(r.chance * samples / 100.f);
            }
            else
            {
                std::uint32_t seed =
                    0x1234567u ^ static_cast<std::uint32_t>(target->candidate_id * 2654435761u) ^
                    static_cast<std::uint32_t>(dist);
                for (int i = 0; i < samples; ++i)
                {
                    seed = seed * 1664525u + 1013904223u;
                    const float u = (seed >> 8) / static_cast<float>(1u << 24); // [0,1)
                    seed = seed * 1664525u + 1013904223u;
                    const float v = (seed >> 8) / static_cast<float>(1u << 24);
                    // Sample a radius within the spread cone (uniform over disc).
                    const float rad = spread * std::sqrt(u);
                    // v jitters tolerance slightly to avoid a hard step.
                    if (rad <= tol * (0.85f + 0.30f * v))
                        ++hits;
                }
                r.samples = samples;
                r.hits = hits;
                r.chance = 100.f * hits / samples;
            }

            r.state = (r.chance >= r.required) ? GateState::Pass : GateState::Fail;
            return r;
        }

        inline DamageResult eval_damage(
            const RageDryConfig& cfg,
            const WeaponSnapshot& weapon,
            const CandidateSnapshot* target) noexcept
        {
            DamageResult r{};
            r.minimum_damage = static_cast<float>(cfg.minimum_damage);

            if (!target)
            {
                r.state = GateState::Unknown;
                return r;
            }

            if (cfg.damage_override)
            {
                r.predicted_damage = static_cast<float>(cfg.override_damage);
            }
            else
            {
                const float base = group_base_damage(weapon.weapon_type);
                // Simple distance falloff model (illustrative, no game read).
                const float dist = target->distance > 0.f ? target->distance : 0.f;
                const float falloff = std::exp(-dist / 3000.f);
                float dmg = base * (0.55f + 0.45f * falloff);
                // Body preference lowers effective damage vs a head shot.
                if (cfg.prefer_body)
                    dmg *= 0.75f;
                // Clamp by the target's remaining health for a realistic gate.
                if (target->health > 0 && dmg > static_cast<float>(target->health))
                    dmg = static_cast<float>(target->health);
                r.predicted_damage = dmg;
            }

            r.state = (r.predicted_damage >= r.minimum_damage)
                          ? GateState::Pass : GateState::Fail;
            return r;
        }

        inline StopPredictionResult eval_stop(
            const RageDryConfig& cfg,
            const WeaponSnapshot& weapon,
            const PredictionSnapshot& prediction) noexcept
        {
            StopPredictionResult r{};
            (void)cfg;
            if (!prediction.ready)
            {
                r.state = GateState::Unknown;
                return r;
            }
            r.current_speed = prediction.speed_2d;
            // Desired accuracy speed: a fraction of the weapon's run speed, or a
            // conservative constant when weapon max speed is unproven (0).
            const float base = weapon.max_speed > 1.f ? weapon.max_speed : 250.f;
            r.desired_speed = base * 0.34f;
            r.would_stop = r.current_speed > r.desired_speed;
            r.state = r.would_stop ? GateState::Pass : GateState::Fail;
            return r;
        }

        inline DoubletapEligibility eval_doubletap(
            const RageDryConfig& cfg,
            const WeaponSnapshot& weapon,
            const CandidateSnapshot* target,
            bool has_tick_state) noexcept
        {
            DoubletapEligibility r{};
            r.weapon_allowed = weapon.ready && weapon_supports_doubletap(weapon.weapon_type);
            r.target_available = target != nullptr;
            r.cooldown_ready = has_tick_state; // requires validated tick/subtick state
            r.requested_ticks = cfg.doubletap_plan ? 16 : 0;

            if (!has_tick_state)
            {
                // No validated tick/subtick source -> cannot be eligible.
                r.state = GateState::Blocked;
                return r;
            }
            r.state = (r.weapon_allowed && r.target_available && r.cooldown_ready)
                          ? GateState::Pass : GateState::Fail;
            return r;
        }

        inline ExtrapolationResult eval_extrapolation(
            const RageDryConfig& cfg,
            const PredictionSnapshot& prediction) noexcept
        {
            ExtrapolationResult r{};
            if (!cfg.extrapolation_plan)
            {
                r.state = GateState::Unknown;
                return r;
            }
            if (!prediction.ready)
            {
                r.state = GateState::Unknown;
                return r;
            }
            r.ticks_ahead = 1;
            r.before = prediction.origin;
            const float dt = 0.015625f * r.ticks_ahead; // 64-tick
            r.projected = Vec3{
                prediction.origin.x + prediction.velocity.x * dt,
                prediction.origin.y + prediction.velocity.y * dt,
                prediction.origin.z + prediction.velocity.z * dt
            };
            // Velocity-based extrapolation only. Trace-validated extrapolation
            // stays BLOCKED elsewhere; this lane never needs a trace.
            r.state = GateState::Pass;
            return r;
        }

        inline PenetrationResult eval_penetration(Readiness trace) noexcept
        {
            PenetrationResult r{};
            // Autowall/penetration depends on the runtime trace backend, which is
            // BLOCKED. Never reports Pass in P7.
            r.state = GateState::Blocked;
            r.penetrated = false;
            r.surfaces = 0;
            r.predicted_damage = 0.f;
            (void)trace;
            return r;
        }

        inline AntiAimPlan eval_antiaim(const RageDryConfig& cfg) noexcept
        {
            AntiAimPlan r{};
            if (!cfg.anti_aim_plan)
            {
                r.state = GateState::Unknown;
                return r;
            }
            // Planner-only mapping. NEVER written to view angles / CUserCmd.
            r.planned_pitch = 89.f; // "down" baseline
            r.planned_yaw = static_cast<float>(cfg.anti_aim_yaw);
            r.planned_body_yaw = static_cast<float>(cfg.anti_aim_body_yaw);
            r.freestanding_side = cfg.anti_aim_freestanding ? 1 : 0;
            r.state = GateState::Pass;
            return r;
        }

        inline QuickPeekPlan eval_quickpeek(
            const RageDryConfig& cfg,
            const CombatFrameSnapshot& frame) noexcept
        {
            QuickPeekPlan r{};
            if (!cfg.quick_peek_plan)
            {
                r.state = GateState::Unknown;
                return r;
            }
            r.anchor = frame.origin;
            r.planned_return = frame.origin; // return-to-cover anchor (planner only)
            r.state = frame.ready ? GateState::Pass : GateState::Unknown;
            return r;
        }
    } // namespace Eval

    // -------------------------------------------------------------------
    // Hardening: config sanitation + snapshot guards + execution boundary.
    // -------------------------------------------------------------------
    namespace Guards
    {
        inline float clampf(float v, float lo, float hi) noexcept
        {
            if (!std::isfinite(v)) return lo;
            return v < lo ? lo : (v > hi ? hi : v);
        }
        inline int clampi(int v, int lo, int hi) noexcept
        {
            return v < lo ? lo : (v > hi ? hi : v);
        }

        // Clamp a (possibly deserialized / corrupt) config into valid ranges.
        // Applied after config load and before every evaluation.
        inline RageDryConfig sanitize(RageDryConfig c) noexcept
        {
            c.selection = clampi(c.selection, 0, 2);
            c.max_fov = clampf(c.max_fov, 0.f, 180.f);
            c.primary_hitbox = clampi(c.primary_hitbox, 0, kHitboxCount - 1);
            c.point_scale = clampf(c.point_scale, 0.05f, 1.f);
            c.hitchance = clampi(c.hitchance, 0, 100);
            c.minimum_damage = clampi(c.minimum_damage, 0, 500);
            c.override_damage = clampi(c.override_damage, 0, 500);
            c.anti_aim_pitch = clampi(c.anti_aim_pitch, -180, 180);
            c.anti_aim_yaw = clampi(c.anti_aim_yaw, -180, 180);
            c.anti_aim_body_yaw = clampi(c.anti_aim_body_yaw, -180, 180);
            return c;
        }

        inline bool candidate_finite(const CandidateSnapshot& c) noexcept
        {
            return std::isfinite(c.fov) && std::isfinite(c.distance);
        }

        // Drop NaN/inf-poisoned and duplicate candidates (same pawn or same
        // controller). Returns the number removed. In-place, order preserved.
        inline int sanitize_candidates(std::vector<CandidateSnapshot>& v) noexcept
        {
            int removed = 0;
            std::vector<CandidateSnapshot> out;
            out.reserve(v.size());
            for (auto& c : v)
            {
                if (!c.valid)
                {
                    ++removed;
                    continue;
                }
                if (!candidate_finite(c))
                {
                    // Poisoned metric -> push distance/fov out of contention
                    // rather than letting NaN win a comparison.
                    c.fov = std::numeric_limits<float>::infinity();
                    c.distance = std::numeric_limits<float>::infinity();
                }
                bool dup = false;
                for (const auto& o : out)
                {
                    if ((c.pawn && o.pawn == c.pawn) ||
                        (c.controller && o.controller == c.controller))
                    {
                        dup = true;
                        break;
                    }
                }
                if (dup) { ++removed; continue; }
                out.push_back(c);
            }
            v.swap(out);
            return removed;
        }
    } // namespace Guards

    // Hard execution boundary. The dry-run system produces plans only; this is
    // the single choke point a future executor would pass through. The default
    // executor is a NO-OP that ALWAYS rejects: it never touches CUserCmd, view
    // angles, attack/movement/tick state. It exists so the boundary is testable.
    struct ExecutionBoundary
    {
        // Returns true only if execution actually happened. Always false here.
        static bool execute(const DryRunActionPlan& plan) noexcept
        {
            (void)plan;
            return false; // execution is disabled at the boundary, unconditionally
        }

        static constexpr bool execution_permitted() noexcept { return false; }
    };

    struct State
    {
        RageDryConfig config{};

        CommandContext command{};
        CombatFrameSnapshot frame{};
        WeaponSnapshot weapon{};
        PredictionSnapshot prediction{};

        std::vector<CandidateSnapshot> candidates{};
        std::vector<LagRecordSnapshot> lag_records{};
        std::vector<ShootHistorySnapshot> shoot_history{};

        HitchanceResult hitchance{};
        PenetrationResult penetration{};
        DamageResult damage{};
        StopPredictionResult stop_prediction{};
        ExtrapolationResult extrapolation{};
        DoubletapEligibility doubletap{};
        AntiAimPlan anti_aim{};
        QuickPeekPlan quick_peek{};

        DryRunActionPlan action{};
        ReadinessMatrix readiness{};
        SelfTestReport self_test{};
        EvaluatorTestReport evaluator_tests{};

        SourceMode source = SourceMode::None;
        int live_entity_count = 0;

        // True only when a validated tick/subtick source has published. Live
        // capture leaves this false (no validated source) -> doubletap Blocked.
        bool has_tick_state = false;

        std::uint64_t generation = 0;

        void reset_volatile() noexcept
        {
            command.reset();
            frame = {};
            weapon = {};
            prediction = {};

            candidates.clear();
            lag_records.clear();
            shoot_history.clear();

            hitchance = {};
            penetration = {};
            damage = {};
            stop_prediction = {};
            extrapolation = {};
            doubletap = {};
            anti_aim = {};
            quick_peek = {};
            action = {};
            readiness = {};
            source = SourceMode::None;
            live_entity_count = 0;
            has_tick_state = false;
            ++generation;
        }

        // Full P7 dry-run pipeline: select target -> run every evaluator/gate
        // -> populate result DTOs -> build the DryRunActionPlan. Pure w.r.t the
        // game: no CUserCmd, no memory writes, execution stays disabled.
        void evaluate() noexcept
        {
            // Failure-safe: clamp config and strip NaN/inf/duplicate candidates
            // before anything reads them.
            config = Guards::sanitize(config);
            Guards::sanitize_candidates(candidates);

            const CandidateSnapshot* target =
                DecisionEngine::select_candidate(config, candidates);

            // Run all evaluators (they own the result DTOs now).
            hitchance      = Eval::eval_hitchance(config, weapon, target);
            damage         = Eval::eval_damage(config, weapon, target);
            stop_prediction = Eval::eval_stop(config, weapon, prediction);
            doubletap      = Eval::eval_doubletap(config, weapon, target, has_tick_state);
            extrapolation  = Eval::eval_extrapolation(config, prediction);
            penetration    = Eval::eval_penetration(readiness.trace);
            anti_aim       = Eval::eval_antiaim(config);
            quick_peek     = Eval::eval_quickpeek(config, frame);

            DryRunActionPlan out{};
            out.execution_enabled = false;

            if (!config.enabled || !target)
            {
                action = out;
                return;
            }

            // Body-aim preference: settle the hitbox the plan would use.
            const int body_region = 2; // chest
            out.body_hitbox = config.prefer_body ? body_region : config.primary_hitbox;
            out.selected_hitbox = out.body_hitbox;

            out.target_found = true;
            out.target_id = target->candidate_id;
            out.fov = target->fov;
            out.distance = target->distance;
            out.hitchance = hitchance.chance;
            out.predicted_damage = damage.predicted_damage;

            // Gate provenance.
            out.fov_pass = target->fov <= config.max_fov;
            out.visibility_pass =
                !config.require_visibility || (target->visibility_known && target->visible);
            out.hitchance_pass =
                (hitchance.state == GateState::Pass) || config.hitchance <= 0;
            out.damage_pass =
                (damage.state == GateState::Pass) || config.minimum_damage <= 0;

            // Backtrack availability for this target (read-only lag records).
            int recs = 0;
            for (const auto& lr : lag_records)
                if (lr.valid && (lr.candidate_id == target->candidate_id || lr.candidate_id < 0))
                    ++recs;
            out.backtrack_records = recs;

            // would_* decisions — each bound to config AND its gate/data.
            out.would_aim = true;
            out.would_silent = config.silent_plan;
            out.would_scope = config.auto_scope_plan;
            out.would_no_spread = config.no_spread_plan;
            out.would_stop = config.auto_stop_plan && stop_prediction.would_stop;
            out.would_use_penetration =
                config.penetration_crosshair && penetration.state == GateState::Pass; // Blocked -> false
            out.would_use_backtrack = config.lagcomp_snapshots && recs > 0;
            out.would_extrapolate =
                config.extrapolation_plan && extrapolation.state == GateState::Pass;
            out.would_doubletap =
                config.doubletap_plan && doubletap.state == GateState::Pass;
            out.would_quick_peek =
                config.quick_peek_plan && quick_peek.state == GateState::Pass;
            out.would_duck_peek = config.duck_peek_plan;
            out.would_anti_aim =
                config.anti_aim_plan && anti_aim.state == GateState::Pass;

            // Fire only when the accuracy + damage gates both pass.
            out.would_fire =
                config.auto_fire_plan &&
                out.fov_pass &&
                out.visibility_pass &&
                out.hitchance_pass &&
                out.damage_pass;

            action = out;
        }
    };

    inline State g_state{};

    inline SelfTestReport run_builtin_self_test() noexcept
    {
        SelfTestReport report{};
        report.ran = true;

        RageDryConfig cfg{};
        cfg.enabled = true;
        cfg.max_fov = 10.f;
        cfg.require_visibility = true;

        std::vector<CandidateSnapshot> candidates;

        CandidateSnapshot a{};
        a.candidate_id = 1;
        a.valid = a.alive = a.enemy = true;
        a.visibility_known = true;
        a.visible = true;
        a.fov = 4.2f;
        a.distance = 300.f;
        a.health = 100;
        candidates.push_back(a);

        CandidateSnapshot b{};
        b.candidate_id = 2;
        b.valid = b.alive = b.enemy = true;
        b.visibility_known = true;
        b.visible = true;
        b.fov = 1.8f;
        b.distance = 800.f;
        b.health = 70;
        candidates.push_back(b);

        CandidateSnapshot c{};
        c.candidate_id = 3;
        c.valid = c.alive = c.enemy = true;
        c.visibility_known = true;
        c.visible = false;
        c.fov = 0.9f;
        c.distance = 250.f;
        c.health = 40;
        candidates.push_back(c);

        cfg.selection = static_cast<int>(TargetSelection::Fov);
        const CandidateSnapshot* fov =
            DecisionEngine::select_candidate(cfg, candidates);

        report.fov_winner = fov ? fov->candidate_id : -1;
        report.fov_selection_pass = report.fov_winner == 2;
        report.invisible_rejection_pass = report.fov_winner != 3;

        cfg.selection = static_cast<int>(TargetSelection::Distance);
        const CandidateSnapshot* dist =
            DecisionEngine::select_candidate(cfg, candidates);

        report.distance_winner = dist ? dist->candidate_id : -1;
        report.distance_selection_pass = report.distance_winner == 1;

        g_state.self_test = report;
        return report;
    }

    // Deterministic offline tests for every evaluator/gate. Uses fixed inputs
    // and asserts qualitative outcomes; no game state, fully reproducible.
    inline EvaluatorTestReport run_evaluator_tests() noexcept
    {
        EvaluatorTestReport t{};
        t.ran = true;

        RageDryConfig cfg{};
        cfg.enabled = true;
        cfg.hitchance = 50;
        cfg.minimum_damage = 30;
        cfg.point_scale = 0.9f;

        WeaponSnapshot wep{};
        wep.ready = true;
        wep.weapon_type = 2; // rifle
        wep.spread = 0.005f;
        wep.inaccuracy = 0.010f;
        wep.max_speed = 215.f;

        CandidateSnapshot near_t{};
        near_t.candidate_id = 10;
        near_t.valid = near_t.alive = near_t.enemy = true;
        near_t.visibility_known = near_t.visible = true;
        near_t.distance = 200.f; near_t.fov = 1.0f; near_t.health = 100;

        CandidateSnapshot far_t = near_t;
        far_t.candidate_id = 11;
        far_t.distance = 4000.f;

        // Hitchance: near passes, far fails.
        {
            const auto hn = Eval::eval_hitchance(cfg, wep, &near_t);
            const auto hf = Eval::eval_hitchance(cfg, wep, &far_t);
            t.hitchance_pass =
                hn.state == GateState::Pass && hf.state == GateState::Fail;
        }

        // Damage: override passes; weak base at long range fails a high minimum.
        {
            RageDryConfig ov = cfg; ov.damage_override = true; ov.override_damage = 100;
            const auto dov = Eval::eval_damage(ov, wep, &near_t);
            RageDryConfig hi = cfg; hi.minimum_damage = 120;
            WeaponSnapshot pistol{}; pistol.ready = true; pistol.weapon_type = 0;
            const auto dweak = Eval::eval_damage(hi, pistol, &far_t);
            t.damage_pass =
                dov.state == GateState::Pass && dweak.state == GateState::Fail;
        }

        // Stop-prediction: moving -> would_stop; still -> not.
        {
            PredictionSnapshot moving{}; moving.ready = true; moving.speed_2d = 200.f;
            PredictionSnapshot still{};  still.ready = true;  still.speed_2d = 0.f;
            const auto sm = Eval::eval_stop(cfg, wep, moving);
            const auto ss = Eval::eval_stop(cfg, wep, still);
            t.stop_pass = sm.would_stop && !ss.would_stop;
        }

        // Doubletap: BLOCKED without tick state; eligible with tick state.
        {
            const auto d0 = Eval::eval_doubletap(cfg, wep, &near_t, false);
            const auto d1 = Eval::eval_doubletap(cfg, wep, &near_t, true);
            t.doubletap_pass =
                d0.state == GateState::Blocked && d1.state == GateState::Pass;
        }

        // Extrapolation: projects forward from velocity.
        {
            RageDryConfig ex = cfg; ex.extrapolation_plan = true;
            PredictionSnapshot p{}; p.ready = true;
            p.origin = { 0.f, 0.f, 0.f }; p.velocity = { 250.f, 0.f, 0.f };
            const auto e = Eval::eval_extrapolation(ex, p);
            t.extrapolation_pass =
                e.state == GateState::Pass && e.projected.x > e.before.x;
        }

        // Penetration: always BLOCKED (no trace backend).
        {
            const auto pen = Eval::eval_penetration(Readiness::Blocked);
            t.penetration_pass = pen.state == GateState::Blocked;
        }

        // Anti-aim planner: pass when enabled.
        {
            RageDryConfig aa = cfg; aa.anti_aim_plan = true;
            t.antiaim_pass = Eval::eval_antiaim(aa).state == GateState::Pass;
        }

        // Quick-peek planner: pass when enabled + frame ready.
        {
            RageDryConfig qp = cfg; qp.quick_peek_plan = true;
            CombatFrameSnapshot f{}; f.ready = true;
            t.quickpeek_pass = Eval::eval_quickpeek(qp, f).state == GateState::Pass;
        }

        g_state.evaluator_tests = t;
        return t;
    }

    inline const char* readiness_name(Readiness r) noexcept
    {
        switch (r)
        {
        case Readiness::Ready:   return "READY";
        case Readiness::Blocked: return "BLOCKED";
        default:                 return "WAIT";
        }
    }

    // Per-lane classification for the debug UI: a READY lane is qualified by the
    // active source (LIVE vs SYNTH); non-ready lanes report UNAVAIL / BLOCKED.
    inline const char* lane_label(Readiness r, SourceMode src) noexcept
    {
        switch (r)
        {
        case Readiness::Blocked: return "BLOCKED";
        case Readiness::Ready:
            return src == SourceMode::Live      ? "LIVE"
                 : src == SourceMode::Synthetic ? "SYNTH"
                                                : "READY";
        default:                 return "UNAVAIL";
        }
    }

    inline const char* source_name(SourceMode s) noexcept
    {
        switch (s)
        {
        case SourceMode::Live:      return "LIVE";
        case SourceMode::Synthetic: return "SYNTHETIC";
        default:                    return "NONE";
        }
    }

    inline const char* gate_name(GateState s) noexcept
    {
        switch (s)
        {
        case GateState::Pass:    return "PASS";
        case GateState::Fail:    return "FAIL";
        case GateState::Blocked: return "BLOCKED";
        default:                 return "UNKNOWN";
        }
    }
}
