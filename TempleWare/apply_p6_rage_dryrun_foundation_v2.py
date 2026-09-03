#!/usr/bin/env python3
from pathlib import Path
import re
import shutil
import subprocess
import sys
from datetime import datetime

RAGE_HEADER = '#pragma once\n\n// TempleWare P6 dry-run Rage foundation.\n//\n// IMPORTANT:\n// - This layer does not acquire CUserCmd.\n// - This layer does not mutate CUserCmd.\n// - This layer does not write view angles, attack buttons, movement, or tick state.\n// - UI controls publish only dry-run/planner configuration.\n// - Live providers can be attached later by the owner without changing the DTO/evaluator surface.\n\n#include <algorithm>\n#include <array>\n#include <cmath>\n#include <cstdint>\n#include <limits>\n#include <vector>\n\nclass CUserCmd;\n\nnamespace RageDryRun\n{\n    inline constexpr int kHitboxCount = 9;\n\n    enum class Readiness : std::uint8_t\n    {\n        Unavailable = 0,\n        Ready,\n        Blocked\n    };\n\n    enum class GateState : std::uint8_t\n    {\n        Unknown = 0,\n        Pass,\n        Fail,\n        Blocked\n    };\n\n    enum class TargetSelection : int\n    {\n        Fov = 0,\n        Distance = 1,\n        Health = 2\n    };\n\n    struct Vec3\n    {\n        float x = 0.f;\n        float y = 0.f;\n        float z = 0.f;\n    };\n\n    // Trivially-copyable on purpose: gui_config.h snapshots it directly.\n    struct RageDryConfig\n    {\n        bool enabled = false;\n\n        // Targeting / scan policy.\n        int selection = static_cast<int>(TargetSelection::Fov);\n        float max_fov = 5.0f;\n        bool require_visibility = true;\n        bool prefer_body = false;\n\n        // Hitbox / scan geometry policy.\n        int primary_hitbox = 0;\n        bool hitboxes[kHitboxCount] = {\n            true,   // head\n            false,  // neck\n            true,   // chest\n            true,   // stomach\n            true,   // pelvis\n            false,  // left arm\n            false,  // right arm\n            false,  // left leg\n            false   // right leg\n        };\n        bool multipoint = true;\n        float point_scale = 0.80f;\n        bool safe_points = false;\n\n        // Accuracy / damage evaluation configuration.\n        int hitchance = 75;\n        int minimum_damage = 30;\n        bool damage_override = false;\n        int override_damage = 50;\n        bool penetration_crosshair = false;\n\n        // Planner-only features. These NEVER execute from this layer.\n        bool silent_plan = false;\n        bool auto_fire_plan = false;\n        bool auto_scope_plan = false;\n        bool auto_stop_plan = false;\n        bool force_shot_plan = false;\n        bool no_spread_plan = false;\n        bool doubletap_plan = false;\n\n        // Snapshot/simulation lanes.\n        bool lagcomp_snapshots = true;\n        bool extrapolation_plan = false;\n        bool shoot_history = true;\n        bool quick_peek_plan = false;\n        bool duck_peek_plan = false;\n\n        // Anti-aim is represented only as a planner/debug request.\n        bool anti_aim_plan = false;\n        int anti_aim_pitch = 0;\n        int anti_aim_yaw = 0;\n        int anti_aim_body_yaw = 0;\n        bool anti_aim_freestanding = false;\n    };\n\n    struct CommandContext\n    {\n        // Owner-supplied later. This foundation never dereferences or mutates it.\n        CUserCmd* command = nullptr;\n        std::uint64_t sequence = 0;\n        bool available = false;\n\n        void reset() noexcept\n        {\n            command = nullptr;\n            sequence = 0;\n            available = false;\n        }\n    };\n\n    struct CombatFrameSnapshot\n    {\n        std::uint64_t generation = 0;\n\n        std::uintptr_t local_pawn = 0;\n        std::uintptr_t local_controller = 0;\n\n        Vec3 eye_position{};\n        Vec3 origin{};\n        Vec3 velocity{};\n\n        float view_pitch = 0.f;\n        float view_yaw = 0.f;\n\n        int tick = 0;\n        float time = 0.f;\n\n        bool on_ground = false;\n        bool scoped = false;\n        bool ready = false;\n    };\n\n    struct WeaponSnapshot\n    {\n        std::uint64_t generation = 0;\n        std::uintptr_t weapon = 0;\n\n        int weapon_type = 0;\n        int item_definition = 0;\n        int ammo = 0;\n\n        float range = 0.f;\n        float max_speed = 0.f;\n        float spread = 0.f;\n        float inaccuracy = 0.f;\n        float recoil_index = 0.f;\n\n        bool can_scope = false;\n        bool ready = false;\n    };\n\n    struct PredictionSnapshot\n    {\n        std::uint64_t generation = 0;\n        Vec3 origin{};\n        Vec3 velocity{};\n\n        float speed_2d = 0.f;\n        bool on_ground = false;\n        bool ready = false;\n    };\n\n    struct CandidateSnapshot\n    {\n        int candidate_id = -1;\n\n        std::uintptr_t controller = 0;\n        std::uintptr_t pawn = 0;\n\n        int health = 0;\n        int team = 0;\n\n        float fov = std::numeric_limits<float>::infinity();\n        float distance = std::numeric_limits<float>::infinity();\n\n        bool valid = false;\n        bool alive = false;\n        bool enemy = false;\n\n        bool visibility_known = false;\n        bool visible = false;\n\n        bool bones_ready = false;\n        bool hitboxes_ready = false;\n    };\n\n    struct BonePose\n    {\n        Vec3 position{};\n        float qx = 0.f;\n        float qy = 0.f;\n        float qz = 0.f;\n        float qw = 1.f;\n        bool valid = false;\n    };\n\n    struct LagRecordSnapshot\n    {\n        int candidate_id = -1;\n        int tick = 0;\n        float simulation_time = 0.f;\n\n        Vec3 origin{};\n        std::array<BonePose, 128> bones{};\n\n        bool valid = false;\n    };\n\n    struct ShootHistorySnapshot\n    {\n        int client_tick = 0;\n        int server_tick = 0;\n        float fraction = 0.f;\n        bool valid = false;\n    };\n\n    struct HitchanceResult\n    {\n        GateState state = GateState::Unknown;\n        int samples = 0;\n        int hits = 0;\n        float chance = 0.f;\n        float required = 0.f;\n    };\n\n    struct PenetrationResult\n    {\n        GateState state = GateState::Blocked;\n        float predicted_damage = 0.f;\n        bool penetrated = false;\n        int surfaces = 0;\n    };\n\n    struct DamageResult\n    {\n        GateState state = GateState::Unknown;\n        float predicted_damage = 0.f;\n        float minimum_damage = 0.f;\n    };\n\n    struct StopPredictionResult\n    {\n        GateState state = GateState::Unknown;\n        float current_speed = 0.f;\n        float desired_speed = 0.f;\n        bool would_stop = false;\n    };\n\n    struct ExtrapolationResult\n    {\n        GateState state = GateState::Unknown;\n        int ticks_ahead = 0;\n        Vec3 before{};\n        Vec3 projected{};\n    };\n\n    struct DoubletapEligibility\n    {\n        GateState state = GateState::Unknown;\n        bool weapon_allowed = false;\n        bool target_available = false;\n        bool cooldown_ready = false;\n        int requested_ticks = 0;\n    };\n\n    struct AntiAimPlan\n    {\n        GateState state = GateState::Unknown;\n        float planned_pitch = 0.f;\n        float planned_yaw = 0.f;\n        float planned_body_yaw = 0.f;\n        int freestanding_side = 0;\n    };\n\n    struct QuickPeekPlan\n    {\n        GateState state = GateState::Unknown;\n        Vec3 anchor{};\n        Vec3 planned_return{};\n    };\n\n    struct DryRunActionPlan\n    {\n        bool target_found = false;\n        int target_id = -1;\n        int selected_hitbox = -1;\n        int selected_record = -1;\n\n        float fov = 0.f;\n        float distance = 0.f;\n        float hitchance = 0.f;\n        float predicted_damage = 0.f;\n\n        bool would_aim = false;\n        bool would_silent = false;\n        bool would_fire = false;\n        bool would_scope = false;\n        bool would_stop = false;\n        bool would_use_penetration = false;\n        bool would_use_backtrack = false;\n        bool would_extrapolate = false;\n        bool would_doubletap = false;\n        bool would_quick_peek = false;\n        bool would_duck_peek = false;\n        bool would_anti_aim = false;\n\n        // Deliberately immutable policy for P6.\n        bool execution_enabled = false;\n    };\n\n    struct ReadinessMatrix\n    {\n        Readiness local = Readiness::Unavailable;\n        Readiness entities = Readiness::Unavailable;\n        Readiness combat_frame = Readiness::Unavailable;\n        Readiness weapon = Readiness::Unavailable;\n        Readiness bones = Readiness::Unavailable;\n        Readiness hitboxes = Readiness::Unavailable;\n        Readiness prediction = Readiness::Unavailable;\n        Readiness trace = Readiness::Blocked;\n        Readiness penetration = Readiness::Blocked;\n        Readiness lagcomp = Readiness::Unavailable;\n        Readiness shoot_history = Readiness::Unavailable;\n        Readiness command = Readiness::Unavailable;\n    };\n\n    struct SelfTestReport\n    {\n        bool ran = false;\n        bool fov_selection_pass = false;\n        bool distance_selection_pass = false;\n        bool invisible_rejection_pass = false;\n\n        int fov_winner = -1;\n        int distance_winner = -1;\n    };\n\n    class DecisionEngine\n    {\n    public:\n        static const CandidateSnapshot* select_candidate(\n            const RageDryConfig& cfg,\n            const std::vector<CandidateSnapshot>& candidates) noexcept\n        {\n            const CandidateSnapshot* best = nullptr;\n\n            for (const auto& c : candidates)\n            {\n                if (!c.valid || !c.alive || !c.enemy)\n                    continue;\n\n                if (cfg.require_visibility)\n                {\n                    if (!c.visibility_known || !c.visible)\n                        continue;\n                }\n\n                if (c.fov > cfg.max_fov)\n                    continue;\n\n                if (!best)\n                {\n                    best = &c;\n                    continue;\n                }\n\n                switch (static_cast<TargetSelection>(cfg.selection))\n                {\n                case TargetSelection::Distance:\n                    if (c.distance < best->distance)\n                        best = &c;\n                    break;\n                case TargetSelection::Health:\n                    if (c.health < best->health)\n                        best = &c;\n                    break;\n                case TargetSelection::Fov:\n                default:\n                    if (c.fov < best->fov)\n                        best = &c;\n                    break;\n                }\n            }\n\n            return best;\n        }\n\n        static DryRunActionPlan build_plan(\n            const RageDryConfig& cfg,\n            const CandidateSnapshot* target,\n            const HitchanceResult& hitchance,\n            const DamageResult& damage,\n            const StopPredictionResult& stop,\n            const DoubletapEligibility& doubletap) noexcept\n        {\n            DryRunActionPlan out{};\n            out.execution_enabled = false;\n\n            if (!cfg.enabled || !target)\n                return out;\n\n            out.target_found = true;\n            out.target_id = target->candidate_id;\n            out.selected_hitbox = cfg.primary_hitbox;\n            out.fov = target->fov;\n            out.distance = target->distance;\n            out.hitchance = hitchance.chance;\n            out.predicted_damage = damage.predicted_damage;\n\n            const bool hc_pass =\n                hitchance.state == GateState::Pass ||\n                cfg.hitchance <= 0;\n\n            const bool damage_pass =\n                damage.state == GateState::Pass ||\n                cfg.minimum_damage <= 0;\n\n            out.would_aim = true;\n            out.would_silent = cfg.silent_plan;\n            out.would_scope = cfg.auto_scope_plan;\n            out.would_stop = cfg.auto_stop_plan && stop.would_stop;\n            out.would_use_penetration =\n                cfg.penetration_crosshair && damage.state != GateState::Blocked;\n            out.would_use_backtrack = cfg.lagcomp_snapshots;\n            out.would_extrapolate = cfg.extrapolation_plan;\n            out.would_doubletap =\n                cfg.doubletap_plan && doubletap.state == GateState::Pass;\n            out.would_quick_peek = cfg.quick_peek_plan;\n            out.would_duck_peek = cfg.duck_peek_plan;\n            out.would_anti_aim = cfg.anti_aim_plan;\n\n            // Planner output only. No command/button mutation occurs here.\n            out.would_fire =\n                cfg.auto_fire_plan &&\n                hc_pass &&\n                damage_pass;\n\n            return out;\n        }\n    };\n\n    struct State\n    {\n        RageDryConfig config{};\n\n        CommandContext command{};\n        CombatFrameSnapshot frame{};\n        WeaponSnapshot weapon{};\n        PredictionSnapshot prediction{};\n\n        std::vector<CandidateSnapshot> candidates{};\n        std::vector<LagRecordSnapshot> lag_records{};\n        std::vector<ShootHistorySnapshot> shoot_history{};\n\n        HitchanceResult hitchance{};\n        PenetrationResult penetration{};\n        DamageResult damage{};\n        StopPredictionResult stop_prediction{};\n        ExtrapolationResult extrapolation{};\n        DoubletapEligibility doubletap{};\n        AntiAimPlan anti_aim{};\n        QuickPeekPlan quick_peek{};\n\n        DryRunActionPlan action{};\n        ReadinessMatrix readiness{};\n        SelfTestReport self_test{};\n\n        std::uint64_t generation = 0;\n\n        void reset_volatile() noexcept\n        {\n            command.reset();\n            frame = {};\n            weapon = {};\n            prediction = {};\n\n            candidates.clear();\n            lag_records.clear();\n            shoot_history.clear();\n\n            hitchance = {};\n            penetration = {};\n            damage = {};\n            stop_prediction = {};\n            extrapolation = {};\n            doubletap = {};\n            anti_aim = {};\n            quick_peek = {};\n            action = {};\n            readiness = {};\n            ++generation;\n        }\n\n        void evaluate() noexcept\n        {\n            const CandidateSnapshot* target =\n                DecisionEngine::select_candidate(config, candidates);\n\n            action = DecisionEngine::build_plan(\n                config,\n                target,\n                hitchance,\n                damage,\n                stop_prediction,\n                doubletap);\n        }\n    };\n\n    inline State g_state{};\n\n    inline SelfTestReport run_builtin_self_test() noexcept\n    {\n        SelfTestReport report{};\n        report.ran = true;\n\n        RageDryConfig cfg{};\n        cfg.enabled = true;\n        cfg.max_fov = 10.f;\n        cfg.require_visibility = true;\n\n        std::vector<CandidateSnapshot> candidates;\n\n        CandidateSnapshot a{};\n        a.candidate_id = 1;\n        a.valid = a.alive = a.enemy = true;\n        a.visibility_known = true;\n        a.visible = true;\n        a.fov = 4.2f;\n        a.distance = 300.f;\n        a.health = 100;\n        candidates.push_back(a);\n\n        CandidateSnapshot b{};\n        b.candidate_id = 2;\n        b.valid = b.alive = b.enemy = true;\n        b.visibility_known = true;\n        b.visible = true;\n        b.fov = 1.8f;\n        b.distance = 800.f;\n        b.health = 70;\n        candidates.push_back(b);\n\n        CandidateSnapshot c{};\n        c.candidate_id = 3;\n        c.valid = c.alive = c.enemy = true;\n        c.visibility_known = true;\n        c.visible = false;\n        c.fov = 0.9f;\n        c.distance = 250.f;\n        c.health = 40;\n        candidates.push_back(c);\n\n        cfg.selection = static_cast<int>(TargetSelection::Fov);\n        const CandidateSnapshot* fov =\n            DecisionEngine::select_candidate(cfg, candidates);\n\n        report.fov_winner = fov ? fov->candidate_id : -1;\n        report.fov_selection_pass = report.fov_winner == 2;\n        report.invisible_rejection_pass = report.fov_winner != 3;\n\n        cfg.selection = static_cast<int>(TargetSelection::Distance);\n        const CandidateSnapshot* dist =\n            DecisionEngine::select_candidate(cfg, candidates);\n\n        report.distance_winner = dist ? dist->candidate_id : -1;\n        report.distance_selection_pass = report.distance_winner == 1;\n\n        g_state.self_test = report;\n        return report;\n    }\n\n    inline const char* readiness_name(Readiness r) noexcept\n    {\n        switch (r)\n        {\n        case Readiness::Ready:   return "READY";\n        case Readiness::Blocked: return "BLOCKED";\n        default:                 return "WAIT";\n        }\n    }\n\n    inline const char* gate_name(GateState s) noexcept\n    {\n        switch (s)\n        {\n        case GateState::Pass:    return "PASS";\n        case GateState::Fail:    return "FAIL";\n        case GateState::Blocked: return "BLOCKED";\n        default:                 return "UNKNOWN";\n        }\n    }\n}\n'
NEW_PAGE_AIMBOT = '\n    void PageAimbot(float x, float y, float w, float h)\n    {\n        auto& rd = RageDryRun::g_state.config;\n\n        PageTitle(x, y, "AIMBOT");\n        Text(ImVec2(x + S(30.f), y + S(22.f)), U32(DIM),\n             "Rage dry-run foundation - planning/debug only, execution disabled.");\n\n        // Self-test / execution status.\n        const float testW = S(92.f);\n        if (Button("##rage_self_test", x + w - S(210.f), y + S(16.f),\n                   testW, S(28.f), "Self Test", false))\n        {\n            RageDryRun::run_builtin_self_test();\n        }\n\n        Text(ImVec2(x + w - S(105.f), y + S(22.f)),\n             U32(g_accent), "NO EXECUTION");\n\n        const float bodyY = y + S(58.f);\n        const float gap = S(14.f);\n        const float availableH = h - S(58.f);\n\n        const float leftW = (std::max)(S(390.f), w * 0.41f);\n        const float rightX = x + leftW + gap;\n        const float rightW = w - leftW - gap;\n\n        BeginCard("HITBOX", x, bodyY, leftW, 3);\n        {\n            const float ix = x + S(16.f);\n            Text(ImVec2(ix, CardBodyY()), U32(g_accent), "SELECTED");\n\n            const char* names[] = {\n                "Head","Neck","Chest","Stomach","Pelvis",\n                "Left arm","Right arm","Left leg","Right leg"\n            };\n\n            const char* nm =\n                (rd.primary_hitbox >= 0 && rd.primary_hitbox < 9)\n                ? names[rd.primary_hitbox] : "None";\n\n            Text(ImVec2(ix, CardBodyY() + S(20.f)), U32(TEXT), nm);\n            Text(ImVec2(ix, CardBodyY() + S(42.f)), U32(DIM),\n                 "Dry-run scan region");\n\n            const float figureTop = bodyY + S(86.f);\n            const float figureBottom = bodyY + availableH - S(58.f);\n            const float figureH = figureBottom - figureTop;\n            const float figureW = leftW - S(116.f);\n\n            DrawHumanHitbox(\n                x + S(94.f),\n                figureTop,\n                figureW,\n                figureH,\n                &rd.primary_hitbox);\n\n            const float qy = bodyY + availableH - S(48.f);\n            const float qx = x + S(16.f);\n            const float qgap = S(5.f);\n            const float qw =\n                (leftW - S(32.f) - qgap * 4.f) / 5.f;\n\n            const char* qn[] = {\n                "Head","Neck","Chest","Stomach","Pelvis"\n            };\n\n            for (int i = 0; i < 5; ++i)\n            {\n                char id[24];\n                std::snprintf(id, sizeof(id), "##rage_hbq%d", i);\n\n                if (Button(\n                    id,\n                    qx + i * (qw + qgap),\n                    qy,\n                    qw,\n                    S(30.f),\n                    qn[i],\n                    rd.primary_hitbox == i))\n                {\n                    rd.primary_hitbox = i;\n                }\n            }\n        }\n        EndCard(bodyY + availableH - S(14.f));\n\n        const float colGap = S(14.f);\n        const float colW = (rightW - colGap) * 0.5f;\n        const float topH = S(286.f);\n        const float row2 = bodyY + topH + gap;\n        const float bottomH = availableH - topH - gap;\n\n        // ------------------------------------------------------------\n        // TARGET / SCAN\n        // ------------------------------------------------------------\n        BeginCard("TARGET / SCAN", rightX, bodyY, colW, 1);\n        {\n            float ry = CardBodyY();\n            const float ix = rightX + S(16.f);\n            const float iw = colW - S(32.f);\n\n            const char* selection[] = { "FOV", "Distance", "Health" };\n\n            ry = RowToggle("Dry-run enabled", ix, ry, iw, &rd.enabled);\n            ry = RowCombo("Selection", ix, ry, iw,\n                          &rd.selection, selection, 3);\n            ry = RowSlider("Maximum FOV", ix, ry, iw,\n                           &rd.max_fov, 0.f, 30.f, "%.1f");\n            ry = RowToggle("Require Visibility", ix, ry, iw,\n                           &rd.require_visibility);\n            ry = RowToggle("Prefer Body", ix, ry, iw,\n                           &rd.prefer_body);\n            ry = RowToggle("Multipoint", ix, ry, iw,\n                           &rd.multipoint);\n        }\n        EndCard(bodyY + topH - S(14.f));\n\n        // ------------------------------------------------------------\n        // ACCURACY / DAMAGE\n        // ------------------------------------------------------------\n        BeginCard("ACCURACY / DAMAGE",\n                  rightX + colW + colGap,\n                  bodyY,\n                  colW,\n                  2);\n        {\n            float ry = CardBodyY();\n            const float ix = rightX + colW + colGap + S(16.f);\n            const float iw = colW - S(32.f);\n\n            ry = RowSliderI("Hitchance", ix, ry, iw,\n                            &rd.hitchance, 0, 100, "%d%%");\n            ry = RowSliderI("Minimum Damage", ix, ry, iw,\n                            &rd.minimum_damage, 0, 130, "%d");\n            ry = RowToggle("Damage Override", ix, ry, iw,\n                           &rd.damage_override);\n            ry = RowSliderI("Override Damage", ix, ry, iw,\n                            &rd.override_damage, 0, 130, "%d");\n            ry = RowSlider("Point Scale", ix, ry, iw,\n                           &rd.point_scale, 0.20f, 1.00f, "%.2f");\n            ry = RowToggle("Safe Points", ix, ry, iw,\n                           &rd.safe_points);\n            ry = RowToggle("Penetration Crosshair", ix, ry, iw,\n                           &rd.penetration_crosshair);\n        }\n        EndCard(bodyY + topH - S(14.f));\n\n        // ------------------------------------------------------------\n        // ACTION PLANNER (no command mutation)\n        // ------------------------------------------------------------\n        BeginCard("ACTION PLANNER", rightX, row2, colW, 1);\n        {\n            float ry = CardBodyY();\n            const float ix = rightX + S(16.f);\n            const float iw = colW - S(32.f);\n\n            ry = RowToggle("Silent Plan", ix, ry, iw,\n                           &rd.silent_plan);\n            ry = RowToggle("Auto Fire Plan", ix, ry, iw,\n                           &rd.auto_fire_plan);\n            ry = RowToggle("Auto Scope Plan", ix, ry, iw,\n                           &rd.auto_scope_plan);\n            ry = RowToggle("Auto Stop Plan", ix, ry, iw,\n                           &rd.auto_stop_plan);\n            ry = RowToggle("Force Shot Plan", ix, ry, iw,\n                           &rd.force_shot_plan);\n            ry = RowToggle("No Spread Plan", ix, ry, iw,\n                           &rd.no_spread_plan);\n            ry = RowToggle("Doubletap Plan", ix, ry, iw,\n                           &rd.doubletap_plan);\n        }\n        EndCard(row2 + bottomH - S(14.f));\n\n        // ------------------------------------------------------------\n        // SNAPSHOT / SIMULATION\n        // ------------------------------------------------------------\n        BeginCard("SNAPSHOT / SIM",\n                  rightX + colW + colGap,\n                  row2,\n                  colW,\n                  1);\n        {\n            float ry = CardBodyY();\n            const float ix = rightX + colW + colGap + S(16.f);\n            const float iw = colW - S(32.f);\n\n            ry = RowToggle("Lagcomp Snapshots", ix, ry, iw,\n                           &rd.lagcomp_snapshots);\n            ry = RowToggle("Shoot History", ix, ry, iw,\n                           &rd.shoot_history);\n            ry = RowToggle("Extrapolation Plan", ix, ry, iw,\n                           &rd.extrapolation_plan);\n            ry = RowToggle("Quick Peek Plan", ix, ry, iw,\n                           &rd.quick_peek_plan);\n            ry = RowToggle("Duck Peek Plan", ix, ry, iw,\n                           &rd.duck_peek_plan);\n            ry = RowToggle("Anti-Aim Plan", ix, ry, iw,\n                           &rd.anti_aim_plan);\n\n            if (RageDryRun::g_state.self_test.ran)\n            {\n                const auto& st = RageDryRun::g_state.self_test;\n                const bool pass =\n                    st.fov_selection_pass &&\n                    st.distance_selection_pass &&\n                    st.invisible_rejection_pass;\n\n                Text(\n                    ImVec2(ix, ry + S(4.f)),\n                    pass ? U32(GREEN) : U32(g_accent),\n                    pass ? "SELF TEST: PASS" : "SELF TEST: FAIL");\n            }\n            else\n            {\n                Text(\n                    ImVec2(ix, ry + S(4.f)),\n                    U32(DIM),\n                    "SELF TEST: NOT RUN");\n            }\n        }\n        EndCard(row2 + bottomH - S(14.f));\n    }\n'
HANDOFF = '# P6 Rage Dry-Run Foundation\n\nThis batch intentionally builds the Rage feature surface without wiring live execution.\n\n## Added\n\n- `RageDryConfig`\n- `CommandContext` owner slot (`CUserCmd*`, never dereferenced here)\n- `CombatFrameSnapshot`\n- `WeaponSnapshot`\n- `PredictionSnapshot`\n- `CandidateSnapshot`\n- `LagRecordSnapshot`\n- `ShootHistorySnapshot`\n- `HitchanceResult`\n- `PenetrationResult`\n- `DamageResult`\n- `StopPredictionResult`\n- `ExtrapolationResult`\n- `DoubletapEligibility`\n- `AntiAimPlan`\n- `QuickPeekPlan`\n- `DryRunActionPlan`\n- `ReadinessMatrix`\n- `DecisionEngine`\n- Built-in synthetic A/B/C self-test\n- Aimbot UI controls for dry-run feature configuration\n- Config persistence for the dry-run config\n\n## Explicitly not wired\n\n- No CreateMove hook\n- No command acquisition\n- No command mutation\n- No attack button writes\n- No view-angle writes\n- No movement writes\n- No tick shifting\n- No live Entity/Bone/Hitbox/Trace provider binding\n\n## Built-in self-test\n\nThe test creates:\n\n- A: fov 4.2, distance 300, visible\n- B: fov 1.8, distance 800, visible\n- C: fov 0.9, distance 250, invisible\n\nExpected:\n\n- FOV selection => B\n- Distance selection => A\n- Invisible C is rejected when visibility is required\n\n## Next runtime foundation batch\n\nWithout binding execution, the next batch can add owner/provider adapters for:\n\n1. Combat frame snapshot publication\n2. Weapon snapshot publication\n3. Entity candidate publication\n4. Bone/hitbox snapshot publication\n5. Read-only lag record collection\n6. Readiness/debug panel publication\n7. Synthetic trace / penetration evaluator tests\n\nRuntime trace remains blocked until a separately validated trace backend exists.\n'

