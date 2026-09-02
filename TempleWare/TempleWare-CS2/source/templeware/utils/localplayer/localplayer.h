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
    // pointer-only reference provider. This proves pointer provenance only; it
    // does not prove that TempleWare's entity wrapper/layout is semantically safe.
    bool sdk_resolver_pair_proven = false;

    // True only after both resolver provenance and wrapper/layout semantics are
    // separately proven. Until then all entity-wrapper dereferences remain gated.
    bool sdk_deref_safe = false;

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