#pragma once

// TempleWare Rage Execution Layer
// Applies DryRunActionPlan decisions to the game via direct memory writes.
// Uses the same memory-write approach as ESP's UpdateAim (proven path).

#include "rage_dryrun.h"
#include "rage_live_providers.h"
#include "../../esp/esp.h"
#include "../utils/filelog/filelog.h"

#include <windows.h>
#include <cmath>
#include <cstdio>
#include <cstdint>

namespace RageDryRun
{
    namespace Execution
    {
        // Offsets (same as esp.cpp, proven)
        inline constexpr uintptr_t kDw_dwViewAngles = 0x23DC2F8;
        inline constexpr uintptr_t kDw_dwLocalPlayerPawn = 0x23C6268;
        inline constexpr uintptr_t kBaseEntity_m_pGameSceneNode = 0x330;
        inline constexpr uintptr_t kGameSceneNode_m_vecOrigin = 0x80;
        inline constexpr uintptr_t kBaseModelEntity_m_vecViewOffset = 0xE78;
        inline constexpr uintptr_t kBaseEntity_m_fFlags = 0x3F4;
        inline constexpr uintptr_t kCSPlayerPawn_m_iShotsFired = 0x1C8C;

        // Bone access for target aim point
        inline constexpr uintptr_t kBoneData_stride = 0x20;

        // IN_ATTACK flag for force-fire
        inline constexpr int IN_ATTACK = (1 << 0);
        inline constexpr uintptr_t kDw_dwForceAttack = 0x20B38F0;

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
            while (y > 180.f) y -= 360.f;
            while (y < -180.f) y += 360.f;
            return y;
        }

        // Get the target aim point from the selected candidate's bones.
        // Uses the bone data from the live provider's pawn pointer.
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

            // Get local eye position
            uintptr_t localPawn = *reinterpret_cast<uintptr_t*>(clientBase + kDw_dwLocalPlayerPawn);
            if (!IsValidPtr(localPawn)) return result;

            uintptr_t sceneNode = *reinterpret_cast<uintptr_t*>(localPawn + kBaseEntity_m_pGameSceneNode);
            if (!IsValidPtr(sceneNode)) return result;

            const float* lo = reinterpret_cast<const float*>(sceneNode + kGameSceneNode_m_vecOrigin);
            const float* vo = reinterpret_cast<const float*>(localPawn + kBaseModelEntity_m_vecViewOffset);
            float eyePos[3] = {
                lo[0] + vo[0],
                lo[1] + vo[1],
                lo[2] + vo[2]
            };

            // Get target bone position (head = bone 7, chest = bone 4)
            int boneIndex = 7; // head default
            if (plan.selected_hitbox == 2 || plan.selected_hitbox == 3)
                boneIndex = 4; // chest/stomach -> spine2
            else if (plan.selected_hitbox == 4)
                boneIndex = 1; // pelvis

            uintptr_t tgtScene = *reinterpret_cast<uintptr_t*>(target->pawn + kBaseEntity_m_pGameSceneNode);
            if (!IsValidPtr(tgtScene)) return result;

            // Try known bone array offsets
            static const uintptr_t kBoneOffsets[] = { 0x1C0, 0x1F0, 0x1E0, 0x200, 0x1D0, 0x210, 0x220, 0x230, 0x240, 0x250, 0x260 };
            float bonePos[3] = {};
            bool found = false;

