#pragma once

// TempleWare Rage CUserCmd Execution Layer
// Applies DryRunActionPlan to the game via CUserCmd mutation in CreateMove.
// Replaces the old memory-write approach with proper command-level execution.
// Silent aim: only set cmd viewangles (server-side), don't touch visual angles.
// Non-silent: set cmd viewangles + visual angles via SetViewAngle.

#include "rage_dryrun.h"
#include "rage_live_providers.h"
#include "rage_nospread.h"
#include "rage_doubletap.h"
#include "../../esp/esp.h"
#include "../utils/filelog/filelog.h"
#include "../interfaces/CCSGOInput/CCSGOInput.h"

#include <cmath>
#include <cstdio>
#include <cstdint>

namespace RageDryRun
{
    namespace CmdExecution
    {
        inline bool g_execution_active = false;
        inline int  g_last_target_id = -1;
        inline int  g_shots_fired = 0;

        inline bool IsValidPtr(uintptr_t p)
        {
            return p >= 0x10000 && p < 0x0000FFFFFFFFFFFFull;
        }

        struct AimTarget
        {
            float pitch = 0.f;
            float yaw = 0.f;
            bool valid = false;
        };

        inline void CalcAngle(const float* src, const float* dst, float& pitch, float& yaw)
        {
            float dx = dst[0] - src[0];
            float dy = dst[1] - src[1];
            float dz = dst[2] - src[2];
            float dist = std::sqrt(dx * dx + dy * dy);
            pitch = -std::atan2(dz, dist) * 57.2957795f;
            yaw = std::atan2(dy, dx) * 57.2957795f;
        }

        inline float NormYaw(float y)
        {
            while (y > 180.f)  y -= 360.f;
            while (y < -180.f) y += 360.f;
            return y;
        }

        // Resolve aim angles from target candidate's bone data.
        inline AimTarget resolve_aim_point(
            const DryRunActionPlan& plan,
            const std::vector<CandidateSnapshot>& candidates,
            uintptr_t clientBase) noexcept
        {
            AimTarget result{};

            if (!plan.target_found || plan.target_id < 0)
                return result;

            const CandidateSnapshot* target = nullptr;
            for (const auto& c : candidates)
            {
                if (c.candidate_id == plan.target_id)
                {
                    target = &c;
                    break;
                }
            }
            if (!target || !target->pawn || !IsValidPtr(target->pawn))
                return result;

            // Local eye position
            static constexpr uintptr_t kGameSceneNode = 0x330;
            static constexpr uintptr_t kVecOrigin = 0x80;
            static constexpr uintptr_t kViewOffset = 0xE78;
            static constexpr uintptr_t kLocalPawn = 0x23C6268;

            uintptr_t localPawn = *reinterpret_cast<uintptr_t*>(clientBase + kLocalPawn);
            if (!IsValidPtr(localPawn)) return result;

            uintptr_t sceneNode = *reinterpret_cast<uintptr_t*>(localPawn + kGameSceneNode);
            if (!IsValidPtr(sceneNode)) return result;

            const float* lo = reinterpret_cast<const float*>(sceneNode + kVecOrigin);
            const float* vo = reinterpret_cast<const float*>(localPawn + kViewOffset);
            float eyePos[3] = { lo[0] + vo[0], lo[1] + vo[1], lo[2] + vo[2] };

            // Target bone
            int boneIndex = 7; // head
            if (plan.selected_hitbox == 2 || plan.selected_hitbox == 3)
                boneIndex = 4;
            else if (plan.selected_hitbox == 4)
                boneIndex = 1;

            uintptr_t tgtScene = *reinterpret_cast<uintptr_t*>(target->pawn + kGameSceneNode);
            if (!IsValidPtr(tgtScene)) return result;

            static const uintptr_t kBoneOffsets[] = { 0x1C0, 0x1F0, 0x1E0, 0x200, 0x1D0 };
            static constexpr uintptr_t kBoneStride = 0x20;
            float bonePos[3] = {};
            bool found = false;

            for (uintptr_t off : kBoneOffsets)
            {
                uintptr_t boneArray = *reinterpret_cast<uintptr_t*>(tgtScene + off);
                if (!IsValidPtr(boneArray)) continue;

                __try
                {
                    const float* b = reinterpret_cast<const float*>(
                        boneArray + static_cast<uintptr_t>(boneIndex) * kBoneStride);
                    if (std::isfinite(b[0]) && std::isfinite(b[1]) && std::isfinite(b[2]) &&
                        std::fabs(b[0]) < 32768.f && std::fabs(b[1]) < 32768.f)
                    {
                        bonePos[0] = b[0];
                        bonePos[1] = b[1];
                        bonePos[2] = b[2];
                        found = true;
                        break;
                    }
                }
                __except (EXCEPTION_EXECUTE_HANDLER) {}
            }

            if (!found) return result;

            CalcAngle(eyePos, bonePos, result.pitch, result.yaw);
            result.valid = true;
            return result;
        }

