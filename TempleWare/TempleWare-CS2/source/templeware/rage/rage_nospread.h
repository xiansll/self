#pragma once

// TempleWare No-Spread (Spread Compensation)
// Brute-forces a viewangle whose spread vector cancels the natural weapon
// spread, so bullets land exactly where aimed. Based on velocity's approach:
// loop pitch candidates, get spread seed from game, calculate spread vector,
// compute correction angle, verify seed matches.

#include "../utils/memory/patternscan/patternscan.h"
#include "../utils/filelog/filelog.h"

#include <cmath>
#include <cstdint>
#include <cstdio>

namespace RageDryRun
{
    namespace NoSpread
    {
        // Game function signatures (client.dll)
        using GetTickViewAngles_t = uint32_t(__fastcall*)(void*, float*, int);
        using WeaponCalcSpread_t = void(__fastcall*)(int, int, int, int, float, float, float, float*, float*);

        inline GetTickViewAngles_t fn_get_tick_view_angles = nullptr;
        inline WeaponCalcSpread_t  fn_weapon_calc_spread = nullptr;
        inline bool g_initialized = false;
        inline bool g_available = false;

        inline constexpr float kDeg2Rad = 0.01745329251f;
        inline constexpr float kRad2Deg = 57.2957795131f;

        inline bool Initialize() noexcept
        {
            if (g_initialized) return g_available;
            g_initialized = true;

            auto addr1 = M::scan("client.dll",
                "48 89 5C 24 08 57 48 81 EC F0 00 00 00");
            if (addr1)
                fn_get_tick_view_angles = reinterpret_cast<GetTickViewAngles_t>(addr1);

            auto addr2 = M::scan("client.dll",
                "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 48 83 EC 30 44 8B C2");
            if (addr2)
                fn_weapon_calc_spread = reinterpret_cast<WeaponCalcSpread_t>(addr2);

            g_available = fn_get_tick_view_angles && fn_weapon_calc_spread;

            char buf[128];
            std::snprintf(buf, sizeof(buf), "[NOSPREAD] Init: getTickVA=%p calcSpread=%p avail=%d",
                fn_get_tick_view_angles, fn_weapon_calc_spread, g_available ? 1 : 0);
            FileLog::Log(buf);

            return g_available;
        }

        struct SpreadCorrection
        {
            float pitch = 0.f;
            float yaw = 0.f;
            float roll = 0.f;
            bool valid = false;
        };

        // Brute-force a viewangle that compensates weapon spread.
        // Loops 720 pitch candidates to find one where the game's spread
        // calculation produces a vector we can cancel by adjusting the angle.
        inline SpreadCorrection find_correction(
            float aim_pitch, float aim_yaw, int tick,
            int item_def_idx, int num_bullets,
            float inaccuracy, float spread, float recoil_index) noexcept
        {
            SpreadCorrection result{};

            if (!g_available) return result;
            if (inaccuracy <= 0.f && spread <= 0.f)
            {
                result.pitch = aim_pitch;
                result.yaw = aim_yaw;
                result.roll = 0.f;
                result.valid = true;
                return result;
            }

            __try
            {
                for (int i = 0; i < 720; ++i)
                {
                    float test_angles[3] = { i / 2.0f, aim_yaw, 0.f };
                    uint32_t seed = fn_get_tick_view_angles(nullptr, test_angles, tick);

                    float sx = 0.f, sy = 0.f;
                    fn_weapon_calc_spread(
                        item_def_idx, num_bullets, 0, static_cast<int>(seed + 1),
                        inaccuracy, spread, recoil_index, &sx, &sy);

                    float spread_mag = std::sqrt(sx * sx + sy * sy);
                    if (spread_mag < 0.00001f) continue;

                    float adj_pitch = aim_pitch + kRad2Deg * std::atan(spread_mag);
                    float adj_roll = -kRad2Deg * std::atan2(sx, sy);

                    float verify_angles[3] = { adj_pitch, aim_yaw, adj_roll };
                    uint32_t verify_seed = fn_get_tick_view_angles(nullptr, verify_angles, tick);

                    if (verify_seed == seed)
                    {
                        result.pitch = adj_pitch;
                        result.yaw = aim_yaw;
                        result.roll = adj_roll;
                        result.valid = true;
                        return result;
                    }
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {}

            return result;
        }

    } // namespace NoSpread
} // namespace RageDryRun
