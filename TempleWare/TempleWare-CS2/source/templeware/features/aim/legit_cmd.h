#pragma once

// CUserCmd-based legit aimbot + triggerbot + RCS execution.
// Called from CreateMove instead of Present/FrameStageNotify.
// Replaces raw viewangle memory writes and mouse_event() calls.

#include "../../interfaces/CUserCmd/CUserCmd.h"
#include "../../interfaces/CCSGOInput/CCSGOInput.h"
#include "../../interfaces/interfaces.h"
#include "../../globals/globals.h"
#include "../../utils/filelog/filelog.h"
#include "../../../esp/esp.h"

#include <cmath>
#include <cstdint>

namespace LegitCmd
{
    inline constexpr uintptr_t kGameSceneNode = 0x330;
    inline constexpr uintptr_t kVecAbsOrigin = 0xC8;
    inline constexpr uintptr_t kVecOrigin = 0x80;
    inline constexpr uintptr_t kViewOffset = 0xE78;
    inline constexpr uintptr_t kTeamNum = 0x3E7;
    inline constexpr uintptr_t kHealth = 0x34C;
    inline constexpr uintptr_t kShotsFired = 0x1C8C;
    inline constexpr uintptr_t kAimPunchServices = 0x14B8;
    inline constexpr uintptr_t kIDEntIndex = 0x342C;

    inline constexpr uintptr_t kBoneStride = 0x20;
    inline constexpr int B_HEAD = 7;
    inline constexpr int B_NECK = 6;
    inline constexpr int B_SPINE2 = 4;
    inline constexpr int B_SPINE1 = 3;
    inline constexpr int B_PELVIS = 1;
    inline constexpr int B_SHOULDER_L = 9;
    inline constexpr int B_SHOULDER_R = 13;
    inline constexpr int B_KNEE_L = 18;
    inline constexpr int B_KNEE_R = 21;

    inline bool IsValidPtr(uintptr_t p) { return p >= 0x10000 && p < 0x0000FFFFFFFFFFFFull; }

    inline float NormYaw(float y) { while (y > 180.f) y -= 360.f; while (y < -180.f) y += 360.f; return y; }

    inline void CalcAngle(float sx, float sy, float sz, float dx, float dy, float dz, float& pitch, float& yaw)
    {
        float ddx = dx - sx, ddy = dy - sy, ddz = dz - sz;
        float len = std::sqrt(ddx * ddx + ddy * ddy);
        pitch = -std::atan2(ddz, len) * 57.2957795f;
        yaw = std::atan2(ddy, ddx) * 57.2957795f;
    }

    inline int HitboxBone(int s)
    {
        switch (s) {
        case 0: return B_HEAD;
        case 1: return B_NECK;
        case 2: return B_SPINE2;
        case 3: return B_SPINE1;
        case 4: return B_PELVIS;
        case 5: return B_SHOULDER_L;
        case 6: return B_SHOULDER_R;
        case 7: return B_KNEE_L;
        default: return B_KNEE_R;
        }
    }

