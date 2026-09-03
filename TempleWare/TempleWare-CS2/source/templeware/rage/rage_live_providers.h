#pragma once

// TempleWare P6 LIVE provider layer.
//
// These providers are read-only adapters over data captured by the existing,
// runtime-proven TempleWare read path (Esp:: enumeration + schema-safe field
// reads in esp.cpp). They introduce NO new offsets, signatures, or vtable
// indices; esp.cpp remains the single owner of those. The ESP capture routine
// fills the LiveData buffer below, and these provider objects publish it into
// the P6 ProviderHub exactly like any bound provider.
//
// Contract preserved:
// - unread/failed field  -> provider not ready -> UNAVAILABLE
// - trace / penetration  -> wired via rage_runtime_providers.h (P9)
// - CUserCmd             -> untouched, command stays UNAVAILABLE

#include "rage_dryrun.h"
#include "rage_dryrun_providers.h"
#include "rage_runtime_providers.h"

#include <vector>

namespace RageDryRun
{
    namespace Live
    {
        // Single-producer (Present thread) buffer holding the most recent
        // capture. Populated by Esp::PublishP6Live().
        struct LiveData
        {
            CombatFrameSnapshot frame{};
            WeaponSnapshot weapon{};
            PredictionSnapshot prediction{};
            std::vector<CandidateSnapshot> candidates{};

            // Read-only lag-record history derived from the live entity origins.
            // Recording only: no apply/restore is ever performed on the game.
            std::vector<LagRecordSnapshot> lag_history{};

            bool frame_ready = false;
            bool weapon_ready = false;
            bool prediction_ready = false;
            bool entities_ready = false;
            bool bones_ready = false;
            bool hitboxes_ready = false;

            std::uint64_t generation = 0;

            // Lightweight provenance for the debug UI / log.
            std::uintptr_t local_pawn = 0;
            std::uintptr_t local_controller = 0;
            int entity_count = 0;
            int bone_ready_count = 0;   // candidates with a resolved bone array
            int publishes = 0;          // successful capture cycles this session

            void clear() noexcept
            {
                frame = {};
                weapon = {};
                prediction = {};
                candidates.clear();
                lag_history.clear();
                frame_ready = weapon_ready = prediction_ready = false;
                entities_ready = bones_ready = hitboxes_ready = false;
                local_pawn = local_controller = 0;
                entity_count = 0;
                bone_ready_count = 0;
                // publishes intentionally NOT reset: it is a session-lifetime counter.
            }
        };

        inline constexpr std::size_t kLagHistoryMax = 64;

        // Bound the lag-record ring to its cap, dropping oldest first.
        inline void cap_lag_history(std::vector<LagRecordSnapshot>& h,
                                    std::size_t cap = kLagHistoryMax) noexcept
        {
            while (h.size() > cap)
                h.erase(h.begin());
        }

        inline LiveData g_live{};

        // Debug/enable gate. When false the pipeline leaves any synthetic
        // bundle intact and never overwrites g_state with live data.
        inline bool g_enabled = false;

        class CombatFrameProvider final : public ICombatFrameProvider
        {
        public:
            bool ready() const noexcept override { return g_live.frame_ready; }
            std::uint64_t generation() const noexcept override { return g_live.generation; }
            bool snapshot(CombatFrameSnapshot& out) const noexcept override
            {
                if (!g_live.frame_ready) return false;
                out = g_live.frame;
                return true;
            }
        };

        class WeaponProvider final : public IWeaponProvider
        {
        public:
            bool ready() const noexcept override { return g_live.weapon_ready; }
            std::uint64_t generation() const noexcept override { return g_live.generation; }
            bool snapshot(WeaponSnapshot& out) const noexcept override
            {
                if (!g_live.weapon_ready) return false;
                out = g_live.weapon;
                return true;
            }
        };

