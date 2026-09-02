#pragma once

// Velocity Rage port compatibility contract.
//
// This layer is intentionally non-operational: it does not aim, fire, mutate
// commands, install hooks, scan for new signatures, or dereference pointer-only
// game objects. Its job is to give the future port one TempleWare-facing type
// and capability surface so Velocity-specific dependencies can be adapted one
// at a time instead of copied directly into the project.

#include <cstdint>
#include <cstdio>

#include "../globals/globals.h"
#include "../features/prediction/prediction.h"
#include "../utils/filelog/filelog.h"
#include "../utils/localplayer/localplayer.h"
#include "velocity_command_compat.h"
#include "velocity_data_compat.h"
#include "velocity_config_compat.h"
#include "velocity_runtime_compat.h"
#include "../../trace/trace.h"

namespace VelocityRageCompat {

using vector2 = Vector2D_t;
using vector3 = Vector_t;
using angles3 = QAngle_t;
using usercmd = CUserCmd;

static_assert(sizeof(vector2) == sizeof(float) * 2, "Unexpected TempleWare Vector2D_t layout");
static_assert(sizeof(vector3) == sizeof(float) * 3, "Unexpected TempleWare Vector_t layout");

struct local_state {
    std::uintptr_t controller{};
    std::uintptr_t pawn{};
    std::uintptr_t observer_pawn{};
    std::uintptr_t observer_controller{};
    int team{};
    int view_team{};
    bool is_alive{};
    bool is_team_mode{};
    bool sdk_deref_safe{};

    [[nodiscard]] std::uintptr_t view_controller() const {
        return is_alive ? controller : observer_controller;
    }

    [[nodiscard]] std::uintptr_t view_pawn() const {
        return is_alive ? pawn : observer_pawn;
    }

    [[nodiscard]] bool is_valid() const {
        return pawn != 0 || observer_pawn != 0;
    }
};

[[nodiscard]] inline local_state adapt_local(const LocalPlayerSnapshot& src) {
    local_state out{};
    out.controller = src.controller;
    out.pawn = src.pawn;
    out.observer_pawn = src.observer_pawn;
    out.observer_controller = src.observer_controller;
    out.team = src.team;
    out.view_team = src.view_team;
    out.is_alive = src.is_alive;
    out.is_team_mode = src.is_team_mode;
    out.sdk_deref_safe = src.sdk_deref_safe;
    return out;
}

struct readiness {
    bool vector_types = true;
    bool usercmd_type = true;
    bool local_snapshot_adapter = true;
    bool prediction_object = true;
    bool basic_trace_api = true;
    bool command_pipeline_adapter = true;
    bool entity_cache_adapter = data_contracts::entity_cache;
    bool bone_adapter = data_contracts::bones;
    bool hitbox_adapter = data_contracts::hitboxes;
    bool rich_trace_adapter = data_contracts::rich_trace;
    bool rage_config_adapter = config_contracts::rage_config;

    bool local_pair = false;
    bool sdk_deref_safe = false;
    bool input_runtime = false;
    bool prediction_runtime = false;
    bool trace_runtime = false;
    bool command_runtime = false;
    bool entity_cache_runtime = false;
    bool bone_runtime = false;
    bool hitbox_runtime = false;
    bool rich_trace_runtime = false;
    bool rage_config_runtime = false;

