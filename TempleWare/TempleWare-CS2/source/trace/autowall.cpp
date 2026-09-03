#include "autowall.h"
#include "trace.h"
#include <windows.h>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <algorithm>

#include <CS2/SDK/SDK.hpp>
#include <CS2/SDK/Update/GameTrace.hpp>
#include <CS2/SDK/FunctionListSDK.hpp>

namespace {

constexpr int   MAX_PENETRATIONS = 4;
constexpr float TRACE_NUDGE      = 3.0f;
constexpr float MIN_TRACE_LENGTH = 0.25f;

// CS2 surface data layout accessed through CGameTrace::pSurfaceProperties.
// The void* points to a CPhysSurfaceProperties runtime object.
// Penetration-relevant fields at known runtime offsets:
//   +0x30 penetrationModifier (float) — how easily bullets pass through
//   +0x34 damageModifier      (float) — how much damage the surface absorbs
struct SurfacePenData
{
    float penetration_modifier;
    float damage_modifier;
};

SurfacePenData GetSurfaceData(const CGameTrace& trace) noexcept
{
    SurfacePenData data{1.0f, 1.0f};

    if (!trace.pSurfaceProperties) return data;

    __try
    {
        auto* base = reinterpret_cast<const uint8_t*>(trace.pSurfaceProperties);
        data.penetration_modifier = *reinterpret_cast<const float*>(base + 0x30);
        data.damage_modifier      = *reinterpret_cast<const float*>(base + 0x34);

        if (data.penetration_modifier <= 0.f) data.penetration_modifier = 0.1f;
        if (data.damage_modifier <= 0.f)      data.damage_modifier = 0.1f;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        data.penetration_modifier = 1.0f;
        data.damage_modifier = 1.0f;
    }

    return data;
}

float GetHitgroupDamageMultiplier(int hitgroup) noexcept
{
    switch (hitgroup)
    {
    case 1: return 4.0f;  // head
    case 2: return 1.0f;  // chest
    case 3: return 1.25f; // stomach
    case 4:               // left arm
    case 5: return 1.0f;  // right arm
    case 6:               // left leg
    case 7: return 0.75f; // right leg
    default: return 1.0f;
    }
}

float ApplyArmorReduction(float damage, float armor_ratio, int hitgroup,
                          bool has_helmet, bool has_armor, int armor_value) noexcept
{
    if (!has_armor || armor_value <= 0) return damage;

    // Helmet only protects the head
    if (hitgroup == 1 && !has_helmet) return damage;
    // Armor only covers chest/arms/stomach in CS2
    if (hitgroup >= 6) return damage;

    float armor_bonus = 0.5f;
    float armor_reduction = armor_ratio * 0.5f;
    float new_damage = damage * armor_reduction;

    if ((damage - new_damage) * (armor_bonus / armor_reduction) > static_cast<float>(armor_value))
        new_damage = damage - static_cast<float>(armor_value) / armor_bonus;

    return (std::max)(new_damage, 0.f);
}

struct Vec3 { float x, y, z; };

float VecLength(const Vec3& v) noexcept
{
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

Vec3 VecSub(const Vec3& a, const Vec3& b) noexcept
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 VecNormalize(const Vec3& v) noexcept
{
    float len = VecLength(v);
    if (len < 0.0001f) return {0, 0, 0};
    return {v.x / len, v.y / len, v.z / len};
}

Vec3 VecAdd(const Vec3& a, const Vec3& b) noexcept
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 VecScale(const Vec3& v, float s) noexcept
{
    return {v.x * s, v.y * s, v.z * s};
}

struct TraceResult
{
    bool  hit;
    bool  call_ok;
    float end[3];
    float normal[3];
    float fraction;
    std::uintptr_t hit_entity;
    SurfacePenData surface;
    int   hitgroup;
};

__declspec(noinline) TraceResult DoFullTrace(IVPhysics2World** pPhysicsWorld,
                                             const float* start, const float* end,
                                             std::uintptr_t skip) noexcept
{
    TraceResult result{};
    result.call_ok = false;
    result.hit = false;
    result.fraction = 1.0f;

    Vector3 vecStart(start[0], start[1], start[2]);
    Vector3 vecEnd(end[0], end[1], end[2]);

    CTraceFilter filter(0x1c3003, (C_CSPlayerPawn*)skip, 4, 7);
    Ray_t ray{};
    CGameTrace trace{};

    if (!IGamePhysicsQuery_TraceShape(pPhysicsWorld, ray, vecStart, vecEnd, &filter, &trace))
        return result;

    result.call_ok = true;
    result.fraction = trace.flFraction;
    result.end[0] = trace.vecEnd.m_x;
    result.end[1] = trace.vecEnd.m_y;
    result.end[2] = trace.vecEnd.m_z;
    result.normal[0] = trace.vecNormal.m_x;
    result.normal[1] = trace.vecNormal.m_y;
    result.normal[2] = trace.vecNormal.m_z;
    result.hit_entity = reinterpret_cast<std::uintptr_t>(trace.pHitEntity);
    result.hit = trace.DidHit();
    result.surface = GetSurfaceData(trace);
    result.hitgroup = trace.GetHitGroup();

    return result;
}

// Simulate bullet penetration through surfaces
__declspec(noinline) Autowall::PenetrationOutput SimulateBulletInner(
    IVPhysics2World** pPhysicsWorld,
    const Autowall::PenetrationInput& input) noexcept
{
    Autowall::PenetrationOutput out{};

    Vec3 origin = {input.origin[0], input.origin[1], input.origin[2]};
    Vec3 target = {input.target[0], input.target[1], input.target[2]};
    Vec3 dir = VecNormalize(VecSub(target, origin));

    float remaining_distance = (std::min)(VecLength(VecSub(target, origin)), input.weapon_range);
    float current_damage = static_cast<float>(input.weapon_damage);
    float current_penetration = input.weapon_penetration;

    Vec3 current_pos = origin;
    int surfaces_penetrated = 0;
    float total_distance = 0.f;

    for (int i = 0; i <= MAX_PENETRATIONS; ++i)
    {
        Vec3 trace_end = VecAdd(current_pos, VecScale(dir, remaining_distance));

        float start[3] = {current_pos.x, current_pos.y, current_pos.z};
        float end[3] = {trace_end.x, trace_end.y, trace_end.z};

        TraceResult tr = DoFullTrace(pPhysicsWorld, start, end, input.attacker);

        if (!tr.call_ok)
            break;

        float trace_distance = remaining_distance * tr.fraction;
        total_distance += trace_distance;

        // Apply range-based damage falloff
        float range_modifier = powf(input.weapon_range_modifier, total_distance / 500.f);
        float damage_at_point = current_damage * range_modifier;

        // Did we hit our target entity?
        if (tr.hit && tr.hit_entity == input.target_entity)
        {
            // Apply hitgroup multiplier
            int hitgroup = (input.hitgroup > 0) ? input.hitgroup : tr.hitgroup;
            float hg_mult = GetHitgroupDamageMultiplier(hitgroup);
            if (hitgroup == 1)
                hg_mult = input.headshot_multiplier;
            damage_at_point *= hg_mult;

            // Apply armor
            damage_at_point = ApplyArmorReduction(
                damage_at_point, input.weapon_armor_ratio,
                hitgroup, input.target_has_helmet,
                input.target_has_armor, input.target_armor_value);

            out.did_hit = true;
            out.did_penetrate = surfaces_penetrated > 0;
            out.damage = (std::max)(damage_at_point, 0.f);
            out.surfaces_penetrated = surfaces_penetrated;
            out.total_distance = total_distance;
            return out;
        }

        // Didn't hit anything — clear LOS to end point (no wall in the way)
        if (!tr.hit || tr.fraction >= 0.999f)
        {
            // If this is the first trace and it went clean through,
            // the target is reachable with no penetration needed.
            if (i == 0)
            {
                int hitgroup = (input.hitgroup > 0) ? input.hitgroup : 0;
                float hg_mult = GetHitgroupDamageMultiplier(hitgroup);
                if (hitgroup == 1)
                    hg_mult = input.headshot_multiplier;
                damage_at_point *= hg_mult;
                damage_at_point = ApplyArmorReduction(
                    damage_at_point, input.weapon_armor_ratio,
                    hitgroup, input.target_has_helmet,
                    input.target_has_armor, input.target_armor_value);

                out.did_hit = true;
                out.did_penetrate = false;
                out.damage = (std::max)(damage_at_point, 0.f);
                out.surfaces_penetrated = 0;
                out.total_distance = remaining_distance;
            }
            return out;
        }

        // Hit a wall/surface — can we penetrate?
        if (i >= MAX_PENETRATIONS)
            break;

        if (current_penetration <= 0.f)
            break;

        // Calculate penetration through this surface
        float pen_mod = tr.surface.penetration_modifier;
        float dmg_mod = tr.surface.damage_modifier;

        // Find exit point: trace backward from a nudged-forward position
        Vec3 enter_point = {tr.end[0], tr.end[1], tr.end[2]};
        Vec3 nudged = VecAdd(enter_point, VecScale(dir, TRACE_NUDGE));

        // Trace from nudged position back to entry to find surface thickness
        float rev_start[3] = {nudged.x, nudged.y, nudged.z};
        float rev_end[3] = {enter_point.x, enter_point.y, enter_point.z};
        TraceResult exit_tr = DoFullTrace(pPhysicsWorld, rev_start, rev_end, input.attacker);

        float thickness;
        if (exit_tr.call_ok && exit_tr.hit)
        {
            // Reverse trace hit — thickness is distance between entry and exit
            Vec3 exit_point = {exit_tr.end[0], exit_tr.end[1], exit_tr.end[2]};
            thickness = VecLength(VecSub(exit_point, enter_point));
        }
        else
        {
            // Couldn't find exit — use nudge distance as minimum thickness
            thickness = TRACE_NUDGE;
        }

        if (thickness < MIN_TRACE_LENGTH) thickness = MIN_TRACE_LENGTH;

        // CS2 penetration formula:
        // damage_lost = (thickness / pen_mod) * (1/current_penetration) * dmg_mod
        float damage_lost = (thickness / pen_mod) * (1.f / current_penetration) * dmg_mod;
        damage_lost = (std::max)(damage_lost, 0.f);

        // Reduce penetration power for next surface
        current_penetration -= thickness / pen_mod;
        if (current_penetration < 0.f) current_penetration = 0.f;

        current_damage -= damage_lost;
        if (current_damage <= 0.f)
            break;

        // Advance past the surface
        current_pos = nudged;
        remaining_distance -= (trace_distance + TRACE_NUDGE);
        if (remaining_distance <= 0.f)
            break;

        ++surfaces_penetrated;
    }

    out.did_hit = false;
    out.did_penetrate = surfaces_penetrated > 0;
    out.damage = 0.f;
    out.surfaces_penetrated = surfaces_penetrated;
    out.total_distance = total_distance;
    return out;
}

} // namespace

namespace Autowall
{
    PenetrationOutput SimulateBullet(const PenetrationInput& input)
    {
        PenetrationOutput out{};

        if (!Trace::Ready()) return out;

        auto* pPhysicsWorld = SDK::Pointers::CVPhys2World();
        if (!pPhysicsWorld || !*pPhysicsWorld) return out;

        __try
        {
            return SimulateBulletInner(pPhysicsWorld, input);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return out;
        }
    }

    bool CanDamage(const PenetrationInput& input)
    {
        auto result = SimulateBullet(input);
        return result.did_hit && result.damage > 0.f;
    }
}