        class PredictionProvider final : public IPredictionProvider
        {
        public:
            bool ready() const noexcept override { return g_live.prediction_ready; }
            std::uint64_t generation() const noexcept override { return g_live.generation; }
            bool snapshot(PredictionSnapshot& out) const noexcept override
            {
                if (!g_live.prediction_ready) return false;
                out = g_live.prediction;
                return true;
            }
        };

        class EntityCandidateProvider final : public IEntityCandidateProvider
        {
        public:
            bool ready() const noexcept override { return g_live.entities_ready; }
            std::uint64_t generation() const noexcept override { return g_live.generation; }
            bool snapshot(std::vector<CandidateSnapshot>& out) const noexcept override
            {
                if (!g_live.entities_ready) return false;
                out = g_live.candidates;
                return true;
            }
        };

        class BoneSnapshotProvider final : public IBoneSnapshotProvider
        {
        public:
            bool ready() const noexcept override { return g_live.bones_ready; }
            std::uint64_t generation() const noexcept override { return g_live.generation; }
        };

        class HitboxSnapshotProvider final : public IHitboxSnapshotProvider
        {
        public:
            bool ready() const noexcept override { return g_live.hitboxes_ready; }
            std::uint64_t generation() const noexcept override { return g_live.generation; }
        };

        inline CombatFrameProvider    g_combat_frame_provider{};
        inline WeaponProvider         g_weapon_provider{};
        inline PredictionProvider     g_prediction_provider{};
        inline EntityCandidateProvider g_entity_provider{};
        inline BoneSnapshotProvider   g_bone_provider{};
        inline HitboxSnapshotProvider g_hitbox_provider{};

        // Bind the live adapters into the shared ProviderHub.
        // Lag records and shoot history stay unbound (no proven read-only backend).
        // Trace provider is wired by P9 RuntimeProviders layer.
        inline void bind() noexcept
        {
            g_providers.combat_frame = &g_combat_frame_provider;
            g_providers.weapon = &g_weapon_provider;
            g_providers.prediction = &g_prediction_provider;
            g_providers.entities = &g_entity_provider;
            g_providers.bones = &g_bone_provider;
            g_providers.hitboxes = &g_hitbox_provider;
            g_providers.trace = &RuntimeProviders::g_trace_provider;
        }

        inline void unbind() noexcept
        {
            g_providers.clear();
            g_live.clear();
        }

        // Emits one line per lane ONLY when its readiness (or entity count)
        // changes, so P6_runtime.log records meaningful transitions, not spam.
        inline void log_live_transitions() noexcept
        {
            const auto& r = g_state.readiness;

            struct Prev
            {
                bool first = true;
                Readiness frame{}, weapon{}, prediction{}, bones{}, hitboxes{};
                int entities = -1;
            };
            static Prev p;

            auto emit = [](const char* lane, Readiness cur)
            {
                char b[64];
                std::snprintf(b, sizeof(b), "[P6] LIVE %s=%s", lane,
                              readiness_name(cur));
                p6_log(b);
            };

            if (p.first || r.combat_frame != p.frame)  emit("frame", r.combat_frame);
            if (p.first || r.weapon != p.weapon)       emit("weapon", r.weapon);
            if (p.first || r.prediction != p.prediction) emit("prediction", r.prediction);
            if (p.first || r.bones != p.bones)         emit("bones", r.bones);
            if (p.first || r.hitboxes != p.hitboxes)   emit("hitboxes", r.hitboxes);

            if (p.first || g_state.live_entity_count != p.entities)
            {
                char b[64];
                std::snprintf(b, sizeof(b), "[P6] LIVE entities=%d",
                              g_state.live_entity_count);
                p6_log(b);
            }

            p.frame = r.combat_frame;
            p.weapon = r.weapon;
            p.prediction = r.prediction;
            p.bones = r.bones;
            p.hitboxes = r.hitboxes;
            p.entities = g_state.live_entity_count;
            p.first = false;
        }
    } // namespace Live
} // namespace RageDryRun
