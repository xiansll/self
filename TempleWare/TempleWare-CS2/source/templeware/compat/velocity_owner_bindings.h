#pragma once

// Single owner-editable entry point for final integration.
//
// TempleWare runtime/main should not need feature-specific edits after this file
// is wired. Add process-lifetime provider objects, config translation, and
// IIntegratedFeature registrations here. The default implementation is a no-op
// and therefore cannot enable any gameplay behavior.

#include <atomic>

#include "velocity_feature_integration.h"
#include "../utils/filelog/filelog.h"

namespace VelocityRageCompat::OwnerBindings {

inline std::atomic<bool> g_installed{false};

inline void install() noexcept {
    if (g_installed.exchange(true, std::memory_order_acq_rel))
        return;

    // OWNER BINDING SLOT
    // ------------------
    // Keep bound objects alive for the process lifetime (or explicitly unbind
    // before destruction). Typical owner-side registrations are:
    //
    // g_provider_bindings.bind_entities(&your_entity_provider);
    // g_provider_bindings.bind_bones(&your_bone_provider);
    // g_provider_bindings.bind_hitboxes(&your_hitbox_provider);
    // g_provider_bindings.bind_rich_trace(&your_trace_provider);
    // FeatureIntegration::bind_config_refresh(&your_config_publish_function);
    // g_feature_registry.register_feature(&your_feature);
    //
    // No provider/feature is bound by default. Binding a provider is not enough
    // to open readiness: its ready() method must independently return true.

    FileLog::Log("[P5D] OWNER BINDING SLOT READY - default bindings empty");
}

inline void uninstall() noexcept {
    if (!g_installed.exchange(false, std::memory_order_acq_rel))
        return;

    // Registry/provider shutdown is centrally owned by FeatureIntegration.
    // This hook exists only for future owner-specific non-runtime cleanup.
    FileLog::Log("[P5D] OWNER BINDING SLOT UNINSTALLED");
}

} // namespace VelocityRageCompat::OwnerBindings