def find_root():
    candidates = [
        Path.cwd(),
        Path.cwd() / "TempleWare-CS2",
        Path(r"C:\CS\TempleWare"),
        Path(r"C:\CS\TempleWare\TempleWare-CS2"),
    ]
    for c in candidates:
        if (c / "TempleWare-CS2" / "source" / "gui" / "gui.cpp").is_file():
            return c / "TempleWare-CS2"
        if (c / "source" / "gui" / "gui.cpp").is_file():
            return c
    raise FileNotFoundError("TempleWare-CS2/source/gui/gui.cpp bulunamadi.")

def replace_function(src, signature, replacement):
    start = src.find(signature)
    if start < 0:
        raise RuntimeError("Function bulunamadi: " + signature)

    brace = src.find("{", start)
    if brace < 0:
        raise RuntimeError("Function body bulunamadi: " + signature)

    depth = 0
    i = brace
    in_str = in_char = esc = line_comment = block_comment = False

    while i < len(src):
        c = src[i]
        n = src[i + 1] if i + 1 < len(src) else ""

        if line_comment:
            if c == "\n":
                line_comment = False
            i += 1
            continue

        if block_comment:
            if c == "*" and n == "/":
                block_comment = False
                i += 2
            else:
                i += 1
            continue

        if in_str:
            if esc:
                esc = False
            elif c == "\\":
                esc = True
            elif c == '"':
                in_str = False
            i += 1
            continue

        if in_char:
            if esc:
                esc = False
            elif c == "\\":
                esc = True
            elif c == "'":
                in_char = False
            i += 1
            continue

        if c == "/" and n == "/":
            line_comment = True
            i += 2
            continue

        if c == "/" and n == "*":
            block_comment = True
            i += 2
            continue

        if c == '"':
            in_str = True
            i += 1
            continue

        if c == "'":
            in_char = True
            i += 1
            continue

        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return src[:start] + replacement.rstrip() + src[i + 1:]

        i += 1

    raise RuntimeError("Function sonu bulunamadi: " + signature)

