#pragma once

// Final non-operational integration boundary for future owner-supplied feature
// code. TempleWare owns lifecycle/context/registration; feature implementations
// only consume the context and may bind validated producers/config translation.
// No game hook, target selection, aiming, firing, anti-aim, or command mutation
// is implemented by this file.

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <shared_mutex>

#include "velocity_port_context.h"
#include "velocity_provider_bindings.h"
#include "../utils/filelog/filelog.h"

namespace VelocityRageCompat {

struct feature_runtime_context {
    port_context_snapshot port{};
    readiness compat{};
    rage_config_snapshot config{};
    provider_binding_snapshot providers{};

    // Non-null only while dispatch_command() owns a validated externally
    // supplied command scope. Present/frame dispatch never invents a command.
    CUserCmd* command{};

    std::uint64_t dispatch_sequence{};

    [[nodiscard]] bool in_game() const noexcept { return port.in_game; }
    [[nodiscard]] bool local_pair_ready() const noexcept { return compat.local_pair; }
    [[nodiscard]] bool basic_sdk_ready() const noexcept { return compat.sdk_deref_safe; }
    [[nodiscard]] bool command_ready() const noexcept {
        return command != nullptr && compat.command_runtime;
    }
};

class IIntegratedFeature {
public:
    virtual ~IIntegratedFeature() = default;

    // Stable diagnostic name only; registry does not infer behavior from it.
    [[nodiscard]] virtual const char* integration_name() const noexcept = 0;

    // Present/read-only lane. Implementations must inspect readiness before
    // consuming optional providers.
    virtual void on_frame(const feature_runtime_context&) noexcept {}

    // Command lane. This is dormant until the owner explicitly calls
    // FeatureIntegration::dispatch_command() from a separately validated source.
    virtual void on_command(const feature_runtime_context&) noexcept {}

    // Called on menu-visible frames. The registry provides timing/ownership only;
    // actual controls/settings remain owner-supplied.
    virtual void on_menu() noexcept {}

    // Map/disconnect-style volatile reset.
    virtual void on_reset() noexcept {}

    // Process shutdown/unregistration cleanup.
    virtual void on_shutdown() noexcept {}
};

class feature_registry {
public:
    static constexpr std::size_t k_max_features = 32;

    bool register_feature(IIntegratedFeature* feature) noexcept {
        if (!feature)
            return false;

        std::unique_lock lock(m_mutex);
        for (IIntegratedFeature* current : m_features) {
            if (current == feature)
                return true;
        }

        for (IIntegratedFeature*& slot : m_features) {
            if (!slot) {
                slot = feature;
                ++m_generation;
                return true;
            }
        }
        return false;
    }