    inline bool GetBonePos(uintptr_t pawn, int boneIdx, float* out)
    {
        uintptr_t scene = *reinterpret_cast<uintptr_t*>(pawn + kGameSceneNode);
        if (!IsValidPtr(scene)) return false;

        static const uintptr_t kBoneOffsets[] = { 0x1C0, 0x1F0, 0x1E0, 0x200, 0x1D0 };
        for (uintptr_t off : kBoneOffsets)
        {
            uintptr_t arr = *reinterpret_cast<uintptr_t*>(scene + off);
            if (!IsValidPtr(arr)) continue;
            __try {
                const float* b = reinterpret_cast<const float*>(arr + static_cast<uintptr_t>(boneIdx) * kBoneStride);
                if (std::isfinite(b[0]) && std::isfinite(b[1]) && std::isfinite(b[2]) &&
                    std::fabs(b[0]) < 32768.f && std::fabs(b[1]) < 32768.f) {
                    out[0] = b[0]; out[1] = b[1]; out[2] = b[2];
                    return true;
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
        return false;
    }

    // RCS: aim punch via pattern-scanned game function (matches esp.cpp)
    using fnGetAimPunch_t = void(__fastcall*)(uintptr_t, float*, unsigned int);
    inline fnGetAimPunch_t g_getAimPunch = nullptr;
    inline bool g_aimPunchResolved = false;

    inline void ResolveAimPunch()
    {
        if (g_aimPunchResolved) return;
        __try {
            auto p = M::scan("client.dll", "14 00 00 48 8D 54 24 20 E8 ? ? ? ?");
            if (p) {
                int32_t rel = *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(p) + 9);
                g_getAimPunch = reinterpret_cast<fnGetAimPunch_t>(reinterpret_cast<uintptr_t>(p) + 13 + rel);
                g_aimPunchResolved = true;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    inline bool GetAimPunch(uintptr_t pawn, float& px, float& py)
    {
        ResolveAimPunch();
        if (!g_getAimPunch) return false;
        uintptr_t svc = *reinterpret_cast<uintptr_t*>(pawn + kAimPunchServices);
        if (!IsValidPtr(svc)) return false;
        float o[3] = { 0, 0, 0 };
        __try { g_getAimPunch(svc, o, 0u); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        if (std::isfinite(o[0]) && std::isfinite(o[1])) { px = o[0]; py = o[1]; return true; }
        return false;
    }

    // Entity list: resolve CGameEntitySystem** global address via pattern (matches esp.cpp)
    // We cache the GLOBAL ADDRESS (stable in .data), NOT the pointer value —
    // the value changes on level transitions when the entity system is recreated.
    inline uintptr_t g_entitySystemGlobal = 0;
    inline bool g_entitySystemResolved = false;
    inline constexpr uintptr_t kEntityListOffset = 0x10;
    inline constexpr uintptr_t kEntityEntryStride = 0x70;
    inline constexpr uintptr_t kEntityEntryEntity = 0x00;
    inline constexpr uintptr_t kEntityEntryHandle = 0x10;

    inline void ResolveEntitySystem()
    {
        if (g_entitySystemResolved) return;
        __try {
            auto p = M::scan("client.dll", "48 8B 0D ? ? ? ? 48 89 7C 24 ? 8B FA C1 EB");
            if (p) {
                uintptr_t addr = reinterpret_cast<uintptr_t>(p);
                int32_t rel = *reinterpret_cast<int32_t*>(addr + 3);
                g_entitySystemGlobal = addr + 7 + rel;
                g_entitySystemResolved = true;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    inline uintptr_t GetEntity(int index)
    {
        if (!g_entitySystemGlobal) { ResolveEntitySystem(); }
        if (!g_entitySystemGlobal || index < 0 || index > 64) return 0;

        __try {
            uintptr_t entitySystem = *reinterpret_cast<uintptr_t*>(g_entitySystemGlobal);
            if (!IsValidPtr(entitySystem)) return 0;

            int chunk = index >> 9;
            if (chunk > 0x3F) return 0;

            uintptr_t chunkPtr = *reinterpret_cast<uintptr_t*>(
                entitySystem + kEntityListOffset + static_cast<uintptr_t>(chunk) * 8);
            if (!IsValidPtr(chunkPtr)) return 0;

            uintptr_t entry = chunkPtr + static_cast<uintptr_t>(index & 0x1FF) * kEntityEntryStride;
            uint32_t handle = *reinterpret_cast<uint32_t*>(entry + kEntityEntryHandle);
            if ((handle & 0x7FFF) != static_cast<uint32_t>(index)) return 0;

            uintptr_t ent = *reinterpret_cast<uintptr_t*>(entry + kEntityEntryEntity);
            return IsValidPtr(ent) ? ent : 0;
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
        return 0;
    }

    // Main: called from CreateMove ONLY when RuntimeGate is READY.
    // g_ctx->local_pawn and g_ctx->local_controller are guaranteed
    // valid and stable by the gate — no redundant engine/SDK checks needed.
    inline void OnCreateMove(CUserCmd* cmd, CCSGOInput* input) noexcept
    {
        auto& ab = Esp::g_aimbot;
        auto& tg = Esp::g_trigger;

        if (!ab.enable && !ab.rcs && !tg.enable)
            return;

        if (!cmd || !input) return;

        // Pawn is gate-validated, just cast
        uintptr_t localPawn = reinterpret_cast<uintptr_t>(g_ctx->local_pawn);
        if (!IsValidPtr(localPawn)) return;

        auto* base = cmd->csgoUserCmd.mutable_base();
        if (!base) return;

        float curP = base->viewangles().x();
        float curY = base->viewangles().y();
        if (!std::isfinite(curP) || !std::isfinite(curY)) return;

        int localTeam = 0;
        int shots = 0;
        __try {
            localTeam = *reinterpret_cast<uint8_t*>(localPawn + kTeamNum);
            shots = *reinterpret_cast<int32_t*>(localPawn + kShotsFired);
        } __except (EXCEPTION_EXECUTE_HANDLER) { return; }
        float punchX = 0.f, punchY = 0.f;
        bool hasPunch = ab.rcs && shots > 1 && GetAimPunch(localPawn, punchX, punchY);
        static float lastPunchX = 0.f, lastPunchY = 0.f;
        float rxf = ab.rcsX / 100.f, ryf = ab.rcsY / 100.f;

        // --- Aimbot activation ---
        bool aimActive = false;
        if (ab.enable)
        {
            if (ab.aimType == 2) aimActive = true;
            else if (ab.aimKey != 0) {
                bool down = (GetAsyncKeyState(ab.aimKey) & 0x8000) != 0;
                if (ab.aimType == 1) {
                    static bool prev = false, tog = false;
                    if (down && !prev) tog = !tog;
                    prev = down; aimActive = tog;
                } else aimActive = down;
            }
        }

        bool aimApplied = false;
        if (aimActive)
        {
            // Eye position
            uintptr_t scene = *reinterpret_cast<uintptr_t*>(localPawn + kGameSceneNode);
            if (IsValidPtr(scene))
            {
                const float* lo = reinterpret_cast<const float*>(scene + kVecOrigin);
                const float* vo = reinterpret_cast<const float*>(localPawn + kViewOffset);
                float ex = lo[0] + vo[0], ey = lo[1] + vo[1], ez = lo[2] + vo[2];

                int boneId = HitboxBone(ab.hitbox);
                float bestFov = ab.fov > 0.f ? ab.fov : 1.f;
                bool found = false;
                float tp = 0.f, ty = 0.f;

                for (int i = 1; i <= 64; ++i)
                {
                    uintptr_t ent = GetEntity(i);
                    if (!ent || ent == localPawn) continue;
                    int team = *reinterpret_cast<uint8_t*>(ent + kTeamNum);
                    if ((team != 2 && team != 3) || team == localTeam) continue;
                    int hp = *reinterpret_cast<int32_t*>(ent + kHealth);
                    if (hp <= 0 || hp > 1000) continue;

                    float bone[3];
                    if (!GetBonePos(ent, boneId, bone)) continue;
                    if (bone[0] == 0.f && bone[1] == 0.f && bone[2] == 0.f) continue;

                    float ap, ay;
                    CalcAngle(ex, ey, ez, bone[0], bone[1], bone[2], ap, ay);
                    float fp = ap - curP, fy = NormYaw(ay - curY);
                    float fov = std::sqrt(fp * fp + fy * fy);
                    if (fov <= bestFov) { bestFov = fov; tp = ap; ty = ay; found = true; }
                }

                if (found)
                {
                    if (hasPunch) { tp -= punchX * ryf; ty -= punchY * rxf; }
                    float frac = 1.f - ab.smooth;
                    if (frac < 0.04f) frac = 0.04f;
                    if (frac > 1.f) frac = 1.f;
                    float dp = tp - curP, dy = NormYaw(ty - curY);
                    float np = curP + dp * frac;
                    float ny = NormYaw(curY + dy * frac);
                    if (np < -89.f) np = -89.f;
                    if (np > 89.f)  np = 89.f;

                    // Write to CUserCmd viewangles (tick-synced)
                    base->mutable_viewangles()->set_x(np);
                    base->mutable_viewangles()->set_y(ny);
                    base->mutable_viewangles()->set_z(0.f);

                    // Legit aim is always visual (non-silent)
                    Vector_t vis = { np, ny, 0.f };
                    input->SetViewAngle(vis);

                    aimApplied = true;
                }
            }
        }
        else if (hasPunch && ab.standaloneRcs)
        {
            // Standalone RCS via CUserCmd
            float dpx = punchX - lastPunchX, dpy = punchY - lastPunchY;
            float np = curP - dpx * ryf;
            float ny = NormYaw(curY - dpy * rxf);
            if (np < -89.f) np = -89.f;
            if (np > 89.f)  np = 89.f;

            base->mutable_viewangles()->set_x(np);
            base->mutable_viewangles()->set_y(ny);
            base->mutable_viewangles()->set_z(0.f);

            Vector_t vis = { np, ny, 0.f };
            input->SetViewAngle(vis);
        }

        lastPunchX = (shots > 1) ? punchX : 0.f;
        lastPunchY = (shots > 1) ? punchY : 0.f;

        // --- Triggerbot via CUserCmd ---
        if (tg.enable && tg.key != 0 && (GetAsyncKeyState(tg.key) & 0x8000))
        {
            static ULONGLONG s_lastFire = 0;
            ULONGLONG now = GetTickCount64();
            ULONGLONG gap = static_cast<ULONGLONG>(tg.delayMs < 0 ? 0 : tg.delayMs) + 15;

            if (now - s_lastFire >= gap)
            {
                int idx = *reinterpret_cast<int32_t*>(localPawn + kIDEntIndex);
                if (idx > 0 && idx <= 0x7FFE)
                {
                    uintptr_t ent = GetEntity(idx);
                    if (ent && ent != localPawn)
                    {
                        int team = *reinterpret_cast<uint8_t*>(ent + kTeamNum);
                        if ((team == 2 || team == 3) && (!tg.teamCheck || team != localTeam))
                        {
                            int hp = *reinterpret_cast<int32_t*>(ent + kHealth);
                            if (hp > 0 && hp <= 1000)
                            {
                                cmd->nButtons.nValue |= IN_ATTACK;
                                cmd->nButtons.nValueChanged |= IN_ATTACK;
                                cmd->csgoUserCmd.set_attack1_start_history_index(0);
                                s_lastFire = now;
                            }
                        }
                    }
                }
            }
        }
    }

} // namespace LegitCmd
