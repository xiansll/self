#pragma once

#include <cstdint>
#include <atomic>
#include <chrono>

class CEntityInstance;
class C_CSPlayerPawn;
class CBaseHandle;
struct LocalPlayerSnapshot;

namespace Validation {

enum class CheckResult : uint8_t {
    VALID = 0,
    NULL_UNAVAILABLE = 1,
    INCONSISTENT = 2,
    NOT_TESTED = 3
};

struct ValidationCounters {
    std::atomic<uint32_t> local_player_cache_updates{0};
    std::atomic<uint32_t> local_player_cache_resets{0};
    std::atomic<uint32_t> handle_lookups{0};
    std::atomic<uint32_t> handle_mismatches{0};
    std::atomic<uint32_t> entity_identity_checks{0};
    std::atomic<uint32_t> entity_identity_mismatches{0};
    std::atomic<uint32_t> scene_node_chain_checks{0};
    std::atomic<uint32_t> scene_node_chain_failures{0};
    std::atomic<uint32_t> vtable_calls{0};
    std::atomic<uint32_t> vtable_failures{0};
    std::atomic<uint32_t> pattern_resolutions{0};
    std::atomic<uint32_t> pattern_failures{0};
};

inline ValidationCounters g_counters;

struct RateLimiter {
    std::atomic<uint64_t> last_log_tick{0};
    uint32_t min_tick_interval;

    RateLimiter(uint32_t interval = 64) : min_tick_interval(interval) {}

    bool should_log(uint64_t current_tick) {
        uint64_t last = last_log_tick.load(std::memory_order_relaxed);
        if (current_tick - last >= min_tick_interval) {
            return last_log_tick.compare_exchange_strong(last, current_tick, std::memory_order_relaxed);
        }
        return false;
    }
};

inline RateLimiter g_rate_limiter(64);

void Initialize();

void OnLocalPlayerCacheUpdate(const LocalPlayerSnapshot& snapshot);
void OnLocalPlayerCacheReset();

void OnEntityHandleLookup(const CBaseHandle& handle, void* resolved_entity, const CBaseHandle& entity_handle);
void OnEntityIdentityCheck(CEntityInstance* entity);
void OnSceneNodeChainCheck(C_CSPlayerPawn* pawn);

void OnVTableCall(const char* name, uint32_t index, void* this_ptr, bool success);
void OnPatternResolution(const char* name, void* resolved_addr, bool success);

void LogPeriodicSummary(uint64_t current_tick);

} // namespace Validation