            for (uintptr_t off : kBoneOffsets)
            {
                uintptr_t boneArray = *reinterpret_cast<uintptr_t*>(tgtScene + off);
                if (!IsValidPtr(boneArray)) continue;

                __try
                {
                    const float* b = reinterpret_cast<const float*>(
                        boneArray + static_cast<uintptr_t>(boneIndex) * kBoneData_stride);
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

        // Pending angle restore for silent aim (applied next frame via
        // post_frame_cleanup so the engine has one tick to sample the
        // snapped angles before we put the originals back).
        inline bool  g_silent_restore_pending = false;
        inline float g_silent_saved_pitch = 0.f;
        inline float g_silent_saved_yaw   = 0.f;

        // Non-silent fire state machine (20ms hold).
        inline int       s_fireState = 0;   // 0=idle, 1=down
        inline ULONGLONG s_downTime  = 0;

        inline bool execute(const DryRunActionPlan& plan) noexcept
        {
            if (!plan.execution_enabled || !plan.target_found || !plan.would_aim)
                return false;

            __try
            {
                HMODULE client = GetModuleHandleA("client.dll");
                if (!client) return false;
                uintptr_t clientBase = reinterpret_cast<uintptr_t>(client);

                uintptr_t localPawn = *reinterpret_cast<uintptr_t*>(clientBase + kDw_dwLocalPlayerPawn);
                if (!IsValidPtr(localPawn)) return false;

                int localHealth = *reinterpret_cast<int32_t*>(localPawn + 0x34C);
                if (localHealth <= 0) return false;

                AimTarget aim = resolve_aim_point(plan, g_state.candidates, clientBase);
                if (!aim.valid) return false;

                float* va = reinterpret_cast<float*>(clientBase + kDw_dwViewAngles);
                float curP = va[0], curY = va[1];

                if (!std::isfinite(curP) || !std::isfinite(curY))
                    return false;

                float newP = aim.pitch;
                float newY = aim.yaw;
                if (newP < -89.f) newP = -89.f;
                if (newP > 89.f)  newP = 89.f;
                newY = NormYaw(newY);

                const bool silent = plan.would_silent;

                if (silent)
                {
                    // --- SILENT AIM ---
                    // Frame N: save originals, snap angles, mouse down.
                    // Frame N+1 (post_frame_cleanup): mouse up, restore
                    // original angles. This gives the engine one full
                    // frame to sample the snapped viewangles + attack.
                    g_silent_saved_pitch = curP;
                    g_silent_saved_yaw   = curY;

                    va[0] = newP;
                    va[1] = newY;
                    va[2] = 0.f;

                    if (plan.would_fire)
                        mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);

                    g_silent_restore_pending = true;
                }
                else
                {
                    // --- NORMAL RAGE ---
                    // Hard snap (viewangles stay on target).
                    va[0] = newP;
                    va[1] = newY;
                    va[2] = 0.f;

                    if (plan.would_fire)
                    {
                        ULONGLONG now = GetTickCount64();
                        if (s_fireState == 1)
                        {
                            if (now - s_downTime >= 20)
                            {
                                mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                                s_fireState = 0;
                            }
                        }
                        else
                        {
                            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
                            s_downTime = now;
                            s_fireState = 1;
                        }
                    }
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

        // Called every frame after execute(). For silent aim: releases
        // the fire button and restores the original view angles one frame
        // after the snap, giving the engine time to sample the snapped
        // viewangles + attack for the shot command.
        inline void post_frame_cleanup(uintptr_t clientBase) noexcept
        {
            if (!g_silent_restore_pending)
                return;

            // Release fire button first (engine already sampled the click).
            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);

            __try
            {
                float* va = reinterpret_cast<float*>(clientBase + kDw_dwViewAngles);
                if (std::isfinite(g_silent_saved_pitch) && std::isfinite(g_silent_saved_yaw))
                {
                    va[0] = g_silent_saved_pitch;
                    va[1] = g_silent_saved_yaw;
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {}

            g_silent_restore_pending = false;
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
                "[P6LIVE] EXECUTED aim=1 fire=%d target=%d hc=%.0f dmg=%.0f silent=%d pen=%d bt=%d",
                plan.would_fire ? 1 : 0,
                plan.target_id,
                plan.hitchance,
                plan.predicted_damage,
                plan.would_silent ? 1 : 0,
                plan.would_use_penetration ? 1 : 0,
                plan.would_use_backtrack ? 1 : 0);
            FileLog::Log(buf);
        }
    } // namespace Execution
} // namespace RageDryRun