def patch_gui(gui):
    text = gui.read_text(encoding="utf-8-sig")

    include = '#include "../templeware/rage/rage_dryrun.h"'
    if include not in text:
        anchor = '#include "../templeware/config/gui_config.h"'
        if anchor not in text:
            raise RuntimeError("gui.cpp config include anchor bulunamadi.")
        text = text.replace(anchor, anchor + "\n" + include, 1)

    text = replace_function(
        text,
        "void PageAimbot(float x, float y, float w, float h)",
        NEW_PAGE_AIMBOT
    )

    gui.write_text(text, encoding="utf-8-sig")

def patch_config(cfg):
    text = cfg.read_text(encoding="utf-8-sig")

    include = '#include "../rage/rage_dryrun.h"'
    if include not in text:
        anchor = '#include "../../esp/esp.h"'
        if anchor not in text:
            raise RuntimeError("gui_config.h esp include anchor bulunamadi.")
        text = text.replace(anchor, anchor + "\n" + include, 1)

    # Version bump only once.
    text = text.replace(
        "static const uint32_t VERSION = 1;",
        "static const uint32_t VERSION = 2;",
        1
    )

    # Save block.
    save_anchor = "WriteBlock(o, Esp::g_trigger);"
    if "WriteBlock(o, RageDryRun::g_state.config);" not in text:
        if save_anchor not in text:
            raise RuntimeError("gui_config.h Save anchor bulunamadi.")
        text = text.replace(
            save_anchor,
            save_anchor + "\n        WriteBlock(o, RageDryRun::g_state.config);",
            1
        )

    # Temporary read object.
    temp_anchor = "Esp::TriggerCfg  tg = Esp::g_trigger;"
    if "RageDryRun::RageDryConfig rd = RageDryRun::g_state.config;" not in text:
        if temp_anchor not in text:
            raise RuntimeError("gui_config.h temp anchor bulunamadi.")
        text = text.replace(
            temp_anchor,
            temp_anchor + "\n        RageDryRun::RageDryConfig rd = RageDryRun::g_state.config;",
            1
        )

    # Read chain.
    old_chain = """if (!ReadBlock(i, c) || !ReadBlock(i, a) || !ReadBlock(i, aa) ||
            !ReadBlock(i, mv) || !ReadBlock(i, tg))
            return false;"""

    new_chain = """if (!ReadBlock(i, c) || !ReadBlock(i, a) || !ReadBlock(i, aa) ||
            !ReadBlock(i, mv) || !ReadBlock(i, tg) || !ReadBlock(i, rd))
            return false;"""

    if "!ReadBlock(i, rd)" not in text:
        if old_chain not in text:
            raise RuntimeError("gui_config.h ReadBlock chain anchor bulunamadi.")
        text = text.replace(old_chain, new_chain, 1)

    # Commit loaded config.
    assign_anchor = "Esp::g_trigger = tg;"
    if "RageDryRun::g_state.config = rd;" not in text:
        if assign_anchor not in text:
            raise RuntimeError("gui_config.h assign anchor bulunamadi.")
        text = text.replace(
            assign_anchor,
            assign_anchor + "\n        RageDryRun::g_state.config = rd;",
            1
        )

    cfg.write_text(text, encoding="utf-8-sig")

