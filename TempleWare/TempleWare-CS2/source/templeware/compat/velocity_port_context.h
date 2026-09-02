#pragma once

// Single read-only context boundary for future mechanically adapted Velocity
// combat code. The context aggregates already-owned compatibility state; it does
// not acquire game data, mutate commands, run features, or open blocked runtime
// producers.

#include <cstdint>
#include <mutex>
#include <shared_mutex>

#include "velocity_rage_compat.h"

namespace VelocityRageCompat {

struct port_context_snapshot {
    local_state local{};

    bool in_game{};
    bool gate_open{};

    std::uint64_t frame_sequence{};
    std::uint64_t runtime_epoch{};
    std::uint64_t command_generation{};
    std::uint64_t config_generation{};
};

class port_context_store {
public:
    void update(const LocalPlayerSnapshot& local_snapshot, bool in_game) noexcept {
        const readiness r = query(local_snapshot);

        std::unique_lock lock(m_mutex);
        m_snapshot.local = adapt_local(local_snapshot);
        m_snapshot.in_game = in_game;
        m_snapshot.gate_open = port_gate_open(r);
        m_snapshot.runtime_epoch = r.runtime_epoch;
        m_snapshot.command_generation = r.command_generation;
        m_snapshot.config_generation = r.config_generation;
        ++m_snapshot.frame_sequence;
    }

    void reset_volatile() noexcept {
        std::unique_lock lock(m_mutex);
        m_snapshot.local = {};
        m_snapshot.in_game = false;
        m_snapshot.gate_open = false;
        m_snapshot.runtime_epoch = g_runtime_services.epoch();
        m_snapshot.command_generation = g_command_lifecycle.generation();
        m_snapshot.config_generation = g_rage_config_store.generation();
        ++m_snapshot.frame_sequence;
    }

    [[nodiscard]] port_context_snapshot get() const noexcept {
        std::shared_lock lock(m_mutex);
        return m_snapshot;
    }

private:
    mutable std::shared_mutex m_mutex;
    port_context_snapshot m_snapshot{};
};

inline port_context_store g_port_context{};

} // namespace VelocityRageCompat
