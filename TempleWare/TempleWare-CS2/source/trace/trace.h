#pragma once
#include <cstdint>

// World line-of-sight through the project's existing TraceShape backend.
// ResolverDiagnostics keeps its legacy field names for compatibility:
// manager=resolved physics-world object, trace_ray=resolved TraceShape address.
namespace Trace
{
    struct ResolverDiagnostics {
        bool client_loaded = false;
        bool ready = false;
        std::uintptr_t manager = 0;
        std::uintptr_t trace_ray = 0;
        std::uintptr_t filter_init = 0;
        std::uintptr_t fresh_manager = 0;
        std::uintptr_t fresh_trace_ray = 0;
        std::uintptr_t fresh_filter_init = 0;
    };

    bool Initialize();
    bool Ready();

    // Re-check only the project's already-existing resolved dependencies and
    // classify whether TraceShape can be considered ready. No new resolver is added.
    bool DiagnoseAndRetryExistingResolvers();
    ResolverDiagnostics GetResolverDiagnostics();

    // Throttled retry: call every frame, internally gates to ~500ms intervals
    // with a total 8-second wall-clock cap. Returns true once ready or when
    // the retry budget is exhausted (check Ready() for actual state).
    bool ThrottledRetry();

    // start/end are float[3] world positions. Returns true if `target` is
    // reachable from `start` (direct LOS to `end`), skipping `skip`.
    bool IsVisible(const float start[3], const float end[3], uintptr_t target, uintptr_t skip);

    // Full line trace. Fills outEnd[3] (hit/endpoint), outNormal[3] (surface
    // normal), outFrac (0..1). Returns true if it hit something before `end`.
    bool Line(const float start[3], const float end[3], uintptr_t skip,
              float outEnd[3], float outNormal[3], float* outFrac);
}
