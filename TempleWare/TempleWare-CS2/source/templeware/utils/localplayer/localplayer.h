#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <atomic>

class C_CSPlayerPawn;
class CCSPlayerController;

#include "../../../templeware/interfaces/interfaces.h"
#include "../../../templeware/interfaces/CGameEntitySystem/CGameEntitySystem.h"

// Runtime trust is deliberately split into layers. Resolver provenance can be
// proven independently from wrapper semantics, and basic wrapper semantics can
// be proven independently from deeper identity/scene/skeleton graph traversal.
// This prevents one successful pointer lookup from silently opening every SDK
// dereference path at once.
namespace LocalPlayerTrust {
inline std::atomic<bool> g_basic_wrapper_semantics_proven{false};

inline void publish_basic_wrapper_semantics(bool proven) noexcept {
    g_basic_wrapper_semantics_proven.store(proven, std::memory_order_release);
}

[[nodiscard]] inline bool basic_wrapper_semantics_proven() noexcept {
    return g_basic_wrapper_semantics_proven.load(std::memory_order_acquire);
}
} // namespace LocalPlayerTrust

struct LocalPlayerSnapshot {
    std::uintptr_t controller = 0;
    std::uintptr_t pawn = 0;
    std::uintptr_t observer_pawn = 0;
    std::uintptr_t observer_controller = 0;
    int team = 0;
    int view_team = 0;
    bool is_alive = false;
    bool is_team_mode = false;

    // True when the SDK resolver pair independently agrees with the existing
    // pointer-only reference provider. This proves pointer provenance only.
    bool sdk_resolver_pair_proven = false;

    // True after a read-only semantic probe has validated the current
    // schema-backed basic fields (health/team/local-controller/alive) without
    // exceptions and with sane values. It does not cover view-team or graph data.
    bool sdk_wrapper_semantics_proven = false;

    // Compatibility-facing basic wrapper gate. This intentionally does NOT mean
    // scene-node/entity-identity/skeleton graph traversal has been proven safe.
    bool sdk_deref_safe = false;

    // Separate hard gate for deeper graph traversal. P3D leaves this false; a
    // later independent checkpoint must prove identity/scene/skeleton semantics.
    bool sdk_deep_graph_safe = false;

    [[nodiscard]] std::uintptr_t view_controller() const { return is_alive ? controller : observer_controller; }
    [[nodiscard]] std::uintptr_t view_pawn() const { return is_alive ? pawn : observer_pawn; }
    [[nodiscard]] bool is_valid() const { return pawn != 0 || observer_pawn != 0; }
    [[nodiscard]] bool is_this_other_team(int other_team) const { return !is_team_mode || view_team != other_team; }
};

class LocalPlayerCache {
public:
    void update();

    [[nodiscard]] LocalPlayerSnapshot get() const {
        std::shared_lock lock(m_mutex);
        return m_snapshot;
    }

    [[nodiscard]] bool is_in_cinematic() const { return m_is_in_cinematic.load(); }
    [[nodiscard]] bool is_in_time_freeze() const { return m_is_in_time_freeze.load(); }
    [[nodiscard]] bool is_in_deathmatch() const { return m_is_deathmatch.load(); }

    void reset();

private:
    mutable std::shared_mutex m_mutex;
    LocalPlayerSnapshot m_snapshot{};

    std::atomic<bool> m_is_deathmatch{false};
    std::atomic<bool> m_is_in_cinematic{false};
    std::atomic<bool> m_is_in_time_freeze{false};
};

inline std::unique_ptr<LocalPlayerCache> g_local_player_cache = std::make_unique<LocalPlayerCache>();
