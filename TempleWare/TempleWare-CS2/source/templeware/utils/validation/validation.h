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

// Wall-clock based limiter so diagnostics still work before/without GlobalVars.
// Each validation category owns its own limiter; one category cannot suppress
// all other output in the same frame/tick.
struct RateLimiter {
    std::atomic<int64_t> last_log_ms{0};
    uint32_t min_interval_ms;

    explicit RateLimiter(uint32_t interval_ms = 1000) : min_interval_ms(interval_ms) {}

    bool should_log() {
        using namespace std::chrono;
        const int64_t now = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
        int64_t last = last_log_ms.load(std::memory_order_relaxed);

        if (last != 0 && (now - last) < static_cast<int64_t>(min_interval_ms))
            return false;

        return last_log_ms.compare_exchange_strong(last, now, std::memory_order_relaxed);
    }
};

inline RateLimiter g_cache_rate_limiter(1000);
inline RateLimiter g_handle_rate_limiter(1000);
inline RateLimiter g_identity_rate_limiter(1000);
inline RateLimiter g_scene_rate_limiter(1000);
inline RateLimiter g_vtable_rate_limiter(1000);
inline RateLimiter g_pattern_rate_limiter(1000);
inline RateLimiter g_summary_rate_limiter(1000);

void Initialize();

void LogFoundationInitBegin();
void LogInterfacesReady();
void LogInterfacesFailed();
void LogHookInitBegin();
void LogFramestageHookInstalled();
void LogFramestageHookFailed();
void LogFramestageFirstCall();

void OnLocalPlayerCacheUpdate(const LocalPlayerSnapshot& snapshot);
void OnLocalPlayerCacheReset();

void OnEntityHandleLookup(const CBaseHandle& handle, void* resolved_entity, const CBaseHandle& entity_handle);
void OnEntityIdentityCheck(CEntityInstance* entity);
void OnSceneNodeChainCheck(C_CSPlayerPawn* pawn);

void OnVTableCall(const char* name, uint32_t index, void* this_ptr, bool success);
void OnPatternResolution(const char* name, void* resolved_addr, bool success);

// current_tick is retained for call-site compatibility; rate limiting no longer
// depends on engine tick availability.
void LogPeriodicSummary(uint64_t current_tick);

} // namespace Validation
