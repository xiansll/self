#pragma once

// Central runtime lifecycle for the Velocity -> TempleWare compatibility layer.
//
// This file is intentionally non-operational. It does not acquire commands,
// entities, bones, hitboxes, or traces and it does not mutate gameplay state.
// It only records which already-existing compatibility producers have been
// independently validated, owns their lifecycle epoch, and provides one reset
// point for volatile port state.

#include <atomic>
#include <cstdint>

#include "velocity_command_compat.h"
#include "velocity_config_compat.h"

namespace VelocityRageCompat {

enum class runtime_service : std::uint8_t {
    entity_cache,
    bones,
    hitboxes,
    rich_trace
};

class runtime_services {
public:
    [[nodiscard]] bool ready(runtime_service service) const noexcept {
        switch (service) {
        case runtime_service::entity_cache:
            return m_entity_cache.load(std::memory_order_acquire);
        case runtime_service::bones:
            return m_bones.load(std::memory_order_acquire);
        case runtime_service::hitboxes:
            return m_hitboxes.load(std::memory_order_acquire);
        case runtime_service::rich_trace:
            return m_rich_trace.load(std::memory_order_acquire);
        }
        return false;
    }

    // Future validated producers may publish readiness only after their own data
    // store has been populated and independently checked. No current runtime
    // path calls this with ready=true.
    void publish_validated(runtime_service service, bool ready = true) noexcept {
        switch (service) {
        case runtime_service::entity_cache:
            m_entity_cache.store(ready, std::memory_order_release);
            break;
        case runtime_service::bones:
            m_bones.store(ready, std::memory_order_release);
            break;
        case runtime_service::hitboxes:
            m_hitboxes.store(ready, std::memory_order_release);
            break;
        case runtime_service::rich_trace:
            m_rich_trace.store(ready, std::memory_order_release);
            break;
        }
        m_epoch.fetch_add(1, std::memory_order_acq_rel);
    }

    // Map/disconnect/respawn-style lifecycle reset for state that is only valid
    // while a command/game-data producer is active. Config is intentionally not
    // reset here because settings ownership is process-lifetime, not map-lifetime.
    void reset_volatile() noexcept {
        g_command_lifecycle.reset();
        m_entity_cache.store(false, std::memory_order_release);
        m_bones.store(false, std::memory_order_release);
        m_hitboxes.store(false, std::memory_order_release);
        m_rich_trace.store(false, std::memory_order_release);
        m_epoch.fetch_add(1, std::memory_order_acq_rel);
    }

    void shutdown() noexcept {
        reset_volatile();
        g_rage_config_store.reset();
        m_epoch.fetch_add(1, std::memory_order_acq_rel);
    }

    [[nodiscard]] std::uint64_t epoch() const noexcept {
        return m_epoch.load(std::memory_order_acquire);
    }

private:
    std::atomic<bool> m_entity_cache{false};
    std::atomic<bool> m_bones{false};
    std::atomic<bool> m_hitboxes{false};
    std::atomic<bool> m_rich_trace{false};
    std::atomic<std::uint64_t> m_epoch{1};
};

inline runtime_services g_runtime_services{};

// Config ownership itself is safe to establish without any gameplay producer.
// Publishing a disabled default snapshot proves only the config-store lifecycle;
// it does not enable or execute a Rage feature.
inline void initialize_non_gameplay_defaults() noexcept {
    if (g_rage_config_store.runtime_ready())
        return;

    rage_config_snapshot snapshot{};
    snapshot.enabled = false;
    snapshot.schema_version = 1;
    snapshot.revision = 1;
    g_rage_config_store.publish(snapshot);
}

inline void reset_volatile_runtime() noexcept {
    g_runtime_services.reset_volatile();
}

inline void shutdown_runtime() noexcept {
    g_runtime_services.shutdown();
}

} // namespace VelocityRageCompat
