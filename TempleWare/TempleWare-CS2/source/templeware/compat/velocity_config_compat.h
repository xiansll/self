#pragma once

// Velocity-facing configuration contract for the future Rage port.
//
// This layer intentionally carries no aiming, firing, targeting, anti-aim, or
// other gameplay behaviour. It only provides an isolated, versioned config
// snapshot/store so ported code does not read TempleWare GUI/config globals
// directly. A real translation from TempleWare settings can be added and
// validated later without changing the consumer-facing ownership model.

#include <cstdint>
#include <mutex>

namespace VelocityRageCompat {

struct rage_config_snapshot {
    // Master availability/ownership flag only. It is not consumed by gameplay
    // code in the current compatibility phase.
    bool enabled = false;

    // Contract version for future translation/migration diagnostics.
    std::uint32_t schema_version = 1;

    // Producer-supplied revision. The store also maintains its own generation
    // counter so stale publications can be diagnosed independently.
    std::uint64_t revision = 0;
};

class rage_config_store {
public:
    void publish(const rage_config_snapshot& snapshot) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_snapshot = snapshot;
        m_published = true;
        ++m_generation;
    }

    void reset() noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_snapshot = {};
        m_published = false;
        ++m_generation;
    }

    [[nodiscard]] rage_config_snapshot get() const noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_snapshot;
    }

    [[nodiscard]] bool runtime_ready() const noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_published;
    }

    [[nodiscard]] std::uint64_t generation() const noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_generation;
    }

private:
    mutable std::mutex m_mutex;
    rage_config_snapshot m_snapshot{};
    bool m_published = false;
    std::uint64_t m_generation = 0;
};

inline rage_config_store g_rage_config_store{};

namespace config_contracts {
inline constexpr bool rage_config = true;
}

} // namespace VelocityRageCompat