    bool unregister_feature(IIntegratedFeature* feature) noexcept {
        if (!feature)
            return false;

        std::unique_lock lock(m_mutex);
        for (IIntegratedFeature*& slot : m_features) {
            if (slot == feature) {
                slot = nullptr;
                ++m_generation;
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] std::size_t count() const noexcept {
        std::shared_lock lock(m_mutex);
        std::size_t result = 0;
        for (IIntegratedFeature* feature : m_features)
            result += feature ? 1u : 0u;
        return result;
    }

    [[nodiscard]] std::uint64_t generation() const noexcept {
        std::shared_lock lock(m_mutex);
        return m_generation;
    }

    void dispatch_frame(const feature_runtime_context& ctx) noexcept {
        feature_array copy{};
        const std::size_t n = copy_features(copy);
        for (std::size_t i = 0; i < n; ++i)
            copy[i]->on_frame(ctx);
    }

    void dispatch_command(const feature_runtime_context& ctx) noexcept {
        feature_array copy{};
        const std::size_t n = copy_features(copy);
        for (std::size_t i = 0; i < n; ++i)
            copy[i]->on_command(ctx);
    }

    void dispatch_menu() noexcept {
        feature_array copy{};
        const std::size_t n = copy_features(copy);
        for (std::size_t i = 0; i < n; ++i)
            copy[i]->on_menu();
    }

    void dispatch_reset() noexcept {
        feature_array copy{};
        const std::size_t n = copy_features(copy);
        for (std::size_t i = 0; i < n; ++i)
            copy[i]->on_reset();
    }

    void dispatch_shutdown() noexcept {
        feature_array copy{};
        const std::size_t n = copy_features(copy);
        for (std::size_t i = 0; i < n; ++i)
            copy[i]->on_shutdown();
    }

private:
    using feature_array = std::array<IIntegratedFeature*, k_max_features>;

    [[nodiscard]] std::size_t copy_features(feature_array& out) const noexcept {
        std::shared_lock lock(m_mutex);
        std::size_t n = 0;
        for (IIntegratedFeature* feature : m_features) {
            if (feature && n < out.size())
                out[n++] = feature;
        }
        return n;
    }

    mutable std::shared_mutex m_mutex;
    feature_array m_features{};
    std::uint64_t m_generation{};
};

inline feature_registry g_feature_registry{};

namespace FeatureIntegration {

using config_refresh_fn = void(*)() noexcept;

inline std::atomic<config_refresh_fn> g_config_refresh{nullptr};
inline std::atomic<std::uint64_t> g_dispatch_sequence{0};
inline std::atomic<bool> g_initialized{false};

inline void bind_config_refresh(config_refresh_fn refresh) noexcept {
    g_config_refresh.store(refresh, std::memory_order_release);
}

inline void initialize() noexcept {
    if (g_initialized.exchange(true, std::memory_order_acq_rel))
        return;

    FileLog::Log("[P5C] FEATURE INTEGRATION READY - frame/menu/reset wired; command lane dormant until owner dispatch");
}

[[nodiscard]] inline feature_runtime_context build_context(CUserCmd* command = nullptr) noexcept {
    feature_runtime_context ctx{};
    ctx.port = g_port_context.get();

    const LocalPlayerSnapshot local_snapshot = g_local_player_cache->get();
    ctx.compat = query(local_snapshot);
    ctx.config = g_rage_config_store.get();
    ctx.providers = g_provider_bindings.snapshot();
    ctx.command = command;
    ctx.dispatch_sequence = g_dispatch_sequence.fetch_add(1, std::memory_order_relaxed) + 1;
    return ctx;
}

inline void log_status_if_changed(const feature_runtime_context& ctx) noexcept {
    static bool s_logged = false;
    static bool s_local = false;
    static bool s_sdk = false;
    static bool s_trace = false;
    static bool s_command = false;
    static bool s_entities = false;
    static bool s_bones = false;
    static bool s_hitboxes = false;
    static bool s_rich_trace = false;
    static std::uint64_t s_bind_gen = 0;
    static std::uint64_t s_registry_gen = 0;

    const bool local = ctx.compat.local_pair;
    const bool sdk = ctx.compat.sdk_deref_safe;
    const bool trace = ctx.compat.trace_runtime;
    const bool command = ctx.compat.command_runtime;
    const bool entities = ctx.providers.entities_ready;
    const bool bones = ctx.providers.bones_ready;
    const bool hitboxes = ctx.providers.hitboxes_ready;
    const bool rich_trace = ctx.providers.rich_trace_ready;
    const std::uint64_t registry_gen = g_feature_registry.generation();

    if (s_logged &&
        s_local == local && s_sdk == sdk && s_trace == trace &&
        s_command == command && s_entities == entities && s_bones == bones &&
        s_hitboxes == hitboxes && s_rich_trace == rich_trace &&
        s_bind_gen == ctx.providers.binding_generation &&
        s_registry_gen == registry_gen) {
        return;
    }

    char buf[640];
    std::snprintf(buf, sizeof(buf),
        "[P5C] STATUS features=%zu local=%d sdk=%d trace=%d command=%d entities=%d bones=%d hitboxes=%d rich_trace=%d bind_gen=%llu registry_gen=%llu",
        g_feature_registry.count(),
        local ? 1 : 0,
        sdk ? 1 : 0,
        trace ? 1 : 0,
        command ? 1 : 0,
        entities ? 1 : 0,
        bones ? 1 : 0,
        hitboxes ? 1 : 0,
        rich_trace ? 1 : 0,
        static_cast<unsigned long long>(ctx.providers.binding_generation),
        static_cast<unsigned long long>(registry_gen));
    FileLog::Log(buf);

    s_logged = true;
    s_local = local;
    s_sdk = sdk;
    s_trace = trace;
    s_command = command;
    s_entities = entities;
    s_bones = bones;
    s_hitboxes = hitboxes;
    s_rich_trace = rich_trace;
    s_bind_gen = ctx.providers.binding_generation;
    s_registry_gen = registry_gen;
}

inline void on_frame() noexcept {
    if (!g_initialized.load(std::memory_order_acquire))
        initialize();

    if (config_refresh_fn refresh = g_config_refresh.load(std::memory_order_acquire))
        refresh();

    g_provider_bindings.sync_runtime_services();
    const feature_runtime_context ctx = build_context(nullptr);
    log_status_if_changed(ctx);
    g_feature_registry.dispatch_frame(ctx);
}

// This function intentionally is NOT called by TempleWare's current runtime.
// The owner may invoke it from a separately validated command source. The scope
// publishes the supplied command only for the duration of this dispatch.
inline void dispatch_command(CUserCmd* command) noexcept {
    if (!command)
        return;

    if (!g_initialized.load(std::memory_order_acquire))
        initialize();

    g_command_lifecycle.begin(command);
    g_provider_bindings.sync_runtime_services();
    const feature_runtime_context ctx = build_context(command);
    g_feature_registry.dispatch_command(ctx);
    g_command_lifecycle.end(command);
}

inline void on_menu() noexcept {
    if (!g_initialized.load(std::memory_order_acquire))
        initialize();
    g_feature_registry.dispatch_menu();
}

inline void reset_volatile() noexcept {
    g_feature_registry.dispatch_reset();
    g_provider_bindings.reset_volatile();
    g_command_lifecycle.reset();
    FileLog::Log("[P5C] FEATURE/PROVIDER VOLATILE RESET CLEAN");
}

inline void shutdown() noexcept {
    if (!g_initialized.exchange(false, std::memory_order_acq_rel))
        return;

    g_feature_registry.dispatch_shutdown();
    g_provider_bindings.shutdown();
    g_command_lifecycle.reset();
    g_config_refresh.store(nullptr, std::memory_order_release);
    FileLog::Log("[P5C] FEATURE INTEGRATION SHUTDOWN CLEAN");
}

} // namespace FeatureIntegration

} // namespace VelocityRageCompat
