#pragma once
#include <cstdint>

// World line-of-sight via CS2's TraceShape (client.dll). Existing runtime calls
// remain unchanged. P5B adds resolver-health diagnostics only; it does not add
// new signatures or new trace behavior.
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

    // Re-run only the already-existing resolver expressions and classify the
    // missing layer. Any pointer that was null during the early Initialize()
    // call may be filled if the same existing resolver succeeds later.
    bool DiagnoseAndRetryExistingResolvers();
    ResolverDiagnostics GetResolverDiagnostics();

    // start/end are float[3] world positions. Returns true if `target` is
    // reachable from `start` (direct LOS to `end`), skipping `skip`.
    bool IsVisible(const float start[3], const float end[3], uintptr_t target, uintptr_t skip);

    // Full line trace. Fills outEnd[3] (hit/endpoint), outNormal[3] (surface
    // normal), outFrac (0..1). Returns true if it hit something before `end`.
    bool Line(const float start[3], const float end[3], uintptr_t skip,
              float outEnd[3], float outNormal[3], float* outFrac);
}