        // Execute the rage action plan via CUserCmd mutation.
        // Called from CreateMove hook. This is the proper execution path.
        //
        // Silent: set cmd viewangles only (server sees target, player sees nothing).
        // Non-silent: set cmd viewangles + call SetViewAngle (player screen snaps).
        inline bool execute_cmd(
            CUserCmd* cmd,
            CCSGOInput* input,
            const DryRunActionPlan& plan) noexcept
        {
            if (!cmd || !plan.execution_enabled || !plan.target_found || !plan.would_aim)
                return false;

            __try
            {
                HMODULE client = GetModuleHandleA("client.dll");
                if (!client) return false;
                uintptr_t clientBase = reinterpret_cast<uintptr_t>(client);

                uintptr_t localPawn = *reinterpret_cast<uintptr_t*>(clientBase + 0x23C6268);
                if (!IsValidPtr(localPawn)) return false;

                int localHealth = *reinterpret_cast<int32_t*>(localPawn + 0x34C);
                if (localHealth <= 0) return false;

                AimTarget aim = resolve_aim_point(plan, g_state.candidates, clientBase);
                if (!aim.valid) return false;

                float newP = aim.pitch;
                float newY = aim.yaw;
                if (newP < -89.f) newP = -89.f;
                if (newP > 89.f)  newP = 89.f;
                newY = NormYaw(newY);

                // Set command viewangles (server-side aim direction)
                auto* base = cmd->csgoUserCmd.mutable_base();
                if (!base) return false;

                float finalP = newP, finalY = newY, finalR = 0.f;

                // No-spread: correct viewangles to compensate weapon spread
                if (plan.would_no_spread && NoSpread::g_available)
                {
                    int tick = base->has_client_tick() ? base->client_tick() : 0;
                    auto correction = NoSpread::find_correction(
                        newP, newY, tick,
                        g_state.weapon.item_definition,
                        1, // num_bullets (single bullet weapons; shotgun pellets need weapon data)
                        g_state.weapon.inaccuracy,
                        g_state.weapon.spread,
                        g_state.weapon.recoil_index);

                    if (correction.valid)
                    {
                        finalP = correction.pitch;
                        finalY = correction.yaw;
                        finalR = correction.roll;
                    }
                }

                base->mutable_viewangles()->set_x(finalP);
                base->mutable_viewangles()->set_y(finalY);
                base->mutable_viewangles()->set_z(finalR);

                // Doubletap: inject subtick moves for double-fire in one tick
                bool dt_applied = false;
                if (plan.would_doubletap && plan.would_fire)
                {
                    uintptr_t weaponPtr = g_state.weapon.weapon;
                    uintptr_t ctrlPtr = g_state.frame.local_controller;
                    dt_applied = Doubletap::apply(cmd, weaponPtr, ctrlPtr, true);
                }

                // Fire: set attack button in the command (skip if DT handled it)
                if (plan.would_fire && !dt_applied)
                {
                    cmd->nButtons.nValue |= IN_ATTACK;
                    cmd->nButtons.nValueChanged |= IN_ATTACK;
                    cmd->csgoUserCmd.set_attack1_start_history_index(0);
                }

                // Non-silent: also update the visual view angles
                if (!plan.would_silent && input)
                {
                    Vector_t visAngle = { newP, newY, 0.f };
                    input->SetViewAngle(visAngle);
                }

                g_execution_active = true;
                g_last_target_id = plan.target_id;
                ++g_shots_fired;

                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        inline void log_execution(const DryRunActionPlan& plan) noexcept
        {
            static int s_lastLogGen = -1;
            int gen = static_cast<int>(g_state.generation);
            if (gen == s_lastLogGen) return;
            s_lastLogGen = gen;

            if (!plan.target_found) return;

            char buf[384];
            std::snprintf(buf, sizeof(buf),
                "[P6CMD] EXECUTED aim=1 fire=%d target=%d hc=%.0f dmg=%.0f silent=%d force=%d ns=%d dt=%d",
                plan.would_fire ? 1 : 0,
                plan.target_id,
                plan.hitchance,
                plan.predicted_damage,
                plan.would_silent ? 1 : 0,
                plan.force_shot_active ? 1 : 0,
                plan.would_no_spread ? 1 : 0,
                plan.would_doubletap ? 1 : 0);
            FileLog::Log(buf);
        }

    } // namespace CmdExecution
} // namespace RageDryRun
