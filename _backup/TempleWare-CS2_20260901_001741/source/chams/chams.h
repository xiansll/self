#pragma once
#include <cstdint>

// Player chams via a hook on scenesystem.dll!GenerateMeshPrimitives.
// On each model draw for a target enemy we let the game build its primitive
// batch, then overwrite each new primitive's material pointer + color with a
// preloaded flat material. Ported (minimal, enemy-only, single layer) from the
// velocity-cs2 reference.
#ifdef TEMPLEWARE_EXPORTS
#define CHAMS_API __declspec(dllexport)
#else
#define CHAMS_API __declspec(dllimport)
#endif

namespace Chams
{
    // Resolve patterns, create the flat material, and install the hook.
    // Safe to call once; returns false (and logs) if anything is unavailable.
    CHAMS_API bool Initialize();

    // Remove the hook.
    CHAMS_API void Shutdown();

    // Feed the current target pointers per scope (called each frame from Esp::Draw).
    CHAMS_API void UpdateTargets(const uintptr_t* enemies, int nEnemy,
                       const uintptr_t* team, int nTeam,
                       uintptr_t local,
                       const uintptr_t* items, int nItems,
                       const uintptr_t* ragdolls, int nRagdolls);
}
