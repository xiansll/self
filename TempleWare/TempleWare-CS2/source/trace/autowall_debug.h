#pragma once

namespace AutowallDebug
{
    // Call every frame from Present hook. Internally throttled to 2s intervals.
    // When local player is alive and an enemy exists, runs SimulateBullet
    // and logs the result. Pure read-only — no gameplay effect.
    void Tick();
}
