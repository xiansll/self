#pragma once

// Velocity-facing command lifecycle contract for the future combat port.
//
// This adapter deliberately does not hook CreateMove, acquire commands from the
// game, mutate command contents, or invoke any gameplay feature. It only owns
// the lifetime of a CUserCmd pointer that a separately validated runtime path
// may provide later.

#include <atomic>
#include <cstdint>

#include "../interfaces/CUserCmd/CUserCmd.h"

namespace VelocityRageCompat {

class command_lifecycle {
public:
    using command_type = CUserCmd;

    // Called only by a future validated command source. Supplying nullptr is
    // equivalent to a reset and never makes the adapter runtime-ready.
    void begin(command_type* cmd) noexcept {
        if (!cmd) {
            reset();
            return;
        }

        m_current.store(cmd, std::memory_order_release);
        m_generation.fetch_add(1, std::memory_order_relaxed);
    }

    // Clears ownership only if the caller is ending the currently published
    // command. This prevents an older scope from clearing a newer command.
    void end(command_type* cmd) noexcept {
        command_type* expected = cmd;
        m_current.compare_exchange_strong(
            expected,
            nullptr,
            std::memory_order_acq_rel,
            std::memory_order_acquire);
    }

    void reset() noexcept {
        m_current.store(nullptr, std::memory_order_release);
    }

    [[nodiscard]] command_type* current() const noexcept {
        return m_current.load(std::memory_order_acquire);
    }

    [[nodiscard]] bool runtime_ready() const noexcept {
        return current() != nullptr;
    }

    [[nodiscard]] std::uint64_t generation() const noexcept {
        return m_generation.load(std::memory_order_relaxed);
    }

private:
    std::atomic<command_type*> m_current{nullptr};
    std::atomic<std::uint64_t> m_generation{0};
};

inline command_lifecycle g_command_lifecycle{};

} // namespace VelocityRageCompat
