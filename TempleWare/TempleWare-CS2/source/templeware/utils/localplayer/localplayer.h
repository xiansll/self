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

    // True only when the selected pointers came from TempleWare SDK accessors
    // that are safe to dereference with TempleWare entity wrappers. A non-null
    // pointer from the client-global fallback is useful as a liveness signal,
    // but must remain pointer-only until its layout compatibility is validated.
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