def run(cmd, cwd):
    print(">", " ".join(cmd))
    return subprocess.run(cmd, cwd=str(cwd), text=True).returncode

def main():
    root = find_root()
    gui = root / "source" / "gui" / "gui.cpp"
    cfg = root / "source" / "templeware" / "config" / "gui_config.h"
    rage_dir = root / "source" / "templeware" / "rage"
    rage_dir.mkdir(parents=True, exist_ok=True)

    rage_h = rage_dir / "rage_dryrun.h"
    handoff = root / "P6_RAGE_DRYRUN_HANDOFF.md"

    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    backup_dir = root / ("p6_backup_" + stamp)
    backup_dir.mkdir(parents=True, exist_ok=True)

    shutil.copy2(gui, backup_dir / "gui.cpp")
    shutil.copy2(cfg, backup_dir / "gui_config.h")
    if rage_h.exists():
        shutil.copy2(rage_h, backup_dir / "rage_dryrun.h")

    rage_h.write_text(RAGE_HEADER, encoding="utf-8")
    handoff.write_text(HANDOFF, encoding="utf-8")

    patch_gui(gui)
    patch_config(cfg)

    print()
    print("[P6] Rage dry-run foundation installed.")
    print("[P6] Created:", rage_h)
    print("[P6] Created:", handoff)
    print("[P6] Patched:", gui)
    print("[P6] Patched:", cfg)
    print("[P6] Backup:", backup_dir)
    print()
    print("[P6] Execution remains disabled by construction.")
    print("[P6] No provider or CUserCmd binding was installed.")

    repo = root.parent if (root.parent / ".git").exists() else root

    if repo.joinpath(".git").exists():
        paths = [
            rage_h.relative_to(repo),
            handoff.relative_to(repo),
            gui.relative_to(repo),
            cfg.relative_to(repo),
        ]

        run(["git", "add"] + [str(p) for p in paths], repo)

        rc = run(
            ["git", "commit", "-m", "P6: add Rage dry-run foundation and UI"],
            repo
        )

        if rc == 0 and "--push" in sys.argv:
            run(["git", "push"], repo)
        elif rc == 0:
            print("[git] Commit created. Add --push to push it.")
        else:
            print("[git] Commit not created; inspect git status.")
    else:
        print("[git] .git not found; source changes still applied.")

    print()
    print("Build:")
    print(
        r'"C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" '
        + '"' + str(root / "TempleWare-CS2.vcxproj") + '" '
        + r'/t:Rebuild /p:Configuration=Release /p:Platform=x64'
    )

if __name__ == "__main__":
    main()
