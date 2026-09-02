#include "validation.h"
#include <cstdio>
#include <string>
#include "../../interfaces/interfaces.h"
#include "../../utils/logging/log.h"
#include "../../../cs2/entity/C_CSPlayerPawn/C_CSPlayerPawn.h"
#include "../../../cs2/entity/CCSPlayerController/CCSPlayerController.h"
#include "../../../cs2/entity/C_EntityInstance/C_EntityInstance.h"
#include "../../../cs2/entity/handle.h"
#include "../../interfaces/CGameEntitySystem/CGameEntitySystem.h"
#include "../../utils/localplayer/localplayer.h"

namespace Validation {

void Initialize() {
    g_counters.local_player_cache_updates.store(0);
    g_counters.local_player_cache_resets.store(0);
    g_counters.handle_lookups.store(0);
    g_counters.handle_mismatches.store(0);
    g_counters.entity_identity_checks.store(0);
    g_counters.entity_identity_mismatches.store(0);
    g_counters.scene_node_chain_checks.store(0);
    g_counters.scene_node_chain_failures.store(0);
    g_counters.vtable_calls.store(0);
    g_counters.vtable_failures.store(0);
    g_counters.pattern_resolutions.store(0);
    g_counters.pattern_failures.store(0);
    Logger::Log("[Validation] Diagnostic harness initialized", LogType::Info);
}

void OnLocalPlayerCacheUpdate(const LocalPlayerSnapshot& snapshot) {
    g_counters.local_player_cache_updates.fetch_add(1, std::memory_order_relaxed);

    uint64_t current_tick = 0;
    if (I::GlobalVars) {
        current_tick = I::GlobalVars->m_tick_count;
    }

    if (g_rate_limiter.should_log(current_tick)) {
        char buf[512];
        std::snprintf(buf, sizeof(buf),
            "[Validation] LocalPlayerCache update #%u: pawn=0x%p controller=0x%p observer_pawn=0x%p observer_ctrl=0x%p team=%d alive=%d valid=%d",
            g_counters.local_player_cache_updates.load(),
            reinterpret_cast<void*>(snapshot.pawn),
            reinterpret_cast<void*>(snapshot.controller),
            reinterpret_cast<void*>(snapshot.observer_pawn),
            reinterpret_cast<void*>(snapshot.observer_controller),
            snapshot.team,
            snapshot.is_alive,
            snapshot.is_valid()
        );
        Logger::Log(buf, LogType::Info);
    }
}

void OnLocalPlayerCacheReset() {
    g_counters.local_player_cache_resets.fetch_add(1, std::memory_order_relaxed);
    Logger::Log("[Validation] LocalPlayerCache reset", LogType::Warning);
}

void OnEntityHandleLookup(const CBaseHandle& handle, void* resolved_entity, const CBaseHandle& entity_handle) {
    g_counters.handle_lookups.fetch_add(1, std::memory_order_relaxed);

    bool mismatch = false;
    if (resolved_entity && handle.valid()) {
        if (entity_handle.serial_number() != handle.serial_number()) {
            mismatch = true;
            g_counters.handle_mismatches.fetch_add(1, std::memory_order_relaxed);
        }
    } else if (handle.valid() && !resolved_entity) {
        g_counters.handle_mismatches.fetch_add(1, std::memory_order_relaxed);
    }

    uint64_t current_tick = 0;
    if (I::GlobalVars) {
        current_tick = I::GlobalVars->m_tick_count;
    }

    if (mismatch && g_rate_limiter.should_log(current_tick)) {
        char buf[512];
        std::snprintf(buf, sizeof(buf),
            "[Validation] HANDLE MISMATCH: handle_idx=%d handle_serial=%d entity_idx=%d entity_serial=%d resolved=%p",
            handle.index(), handle.serial_number(),
            entity_handle.index(), entity_handle.serial_number(),
            resolved_entity
        );
        Logger::Log(buf, LogType::Warning);
    } else if (!mismatch && g_rate_limiter.should_log(current_tick)) {
        char buf[512];
        std::snprintf(buf, sizeof(buf),
            "[Validation] Handle OK: idx=%d serial=%d resolved=%p",
            handle.index(), handle.serial_number(), resolved_entity
        );
        Logger::Log(buf, LogType::Info);
    }
}

void OnEntityIdentityCheck(CEntityInstance* entity) {
    if (!entity) return;

    g_counters.entity_identity_checks.fetch_add(1, std::memory_order_relaxed);

    CEntityIdentity* identity = entity->m_pEntityIdentity();
    if (!identity) {
        g_counters.entity_identity_mismatches.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    int idx = identity->get_index();
    int serial = identity->get_serial_number();
    bool valid = identity->valid();
    std::string schema_name = identity->GetSchemaName();

    CBaseHandle handle = entity->handle();
    bool handle_matches = (handle.index() == idx) && (handle.serial_number() == serial);

    if (!valid || !handle_matches) {
        g_counters.entity_identity_mismatches.fetch_add(1, std::memory_order_relaxed);
    }

    uint64_t current_tick = 0;
    if (I::GlobalVars) {
        current_tick = I::GlobalVars->m_tick_count;
    }

    if (g_rate_limiter.should_log(current_tick)) {
        char buf[512];
        std::snprintf(buf, sizeof(buf),
            "[Validation] EntityIdentity: idx=%d serial=%d valid=%d schema='%s' handle_match=%d",
            idx, serial, valid, schema_name.c_str(), handle_matches
        );
        Logger::Log(buf, valid && handle_matches ? LogType::Info : LogType::Warning);
    }
}

void OnSceneNodeChainCheck(C_CSPlayerPawn* pawn) {
    if (!pawn) return;

    g_counters.scene_node_chain_checks.fetch_add(1, std::memory_order_relaxed);

    CGameSceneNode* scene = pawn->m_pGameSceneNode();
    if (!scene) {
        g_counters.scene_node_chain_failures.fetch_add(1, std::memory_order_relaxed);
        Logger::Log("[Validation] SceneNode: NULL", LogType::Warning);
        return;
    }

    CSkeletonInstance* skeleton = scene->GetSkeletonInstance();
    if (!skeleton) {
        g_counters.scene_node_chain_failures.fetch_add(1, std::memory_order_relaxed);
        Logger::Log("[Validation] SkeletonInstance: NULL", LogType::Warning);
        return;
    }

    Matrix2x4_t* bone_cache = skeleton->m_bone_cache;
    if (!bone_cache) {
        g_counters.scene_node_chain_failures.fetch_add(1, std::memory_order_relaxed);
        Logger::Log("[Validation] BoneCache: NULL", LogType::Warning);
        return;
    }

    uint64_t current_tick = 0;
    if (I::GlobalVars) {
        current_tick = I::GlobalVars->m_tick_count;
    }

    if (g_rate_limiter.should_log(current_tick)) {
        char buf[512];
        std::snprintf(buf, sizeof(buf),
            "[Validation] SceneNodeChain: scene=0x%p skeleton=0x%p bone_cache=0x%p bone_count=%d",
            scene, skeleton, bone_cache, skeleton->m_bone_count
        );
        Logger::Log(buf, LogType::Info);
    }
}

void OnVTableCall(const char* name, uint32_t index, void* this_ptr, bool success) {
    g_counters.vtable_calls.fetch_add(1, std::memory_order_relaxed);
    if (!success) {
        g_counters.vtable_failures.fetch_add(1, std::memory_order_relaxed);
    }

    uint64_t current_tick = 0;
    if (I::GlobalVars) {
        current_tick = I::GlobalVars->m_tick_count;
    }

    if (g_rate_limiter.should_log(current_tick)) {
        char buf[512];
        std::snprintf(buf, sizeof(buf),
            "[Validation] VTableCall: %s[%u] this=0x%p %s",
            name, index, this_ptr, success ? "OK" : "FAILED"
        );
        Logger::Log(buf, success ? LogType::Info : LogType::Error);
    }
}

void OnPatternResolution(const char* name, void* resolved_addr, bool success) {
    g_counters.pattern_resolutions.fetch_add(1, std::memory_order_relaxed);
    if (!success) {
        g_counters.pattern_failures.fetch_add(1, std::memory_order_relaxed);
    }

    uint64_t current_tick = 0;
    if (I::GlobalVars) {
        current_tick = I::GlobalVars->m_tick_count;
    }

    if (g_rate_limiter.should_log(current_tick)) {
        char buf[512];
        std::snprintf(buf, sizeof(buf),
            "[Validation] Pattern: %s -> 0x%p %s",
            name, resolved_addr, success ? "OK" : "FAILED"
        );
        Logger::Log(buf, success ? LogType::Info : LogType::Error);
    }
}

void LogPeriodicSummary(uint64_t current_tick) {
    if (!g_rate_limiter.should_log(current_tick)) return;

    char buf[1024];
    std::snprintf(buf, sizeof(buf),
        "[Validation] Summary: cache_updates=%u resets=%u handle_lookups=%u handle_mismatches=%u "
        "identity_checks=%u identity_mismatches=%u scene_checks=%u scene_failures=%u "
        "vtable_calls=%u vtable_failures=%u pattern_resolutions=%u pattern_failures=%u",
        g_counters.local_player_cache_updates.load(),
        g_counters.local_player_cache_resets.load(),
        g_counters.handle_lookups.load(),
        g_counters.handle_mismatches.load(),
        g_counters.entity_identity_checks.load(),
        g_counters.entity_identity_mismatches.load(),
        g_counters.scene_node_chain_checks.load(),
        g_counters.scene_node_chain_failures.load(),
        g_counters.vtable_calls.load(),
        g_counters.vtable_failures.load(),
        g_counters.pattern_resolutions.load(),
        g_counters.pattern_failures.load()
    );
    Logger::Log(buf, LogType::Info);
}

} // namespace Validation