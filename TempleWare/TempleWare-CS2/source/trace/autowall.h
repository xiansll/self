#pragma once
#include <cstdint>

namespace Autowall
{
    struct PenetrationInput
    {
        float origin[3];
        float target[3];
        std::uintptr_t attacker;     // pawn to skip in trace
        std::uintptr_t target_entity; // target pawn for hit detection
        float weapon_damage;
        float weapon_penetration;    // CCSWeaponBaseVData::m_flPenetration
        float weapon_range;
        float weapon_range_modifier;
        float weapon_armor_ratio;
        float headshot_multiplier;
        int   hitgroup;              // target hitgroup (for damage scaling)
        bool  target_has_helmet;
        bool  target_has_armor;
        int   target_armor_value;
    };

    struct PenetrationOutput
    {
        bool  did_hit;
        bool  did_penetrate;         // went through at least one surface
        float damage;                // final predicted damage
        int   surfaces_penetrated;
        float total_distance;
    };

    // Simulate a bullet from origin to target, penetrating surfaces.
    // Returns predicted damage at the target position.
    PenetrationOutput SimulateBullet(const PenetrationInput& input);

    // Quick check: can a bullet reach the target with > 0 damage?
    bool CanDamage(const PenetrationInput& input);
}
