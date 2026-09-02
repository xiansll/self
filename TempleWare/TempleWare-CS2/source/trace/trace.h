#pragma once
#include <cstdint>

// World line-of-sight via CS2's TraceShape (client.dll). Used for real ESP
// visibility (eye -> head). All calls are SEH-guarded; if the game functions
// can't be resolved, IsVisible() returns true (fail-open, ESP keeps working).
namespace Trace
{
    bool Initialize();
    bool Ready();

    // start/end are float[3] world positions. Returns true if `target` is
    // reachable from `start` (direct LOS to `end`), skipping `skip`.
    bool IsVisible(const float start[3], const float end[3], uintptr_t target, uintptr_t skip);

    // Full line trace. Fills outEnd[3] (hit/endpoint), outNormal[3] (surface
    // normal), outFrac (0..1). Returns true if it hit something before `end`.
    bool Line(const float start[3], const float end[3], uintptr_t skip,
              float outEnd[3], float outNormal[3], float* outFrac);
}