    std::uint64_t runtime_epoch = 0;
    std::uint64_t command_generation = 0;
    std::uint64_t config_generation = 0;
};

[[nodiscard]] inline readiness query(const LocalPlayerSnapshot& snapshot) {
    readiness r{};
    r.local_pair = snapshot.pawn != 0 && snapshot.controller != 0;
    r.sdk_deref_safe = snapshot.sdk_deref_safe;
    r.input_runtime = I::Input != nullptr;
    r.prediction_runtime = static_cast<bool>(g_prediction);
    r.trace_runtime = Trace::Ready();
    r.command_runtime = g_command_lifecycle.runtime_ready();
    r.entity_cache_runtime = g_runtime_services.ready(runtime_service::entity_cache);
    r.bone_runtime = g_runtime_services.ready(runtime_service::bones);
    r.hitbox_runtime = g_runtime_services.ready(runtime_service::hitboxes);
    r.rich_trace_runtime = g_runtime_services.ready(runtime_service::rich_trace);
    r.rage_config_runtime = g_rage_config_store.runtime_ready();
    r.runtime_epoch = g_runtime_services.epoch();
    r.command_generation = g_command_lifecycle.generation();
    r.config_generation = g_rage_config_store.generation();
    return r;
}

[[nodiscard]] inline bool port_gate_open(const readiness& r) {
    return r.local_pair &&
           r.sdk_deref_safe &&
           r.input_runtime &&
           r.prediction_runtime &&
           r.trace_runtime &&
           r.command_pipeline_adapter &&
           r.command_runtime &&
           r.entity_cache_adapter &&
           r.entity_cache_runtime &&
           r.bone_adapter &&
           r.bone_runtime &&
           r.hitbox_adapter &&
           r.hitbox_runtime &&
           r.rich_trace_adapter &&
           r.rich_trace_runtime &&
           r.rage_config_adapter &&
           r.rage_config_runtime;
}

inline void log_readiness(const LocalPlayerSnapshot& snapshot) {
    const readiness r = query(snapshot);

    static std::uintptr_t s_lastPawn = 0;
    static std::uintptr_t s_lastController = 0;
    static bool s_lastSdkSafe = false;
    static std::uint64_t s_lastRuntimeEpoch = 0;
    static std::uint64_t s_lastCommandGeneration = 0;
    static std::uint64_t s_lastConfigGeneration = 0;
    static bool s_logged = false;

    if (s_logged &&
        s_lastPawn == snapshot.pawn &&
        s_lastController == snapshot.controller &&
        s_lastSdkSafe == snapshot.sdk_deref_safe &&
        s_lastRuntimeEpoch == r.runtime_epoch &&
        s_lastCommandGeneration == r.command_generation &&
        s_lastConfigGeneration == r.config_generation) {
        return;
    }

    char buf[768];
    std::snprintf(buf, sizeof(buf),
        "[P4COMPAT] TYPE MAP vector=%d usercmd=%d local=%d prediction=%d trace_api=%d command_contract=%d config_contract=%d",
        r.vector_types ? 1 : 0,
        r.usercmd_type ? 1 : 0,
        r.local_snapshot_adapter ? 1 : 0,
        r.prediction_object ? 1 : 0,
        r.basic_trace_api ? 1 : 0,
        r.command_pipeline_adapter ? 1 : 0,
        r.rage_config_adapter ? 1 : 0);
    FileLog::Log(buf);

    std::snprintf(buf, sizeof(buf),
        "[P4COMPAT] RUNTIME local_pair=%d sdk_safe=%d input=%d prediction=%d trace=%d command=%d entities=%d bones=%d hitboxes=%d rich_trace=%d config=%d",
        r.local_pair ? 1 : 0,
        r.sdk_deref_safe ? 1 : 0,
        r.input_runtime ? 1 : 0,
        r.prediction_runtime ? 1 : 0,
        r.trace_runtime ? 1 : 0,
        r.command_runtime ? 1 : 0,
        r.entity_cache_runtime ? 1 : 0,
        r.bone_runtime ? 1 : 0,
        r.hitbox_runtime ? 1 : 0,
        r.rich_trace_runtime ? 1 : 0,
        r.rage_config_runtime ? 1 : 0);
    FileLog::Log(buf);

    std::snprintf(buf, sizeof(buf),
        "[P4COMPAT] ADAPTERS command=%d entities=%d bones=%d hitboxes=%d rich_trace=%d rage_config=%d",
        r.command_pipeline_adapter ? 1 : 0,
        r.entity_cache_adapter ? 1 : 0,
        r.bone_adapter ? 1 : 0,
        r.hitbox_adapter ? 1 : 0,
        r.rich_trace_adapter ? 1 : 0,
        r.rage_config_adapter ? 1 : 0);
    FileLog::Log(buf);

    std::snprintf(buf, sizeof(buf),
        "[P4COMPAT] LIFECYCLE epoch=%llu command_gen=%llu config_gen=%llu",
        static_cast<unsigned long long>(r.runtime_epoch),
        static_cast<unsigned long long>(r.command_generation),
        static_cast<unsigned long long>(r.config_generation));
    FileLog::Log(buf);

    FileLog::Log(port_gate_open(r)
        ? "[P4COMPAT] PORT GATE OPEN - compatibility prerequisites proven"
        : "[P4COMPAT] PORT GATE BLOCKED - runtime producers still missing/unproven");

    s_lastPawn = snapshot.pawn;
    s_lastController = snapshot.controller;
    s_lastSdkSafe = snapshot.sdk_deref_safe;
    s_lastRuntimeEpoch = r.runtime_epoch;
    s_lastCommandGeneration = r.command_generation;
    s_lastConfigGeneration = r.config_generation;
    s_logged = true;
}

} // namespace VelocityRageCompat
