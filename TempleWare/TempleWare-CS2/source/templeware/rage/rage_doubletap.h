#pragma once

// TempleWare Doubletap
// Fires two shots in one tick using subtick move injection and
// input_history player_tick_count manipulation. Based on velocity's approach:
// - Inject two SubtickMoveStep entries (press at 0.0, release at 1.0)
// - Set all input_history entries' player_tick_count to next_primary_attack_tick
// - Alternate firing to prevent consecutive double-fire

#include "rage_dryrun.h"
#include "../utils/filelog/filelog.h"
#include "../interfaces/CUserCmd/CUserCmd.h"

#include <cstdint>
#include <cstdio>

namespace RageDryRun
{
    namespace Doubletap
    {
        // Weapon offsets from schema table
        inline constexpr uintptr_t kNextPrimaryAttackTick = 0x16F0;
        inline constexpr uintptr_t kClip1 = 0x1700;
        inline constexpr uintptr_t kInReload = 0x1814;

        // Controller offset
        inline constexpr uintptr_t kTickBase = 0x6B8;

        // Pawn offset for m_iShotsFired
        inline constexpr uintptr_t kShotsFired = 0x1C8C;

        inline bool g_doubletap_fired = false;
        inline int  g_last_doubletap_tick = 0;
        inline int  g_dt_shots = 0;

        inline bool IsValidPtr(uintptr_t p)
        {
            return p >= 0x10000 && p < 0x0000FFFFFFFFFFFFull;
        }

        struct DoubletapState
        {
            bool can_attack = false;
            int next_attack_tick = 0;
            int tick_base = 0;
            int clip = 0;
            bool reloading = false;
        };

        // Read weapon/player state needed for doubletap eligibility.
        inline DoubletapState read_state(uintptr_t weapon, uintptr_t controller) noexcept
        {
            DoubletapState st{};
            if (!IsValidPtr(weapon) || !IsValidPtr(controller)) return st;

            __try
            {
                st.next_attack_tick = *reinterpret_cast<int32_t*>(weapon + kNextPrimaryAttackTick);
                st.clip = *reinterpret_cast<int32_t*>(weapon + kClip1);
                st.reloading = *reinterpret_cast<bool*>(weapon + kInReload);
                st.tick_base = *reinterpret_cast<int32_t*>(controller + kTickBase);

                st.can_attack = !st.reloading &&
                                st.clip > 0 &&
                                st.tick_base >= st.next_attack_tick;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) { return st; }

            return st;
        }

        // Apply doubletap: inject subtick moves and manipulate input_history ticks.
        // Returns true if doubletap was applied to the command.
        inline bool apply(CUserCmd* cmd, uintptr_t weapon, uintptr_t controller,
                          bool should_attack) noexcept
        {
            if (!cmd) return false;

            auto* base = cmd->csgoUserCmd.mutable_base();
            if (!base) return false;

            DoubletapState st = read_state(weapon, controller);
            if (!st.can_attack) return false;

            __try
            {
                if (should_attack && !g_doubletap_fired)
                {
                    // Inject two subtick moves: press IN_ATTACK at 0.0, release at 1.0
                    auto* press = base->add_subtick_moves();
                    if (press)
                    {
                        press->set_button(IN_ATTACK);
                        press->set_pressed(true);
                        press->set_when(0.0f);
                    }

                    auto* release = base->add_subtick_moves();
                    if (release)
                    {
                        release->set_button(IN_ATTACK);
                        release->set_pressed(false);
                        release->set_when(1.0f);
                    }

                    // Manipulate input_history: set player_tick_count to
                    // next_primary_attack_tick so server thinks weapon is ready
                    int hist_count = cmd->csgoUserCmd.input_history_size();
                    for (int i = 0; i < hist_count; ++i)
                    {
                        auto* entry = cmd->csgoUserCmd.mutable_input_history(i);
                        if (entry)
                            entry->set_player_tick_count(st.next_attack_tick);
                    }

                    cmd->csgoUserCmd.set_attack1_start_history_index(-1);

                    g_doubletap_fired = true;
                    g_last_doubletap_tick = st.tick_base;
                    ++g_dt_shots;
                    return true;
                }
                else
                {
                    // Not attacking or alternating: clear attack and reset
                    cmd->nButtons.nValue &= ~IN_ATTACK;
                    g_doubletap_fired = false;
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER) { return false; }

            return false;
        }

    } // namespace Doubletap
} // namespace RageDryRun
