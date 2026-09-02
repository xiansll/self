#pragma once

// Process-lifetime binding surface between TempleWare-owned runtime producers
// and future feature consumers. This file does not discover game objects, scan
// signatures, read bones/hitboxes, mutate commands, or implement gameplay.
// Producers are supplied by the owner after they have been independently
// validated; until then each slot remains unbound/not-ready.

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <shared_mutex>

#include "velocity_data_compat.h"
#include "velocity_runtime_compat.h"

namespace VelocityRageCompat {

class IEntityProvider {
public:
    virtual ~IEntityProvider() = default;
    [[nodiscard]] virtual bool ready() const noexcept = 0;
    [[nodiscard]] virtual std::uint64_t generation() const noexcept = 0;
    [[nodiscard]] virtual std::size_t count() const noexcept = 0;
    [[nodiscard]] virtual bool read(std::size_t index, entity_ref& out) const noexcept = 0;
    virtual void reset() noexcept {}
};

class IBoneProvider {
public:
    virtual ~IBoneProvider() = default;
    [[nodiscard]] virtual bool ready() const noexcept = 0;
    [[nodiscard]] virtual std::uint64_t generation() const noexcept = 0;
    [[nodiscard]] virtual bool read(std::uint32_t entity_handle,
                                    skeleton_snapshot& out) const noexcept = 0;
    virtual void reset() noexcept {}
};

class IHitboxProvider {
public:
    virtual ~IHitboxProvider() = default;
    [[nodiscard]] virtual bool ready() const noexcept = 0;
    [[nodiscard]] virtual std::uint64_t generation() const noexcept = 0;
    [[nodiscard]] virtual bool read(std::uint32_t entity_handle,
                                    hitbox_set& out) const noexcept = 0;
    virtual void reset() noexcept {}
};

class IRichTraceProvider {
public:
    virtual ~IRichTraceProvider() = default;
    [[nodiscard]] virtual bool ready() const noexcept = 0;
    [[nodiscard]] virtual std::uint64_t generation() const noexcept = 0;

    // Generic read-only trace contract. No implementation is supplied here.
    [[nodiscard]] virtual bool line(const Vector_t& start,
                                    const Vector_t& end,
                                    std::uintptr_t skip,
                                    rich_trace_result& out) const noexcept = 0;
    virtual void reset() noexcept {}
};

struct provider_binding_snapshot {
    IEntityProvider* entities{};
    IBoneProvider* bones{};
    IHitboxProvider* hitboxes{};
    IRichTraceProvider* rich_trace{};

    bool entities_ready{};
    bool bones_ready{};
    bool hitboxes_ready{};
    bool rich_trace_ready{};

    std::uint64_t entities_generation{};
    std::uint64_t bones_generation{};
    std::uint64_t hitboxes_generation{};
    std::uint64_t rich_trace_generation{};
    std::uint64_t binding_generation{};
};

class provider_bindings {
public:
    // Bound providers must have process-lifetime (or otherwise externally
    // synchronized) lifetime until explicitly unbound. Binding does not imply
    // readiness; provider::ready() remains the authoritative runtime gate.
    void bind_entities(IEntityProvider* provider) noexcept {
        std::unique_lock lock(m_mutex);
        if (m_entities == provider)
            return;
        m_entities = provider;
        ++m_binding_generation;
    }

    void bind_bones(IBoneProvider* provider) noexcept {
        std::unique_lock lock(m_mutex);
        if (m_bones == provider)
            return;
        m_bones = provider;
        ++m_binding_generation;
    }

    void bind_hitboxes(IHitboxProvider* provider) noexcept {
        std::unique_lock lock(m_mutex);
        if (m_hitboxes == provider)
            return;
        m_hitboxes = provider;
        ++m_binding_generation;
    }

    void bind_rich_trace(IRichTraceProvider* provider) noexcept {
        std::unique_lock lock(m_mutex);
        if (m_rich_trace == provider)
            return;
        m_rich_trace = provider;
        ++m_binding_generation;
    }

    void unbind_all() noexcept {
        std::unique_lock lock(m_mutex);
        m_entities = nullptr;
        m_bones = nullptr;
        m_hitboxes = nullptr;
        m_rich_trace = nullptr;
        ++m_binding_generation;
    }

    [[nodiscard]] provider_binding_snapshot snapshot() const noexcept {
        provider_binding_snapshot out{};
        {
            std::shared_lock lock(m_mutex);
            out.entities = m_entities;
            out.bones = m_bones;
            out.hitboxes = m_hitboxes;
            out.rich_trace = m_rich_trace;
            out.binding_generation = m_binding_generation;
        }

        // Provider calls happen outside the registry lock so a provider is free
        // to use its own synchronization without lock-order coupling.
        if (out.entities) {
            out.entities_ready = out.entities->ready();
            out.entities_generation = out.entities->generation();
        }
        if (out.bones) {
            out.bones_ready = out.bones->ready();
            out.bones_generation = out.bones->generation();
        }
        if (out.hitboxes) {
            out.hitboxes_ready = out.hitboxes->ready();
            out.hitboxes_generation = out.hitboxes->generation();
        }
        if (out.rich_trace) {
            out.rich_trace_ready = out.rich_trace->ready();
            out.rich_trace_generation = out.rich_trace->generation();
        }
        return out;
    }

    // Mirror only independently validated provider readiness into the existing
    // compatibility gate. State transitions increment the central epoch; steady
    // state does not generate epoch/log churn.
    void sync_runtime_services() const noexcept {
        const provider_binding_snapshot s = snapshot();
        sync_one(runtime_service::entity_cache, s.entities_ready);
        sync_one(runtime_service::bones, s.bones_ready);
        sync_one(runtime_service::hitboxes, s.hitboxes_ready);
        sync_one(runtime_service::rich_trace, s.rich_trace_ready);
    }

    void reset_volatile() noexcept {
        const provider_binding_snapshot s = snapshot();
        if (s.entities) s.entities->reset();
        if (s.bones) s.bones->reset();
        if (s.hitboxes) s.hitboxes->reset();
        if (s.rich_trace) s.rich_trace->reset();

        sync_one(runtime_service::entity_cache, false);
        sync_one(runtime_service::bones, false);
        sync_one(runtime_service::hitboxes, false);
        sync_one(runtime_service::rich_trace, false);
    }

    void shutdown() noexcept {
        reset_volatile();
        unbind_all();
    }

private:
    static void sync_one(runtime_service service, bool ready) noexcept {
        if (g_runtime_services.ready(service) != ready)
            g_runtime_services.publish_validated(service, ready);
    }

    mutable std::shared_mutex m_mutex;
    IEntityProvider* m_entities{};
    IBoneProvider* m_bones{};
    IHitboxProvider* m_hitboxes{};
    IRichTraceProvider* m_rich_trace{};
    std::uint64_t m_binding_generation{};
};

inline provider_bindings g_provider_bindings{};

} // namespace VelocityRageCompat
