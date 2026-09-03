// Minimal, self-contained box ESP for CS2 (manual-map skeleton).
// No game hooks, no class-name string matching: players are identified by
// team + health + controller handle using verified offsets only.
#include "esp.h"

#include <windows.h>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>

#include "../../external/imgui/imgui.h"
#include "../chams/chams.h"
#include "../trace/trace.h"
#include "../icons/icons.h"
#include "../templeware/rage/rage_live_providers.h"

// ---------------------------------------------------------------------------
// Verified offsets (CS2 build 14178, cross-checked against the Antigravity
// schema dump and the GetBaseEntity disassembly).
// ---------------------------------------------------------------------------

// CGameEntitySystem entity list (chunk array) + entry stride.
// GetBaseEntity(client.dll): lea r9,[rcx+0x10]; chunk=[r9+i*8];
// entry = chunk + (i&0x1FF)*0x70; return entry[0]; handle=entry[0x10].
constexpr uintptr_t kEntityListOffset   = 0x10;   // CGameEntitySystem::m_entityList
constexpr uintptr_t kEntityEntryStride  = 0x70;   // 112 bytes per slot (disasm-verified)
constexpr uintptr_t kEntityEntryEntity  = 0x00;   // entry[0]  = CEntityInstance*
constexpr uintptr_t kEntityEntryHandle  = 0x10;   // entry[0x10] = uint32 handle (index|serial<<15)

// C_BaseEntity (inherited by C_CSPlayerPawn)
constexpr uintptr_t kBaseEntity_m_pGameSceneNode = 0x330; // -> CGameSceneNode*
constexpr uintptr_t kBaseEntity_m_iHealth        = 0x34C; // int32
constexpr uintptr_t kBaseEntity_m_iTeamNum       = 0x3E7; // uint8

// CGameSceneNode
constexpr uintptr_t kGameSceneNode_m_vecOrigin   = 0x80;  // Vector (3 floats)

// C_BasePlayerPawn -> controller
constexpr uintptr_t kBasePlayerPawn_m_hController = 0x13D0; // CBaseHandle (uint32)

// C_CSPlayerPawn status
constexpr uintptr_t kCSPlayerPawn_m_bIsScoped   = 0x1C78; // bool
constexpr uintptr_t kCSPlayerPawn_m_bIsDefusing = 0x1C7A; // bool
constexpr uintptr_t kCSPlayerPawnBase_m_flFlashDuration = 0x1428; // float

// Spotted state (lightweight visibility proxy: set when spotted by the local team)
constexpr uintptr_t kCSPlayerPawn_m_entitySpottedState = 0x1C60; // EntitySpottedState_t
constexpr uintptr_t kSpottedState_m_bSpotted           = 0x08;   // bool
constexpr uintptr_t kSpottedState_m_bSpottedByMask     = 0x0C;   // uint32[2] (bit = spotter slot)

// Collision bounds (for the 3D box)
constexpr uintptr_t kBaseEntity_m_pCollision = 0x340; // CCollisionProperty*
constexpr uintptr_t kCollision_m_vecMins     = 0x40;  // Vector (local AABB min)
constexpr uintptr_t kCollision_m_vecMaxs     = 0x4C;  // Vector (local AABB max)

// Entity identity (for designer-name based identification, e.g. planted_c4)
constexpr uintptr_t kBaseEntity_m_pEntity      = 0x10;  // CEntityIdentity*
constexpr uintptr_t kIdentity_m_designerName   = 0x20;  // const char* (CUtlSymbolLarge)
constexpr uintptr_t kBaseEntity_m_flSimulationTime = 0x3B8; // float (server "now")
constexpr uintptr_t kBaseEntity_m_vecVelocity      = 0x430; // Vector (units/s)

// Player extras
constexpr uintptr_t kCSPlayerPawn_m_ArmorValue = 0x1CA4; // int32
constexpr uintptr_t kBasePlayerPawn_m_pItemServices = 0x1210; // CPlayer_ItemServices*
constexpr uintptr_t kItemServices_m_bHasDefuser = 0x48;  // bool
constexpr uintptr_t kController_m_pInGameMoneyServices = 0x810; // ptr
constexpr uintptr_t kMoneyServices_m_iAccount  = 0x40;   // int32
constexpr uintptr_t kController_m_iPing         = 0x830;  // uint32
constexpr uintptr_t kBaseEntity_m_hOwnerEntity  = 0x520;  // CHandle (dropped weapon = invalid)
constexpr uintptr_t kBaseModelEntity_m_vecViewOffset = 0xE78; // eye offset (Vector)

// client.dll globals (RVA, from the schema dump — build-specific like the offsets)
constexpr uintptr_t kDw_dwLocalPlayerPawn = 0x23C6268;
constexpr uintptr_t kDw_dwViewAngles      = 0x23DC2F8;

constexpr uintptr_t kCSPlayerPawn_m_iShotsFired        = 0x1C8C; // int32
constexpr uintptr_t kBasePlayerPawn_m_pAimPunchServices = 0x14B8; // ptr
constexpr uintptr_t kCSPlayerPawn_m_iIDEntIndex        = 0x342C; // int32 (crosshair entity)
constexpr uintptr_t kCSPlayerPawnBase_m_flFlashMaxAlpha = 0x1424; // float
constexpr uintptr_t kController_m_iDesiredFOV           = 0x78C;  // uint32
constexpr uintptr_t kDw_dwLocalPlayerController         = 0x23A0F30;
constexpr uintptr_t kBaseModelEntity_m_clrRender       = 0xC98;  // Color (render tint/alpha)
constexpr uintptr_t kBaseEntity_m_vecAbsVelocity       = 0x3F8;  // Vector
constexpr uintptr_t kInferno_m_firePositions           = 0x1020; // VectorWS[64] (stride 0x18)
constexpr uintptr_t kInferno_m_fireCount               = 0x1960; // int32
constexpr uintptr_t kInferno_fireStride                = 0x18;
constexpr uintptr_t kBaseEntity_m_fFlags               = 0x3F4;  // uint32 (bit0 = on ground)
constexpr uintptr_t kBaseCSGrenade_m_flThrowStrength   = 0x1CF0; // float (0..1, set by throw type)
// Skins (econ)
constexpr uintptr_t kEconEntity_m_AttributeManager     = 0x11A8;
constexpr uintptr_t kAttributeContainer_m_Item         = 0x50;
constexpr uintptr_t kEconItemView_m_iItemIDHigh        = 0x1D0;
constexpr uintptr_t kEconItemView_m_iAccountID         = 0x1D8;
constexpr uintptr_t kEconEntity_m_nFallbackPaintKit    = 0x1680;
constexpr uintptr_t kEconEntity_m_nFallbackSeed        = 0x1684;
constexpr uintptr_t kEconEntity_m_flFallbackWear       = 0x1688;
constexpr uintptr_t kBasePlayerPawn_m_pObserverServices = 0x1220; // ptr
constexpr uintptr_t kObserverServices_m_iObserverMode   = 0x48;   // uint8
constexpr uintptr_t kObserverServices_m_hObserverTarget = 0x4C;   // CHandle

// Planted C4
constexpr uintptr_t kC4_m_bBombTicking  = 0x11A0; // bool
constexpr uintptr_t kC4_m_nBombSite     = 0x11A4; // int32
constexpr uintptr_t kC4_m_flC4Blow      = 0x11D0; // GameTime_t
constexpr uintptr_t kC4_m_bBeingDefused = 0x11DC; // bool
constexpr uintptr_t kC4_m_bBombDefused  = 0x11F4; // bool

// Glow (CGlowProperty embedded in C_BaseModelEntity::m_Glow)
constexpr uintptr_t kBaseModelEntity_m_Glow      = 0xDE0; // CGlowProperty (subobject)
constexpr uintptr_t kGlow_m_iGlowType            = 0x30;  // int32
constexpr uintptr_t kGlow_m_iGlowTeam            = 0x34;  // int32
constexpr uintptr_t kGlow_m_nGlowRange           = 0x38;  // int32
constexpr uintptr_t kGlow_m_glowColorOverride    = 0x40;  // Color (RGBA uint32)
constexpr uintptr_t kGlow_m_bGlowing             = 0x51;  // bool

// Weapon info
constexpr uintptr_t kBasePlayerPawn_m_pWeaponServices  = 0x1208; // -> CPlayer_WeaponServices*
constexpr uintptr_t kPlayerWeaponServices_m_hActiveWeapon = 0x60; // CBaseHandle
constexpr uintptr_t kBaseEntity_m_nSubclassID          = 0x380; // int32 (vdata ptr at +0x8)
constexpr uintptr_t kWeaponBaseVData_m_szName          = 0x720; // const char*
constexpr uintptr_t kBasePlayerWeapon_m_iClip1         = 0x1700; // int32 (current clip, on weapon entity)
constexpr uintptr_t kWeaponBaseVData_m_iMaxClip1       = 0x4D0;  // int32 (max clip, on weapon vdata)

// Skeleton (CSkeletonInstance is the game scene node for player pawns)
// The bone array is the well-known runtime pointer at gameSceneNode + 0x1F0,
// i.e. 0xB0 inside m_modelState (0x140). CModelState has no schema-named bone
// count, so we do not read one: our fixed bone IDs top out at 22 and every real
// player skeleton has ~30 bones, so iterating them is always in range.
constexpr uintptr_t kSkeletonInstance_m_modelState   = 0x140; // C_ModelState
constexpr uintptr_t kModelState_boneArray            = 0xB0;  // -> CBoneStateData* (sceneNode + 0x1F0)
constexpr uintptr_t kBoneData_stride                 = 0x20;  // { pos(12) scale(4) rot(16) }

// CBasePlayerController
constexpr uintptr_t kController_m_bIsLocalPlayerController = 0x788; // bool

// CCSPlayerController (for the optional name display)
constexpr uintptr_t kCCSPlayerController_m_sSanitizedPlayerName = 0x868; // CUtlString (char* first)

namespace
{
    struct Vec2 { float x = 0.f, y = 0.f; };
    struct Vec3 { float x = 0.f, y = 0.f, z = 0.f; };

    struct ViewMatrix { float m[4][4]; };

    uintptr_t g_entitySystem = 0;   // CGameEntitySystem*
    const ViewMatrix* g_viewMatrix = nullptr;

    Esp::Stats g_stats;

    static void LogLine(const char* msg)
    {
        wchar_t path[MAX_PATH] = {};
        GetTempPathW(MAX_PATH, path);
        wcscat_s(path, MAX_PATH, L"TempleWare.log");
        FILE* f = nullptr;
        if (_wfopen_s(&f, path, L"a") == 0 && f)
        {
            fprintf(f, "[esp] %s\n", msg);
            fclose(f);
        }
    }

    bool IsValidPtr(uintptr_t p)
    {
        return p >= 0x10000 && p < 0x0000FFFFFFFFFFFFull;
    }

    static ULONGLONG g_lastSkelDiag = 0;
    static void SkeletonDiag(const char* msg)
    {
        const ULONGLONG now = GetTickCount64();
        if (now - g_lastSkelDiag < 1500)
            return;
        g_lastSkelDiag = now;
        LogLine(msg);
    }

    // Simple IDA-style pattern scanner (space-separated hex, '?' wildcard).
    uintptr_t FindPattern(const char* moduleName, const std::string& pattern)
    {
        HMODULE mod = GetModuleHandleA(moduleName);
        if (!mod)
            return 0;

        const auto dos = reinterpret_cast<PIMAGE_DOS_HEADER>(mod);
        if (!dos || dos->e_magic != IMAGE_DOS_SIGNATURE)
            return 0;

        const auto nt = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<uint8_t*>(mod) + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE)
            return 0;

        const uint8_t* base = reinterpret_cast<const uint8_t*>(mod);
        const size_t size = nt->OptionalHeader.SizeOfImage;

        std::vector<uint8_t> bytes;
        std::vector<bool> mask;
        bytes.reserve(pattern.size() / 3 + 1);
        mask.reserve(pattern.size() / 3 + 1);

        size_t i = 0;
        while (i < pattern.size())
        {
            if (pattern[i] == ' ')
            {
                ++i;
                continue;
            }
            if (pattern[i] == '?')
            {
                bytes.push_back(0);
                mask.push_back(false);
                if (i + 1 < pattern.size() && pattern[i + 1] == '?') ++i;
                ++i;
                continue;
            }
            if (i + 1 < pattern.size())
            {
                char buf[3] = { pattern[i], pattern[i + 1], 0 };
                bytes.push_back(static_cast<uint8_t>(strtoul(buf, nullptr, 16)));
                mask.push_back(true);
                i += 2;
                continue;
            }
            ++i;
        }

        const size_t n = bytes.size();
        if (n == 0 || n > size)
            return 0;

        for (size_t off = 0; off <= size - n; ++off)
        {
            bool ok = true;
            for (size_t j = 0; j < n; ++j)
            {
                if (mask[j] && base[off + j] != bytes[j])
                {
                    ok = false;
                    break;
                }
            }
            if (ok)
                return reinterpret_cast<uintptr_t>(base + off);
        }
        return 0;
    }

    // Resolve a RIP-relative 32-bit displacement at `instr + dispOffset`.
    uintptr_t ResolveRip(uintptr_t instr, int dispOffset)
    {
        const int32_t disp = *reinterpret_cast<int32_t*>(instr + dispOffset);
        return instr + dispOffset + 4 + disp;
    }

    // Retrieve a CEntityInstance* from the entity list for a given index.
    uintptr_t GetEntity(int index)
    {
        if (!g_entitySystem || index < 0 || index > 0x7FFE)
            return 0;

        const int chunk = index >> 9;
        if (chunk > 0x3F)
            return 0;

        const uintptr_t chunkPtr = *reinterpret_cast<uintptr_t*>(g_entitySystem + kEntityListOffset + static_cast<uintptr_t>(chunk) * 8);
        if (!IsValidPtr(chunkPtr))
            return 0;

        const uintptr_t entry = chunkPtr + static_cast<uintptr_t>(index & 0x1FF) * kEntityEntryStride;
        const uint32_t handle = *reinterpret_cast<uint32_t*>(entry + kEntityEntryHandle);
        if ((handle & 0x7FFF) != static_cast<uint32_t>(index))
            return 0;

        const uintptr_t ent = *reinterpret_cast<uintptr_t*>(entry + kEntityEntryEntity);
        return IsValidPtr(ent) ? ent : 0;
    }

    // Entity designer name (e.g. "planted_c4", "cs_player_controller").
    const char* GetDesignerName(uintptr_t ent)
    {
        const uintptr_t id = *reinterpret_cast<uintptr_t*>(ent + kBaseEntity_m_pEntity);
        if (!IsValidPtr(id)) return nullptr;
        const char* n = *reinterpret_cast<const char* const*>(id + kIdentity_m_designerName);
        if (!IsValidPtr(reinterpret_cast<uintptr_t>(n))) return nullptr;
        return n;
    }

    // Classify a weapon short-name into a group: 0 pistol 1 smg 2 rifle 3 shotgun 4 sniper 5 utility
    int WeaponGroup(const char* n)
    {
        auto has = [n](const char* s) { return std::strstr(n, s) != nullptr; };
        if (has("awp") || has("ssg08") || has("scar20") || has("g3sg1")) return 4;
        if (has("nova") || has("xm1014") || has("mag7") || has("sawedoff")) return 3;
        if (has("mp9") || has("mac10") || has("mp7") || has("mp5") || has("ump45") || has("p90") || has("bizon")) return 1;
        if (has("ak47") || has("m4a1") || has("aug") || has("sg556") || has("sg553") || has("galil") || has("famas")) return 2;
        if (has("grenade") || has("flashbang") || has("molotov") || has("incgrenade") || has("decoy") ||
            has("knife") || has("bayonet") || has("taser") || has("c4") || has("healthshot") || has("smoke")) return 5;
        return 0; // pistols and the rest
    }

    bool GetOrigin(uintptr_t pawn, Vec3& out)
    {
        const uintptr_t sceneNode = *reinterpret_cast<uintptr_t*>(pawn + kBaseEntity_m_pGameSceneNode);
        if (!IsValidPtr(sceneNode))
            return false;

        const float* o = reinterpret_cast<const float*>(sceneNode + kGameSceneNode_m_vecOrigin);
        out = Vec3{ o[0], o[1], o[2] };
        return std::isfinite(out.x) && std::isfinite(out.y) && std::isfinite(out.z);
    }

    // Resolve the active weapon's short name (e.g. "ak47") into `out`.
    bool GetWeaponName(uintptr_t pawn, char* out, size_t cap)
    {
        const uintptr_t ws = *reinterpret_cast<uintptr_t*>(pawn + kBasePlayerPawn_m_pWeaponServices);
        if (!IsValidPtr(ws))
            return false;

        const uint32_t hActive = *reinterpret_cast<uint32_t*>(ws + kPlayerWeaponServices_m_hActiveWeapon);
        const uintptr_t weapon = GetEntity(static_cast<int>(hActive & 0x7FFF));
        if (!weapon)
            return false;

        const uintptr_t vdata = *reinterpret_cast<uintptr_t*>(weapon + kBaseEntity_m_nSubclassID + 0x8);
        if (!IsValidPtr(vdata))
            return false;

        const char* name = *reinterpret_cast<const char* const*>(vdata + kWeaponBaseVData_m_szName);
        if (!IsValidPtr(reinterpret_cast<uintptr_t>(name)))
            return false;

        if (std::strncmp(name, "weapon_", 7) == 0)
            name += 7;

        std::snprintf(out, cap, "%s", name);
        return out[0] != 0;
    }

    // Read the active weapon's current/max clip ammo. Returns false if unavailable
    // (e.g. knife/grenade with no magazine).
    bool GetAmmo(uintptr_t pawn, int& clip, int& maxClip)
    {
        const uintptr_t ws = *reinterpret_cast<uintptr_t*>(pawn + kBasePlayerPawn_m_pWeaponServices);
        if (!IsValidPtr(ws))
            return false;

        const uint32_t hActive = *reinterpret_cast<uint32_t*>(ws + kPlayerWeaponServices_m_hActiveWeapon);
        const uintptr_t weapon = GetEntity(static_cast<int>(hActive & 0x7FFF));
        if (!weapon)
            return false;

        const uintptr_t vdata = *reinterpret_cast<uintptr_t*>(weapon + kBaseEntity_m_nSubclassID + 0x8);
        if (!IsValidPtr(vdata))
            return false;

        const int mc = *reinterpret_cast<const int32_t*>(vdata + kWeaponBaseVData_m_iMaxClip1);
        if (mc <= 0 || mc > 250)   // no magazine (knife/grenade) or garbage
            return false;

        int c = *reinterpret_cast<const int32_t*>(weapon + kBasePlayerWeapon_m_iClip1);
        if (c < 0)  c = 0;
        if (c > mc) c = mc;

        clip = c;
        maxClip = mc;
        return true;
    }

    // Skeleton bone IDs (indices into the CS2 skeleton bone cache).
    enum : int
    {
        B_PELVIS = 1, B_SPINE0 = 2, B_SPINE1 = 3, B_SPINE2 = 4,
        B_NECK = 6, B_HEAD = 7,
        B_SHOULDER_L = 9, B_ELBOW_L = 10, B_HAND_L = 11,
        B_SHOULDER_R = 13, B_ELBOW_R = 14, B_HAND_R = 15,
        B_HIP_L = 17, B_KNEE_L = 18, B_FOOT_L = 19,
        B_HIP_R = 20, B_KNEE_R = 21, B_FOOT_R = 22,
        B_MAX = 23
    };

    struct BoneLink { int a; int b; };
    constexpr BoneLink kBoneLinks[] = {
        { B_HEAD, B_NECK }, { B_NECK, B_SPINE2 }, { B_SPINE2, B_SPINE1 }, { B_SPINE1, B_SPINE0 }, { B_SPINE0, B_PELVIS },
        { B_NECK, B_SHOULDER_L }, { B_SHOULDER_L, B_ELBOW_L }, { B_ELBOW_L, B_HAND_L },
        { B_NECK, B_SHOULDER_R }, { B_SHOULDER_R, B_ELBOW_R }, { B_ELBOW_R, B_HAND_R },
        { B_PELVIS, B_HIP_L }, { B_HIP_L, B_KNEE_L }, { B_KNEE_L, B_FOOT_L },
        { B_PELVIS, B_HIP_R }, { B_HIP_R, B_KNEE_R }, { B_KNEE_R, B_FOOT_R },
    };

    // The bone-array pointer offset inside the game scene node varies between CS2
    // builds, so we auto-resolve it once by scanning a few known candidates and
    // keeping the one that yields bones clustered around the player's origin.
    uintptr_t g_boneArrayOffset = 0;

    // Read the bone array at `cache`, writing world positions into `bones[1..B_MAX-1]`.
    // SEH-guarded because a wrong candidate pointer can land on unmapped memory.
    // Returns true only when enough bones sit near `origin` (i.e. it's the real array).
    static bool TryReadBoneArray(uintptr_t cache, Vec3* bones, const Vec3& origin, int& validOut)
    {
        int valid = 0;
        int nearOrigin = 0;
        __try
        {
            for (int i = 1; i < B_MAX; ++i)
            {
                const float* b = reinterpret_cast<const float*>(cache + static_cast<uintptr_t>(i) * kBoneData_stride);
                const Vec3 pos{ b[0], b[1], b[2] };
                if (!std::isfinite(pos.x) || !std::isfinite(pos.y) || !std::isfinite(pos.z) ||
                    std::fabs(pos.x) > 32768.f || std::fabs(pos.y) > 32768.f || std::fabs(pos.z) > 32768.f)
                    continue;
                bones[i] = pos;
                ++valid;

                const float dx = pos.x - origin.x;
                const float dy = pos.y - origin.y;
                const float dz = pos.z - origin.z;
                if (std::fabs(dx) < 150.f && std::fabs(dy) < 150.f && dz > -40.f && dz < 110.f)
                    ++nearOrigin;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            validOut = 0;
            return false;
        }
        validOut = valid;
        return nearOrigin >= 6;
    }

    // Fill `bones[1..B_MAX-1]` with world-space bone positions (valid entries only).
    // Returns false if the bone cache is unavailable.
    bool GetBonePositions(uintptr_t pawn, Vec3* bones, const Vec3& origin)
    {
        const uintptr_t sceneNode = *reinterpret_cast<uintptr_t*>(pawn + kBaseEntity_m_pGameSceneNode);
        if (!IsValidPtr(sceneNode))
        {
            SkeletonDiag("skel: sceneNode invalid");
            return false;
        }

        // Candidate bone-array pointer offsets inside the scene node, most likely first.
        static const uintptr_t kCandidates[] = {
            0x1F0, 0x1E0, 0x200, 0x1D0, 0x210, 0x220, 0x230, 0x240, 0x1C0, 0x250, 0x260,
        };

        auto tryOffset = [&](uintptr_t off, int& valid) -> bool
        {
            const uintptr_t cache = *reinterpret_cast<uintptr_t*>(sceneNode + off);
            if (!IsValidPtr(cache))
                return false;
            return TryReadBoneArray(cache, bones, origin, valid);
        };

        // Fast path: reuse the previously resolved offset.
        if (g_boneArrayOffset != 0)
        {
            int valid = 0;
            if (tryOffset(g_boneArrayOffset, valid))
                return true;
            g_boneArrayOffset = 0; // stopped matching -> re-resolve below
        }

        for (uintptr_t off : kCandidates)
        {
            for (int i = 0; i < B_MAX; ++i)
                bones[i] = Vec3{};

            int valid = 0;
            if (tryOffset(off, valid))
            {
                g_boneArrayOffset = off;
                char b[96];
                std::snprintf(b, sizeof(b), "skel: resolved bone array off=0x%llX valid=%d",
                    static_cast<unsigned long long>(off), valid);
                LogLine(b);
                return true;
            }
        }

        SkeletonDiag("skel: no candidate bone-array offset matched");
        return false;
    }

    static ULONGLONG g_lastColDiag = 0;
    static void CollisionDiag(const char* msg)
    {
        const ULONGLONG now = GetTickCount64();
        if (now - g_lastColDiag < 1500)
            return;
        g_lastColDiag = now;
        LogLine(msg);
    }

    // Read the entity's local-space collision AABB (relative to origin).
    bool GetCollisionBounds(uintptr_t pawn, Vec3& mins, Vec3& maxs)
    {
        const uintptr_t col = *reinterpret_cast<uintptr_t*>(pawn + kBaseEntity_m_pCollision);
        if (!IsValidPtr(col))
        {
            CollisionDiag("col: pointer invalid");
            return false;
        }

        const float* mn = reinterpret_cast<const float*>(col + kCollision_m_vecMins);
        const float* mx = reinterpret_cast<const float*>(col + kCollision_m_vecMaxs);
        mins = Vec3{ mn[0], mn[1], mn[2] };
        maxs = Vec3{ mx[0], mx[1], mx[2] };

        const float dx = maxs.x - mins.x;
        const float dz = maxs.z - mins.z;

        if (!std::isfinite(dx) || !std::isfinite(dz) || dx <= 0.f || dz <= 0.f || dx > 200.f || dz > 200.f)
        {
            char b[192];
            std::snprintf(b, sizeof(b), "col: bad bounds mins(%.1f,%.1f,%.1f) maxs(%.1f,%.1f,%.1f)",
                mins.x, mins.y, mins.z, maxs.x, maxs.y, maxs.z);
            CollisionDiag(b);
            return false;   // garbage or not a player-sized hull
        }
        return true;
    }

    // Enable glow on an entity by writing its embedded CGlowProperty (memory-write,
    // no game function). Written each frame; when disabled the game reverts it.
    void ApplyGlow(uintptr_t pawn, const float col[4])
    {
        const uintptr_t glow = pawn + kBaseModelEntity_m_Glow;

        auto to255 = [](float f) -> uint32_t
        {
            if (f < 0.f) f = 0.f;
            if (f > 1.f) f = 1.f;
            return static_cast<uint32_t>(f * 255.f + 0.5f);
        };
        const uint32_t rgba = to255(col[0]) | (to255(col[1]) << 8) | (to255(col[2]) << 16) | (to255(col[3]) << 24);

        *reinterpret_cast<int32_t*>(glow + kGlow_m_iGlowType) = 3;
        *reinterpret_cast<int32_t*>(glow + kGlow_m_iGlowTeam) = -1;
        *reinterpret_cast<int32_t*>(glow + kGlow_m_nGlowRange) = 0;      // 0 = no range limit
        *reinterpret_cast<uint32_t*>(glow + kGlow_m_glowColorOverride) = rgba;
        *reinterpret_cast<bool*>(glow + kGlow_m_bGlowing) = true;
    }

    // Draw a weapon icon (with a black outline) centred at (cx, topY). Returns
    // the display height used, or 0 if the icon was unavailable.
    float DrawIcon(ImDrawList* dl, const char* name, float cx, float topY, float dispH, ImU32 col)
    {
        if (!Icons::Ready()) return 0.f;
        int iw = 0, ih = 0;
        void* srv = Icons::Get(name, 0.5f, &iw, &ih);
        if (!srv || ih <= 0) return 0.f;
        const float dispW = dispH * (static_cast<float>(iw) / static_cast<float>(ih));
        const ImVec2 a(cx - dispW * 0.5f, topY), b(cx + dispW * 0.5f, topY + dispH);
        const ImTextureID tex = (ImTextureID)srv;
        const ImU32 outline = IM_COL32(0, 0, 0, 200);
        dl->AddImage(tex, ImVec2(a.x + 1, a.y), ImVec2(b.x + 1, b.y), ImVec2(0, 0), ImVec2(1, 1), outline);
        dl->AddImage(tex, ImVec2(a.x - 1, a.y), ImVec2(b.x - 1, b.y), ImVec2(0, 0), ImVec2(1, 1), outline);
        dl->AddImage(tex, ImVec2(a.x, a.y + 1), ImVec2(b.x, b.y + 1), ImVec2(0, 0), ImVec2(1, 1), outline);
        dl->AddImage(tex, ImVec2(a.x, a.y - 1), ImVec2(b.x, b.y - 1), ImVec2(0, 0), ImVec2(1, 1), outline);
        dl->AddImage(tex, a, b, ImVec2(0, 0), ImVec2(1, 1), col);
        return dispH;
    }

    // ---- RCS: aim punch getter (called on CCSPlayer_AimPunchServices) ----
    using fnGetAimPunch = void(__fastcall*)(uintptr_t, float*, unsigned int);
    fnGetAimPunch g_getAimPunch = nullptr;
    bool g_aimPunchTried = false;
    void ResolveAimPunch()
    {
        if (g_aimPunchTried) return;
        g_aimPunchTried = true;
        // pattern: ...14 00 00  48 8D 54 24 20  E8 <rel32> ...  (call get_aim_punch)
        const uintptr_t p = FindPattern("client.dll", "14 00 00 48 8D 54 24 20 E8 ? ? ? ?");
        if (p)
        {
            const int32_t rel = *reinterpret_cast<const int32_t*>(p + 9);
            g_getAimPunch = reinterpret_cast<fnGetAimPunch>(p + 13 + rel);
        }
    }
    __declspec(noinline) bool GetAimPunch(uintptr_t pawn, Vec3& out)
    {
        ResolveAimPunch();
        if (!g_getAimPunch) return false;
        const uintptr_t svc = *reinterpret_cast<uintptr_t*>(pawn + kBasePlayerPawn_m_pAimPunchServices);
        if (!IsValidPtr(svc)) return false;
        float o[3] = { 0, 0, 0 };
        __try { g_getAimPunch(svc, o, 0u); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        out = Vec3{ o[0], o[1], o[2] };
        return true;
    }

    // ---- Aim math ----
    void CalcAngle(const Vec3& src, const Vec3& dst, float& pitch, float& yaw)
    {
        const float dx = dst.x - src.x, dy = dst.y - src.y, dz = dst.z - src.z;
        const float len = std::sqrt(dx * dx + dy * dy);
        pitch = -std::atan2(dz, len) * 57.2957795f;
        yaw = std::atan2(dy, dx) * 57.2957795f;
    }
    float NormYaw(float y) { while (y > 180.f) y -= 360.f; while (y < -180.f) y += 360.f; return y; }

    // Map the hitbox widget selection (0..8) to an engine bone index.
    int HitboxBone(int s)
    {
        switch (s)
        {
        case 0: return B_HEAD;       // 7
        case 1: return B_NECK;       // 6
        case 2: return B_SPINE2;     // 4  (chest)
        case 3: return B_SPINE1;     // 3  (stomach)
        case 4: return B_PELVIS;     // 1
        case 5: return B_SHOULDER_L; // 9
        case 6: return B_SHOULDER_R; // 13
        case 7: return B_KNEE_L;     // 18
        default:return B_KNEE_R;     // 21
        }
    }

    bool WorldToScreen(const Vec3& pos, Vec2& out, const ViewMatrix& vm)
    {
        const float w = vm.m[3][0] * pos.x + vm.m[3][1] * pos.y + vm.m[3][2] * pos.z + vm.m[3][3];
        if (!(w > 0.001f))
            return false;

        const float x = vm.m[0][0] * pos.x + vm.m[0][1] * pos.y + vm.m[0][2] * pos.z + vm.m[0][3];
        const float y = vm.m[1][0] * pos.x + vm.m[1][1] * pos.y + vm.m[1][2] * pos.z + vm.m[1][3];

        const ImVec2 sz = ImGui::GetIO().DisplaySize;
        const float cx = sz.x * 0.5f;
        const float cy = sz.y * 0.5f;
        const float invW = 1.0f / w;

        out.x = cx + (x * invW * cx);
        out.y = cy - (y * invW * cy);

        return std::isfinite(out.x) && std::isfinite(out.y);
    }
}

namespace Esp
{
    bool Initialize()
    {
        if (g_entitySystem && g_viewMatrix)
            return true;

        const uintptr_t clientBase = reinterpret_cast<uintptr_t>(GetModuleHandleA("client.dll"));
        if (!clientBase)
            return false;

        if (!g_entitySystem)
        {
            const uintptr_t pat = FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 89 7C 24 ? 8B FA C1 EB");
            if (pat)
            {
                const uintptr_t global = ResolveRip(pat, 3);
                g_entitySystem = *reinterpret_cast<uintptr_t*>(global);
            }
        }

        if (!g_viewMatrix)
        {
            const uintptr_t pat = FindPattern("client.dll", "48 8D 0D ? ? ? ? 48 C1 E0 06");
            if (pat)
                g_viewMatrix = reinterpret_cast<const ViewMatrix*>(ResolveRip(pat, 3));
        }

        if (g_entitySystem && g_viewMatrix)
            LogLine("init ok");
        return g_entitySystem && g_viewMatrix;
    }

    const Stats& GetStats()
    {
        return g_stats;
    }

    uintptr_t LookupEntity(int index)
    {
        return GetEntity(index);
    }

    // HUD overlay (pure ImGui): aimbot FOV circle + watermark + custom crosshair + debug multipoints.
    void DrawOverlay()
    {
        const bool hasDebugMP = !RageDryRun::g_state.debug_points.empty();
        if (!g_config.watermark && !g_config.crosshair && !g_aimbot.drawFov && !hasDebugMP)
            return;

        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        const ImVec2 disp = ImGui::GetIO().DisplaySize;

        if (g_aimbot.drawFov && disp.x > 1.f && disp.y > 1.f)
        {
            // map the angular FOV (deg) to a screen radius via the vertical FOV.
            const float hfov = g_config.fovChanger ? static_cast<float>(g_config.fovValue) : 90.f;
            const float aspect = disp.x / disp.y;
            const float vfov = 2.f * std::atan(std::tan(hfov * 0.5f * 0.01745329f) / aspect) * 57.2957795f;
            float radius = (vfov > 1.f) ? (g_aimbot.fov / vfov * disp.y) : (g_aimbot.fov * 10.f);
            if (radius < 2.f) radius = 2.f;
            const ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(g_aimbot.fovColor[0], g_aimbot.fovColor[1], g_aimbot.fovColor[2], g_aimbot.fovColor[3]));
            dl->AddCircle(ImVec2(disp.x * 0.5f, disp.y * 0.5f), radius, col, 64, 1.4f);
        }

        if (g_config.crosshair)
        {
            const float cx = disp.x * 0.5f, cy = disp.y * 0.5f;
            const float sz = g_config.crosshairSize, gap = g_config.crosshairGap, th = g_config.crosshairThickness;
            const ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(g_config.crosshairColor[0], g_config.crosshairColor[1], g_config.crosshairColor[2], g_config.crosshairColor[3]));
            const ImU32 oc = IM_COL32(0, 0, 0, 200);
            // outline then color, 4 arms
            auto line = [&](ImVec2 a, ImVec2 b) { dl->AddLine(ImVec2(a.x + 1, a.y + 1), ImVec2(b.x + 1, b.y + 1), oc, th); dl->AddLine(a, b, col, th); };
            line(ImVec2(cx - gap - sz, cy), ImVec2(cx - gap, cy));
            line(ImVec2(cx + gap, cy), ImVec2(cx + gap + sz, cy));
            line(ImVec2(cx, cy - gap - sz), ImVec2(cx, cy - gap));
            line(ImVec2(cx, cy + gap), ImVec2(cx, cy + gap + sz));
            if (g_config.crosshairDot) dl->AddCircleFilled(ImVec2(cx, cy), th, col, 8);
        }

        if (g_config.watermark)
        {
            char buf[96];
            const int fps = static_cast<int>(ImGui::GetIO().Framerate + 0.5f);
            SYSTEMTIME t; GetLocalTime(&t);
            std::snprintf(buf, sizeof(buf), "NEXUS  |  %d fps  |  %02d:%02d", fps, t.wHour, t.wMinute);
            const ImVec2 ts = ImGui::CalcTextSize(buf);
            const float pad = 8.f;
            const float x = disp.x - ts.x - pad * 2.f - 12.f, y = 12.f;
            dl->AddRectFilled(ImVec2(x, y), ImVec2(x + ts.x + pad * 2.f, y + ts.y + pad * 1.5f), IM_COL32(15, 15, 18, 220), 6.f);
            dl->AddRect(ImVec2(x, y), ImVec2(x + ts.x + pad * 2.f, y + ts.y + pad * 1.5f), IM_COL32(40, 40, 47, 255), 6.f);
            const ImU32 wc = ImGui::ColorConvertFloat4ToU32(ImVec4(g_config.watermarkColor[0], g_config.watermarkColor[1], g_config.watermarkColor[2], g_config.watermarkColor[3]));
            dl->AddText(ImVec2(x + pad, y + pad * 0.75f), wc, buf);
        }

        // Debug multipoints: draw scan points on screen (color-coded by hitbox region)
        if (hasDebugMP && g_viewMatrix)
        {
            const auto& rs = RageDryRun::g_state;
            // Find the target candidate's pawn origin for absolute positioning
            Vec3 targetOrigin{};
            bool haveOrigin = false;
            if (rs.action.target_found && rs.action.target_id >= 0)
            {
                for (const auto& c : rs.candidates)
                {
                    if (c.candidate_id == rs.action.target_id && c.pawn && IsValidPtr(c.pawn))
                    {
                        __try
                        {
                            uintptr_t sn = *reinterpret_cast<uintptr_t*>(c.pawn + 0x330);
                            if (IsValidPtr(sn))
                            {
                                const float* o = reinterpret_cast<const float*>(sn + 0x80);
                                if (std::isfinite(o[0]) && std::isfinite(o[1]) && std::isfinite(o[2]))
                                {
                                    targetOrigin = { o[0], o[1], o[2] };
                                    haveOrigin = true;
                                }
                            }
                        }
                        __except (EXCEPTION_EXECUTE_HANDLER) {}
                        break;
                    }
                }
            }

            if (haveOrigin)
            {
                for (const auto& dp : rs.debug_points)
                {
                    Vec3 world = { targetOrigin.x + dp.world.x,
                                   targetOrigin.y + dp.world.y,
                                   targetOrigin.z + dp.world.z };
                    Vec2 scr{};
                    if (!WorldToScreen(world, scr, *g_viewMatrix))
                        continue;

                    ImU32 col;
                    switch (dp.hitbox_index)
                    {
                    case 0:  col = IM_COL32(255, 80,  80,  dp.is_center ? 255 : 160); break; // head
                    case 1:  col = IM_COL32(255, 160, 60,  dp.is_center ? 255 : 160); break; // neck
                    case 2:  col = IM_COL32(255, 160, 60,  dp.is_center ? 255 : 160); break; // chest
                    case 3:  col = IM_COL32(220, 220, 60,  dp.is_center ? 255 : 160); break; // stomach
                    case 4:  col = IM_COL32(220, 220, 60,  dp.is_center ? 255 : 160); break; // pelvis
                    case 5:
                    case 6:  col = IM_COL32(180, 80,  255, dp.is_center ? 255 : 160); break; // arms
                    case 7:
                    case 8:  col = IM_COL32(80,  160, 255, dp.is_center ? 255 : 160); break; // legs
                    default: col = IM_COL32(200, 200, 200, dp.is_center ? 255 : 160); break;
                    }
                    float radius = dp.is_center ? 3.5f : 2.0f;
                    dl->AddCircleFilled(ImVec2(scr.x, scr.y), radius, col);
                }
            }
        }
    }

    // Legit aimbot: pure memory. On aim key, pick the enemy hitbox bone closest
    // to the crosshair within FOV, then smoothly write the view angles.
    void UpdateAim()
    {
        if (!g_aimbot.enable && !g_aimbot.rcs)
            return;
        if (!Initialize())
            return;

        const uintptr_t client = reinterpret_cast<uintptr_t>(GetModuleHandleA("client.dll"));
        if (!client)
            return;

        const uintptr_t localPawn = *reinterpret_cast<uintptr_t*>(client + kDw_dwLocalPlayerPawn);
        if (!IsValidPtr(localPawn))
            return;

        float* va = reinterpret_cast<float*>(client + kDw_dwViewAngles);
        const float curP = va[0], curY = va[1];
        if (!std::isfinite(curP) || !std::isfinite(curY))
            return;

        // shots + aim punch (for RCS).
        const int shots = *reinterpret_cast<const int32_t*>(localPawn + kCSPlayerPawn_m_iShotsFired);
        Vec3 punch{ 0, 0, 0 };
        const bool haveP = g_aimbot.rcs && shots > 1 && GetAimPunch(localPawn, punch);
        static Vec3 lastPunch{ 0, 0, 0 };
        const float rx = g_aimbot.rcsX / 100.f, rcyf = g_aimbot.rcsY / 100.f;

        // aimbot activation: 0 Hold, 1 Toggle, 2 Always
        bool aimActive = false;
        if (g_aimbot.enable)
        {
            if (g_aimbot.aimType == 2) aimActive = true;
            else if (g_aimbot.aimKey != 0)
            {
                const bool down = (GetAsyncKeyState(g_aimbot.aimKey) & 0x8000) != 0;
                if (g_aimbot.aimType == 1)
                {
                    static bool prev = false, tog = false;
                    if (down && !prev) tog = !tog;
                    prev = down; aimActive = tog;
                }
                else aimActive = down;
            }
        }

        if (aimActive)
        {
            const int localTeam = *reinterpret_cast<const uint8_t*>(localPawn + kBaseEntity_m_iTeamNum);
            Vec3 lorg;
            if (GetOrigin(localPawn, lorg))
            {
                const float* vo = reinterpret_cast<const float*>(localPawn + kBaseModelEntity_m_vecViewOffset);
                const Vec3 eye{ lorg.x + vo[0], lorg.y + vo[1], lorg.z + vo[2] };

                const int boneId = HitboxBone(g_aimbot.hitbox);
                float bestFov = g_aimbot.fov; if (bestFov <= 0.f) bestFov = 1.f;
                bool found = false; float tp = 0.f, ty = 0.f;

                for (int i = 0; i <= 0x7FFE; ++i)
                {
                    const uintptr_t ent = GetEntity(i);
                    if (!ent || ent == localPawn) continue;
                    const int team = *reinterpret_cast<const uint8_t*>(ent + kBaseEntity_m_iTeamNum);
                    if ((team != 2 && team != 3) || team == localTeam) continue;
                    const int hp = *reinterpret_cast<const int32_t*>(ent + kBaseEntity_m_iHealth);
                    if (hp <= 0 || hp > 1000) continue;

                    Vec3 org;
                    if (!GetOrigin(ent, org)) continue;
                    Vec3 bones[B_MAX]{};
                    if (!GetBonePositions(ent, bones, org)) continue;
                    const Vec3 tgt = bones[boneId];
                    if (tgt.x == 0.f && tgt.y == 0.f && tgt.z == 0.f) continue;

                    float ap, ay; CalcAngle(eye, tgt, ap, ay);
                    const float fp = ap - curP, fy = NormYaw(ay - curY);
                    const float fov = std::sqrt(fp * fp + fy * fy);
                    if (fov <= bestFov) { bestFov = fov; tp = ap; ty = ay; found = true; }
                }

                if (found)
                {
                    if (haveP) { tp -= punch.x * rcyf; ty -= punch.y * rx; }
                    float frac = 1.f - g_aimbot.smooth;
                    if (frac < 0.04f) frac = 0.04f; if (frac > 1.f) frac = 1.f;
                    const float dp = tp - curP, dy = NormYaw(ty - curY);
                    float np = curP + dp * frac;
                    float ny = NormYaw(curY + dy * frac);
                    if (np < -89.f) np = -89.f; if (np > 89.f) np = 89.f;
                    va[0] = np; va[1] = ny; va[2] = 0.f;
                }
            }
        }
        else if (haveP)
        {
            // standalone RCS: compensate the frame-to-frame punch delta.
            const float dpx = punch.x - lastPunch.x, dpy = punch.y - lastPunch.y;
            float np = curP - dpx * rcyf;
            float ny = NormYaw(curY - dpy * rx);
            if (np < -89.f) np = -89.f; if (np > 89.f) np = 89.f;
            va[0] = np; va[1] = ny; va[2] = 0.f;
        }

        lastPunch = (shots > 1) ? punch : Vec3{ 0, 0, 0 };
    }

    // Basic skin changer: apply a fallback paint kit to weapons owned by the
    // local player (m_iItemIDHigh=-1 forces the fallback fields to be used).
    void UpdateSkins()
    {
        if (!g_config.skinsEnable || g_config.skinPaintKit == 0)
            return;
        if (!Initialize())
            return;

        const uintptr_t client = reinterpret_cast<uintptr_t>(GetModuleHandleA("client.dll"));
        if (!client) return;
        const uintptr_t localPawn = *reinterpret_cast<uintptr_t*>(client + kDw_dwLocalPlayerPawn);
        if (!IsValidPtr(localPawn)) return;

        for (int i = 0; i <= 0x7FFE; ++i)
        {
            const uintptr_t ent = GetEntity(i);
            if (!ent) continue;
            const char* dn = GetDesignerName(ent);
            if (!dn || std::strncmp(dn, "weapon_", 7) != 0) continue;

            const uint32_t ownerH = *reinterpret_cast<const uint32_t*>(ent + kBaseEntity_m_hOwnerEntity);
            if (GetEntity(static_cast<int>(ownerH & 0x7FFF)) != localPawn) continue;   // ours only

            const uintptr_t itemView = ent + kEconEntity_m_AttributeManager + kAttributeContainer_m_Item;
            *reinterpret_cast<int32_t*>(itemView + kEconItemView_m_iItemIDHigh) = -1;   // force fallback
            uint32_t& acc = *reinterpret_cast<uint32_t*>(itemView + kEconItemView_m_iAccountID);
            if (acc == 0) acc = 0x1;   // must be non-zero for the skin to apply

            *reinterpret_cast<int32_t*>(ent + kEconEntity_m_nFallbackPaintKit) = g_config.skinPaintKit;
            *reinterpret_cast<int32_t*>(ent + kEconEntity_m_nFallbackSeed) = g_config.skinSeed;
            *reinterpret_cast<float*>(ent + kEconEntity_m_flFallbackWear) = g_config.skinWear;
        }
    }

    // Misc memory-write QoL: anti-flash + FOV changer.
    void UpdateMisc()
    {
        if (!g_config.antiFlash && !g_config.fovChanger && !g_config.localOpacity)
            return;

        const uintptr_t client = reinterpret_cast<uintptr_t>(GetModuleHandleA("client.dll"));
        if (!client) return;

        const uintptr_t localPawn = *reinterpret_cast<uintptr_t*>(client + kDw_dwLocalPlayerPawn);

        if (g_config.antiFlash && IsValidPtr(localPawn))
            *reinterpret_cast<float*>(localPawn + kCSPlayerPawnBase_m_flFlashMaxAlpha) = 0.f;

        if (g_config.localOpacity && IsValidPtr(localPawn))
        {
            bool apply = true;
            if (g_config.localOnlyScoped)
                apply = *reinterpret_cast<const bool*>(localPawn + kCSPlayerPawn_m_bIsScoped);
            float ov = g_config.localOpacityVal; if (ov < 0.f) ov = 0.f; if (ov > 1.f) ov = 1.f;
            const uint8_t a = apply ? static_cast<uint8_t>(ov * 255.f) : 255;
            *reinterpret_cast<uint32_t*>(localPawn + kBaseModelEntity_m_clrRender) =
                0x00FFFFFFu | (static_cast<uint32_t>(a) << 24);
        }
        if (g_config.fovChanger)
        {
            const uintptr_t ctrl = *reinterpret_cast<uintptr_t*>(client + kDw_dwLocalPlayerController);
            if (IsValidPtr(ctrl))
            {
                int fv = g_config.fovValue; if (fv < 60) fv = 60; if (fv > 160) fv = 160;
                *reinterpret_cast<uint32_t*>(ctrl + kController_m_iDesiredFOV) = static_cast<uint32_t>(fv);
            }
        }
    }

    // Triggerbot: while the trigger key is held, fire (input-level click) when the
    // crosshair entity is an enemy. A small down->up state machine makes each
    // click register and keeps firing at successive targets. No usercmd writes.
    void UpdateTrigger()
    {
        static int st = 0;                 // 0 idle, 1 button-down
        static ULONGLONG tDown = 0, tUp = 0;
        const ULONGLONG now = GetTickCount64();

        // Always finish an in-progress click (release after a short hold), even
        // if the key was released — so the mouse never gets stuck down.
        if (st == 1)
        {
            if (now - tDown >= 20) { mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0); tUp = now; st = 0; }
            return;
        }

        if (!g_trigger.enable || g_trigger.key == 0)
            return;
        if (!(GetAsyncKeyState(g_trigger.key) & 0x8000))
            return;
        if (!Initialize())
            return;

        const uintptr_t client = reinterpret_cast<uintptr_t>(GetModuleHandleA("client.dll"));
        if (!client) return;
        const uintptr_t localPawn = *reinterpret_cast<uintptr_t*>(client + kDw_dwLocalPlayerPawn);
        if (!IsValidPtr(localPawn)) return;

        const int localTeam = *reinterpret_cast<const uint8_t*>(localPawn + kBaseEntity_m_iTeamNum);
        const int idx = *reinterpret_cast<const int32_t*>(localPawn + kCSPlayerPawn_m_iIDEntIndex);
        if (idx <= 0 || idx > 0x7FFE) return;

        const uintptr_t ent = GetEntity(idx);
        if (!ent || ent == localPawn) return;
        const int team = *reinterpret_cast<const uint8_t*>(ent + kBaseEntity_m_iTeamNum);
        if (team != 2 && team != 3) return;
        if (g_trigger.teamCheck && team == localTeam) return;
        const int hp = *reinterpret_cast<const int32_t*>(ent + kBaseEntity_m_iHealth);
        if (hp <= 0 || hp > 1000) return;

        // gap between shots = user delay + a small refractory period
        const ULONGLONG gap = static_cast<ULONGLONG>(g_trigger.delayMs < 0 ? 0 : g_trigger.delayMs) + 25;
        if (now - tUp < gap) return;

        mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
        tDown = now; st = 1;
    }

    void Draw()
    {
        g_stats = Stats{};

        if (!Initialize())
        {
            g_stats.entitySystemReady = g_entitySystem != 0;
            g_stats.viewMatrixReady = g_viewMatrix != nullptr;
            return;
        }
        g_stats.entitySystemReady = true;
        g_stats.viewMatrixReady = true;

        if (!g_config.enabled)
            return;

        const ImVec2 screenSize = ImGui::GetIO().DisplaySize;
        if (screenSize.x < 1.f || screenSize.y < 1.f)
            return;

        struct Player
        {
            uintptr_t pawn;
            uintptr_t controller;
            Vec3 origin;
            int team;
            int health;
            bool scoped;
            bool defusing;
            bool flashed;
            bool spottedAny;        // spotted by anyone (fallback)
            uint32_t spottedMask[2];// per-slot spotter bitmask
            bool hasDefuser;
            int  armor;
            int  money;
            int  ping;
            char weapon[48];
            int  ammo;
            int  maxAmmo;
        };

        std::vector<Player> players;
        players.reserve(64);

        int localTeam = 0;
        bool localFound = false;
        Vec3 localOrigin{};
        int localSlot = -1;   // local controller entity index - 1 (spotted-mask bit)
        uintptr_t localPawn = 0;
        int localPawnIndex = -1;

        // Pass 1: collect alive players (team 2/3, health > 0) and find local.
        for (int i = 0; i <= 0x7FFE; ++i)
        {
            const uintptr_t ent = GetEntity(i);
            if (!ent)
                continue;
            ++g_stats.entitiesScanned;

            const int team = *reinterpret_cast<const uint8_t*>(ent + kBaseEntity_m_iTeamNum);
            if (team != 2 && team != 3)
                continue;

            const int health = *reinterpret_cast<const int32_t*>(ent + kBaseEntity_m_iHealth);
            if (health <= 0 || health > 1000)
                continue;

            Vec3 origin;
            if (!GetOrigin(ent, origin))
                continue;

            const uint32_t hController = *reinterpret_cast<const uint32_t*>(ent + kBasePlayerPawn_m_hController);
            const uintptr_t controller = GetEntity(static_cast<int>(hController & 0x7FFF));

            Player p{};
            p.pawn = ent;
            p.controller = controller;
            p.origin = origin;
            p.team = team;
            p.health = health;
            p.scoped = *reinterpret_cast<const bool*>(ent + kCSPlayerPawn_m_bIsScoped);
            p.defusing = *reinterpret_cast<const bool*>(ent + kCSPlayerPawn_m_bIsDefusing);
            p.flashed = *reinterpret_cast<const float*>(ent + kCSPlayerPawnBase_m_flFlashDuration) > 0.0f;
            {
                const uintptr_t ss = ent + kCSPlayerPawn_m_entitySpottedState;
                p.spottedAny = *reinterpret_cast<const bool*>(ss + kSpottedState_m_bSpotted);
                p.spottedMask[0] = *reinterpret_cast<const uint32_t*>(ss + kSpottedState_m_bSpottedByMask);
                p.spottedMask[1] = *reinterpret_cast<const uint32_t*>(ss + kSpottedState_m_bSpottedByMask + 4);
            }
            p.weapon[0] = 0;
            GetWeaponName(ent, p.weapon, sizeof(p.weapon));
            p.ammo = 0;
            p.maxAmmo = 0;
            GetAmmo(ent, p.ammo, p.maxAmmo);

            p.armor = *reinterpret_cast<const int32_t*>(ent + kCSPlayerPawn_m_ArmorValue);
            if (p.armor < 0 || p.armor > 100) p.armor = 0;
            p.hasDefuser = false;
            {
                const uintptr_t itemSvc = *reinterpret_cast<uintptr_t*>(ent + kBasePlayerPawn_m_pItemServices);
                if (IsValidPtr(itemSvc))
                    p.hasDefuser = *reinterpret_cast<const bool*>(itemSvc + kItemServices_m_bHasDefuser);
            }
            p.money = 0;
            p.ping = 0;
            if (controller)
            {
                const uintptr_t ms = *reinterpret_cast<uintptr_t*>(controller + kController_m_pInGameMoneyServices);
                if (IsValidPtr(ms))
                    p.money = *reinterpret_cast<const int32_t*>(ms + kMoneyServices_m_iAccount);
                p.ping = *reinterpret_cast<const uint32_t*>(controller + kController_m_iPing);
                if (p.ping < 0 || p.ping > 9999) p.ping = 0;
            }

            players.push_back(p);
            ++g_stats.playersFound;

            if (controller)
            {
                const bool isLocal = *reinterpret_cast<const bool*>(controller + kController_m_bIsLocalPlayerController);
                if (isLocal)
                {
                    localFound = true;
                    localTeam = team;
                    localOrigin = origin;
                    localPawn = ent;
                    localPawnIndex = i;
                    localSlot = static_cast<int>(hController & 0x7FFF) - 1; // entindex - 1
                }
            }
        }

        g_stats.localFound = localFound;
        g_stats.localTeam = localTeam;

        // Publish full live data into the rage pipeline.
        if (localFound)
        {
            auto& live = RageDryRun::Live::g_live;
            const uintptr_t clientBase = reinterpret_cast<uintptr_t>(GetModuleHandleA("client.dll"));

            // --- Combat Frame ---
            {
                const float* vo = reinterpret_cast<const float*>(localPawn + kBaseModelEntity_m_vecViewOffset);
                const float* vel = reinterpret_cast<const float*>(localPawn + kBaseEntity_m_vecVelocity);
                const float* va = reinterpret_cast<const float*>(clientBase + kDw_dwViewAngles);
                const uint32_t flags = *reinterpret_cast<const uint32_t*>(localPawn + kBaseEntity_m_fFlags);

                live.frame.generation = live.generation + 1;
                live.frame.local_pawn = localPawn;
                live.frame.local_controller = 0;
                live.frame.origin = RageDryRun::Vec3{ localOrigin.x, localOrigin.y, localOrigin.z };
                live.frame.eye_position = RageDryRun::Vec3{
                    localOrigin.x + vo[0], localOrigin.y + vo[1], localOrigin.z + vo[2] };
                live.frame.velocity = RageDryRun::Vec3{ vel[0], vel[1], vel[2] };
                live.frame.view_pitch = va[0];
                live.frame.view_yaw = va[1];
                live.frame.on_ground = (flags & 1) != 0;
                live.frame.scoped = *reinterpret_cast<const bool*>(localPawn + kCSPlayerPawn_m_bIsScoped);
                live.frame.tick = GetTickCount();
                live.frame.time = static_cast<float>(live.frame.tick) / 1000.f;
                live.frame.ready = true;
                live.frame_ready = true;
            }

            // --- Prediction ---
            {
                const float* vel = reinterpret_cast<const float*>(localPawn + kBaseEntity_m_vecVelocity);
                const uint32_t flags = *reinterpret_cast<const uint32_t*>(localPawn + kBaseEntity_m_fFlags);
                live.prediction.generation = live.generation + 1;
                live.prediction.origin = live.frame.origin;
                live.prediction.velocity = RageDryRun::Vec3{ vel[0], vel[1], vel[2] };
                live.prediction.speed_2d = std::sqrt(vel[0]*vel[0] + vel[1]*vel[1]);
                live.prediction.on_ground = (flags & 1) != 0;
                live.prediction.ready = true;
                live.prediction_ready = true;
            }

            // --- Weapon ---
            {
                live.weapon = RageDryRun::WeaponSnapshot{};
                const uintptr_t ws = *reinterpret_cast<uintptr_t*>(localPawn + kBasePlayerPawn_m_pWeaponServices);
                if (IsValidPtr(ws))
                {
                    const uint32_t hActive = *reinterpret_cast<uint32_t*>(ws + kPlayerWeaponServices_m_hActiveWeapon);
                    const uintptr_t weapon = GetEntity(static_cast<int>(hActive & 0x7FFF));
                    if (weapon)
                    {
                        live.weapon.weapon = weapon;
                        live.weapon.generation = live.generation + 1;

                        int clip = 0, maxClip = 0;
                        GetAmmo(localPawn, clip, maxClip);
                        live.weapon.ammo = clip;

                        const uintptr_t vdata = *reinterpret_cast<uintptr_t*>(weapon + kBaseEntity_m_nSubclassID + 0x8);
                        if (IsValidPtr(vdata))
                        {
                            live.weapon.range = *reinterpret_cast<float*>(vdata + 0x838);
                            live.weapon.item_definition = *reinterpret_cast<int32_t*>(vdata + 0x828);
                            live.weapon.ready = true;
                        }
                    }
                }
                live.weapon_ready = live.weapon.ready;
            }

            // --- Candidates with bones, hitboxes, visibility, FOV, distance ---
            live.candidates.clear();
            int cid = 0;
            int boneReadyCount = 0;

            const float localEyeArr[3] = {
                live.frame.eye_position.x,
                live.frame.eye_position.y,
                live.frame.eye_position.z
            };

            for (const auto& p : players)
            {
                if (p.team == localTeam) continue;

                RageDryRun::CandidateSnapshot c{};
                c.candidate_id = cid++;
                c.controller = p.controller;
                c.pawn = p.pawn;
                c.health = p.health;
                c.team = p.team;
                c.valid = true;
                c.alive = p.health > 0;
                c.enemy = true;

                // Distance
                float dx = p.origin.x - localOrigin.x;
                float dy = p.origin.y - localOrigin.y;
                float dz = p.origin.z - localOrigin.z;
                c.distance = std::sqrt(dx*dx + dy*dy + dz*dz);

                // FOV (angular distance from crosshair to target head)
                Vec3 bones[23]{};
                bool hasBones = GetBonePositions(p.pawn, bones, p.origin);
                c.bones_ready = hasBones;
                if (hasBones)
                {
                    ++boneReadyCount;
                    c.hitboxes_ready = true;
                    // Head bone for FOV calculation
                    const Vec3& head = bones[7]; // B_HEAD
                    if (head.x != 0.f || head.y != 0.f || head.z != 0.f)
                    {
                        float hdx = head.x - localEyeArr[0];
                        float hdy = head.y - localEyeArr[1];
                        float hdz = head.z - localEyeArr[2];
                        float hdist = std::sqrt(hdx*hdx + hdy*hdy);
                        float ap = -std::atan2(hdz, hdist) * 57.2957795f;
                        float ay = std::atan2(hdy, hdx) * 57.2957795f;
                        float dp = ap - live.frame.view_pitch;
                        float dyw = ay - live.frame.view_yaw;
                        while (dyw > 180.f) dyw -= 360.f;
                        while (dyw < -180.f) dyw += 360.f;
                        c.fov = std::sqrt(dp*dp + dyw*dyw);
                    }
                }

                // Visibility via trace (if trace ready)
                if (Trace::Ready() && hasBones)
                {
                    const Vec3& head = bones[7];
                    if (head.x != 0.f || head.y != 0.f || head.z != 0.f)
                    {
                        float headArr[3] = { head.x, head.y, head.z };
                        float endArr[3], normalArr[3];
                        float frac = 1.f;
                        bool blocked = Trace::Line(
                            localEyeArr, headArr,
                            localPawn, endArr, normalArr, &frac);
                        c.visibility_known = true;
                        c.visible = !blocked || frac > 0.97f;
                    }
                }

                // Lag history recording
                {
                    RageDryRun::LagRecordSnapshot lr{};
                    lr.candidate_id = c.candidate_id;
                    lr.tick = static_cast<int>(GetTickCount());
                    lr.simulation_time = *reinterpret_cast<const float*>(p.pawn + kBaseEntity_m_flSimulationTime);
                    lr.origin = RageDryRun::Vec3{ p.origin.x, p.origin.y, p.origin.z };
                    lr.valid = true;
                    if (hasBones)
                    {
                        for (int bi = 1; bi < 23 && bi < 128; ++bi)
                        {
                            lr.bones[bi].position = RageDryRun::Vec3{
                                bones[bi].x, bones[bi].y, bones[bi].z };
                            lr.bones[bi].valid = (bones[bi].x != 0.f || bones[bi].y != 0.f || bones[bi].z != 0.f);
                        }
                    }
                    live.lag_history.push_back(lr);
                    RageDryRun::Live::cap_lag_history(live.lag_history);
                }

                live.candidates.push_back(c);
            }

            live.entity_count = cid;
            live.entities_ready = cid > 0;
            live.bones_ready = boneReadyCount > 0;
            live.hitboxes_ready = boneReadyCount > 0;
            live.bone_ready_count = boneReadyCount;
            live.local_pawn = localPawn;
            live.local_controller = 0;
            ++live.generation;
            ++live.publishes;
            RageDryRun::Live::g_enabled = true;
        }

        if (!localFound)
            return;

        // Local eye position (origin + view offset) for LOS traces.
        Vec3 localEye = localOrigin;
        if (localPawn)
        {
            const float* vo = reinterpret_cast<const float*>(localPawn + kBaseModelEntity_m_vecViewOffset);
            if (std::isfinite(vo[2]))
                localEye = Vec3{ localOrigin.x + vo[0], localOrigin.y + vo[1], localOrigin.z + vo[2] };
        }

        // Glow (memory-write) — per scope, independent of screen projection.
        if (g_config.glow || g_config.glowTeam || g_config.glowLocal)
        {
            for (const Player& p : players)
            {
                if (!p.pawn) continue;
                if (p.pawn == localPawn) { if (g_config.glowLocal) ApplyGlow(p.pawn, g_config.glowLocalColor); }
                else if (p.team != localTeam) { if (g_config.glow) ApplyGlow(p.pawn, g_config.glowColor); }
                else { if (g_config.glowTeam) ApplyGlow(p.pawn, g_config.glowTeamColor); }
            }
        }

        ImDrawList* dl = ImGui::GetBackgroundDrawList();

        for (const Player& p : players)
        {
            if (!p.pawn)
                continue;
            const bool isEnemy = (p.team != localTeam);
            if (!isEnemy && !g_config.teamEsp)   // enemies only unless team ESP is on
                continue;

            // Box color: visibility-based or the fixed box color.
            bool visible = false;
            if (g_config.realVis && Trace::Ready() && localPawn && p.pawn != localPawn)
            {
                // Real LOS: trace eye -> enemy head bone.
                Vec3 hb[B_MAX]{};
                if (GetBonePositions(p.pawn, hb, p.origin))
                {
                    const float s[3] = { localEye.x, localEye.y, localEye.z };
                    const float e[3] = { hb[B_HEAD].x, hb[B_HEAD].y, hb[B_HEAD].z };
                    visible = Trace::IsVisible(s, e, p.pawn, localPawn);
                }
            }
            else if (localSlot >= 0 && localSlot < 64)
                visible = (p.spottedMask[localSlot >> 5] >> (localSlot & 31)) & 1u;
            else
                visible = p.spottedAny;

            const float* bc = g_config.visColor
                ? (visible ? g_config.visibleColor : g_config.occludedColor)
                : g_config.boxColor;
            const ImU32 boxColor = ImGui::ColorConvertFloat4ToU32(ImVec4(bc[0], bc[1], bc[2], bc[3]));

            Vec2 feet, head;
            const Vec3 headPos{ p.origin.x, p.origin.y, p.origin.z + 72.f };
            if (!WorldToScreen(p.origin, feet, *g_viewMatrix) ||
                !WorldToScreen(headPos, head, *g_viewMatrix))
                continue;

            const float height = feet.y - head.y;
            const float width = height * 0.5f;
            if (height < 2.f || width < 2.f)
                continue;

            const float x = (feet.x + head.x) * 0.5f - width * 0.5f;
            const float y = head.y;
            const float cxMid = x + width * 0.5f;

            // Snapline (from a screen anchor to the player)
            if (g_config.snapline)
            {
                ImVec2 from;
                if (g_config.snaplinePos == 1)      from = ImVec2(screenSize.x * 0.5f, 0.f);
                else if (g_config.snaplinePos == 2) from = ImVec2(screenSize.x * 0.5f, screenSize.y * 0.5f);
                else                                from = ImVec2(screenSize.x * 0.5f, screenSize.y);
                dl->AddLine(from, ImVec2(cxMid, y + height * 0.5f),
                    ImGui::ColorConvertFloat4ToU32(ImVec4(g_config.snaplineColor[0], g_config.snaplineColor[1], g_config.snaplineColor[2], g_config.snaplineColor[3])), 1.0f);
            }

            // Head circle — on the actual head bone, radius from head/neck span.
            if (g_config.headCircle)
            {
                Vec3 hbones[B_MAX]{};
                if (GetBonePositions(p.pawn, hbones, p.origin))
                {
                    Vec2 headS, neckS;
                    if (WorldToScreen(hbones[B_HEAD], headS, *g_viewMatrix) &&
                        WorldToScreen(hbones[B_NECK], neckS, *g_viewMatrix))
                    {
                        const float dxr = headS.x - neckS.x, dyr = headS.y - neckS.y;
                        float r = std::sqrt(dxr * dxr + dyr * dyr) * 1.15f;
                        if (r < 2.f) r = 2.f;
                        // lift the centre a touch above the bone (bone sits at the head's base)
                        dl->AddCircle(ImVec2(headS.x, headS.y - r * 0.25f), r,
                            ImGui::ColorConvertFloat4ToU32(ImVec4(g_config.headCircleColor[0], g_config.headCircleColor[1], g_config.headCircleColor[2], g_config.headCircleColor[3])), 20, 1.5f);
                    }
                }
            }

            // Skeleton (drawn behind the box)
            if (g_config.skeleton)
            {
                Vec3 bones[B_MAX]{};
                if (GetBonePositions(p.pawn, bones, p.origin))
                {
                    const float* skc = (g_config.skeletonVisColor && !visible) ? g_config.skeletonOccludedColor : g_config.skeletonColor;
                    const ImU32 skCol = ImGui::ColorConvertFloat4ToU32(ImVec4(skc[0], skc[1], skc[2], skc[3]));
                    for (const auto& link : kBoneLinks)
                    {
                        const Vec3& a = bones[link.a];
                        const Vec3& b = bones[link.b];
                        if ((a.x == 0.f && a.y == 0.f && a.z == 0.f) ||
                            (b.x == 0.f && b.y == 0.f && b.z == 0.f))
                            continue;

                        Vec2 sa, sb;
                        if (!WorldToScreen(a, sa, *g_viewMatrix) || !WorldToScreen(b, sb, *g_viewMatrix))
                            continue;

                        const float ddx = sa.x - sb.x, ddy = sa.y - sb.y;
                        if (ddx * ddx + ddy * ddy > 500.f * 500.f)
                            continue;

                        dl->AddLine(ImVec2(sa.x, sa.y), ImVec2(sb.x, sb.y), skCol, g_config.skeletonThickness);
                    }
                }
            }

            // Filled box
            if (g_config.boxFill)
            {
                ImVec4 c = ImVec4(g_config.boxColor[0], g_config.boxColor[1], g_config.boxColor[2], g_config.boxFillAlpha);
                dl->AddRectFilled(ImVec2(x, y), ImVec2(x + width, y + height), ImGui::ColorConvertFloat4ToU32(c));
            }

            // Box outline
            if (g_config.box)
            {
                const float thick = g_config.boxThickness;
                bool drew3d = false;

                if (g_config.boxType == 2)
                {
                    // 3D box: project the 8 corners of the world-space collision AABB.
                    Vec3 mn, mx;
                    if (GetCollisionBounds(p.pawn, mn, mx))
                    {
                        Vec2 s[8];
                        bool valid[8];
                        int projected = 0;
                        for (int idx = 0; idx < 8; ++idx)
                        {
                            const Vec3 w{
                                p.origin.x + ((idx & 1) ? mx.x : mn.x),
                                p.origin.y + ((idx & 2) ? mx.y : mn.y),
                                p.origin.z + ((idx & 4) ? mx.z : mn.z) };
                            valid[idx] = WorldToScreen(w, s[idx], *g_viewMatrix);
                            if (valid[idx]) ++projected;
                        }
                        if (projected >= 2)
                        {
                            static const int edges[12][2] = {
                                {0,1},{2,3},{4,5},{6,7},   // bottom/top along X
                                {0,2},{1,3},{4,6},{5,7},   // along Y
                                {0,4},{1,5},{2,6},{3,7},   // vertical along Z
                            };
                            for (const auto& e : edges)
                                if (valid[e[0]] && valid[e[1]])
                                    dl->AddLine(ImVec2(s[e[0]].x, s[e[0]].y), ImVec2(s[e[1]].x, s[e[1]].y), boxColor, thick);
                            drew3d = true;
                        }
                    }
                    // fall through to Full box if the 3D bounds were unavailable
                }

                if (drew3d)
                {
                    // done
                }
                else if (g_config.boxType == 1)
                {
                    // Corner box: draw the four L-shaped corners.
                    float frac = g_config.cornerFrac;
                    if (frac < 0.05f) frac = 0.05f;
                    if (frac > 0.5f)  frac = 0.5f;
                    const float clx = width * frac;
                    const float cly = height * frac;

                    auto corner = [&](float cx, float cy, float lenX, float lenY)
                    {
                        dl->AddLine(ImVec2(cx, cy), ImVec2(cx + lenX, cy), boxColor, thick);
                        dl->AddLine(ImVec2(cx, cy), ImVec2(cx, cy + lenY), boxColor, thick);
                    };
                    corner(x,         y,          +clx, +cly);  // top-left
                    corner(x + width, y,          -clx, +cly);  // top-right
                    corner(x,         y + height, +clx, -cly);  // bottom-left
                    corner(x + width, y + height, -clx, -cly);  // bottom-right
                }
                else
                {
                    // Full box.
                    dl->AddRect(ImVec2(x, y), ImVec2(x + width, y + height), boxColor, 0.f, 0, thick);
                }
            }

            // Ammo bar (horizontal, just below the box)
            float ammoBarSpace = 0.f;
            if (g_config.ammoBar && p.maxAmmo > 0)
            {
                const float frac = static_cast<float>(p.ammo) / static_cast<float>(p.maxAmmo);
                const float ah = 3.0f;
                const float ay = y + height + 2.0f;
                const ImU32 aCol = ImGui::ColorConvertFloat4ToU32(ImVec4(
                    g_config.ammoColor[0], g_config.ammoColor[1], g_config.ammoColor[2], g_config.ammoColor[3]));

                dl->AddRectFilled(ImVec2(x, ay), ImVec2(x + width, ay + ah), IM_COL32(20, 20, 20, 200));
                if (frac > 0.f)
                    dl->AddRectFilled(ImVec2(x, ay), ImVec2(x + width * frac, ay + ah), aCol);

                ammoBarSpace = ah + 2.0f;

                if (g_config.ammoText)
                {
                    char abuf[16]; std::snprintf(abuf, sizeof(abuf), "%d/%d", p.ammo, p.maxAmmo);
                    const ImVec2 ts = ImGui::CalcTextSize(abuf);
                    dl->AddText(ImVec2(x + width + 3.f, ay + ah * 0.5f - ts.y * 0.5f + 1.f), IM_COL32(0, 0, 0, 255), abuf);
                    dl->AddText(ImVec2(x + width + 2.f, ay + ah * 0.5f - ts.y * 0.5f), IM_COL32(230, 230, 230, 255), abuf);
                    ammoBarSpace = ah + 2.0f;
                }
            }

            // Health bar (position: 0 left, 1 top, 2 bottom)
            if (g_config.healthBar)
            {
                const float hpf = (p.health > 100 ? 100.f : static_cast<float>(p.health)) / 100.f;
                const ImU32 hpCol = IM_COL32((int)(255 * (1.f - hpf)), (int)(220 * hpf) + 20, 40, 255);

                if (g_config.healthBarPos == 0)
                {
                    const float hh = height * hpf;
                    dl->AddRectFilled(ImVec2(x - 5.f, y), ImVec2(x - 2.f, y + height), IM_COL32(20, 20, 20, 200));
                    dl->AddRectFilled(ImVec2(x - 5.f, y + height - hh), ImVec2(x - 2.f, y + height), hpCol);
                    if (g_config.healthText)
                    {
                        char hb[8]; std::snprintf(hb, sizeof(hb), "%d", p.health);
                        const ImVec2 ts = ImGui::CalcTextSize(hb);
                        float ty = y + height - hh - ts.y * 0.5f; if (ty < y) ty = y;
                        const float tx = (g_config.showArmor && p.armor > 0) ? (x - 11.f - ts.x) : (x - 7.f - ts.x);
                        dl->AddText(ImVec2(tx + 1.f, ty + 1.f), IM_COL32(0, 0, 0, 255), hb);
                        dl->AddText(ImVec2(tx, ty), IM_COL32(255, 255, 255, 255), hb);
                    }
                }
                else
                {
                    const float by = (g_config.healthBarPos == 1) ? (y - 6.f) : (y + height + 3.f);
                    dl->AddRectFilled(ImVec2(x, by), ImVec2(x + width, by + 3.f), IM_COL32(20, 20, 20, 200));
                    dl->AddRectFilled(ImVec2(x, by), ImVec2(x + width * hpf, by + 3.f), hpCol);
                }
            }

            // Armor bar (left of the health bar)
            if (g_config.showArmor && p.armor > 0)
            {
                const float av = height * (static_cast<float>(p.armor) / 100.f);
                dl->AddRectFilled(ImVec2(x - 9.f, y), ImVec2(x - 6.f, y + height), IM_COL32(20, 20, 20, 200));
                dl->AddRectFilled(ImVec2(x - 9.f, y + height - av), ImVec2(x - 6.f, y + height), IM_COL32(70, 140, 240, 255));
            }

            // Name (above the box)
            if (g_config.name && p.controller)
            {
                const char* name = *reinterpret_cast<const char* const*>(p.controller + kCCSPlayerController_m_sSanitizedPlayerName);
                if (IsValidPtr(reinterpret_cast<uintptr_t>(name)) && name[0])
                {
                    const ImVec2 ts = ImGui::CalcTextSize(name);
                    const float nx = (x + width * 0.5f) - ts.x * 0.5f;
                    dl->AddText(ImVec2(nx + 1.f, y - ts.y - 1.f), IM_COL32(0, 0, 0, 255), name);
                    dl->AddText(ImVec2(nx, y - ts.y), IM_COL32(255, 255, 255, 255), name);
                }
            }

            // Status flags (right of the box)
            if (g_config.flags)
            {
                float fx = x + width + 4.f;
                float fy = y;
                auto flagText = [&](const char* txt, ImU32 col)
                {
                    const ImVec2 ts = ImGui::CalcTextSize(txt);
                    dl->AddText(ImVec2(fx + 1.f, fy + 1.f), IM_COL32(0, 0, 0, 255), txt);
                    dl->AddText(ImVec2(fx, fy), col, txt);
                    fy += ts.y + 2.f;
                };
                if (p.scoped)   flagText("SCOPED", IM_COL32(120, 200, 255, 255));
                if (p.flashed)  flagText("BLIND",  IM_COL32(255, 220, 80, 255));
                if (p.defusing) flagText("DEFUSE", IM_COL32(255, 120, 60, 255));
                if (g_config.showDefuser && p.hasDefuser) flagText("KIT", IM_COL32(120, 255, 160, 255));
                if (g_config.showPing && p.ping > 0)
                {
                    char pbuf[16]; std::snprintf(pbuf, sizeof(pbuf), "%dms", p.ping);
                    const ImU32 pc = p.ping < 50 ? IM_COL32(120, 255, 120, 255)
                                   : p.ping < 100 ? IM_COL32(230, 230, 120, 255)
                                   : p.ping < 150 ? IM_COL32(255, 180, 90, 255)
                                                  : IM_COL32(255, 100, 100, 255);
                    flagText(pbuf, pc);
                }
            }

            // Bottom text: weapon name + distance (below the ammo bar if present)
            float bottomY = y + height + 2.f + ammoBarSpace;
            auto bottomText = [&](const char* txt, ImU32 col)
            {
                const ImVec2 ts = ImGui::CalcTextSize(txt);
                const float tx = (x + width * 0.5f) - ts.x * 0.5f;
                dl->AddText(ImVec2(tx + 1.f, bottomY + 1.f), IM_COL32(0, 0, 0, 255), txt);
                dl->AddText(ImVec2(tx, bottomY), col, txt);
                bottomY += ts.y + 1.f;
            };

            if (g_config.weapon && p.weapon[0])
            {
                const bool wantIcon = (g_config.weaponDisplay == 1 || g_config.weaponDisplay == 2);
                const bool wantText = (g_config.weaponDisplay == 0 || g_config.weaponDisplay == 2);
                if (wantIcon)
                {
                    float ih = height * 0.16f; if (ih < 14.f) ih = 14.f; if (ih > 26.f) ih = 26.f;
                    const float used = DrawIcon(dl, p.weapon, x + width * 0.5f, bottomY, ih, IM_COL32(220, 230, 255, 255));
                    if (used > 0.f) bottomY += used + 1.f;
                    else if (!wantText) bottomText(p.weapon, IM_COL32(200, 220, 255, 255)); // icon missing -> fall back
                }
                if (wantText)
                    bottomText(p.weapon, IM_COL32(200, 220, 255, 255));
            }

            if (g_config.distance)
            {
                const float dx = p.origin.x - localOrigin.x;
                const float dy = p.origin.y - localOrigin.y;
                const float dz = p.origin.z - localOrigin.z;
                const float meters = std::sqrt(dx * dx + dy * dy + dz * dz) * 0.01905f;

                char buf[32];
                std::snprintf(buf, sizeof(buf), "%.0fm", meters);
                bottomText(buf, IM_COL32(255, 255, 255, 255));
            }

            if (g_config.showMoney && p.money > 0)
            {
                char buf[24];
                std::snprintf(buf, sizeof(buf), "$%d", p.money);
                bottomText(buf, IM_COL32(120, 255, 120, 255));
            }

            ++g_stats.enemiesDrawn;
        }

        // Off-screen arrows (enemies not on screen) — around the screen center.
        if (g_config.offArrows)
        {
            const float cx = screenSize.x * 0.5f;
            const float cy = screenSize.y * 0.5f;
            const float radius = (screenSize.y < screenSize.x ? screenSize.y : screenSize.x) * g_config.offArrowRadius;
            const ImU32 aCol = ImGui::ColorConvertFloat4ToU32(ImVec4(g_config.offArrowColor[0], g_config.offArrowColor[1], g_config.offArrowColor[2], g_config.offArrowColor[3]));
            const float asz = g_config.offArrowSize;
            const ViewMatrix& vm = *g_viewMatrix;

            for (const Player& p : players)
            {
                if (!p.pawn || p.team == localTeam) continue;

                const Vec3& pos = p.origin;
                float w = vm.m[3][0] * pos.x + vm.m[3][1] * pos.y + vm.m[3][2] * pos.z + vm.m[3][3];
                float sx = vm.m[0][0] * pos.x + vm.m[0][1] * pos.y + vm.m[0][2] * pos.z + vm.m[0][3];
                float sy = vm.m[1][0] * pos.x + vm.m[1][1] * pos.y + vm.m[1][2] * pos.z + vm.m[1][3];

                float ndcx = sx / w;
                float ndcy = sy / w;
                const bool onScreen = (w > 0.001f) && ndcx >= -1.f && ndcx <= 1.f && ndcy >= -1.f && ndcy <= 1.f;
                if (onScreen) continue;   // visible enemies use the box

                if (w < 0.f) { ndcx = -ndcx; ndcy = -ndcy; }

                // direction on screen (y up in ndc -> down in screen)
                float dirx = ndcx;
                float diry = -ndcy;
                const float len = std::sqrt(dirx * dirx + diry * diry);
                if (len < 0.0001f) continue;
                dirx /= len; diry /= len;

                const float ax = cx + dirx * radius;
                const float ay = cy + diry * radius;

                // triangle pointing along (dirx,diry)
                const float ang = std::atan2(diry, dirx);
                const float s = asz;
                if (g_config.offArrowGlow)
                {
                    const float gs = s * 1.7f;
                    const ImVec2 gt(ax + std::cos(ang) * gs, ay + std::sin(ang) * gs);
                    const ImVec2 gl(ax + std::cos(ang + 2.5f) * gs, ay + std::sin(ang + 2.5f) * gs);
                    const ImVec2 gr(ax + std::cos(ang - 2.5f) * gs, ay + std::sin(ang - 2.5f) * gs);
                    dl->AddTriangleFilled(gt, gl, gr, (aCol & 0x00FFFFFF) | 0x40000000);
                }
                const ImVec2 tip(ax + std::cos(ang) * s, ay + std::sin(ang) * s);
                const ImVec2 l(ax + std::cos(ang + 2.5f) * s, ay + std::sin(ang + 2.5f) * s);
                const ImVec2 r(ax + std::cos(ang - 2.5f) * s, ay + std::sin(ang - 2.5f) * s);
                dl->AddTriangleFilled(tip, l, r, aCol);
            }
        }

        // Bomb ESP (planted C4)
        if (g_config.bombEsp)
        {
            const ImU32 bcol = ImGui::ColorConvertFloat4ToU32(ImVec4(g_config.bombColor[0], g_config.bombColor[1], g_config.bombColor[2], g_config.bombColor[3]));
            float now = 0.f;
            if (localPawn) now = *reinterpret_cast<const float*>(localPawn + kBaseEntity_m_flSimulationTime);

            for (int i = 64; i <= 0x7FFE; ++i)   // planted C4 is a non-player entity
            {
                const uintptr_t ent = GetEntity(i);
                if (!ent) continue;
                const char* dn = GetDesignerName(ent);
                if (!dn || std::strcmp(dn, "planted_c4") != 0) continue;

                const bool defused = *reinterpret_cast<const bool*>(ent + kC4_m_bBombDefused);
                Vec3 bpos;
                if (!GetOrigin(ent, bpos)) continue;
                Vec2 bs;
                if (!WorldToScreen(bpos, bs, *g_viewMatrix)) break;

                const int site = *reinterpret_cast<const int32_t*>(ent + kC4_m_nBombSite);
                const bool beingDefused = *reinterpret_cast<const bool*>(ent + kC4_m_bBeingDefused);
                const float blow = *reinterpret_cast<const float*>(ent + kC4_m_flC4Blow);
                const float remain = blow - now;

                char buf[64];
                if (defused)
                    std::snprintf(buf, sizeof(buf), "C4 %c DEFUSED", site == 1 ? 'B' : 'A');
                else if (beingDefused)
                    std::snprintf(buf, sizeof(buf), "C4 %c DEFUSING (%.1fs)", site == 1 ? 'B' : 'A', remain > 0.f ? remain : 0.f);
                else
                    std::snprintf(buf, sizeof(buf), "C4 %c %.1fs", site == 1 ? 'B' : 'A', remain > 0.f ? remain : 0.f);

                const ImVec2 ts = ImGui::CalcTextSize(buf);
                dl->AddText(ImVec2(bs.x - ts.x * 0.5f + 1.f, bs.y + 1.f), IM_COL32(0, 0, 0, 255), buf);
                dl->AddText(ImVec2(bs.x - ts.x * 0.5f, bs.y), bcol, buf);
                break;   // only one planted C4
            }
        }

        // Item ESP (dropped weapons / utility on the ground) + item glow/chams feed.
        static uintptr_t itemPtrs[128];
        int itemCount = 0;
        if (g_config.itemEsp || g_config.itemGlow || g_config.itemChams)
        {
            const ImU32 icol = ImGui::ColorConvertFloat4ToU32(ImVec4(g_config.itemColor[0], g_config.itemColor[1], g_config.itemColor[2], g_config.itemColor[3]));

            for (int i = 64; i <= 0x7FFE; ++i)
            {
                const uintptr_t ent = GetEntity(i);
                if (!ent) continue;

                const char* dn = GetDesignerName(ent);
                if (!dn || std::strncmp(dn, "weapon_", 7) != 0) continue;

                // dropped only: no owner
                const uint32_t owner = *reinterpret_cast<const uint32_t*>(ent + kBaseEntity_m_hOwnerEntity);
                if ((owner & 0x7FFF) != 0x7FFF && owner != 0xFFFFFFFF && owner != 0) continue;

                const char* nm = dn + 7; // strip "weapon_"
                const int grp = WeaponGroup(nm);
                if (!g_config.itemGroup[grp]) continue;

                if (itemCount < 128) itemPtrs[itemCount++] = ent;

                if (g_config.itemGlow)
                    ApplyGlow(ent, g_config.itemGlowColor);

                if (g_config.itemEsp)
                {
                    Vec3 ipos;
                    if (!GetOrigin(ent, ipos)) continue;
                    Vec2 is;
                    if (!WorldToScreen(ipos, is, *g_viewMatrix)) continue;

                    char line[64];
                    if (g_config.itemDistance)
                    {
                        const float ddx = ipos.x - localOrigin.x, ddy = ipos.y - localOrigin.y, ddz = ipos.z - localOrigin.z;
                        const float m = std::sqrt(ddx * ddx + ddy * ddy + ddz * ddz) * 0.01905f;
                        std::snprintf(line, sizeof(line), "%s [%.0fm]", nm, m);
                    }
                    else std::snprintf(line, sizeof(line), "%s", nm);

                    float iy = is.y;
                    if (g_config.itemIcon)
                    {
                        const float used = DrawIcon(dl, nm, is.x, iy, 16.f, icol);
                        if (used > 0.f) iy += used + 1.f;
                    }
                    const ImVec2 ts = ImGui::CalcTextSize(line);
                    dl->AddText(ImVec2(is.x - ts.x * 0.5f + 1.f, iy + 1.f), IM_COL32(0, 0, 0, 255), line);
                    dl->AddText(ImVec2(is.x - ts.x * 0.5f, iy), icol, line);
                }
            }
        }

        // Grenade / projectile ESP (in-air) + Ragdoll ESP/glow/chams.
        static uintptr_t ragdollPtrs[64];
        int ragdollCount = 0;
        if (g_config.nadeEsp || g_config.ragdollEsp || g_config.ragdollGlow || g_config.ragdollChams
            || g_config.nadeTrajectory || g_config.infernoFill)
        {
            const ImU32 ncol = ImGui::ColorConvertFloat4ToU32(ImVec4(g_config.nadeColor[0], g_config.nadeColor[1], g_config.nadeColor[2], g_config.nadeColor[3]));
            const ImU32 rcol = ImGui::ColorConvertFloat4ToU32(ImVec4(g_config.ragdollColor[0], g_config.ragdollColor[1], g_config.ragdollColor[2], g_config.ragdollColor[3]));

            for (int i = 64; i <= 0x7FFE; ++i)
            {
                const uintptr_t ent = GetEntity(i);
                if (!ent) continue;
                const char* dn = GetDesignerName(ent);
                if (!dn) continue;

                const bool isProj    = std::strstr(dn, "projectile") != nullptr;
                const bool isInferno = std::strcmp(dn, "inferno") == 0;
                const bool isNade = g_config.nadeEsp && (isProj || isInferno);
                const bool isRag  = (g_config.ragdollEsp || g_config.ragdollGlow || g_config.ragdollChams) && std::strstr(dn, "ragdoll");
                const bool wantFill = g_config.infernoFill && isInferno;
                const bool wantTraj = g_config.nadeTrajectory && isProj;
                if (!isNade && !isRag && !wantFill && !wantTraj) continue;

                if (isRag)
                {
                    if (ragdollCount < 64) ragdollPtrs[ragdollCount++] = ent;
                    if (g_config.ragdollGlow) ApplyGlow(ent, g_config.ragdollGlowColor);
                }

                // Inferno fire-area fill (real fire positions).
                if (wantFill)
                {
                    const int fc = *reinterpret_cast<const int32_t*>(ent + kInferno_m_fireCount);
                    if (fc > 0 && fc <= 64)
                    {
                        const ImU32 fcol = ImGui::ColorConvertFloat4ToU32(ImVec4(g_config.infernoColor[0], g_config.infernoColor[1], g_config.infernoColor[2], g_config.infernoColor[3]));
                        for (int fi = 0; fi < fc; ++fi)
                        {
                            const float* fp = reinterpret_cast<const float*>(ent + kInferno_m_firePositions + static_cast<uintptr_t>(fi) * kInferno_fireStride);
                            const Vec3 fpos{ fp[0], fp[1], fp[2] };
                            if (!std::isfinite(fpos.x)) continue;
                            Vec2 fs;
                            if (!WorldToScreen(fpos, fs, *g_viewMatrix)) continue;
                            dl->AddCircleFilled(ImVec2(fs.x, fs.y), 9.f, fcol, 12);
                        }
                    }
                }

                // Predicted flight arc — integrate the projectile's networked
                // velocity under gravity (sv_gravity 800 * grenade scale 0.4) with
                // a little air drag, matching how CS2 grenades actually fly.
                if (wantTraj)
                {
                    Vec3 p0;
                    // networked velocity carries the throw direction (abs-velocity is
                    // often empty on the client for server-simulated projectiles).
                    const float* vel = reinterpret_cast<const float*>(ent + kBaseEntity_m_vecVelocity);
                    Vec3 v{ vel[0], vel[1], vel[2] };
                    const float speed2 = v.x * v.x + v.y * v.y + v.z * v.z;
                    if (GetOrigin(ent, p0) && speed2 > 1.f)
                    {
                        Vec3 p = p0;
                        const float dt = 1.f / 64.f;
                        const float g = 800.f * 0.4f;   // grenade gravity scale
                        const float drag = 1.f - 0.5f * dt;   // light air resistance per step
                        Vec2 prev;
                        bool havePrev = WorldToScreen(p, prev, *g_viewMatrix);
                        const ImU32 tcol = ImGui::ColorConvertFloat4ToU32(ImVec4(g_config.nadeColor[0], g_config.nadeColor[1], g_config.nadeColor[2], g_config.nadeColor[3]));
                        for (int step = 0; step < 128; ++step)   // ~2s
                        {
                            v.z -= g * dt;
                            v.x *= drag; v.y *= drag; v.z *= drag;
                            p.x += v.x * dt; p.y += v.y * dt; p.z += v.z * dt;
                            Vec2 sp;
                            const bool ok = WorldToScreen(p, sp, *g_viewMatrix);
                            if (ok && havePrev)
                            {
                                dl->AddLine(ImVec2(prev.x + 1.f, prev.y + 1.f), ImVec2(sp.x + 1.f, sp.y + 1.f), IM_COL32(0, 0, 0, 160), 2.0f);
                                dl->AddLine(ImVec2(prev.x, prev.y), ImVec2(sp.x, sp.y), tcol, 1.6f);
                            }
                            prev = sp; havePrev = ok;
                        }
                    }
                }

                Vec3 pos;
                if (!GetOrigin(ent, pos)) continue;
                Vec2 s;
                if (!WorldToScreen(pos, s, *g_viewMatrix)) continue;

                if (isNade)
                {
                    char nm[48];
                    std::snprintf(nm, sizeof(nm), "%s", dn);
                    char* u = std::strstr(nm, "_projectile"); if (u) *u = 0;
                    char line[64];
                    if (g_config.nadeDistance)
                    {
                        const float ddx = pos.x - localOrigin.x, ddy = pos.y - localOrigin.y, ddz = pos.z - localOrigin.z;
                        const float m = std::sqrt(ddx * ddx + ddy * ddy + ddz * ddz) * 0.01905f;
                        std::snprintf(line, sizeof(line), "%s [%.0fm]", nm, m);
                    }
                    else std::snprintf(line, sizeof(line), "%s", nm);
                    const ImVec2 ts = ImGui::CalcTextSize(line);
                    dl->AddText(ImVec2(s.x - ts.x * 0.5f + 1.f, s.y + 1.f), IM_COL32(0, 0, 0, 255), line);
                    dl->AddText(ImVec2(s.x - ts.x * 0.5f, s.y), ncol, line);
                }
                else if (isRag && g_config.ragdollEsp)
                {
                    const ImVec2 ts = ImGui::CalcTextSize("ragdoll");
                    dl->AddText(ImVec2(s.x - ts.x * 0.5f + 1.f, s.y + 1.f), IM_COL32(0, 0, 0, 255), "ragdoll");
                    dl->AddText(ImVec2(s.x - ts.x * 0.5f, s.y), rcol, "ragdoll");
                }
            }
        }

        // Spectator list (players observing the local pawn).
        if (g_config.specList && localPawnIndex >= 0)
        {
            float sy = screenSize.y * 0.35f;
            const float sx = 12.f;
            const ImU32 scol = ImGui::ColorConvertFloat4ToU32(ImVec4(g_config.specColor[0], g_config.specColor[1], g_config.specColor[2], g_config.specColor[3]));
            dl->AddText(ImVec2(sx + 1.f, sy + 1.f), IM_COL32(0, 0, 0, 255), "Spectators:");
            dl->AddText(ImVec2(sx, sy), scol, "Spectators:");
            sy += 16.f;

            for (int i = 0; i <= 0x7FFE; ++i)
            {
                const uintptr_t ent = GetEntity(i);
                if (!ent || ent == localPawn) continue;

                // Only player pawns have observer services at 0x1220. Gate that
                // large read behind the base-entity team field (as pass 1 does):
                // dereferencing a pawn-only offset on an arbitrary/smaller entity
                // reads past its allocation and crashes. Observers keep their
                // team (2/3) while dead, so this still covers the real case.
                const int team = *reinterpret_cast<const uint8_t*>(ent + kBaseEntity_m_iTeamNum);
                if (team != 2 && team != 3) continue;

                const uintptr_t obs = *reinterpret_cast<uintptr_t*>(ent + kBasePlayerPawn_m_pObserverServices);
                if (!IsValidPtr(obs)) continue;
                const uint32_t tgt = *reinterpret_cast<const uint32_t*>(obs + kObserverServices_m_hObserverTarget);
                if ((tgt & 0x7FFF) != static_cast<uint32_t>(localPawnIndex)) continue;

                const uint32_t hc = *reinterpret_cast<const uint32_t*>(ent + kBasePlayerPawn_m_hController);
                const uintptr_t ctrl = GetEntity(static_cast<int>(hc & 0x7FFF));
                const char* name = "spectator";
                if (ctrl)
                {
                    const char* n = *reinterpret_cast<const char* const*>(ctrl + kCCSPlayerController_m_sSanitizedPlayerName);
                    if (IsValidPtr(reinterpret_cast<uintptr_t>(n)) && n[0]) name = n;
                }
                dl->AddText(ImVec2(sx + 1.f, sy + 1.f), IM_COL32(0, 0, 0, 255), name);
                dl->AddText(ImVec2(sx, sy), scol, name);
                sy += 15.f;
            }
        }

        // Velocity bar (local player horizontal speed).
        if (g_config.velBar && IsValidPtr(localPawn))
        {
            const float vx = *reinterpret_cast<const float*>(localPawn + kBaseEntity_m_vecVelocity + 0);
            const float vy = *reinterpret_cast<const float*>(localPawn + kBaseEntity_m_vecVelocity + 4);
            const float speed = std::sqrt(vx * vx + vy * vy);

            const int   vmax = g_config.velMax > 1 ? g_config.velMax : 400;
            float frac = speed / static_cast<float>(vmax);
            if (frac < 0.f) frac = 0.f; if (frac > 1.f) frac = 1.f;

            const float bw = 260.f, bh = 10.f;
            const float bx = (screenSize.x - bw) * 0.5f;
            const float by = (g_config.velBarPos == 1) ? (screenSize.y * 0.10f)
                                                       : (screenSize.y - screenSize.y * 0.12f);
            const float rnd = bh * 0.5f;

            auto toU32 = [](const float* c, float aMul) {
                return ImGui::ColorConvertFloat4ToU32(ImVec4(c[0], c[1], c[2], c[3] * aMul));
            };
            // color at the filled end (gradient uses both, else just velColor)
            const ImU32 cLeft  = toU32(g_config.velColor, 1.f);
            const ImU32 cRight = g_config.velGradient ? toU32(g_config.velColor2, 1.f) : cLeft;

            // soft glow: a few expanding translucent rounded rects behind the fill
            if (g_config.velGlow && frac > 0.001f)
            {
                const float gx1 = bx, gx2 = bx + bw * frac;
                for (int g = 3; g >= 1; --g)
                {
                    const float pad = static_cast<float>(g) * 2.5f;
                    const ImU32 gc = toU32(g_config.velColor, 0.10f);
                    dl->AddRectFilled(ImVec2(gx1 - pad, by - pad), ImVec2(gx2 + pad, by + bh + pad), gc, rnd + pad);
                }
            }

            // track (background)
            dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + bw, by + bh), IM_COL32(0, 0, 0, 160), rnd);

            // fill
            if (frac > 0.001f)
            {
                const float fx2 = bx + bw * frac;
                if (g_config.velGradient)
                    dl->AddRectFilledMultiColor(ImVec2(bx, by), ImVec2(fx2, by + bh), cLeft, cRight, cRight, cLeft);
                else
                    dl->AddRectFilled(ImVec2(bx, by), ImVec2(fx2, by + bh), cLeft, rnd);
            }
            // border
            dl->AddRect(ImVec2(bx, by), ImVec2(bx + bw, by + bh), IM_COL32(0, 0, 0, 200), rnd, 0, 1.2f);

            // numeric speed
            if (g_config.velText)
            {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%d u/s", static_cast<int>(speed + 0.5f));
                const ImVec2 ts = ImGui::CalcTextSize(buf);
                const float tx = (screenSize.x - ts.x) * 0.5f;
                const float ty = (g_config.velBarPos == 1) ? (by + bh + 3.f) : (by - ts.y - 3.f);
                dl->AddText(ImVec2(tx + 1.f, ty + 1.f), IM_COL32(0, 0, 0, 220), buf);
                dl->AddText(ImVec2(tx, ty), IM_COL32(235, 235, 240, 255), buf);
            }
        }

        // Velocity graph (scrolling history of local horizontal speed).
        if (g_config.velGraph && IsValidPtr(localPawn))
        {
            const float vx = *reinterpret_cast<const float*>(localPawn + kBaseEntity_m_vecVelocity + 0);
            const float vy = *reinterpret_cast<const float*>(localPawn + kBaseEntity_m_vecVelocity + 4);
            const float speed = std::sqrt(vx * vx + vy * vy);

            static float hist[160] = {};
            static int head = 0;
            hist[head] = speed;
            head = (head + 1) % 160;

            const int   vmax = g_config.velMax > 1 ? g_config.velMax : 400;
            const float gw = 260.f, gh = 46.f;
            const float gx = (screenSize.x - gw) * 0.5f;
            const float gy = (g_config.velBarPos == 1)
                ? (screenSize.y * 0.10f + 26.f)
                : (screenSize.y - screenSize.y * 0.12f - gh - 26.f);

            dl->AddRectFilled(ImVec2(gx, gy), ImVec2(gx + gw, gy + gh), IM_COL32(0, 0, 0, 150), 4.f);
            dl->AddRect(ImVec2(gx, gy), ImVec2(gx + gw, gy + gh), IM_COL32(40, 40, 47, 220), 4.f);
            // mid gridline (half of vmax)
            dl->AddLine(ImVec2(gx, gy + gh * 0.5f), ImVec2(gx + gw, gy + gh * 0.5f), IM_COL32(60, 60, 68, 120), 1.f);

            const ImU32 lc = ImGui::ColorConvertFloat4ToU32(ImVec4(g_config.velColor[0], g_config.velColor[1], g_config.velColor[2], g_config.velColor[3]));
            const int N = 160;
            for (int i = 0; i < N - 1; ++i)
            {
                const float s0 = hist[(head + i) % N];
                const float s1 = hist[(head + i + 1) % N];
                float f0 = s0 / vmax, f1 = s1 / vmax;
                if (f0 > 1.f) f0 = 1.f; if (f1 > 1.f) f1 = 1.f;
                const float x0 = gx + gw * (static_cast<float>(i) / (N - 1));
                const float x1 = gx + gw * (static_cast<float>(i + 1) / (N - 1));
                const float y0 = gy + gh - f0 * gh;
                const float y1 = gy + gh - f1 * gh;
                dl->AddLine(ImVec2(x0, y0), ImVec2(x1, y1), lc, 1.4f);
            }
        }

        // Combat FX: hit marker + damage numbers + bullet tracer.
        // All derived from enemy health deltas and local shot count — no hook.
        if (g_config.hitMarker || g_config.damageNumbers || g_config.bulletTracer)
        {
            struct DmgNum { Vec3 pos; int dmg; float life; };
            struct Tracer { Vec3 a, b; float life; };
            static std::unordered_map<uintptr_t, int> hpMap;
            static std::vector<DmgNum> nums;
            static std::vector<Tracer> tracers;
            static int lastShots = -1;
            static float hitTimer = 0.f;
            const float dt = ImGui::GetIO().DeltaTime;

            static ULONGLONG lastShotMs = 0;
            int shots = 0;
            if (IsValidPtr(localPawn)) shots = *reinterpret_cast<const int32_t*>(localPawn + kCSPlayerPawn_m_iShotsFired);
            const bool weFired = (lastShots >= 0 && shots != lastShots);
            lastShots = shots;
            if (weFired) lastShotMs = GetTickCount64();
            // attribute damage to us only within a window after we fired.
            const bool recentShot = (GetTickCount64() - lastShotMs) < 500;

            // bullet tracer: on our shot, trace eye->aim and draw a fading line.
            if (g_config.bulletTracer && weFired && IsValidPtr(localPawn))
            {
                const uintptr_t client = reinterpret_cast<uintptr_t>(GetModuleHandleA("client.dll"));
                if (client)
                {
                    const float* va = reinterpret_cast<const float*>(client + kDw_dwViewAngles);
                    const float pr = va[0] * 0.01745329f, yr = va[1] * 0.01745329f, cpp = std::cos(pr);
                    const Vec3 fwd{ cpp * std::cos(yr), cpp * std::sin(yr), -std::sin(pr) };
                    const float* vo = reinterpret_cast<const float*>(localPawn + kBaseModelEntity_m_vecViewOffset);
                    const Vec3 eye{ localOrigin.x + vo[0], localOrigin.y + vo[1], localOrigin.z + vo[2] };
                    const Vec3 muzzle{ eye.x + fwd.x * 12.f, eye.y + fwd.y * 12.f, eye.z + fwd.z * 12.f - 4.f };
                    const float s3[3] = { eye.x, eye.y, eye.z };
                    const float e3[3] = { eye.x + fwd.x * 8192.f, eye.y + fwd.y * 8192.f, eye.z + fwd.z * 8192.f };
                    float oe[3], on[3], frac;
                    Trace::Line(s3, e3, localPawn, oe, on, &frac);
                    tracers.push_back({ muzzle, Vec3{ oe[0], oe[1], oe[2] }, 0.55f });
                }
            }

            // health tracking -> damage numbers + hit marker.
            for (const Player& p : players)
            {
                if (!p.pawn || p.team == localTeam) continue;
                auto it = hpMap.find(p.pawn);
                // only our damage: enemy health dropped within our shot window.
                if (it != hpMap.end() && p.health < it->second && it->second - p.health <= 100 && recentShot)
                {
                    const int dmg = it->second - p.health;
                    if (g_config.damageNumbers)
                    {
                        // stagger the start height so rapid consecutive hits don't overlap.
                        static int dmgSpawn = 0;
                        const float zoff = 48.f + (dmgSpawn % 4) * 14.f;
                        dmgSpawn++;
                        nums.push_back({ Vec3{ p.origin.x, p.origin.y, p.origin.z + zoff }, dmg, 3.5f });
                    }
                    if (g_config.hitMarker)
                        hitTimer = 0.85f;
                }
                hpMap[p.pawn] = p.health;
            }

            // draw + age damage numbers
            {
                ImFont* font = ImGui::GetFont();
                const float fsz = ImGui::GetFontSize() * 2.6f;   // large, readable
                for (size_t k = 0; k < nums.size(); )
                {
                    DmgNum& d = nums[k];
                    d.pos.z += dt * 16.f;   // float upward
                    d.life -= dt;
                    Vec2 sp;
                    if (d.life > 0.f && WorldToScreen(d.pos, sp, *g_viewMatrix))
                    {
                        float a = d.life / 0.8f; if (a > 1.f) a = 1.f;   // hold full, fade only in the last 0.8s
                        char buf[16]; std::snprintf(buf, sizeof(buf), "-%d", d.dmg);
                        const ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(g_config.damageColor[0], g_config.damageColor[1], g_config.damageColor[2], a));
                        const ImVec2 ts = font->CalcTextSizeA(fsz, 100000.f, 0.f, buf);
                        const ImVec2 tp(sp.x - ts.x * 0.5f, sp.y - ts.y * 0.5f);
                        const ImU32 oc = ImGui::ColorConvertFloat4ToU32(ImVec4(0, 0, 0, a));
                        dl->AddText(font, fsz, ImVec2(tp.x + 2.f, tp.y + 2.f), oc, buf);
                        dl->AddText(font, fsz, ImVec2(tp.x - 1.f, tp.y), oc, buf);
                        dl->AddText(font, fsz, ImVec2(tp.x + 1.f, tp.y), oc, buf);
                        dl->AddText(font, fsz, tp, col, buf);
                    }
                    if (d.life <= 0.f) nums.erase(nums.begin() + k); else ++k;
                }
            }

            // draw + age tracers — laser beam from the gun (screen bottom) to the
            // world-anchored impact point; shoots out then fades.
            {
                const float TLIFE = 0.55f;
                const float* tc = g_config.tracerColor;
                // muzzle origin in screen space — right-handed viewmodel (gun side).
                const ImVec2 A(screenSize.x * 0.66f, screenSize.y * 0.80f);
                for (size_t k = 0; k < tracers.size(); )
                {
                    Tracer& t = tracers[k];
                    t.life -= dt;
                    Vec2 sb;
                    if (t.life > 0.f && WorldToScreen(t.b, sb, *g_viewMatrix))
                    {
                        const ImVec2 B(sb.x, sb.y);
                        const float prog = 1.f - t.life / TLIFE;       // 0..1
                        float tf = prog / 0.18f; if (tf > 1.f) tf = 1.f; // tip travels over first 18%
                        float a = t.life / TLIFE; a = a * a;           // smooth (eased) fade
                        const ImVec2 tip(A.x + (B.x - A.x) * tf, A.y + (B.y - A.y) * tf);

                        auto C = [&](float mul) { return ImGui::ColorConvertFloat4ToU32(ImVec4(tc[0], tc[1], tc[2], a * mul)); };
                        // wide soft glow -> mid -> bright near-white core (thicker)
                        dl->AddLine(A, tip, C(0.10f), 11.0f);
                        dl->AddLine(A, tip, C(0.22f), 6.5f);
                        dl->AddLine(A, tip, C(0.55f), 3.4f);
                        const ImU32 core = ImGui::ColorConvertFloat4ToU32(ImVec4(
                            0.6f + tc[0] * 0.4f, 0.6f + tc[1] * 0.4f, 0.6f + tc[2] * 0.4f, a));
                        dl->AddLine(A, tip, core, 2.0f);
                        // impact flash once the beam reaches the target
                        if (tf >= 1.f)
                        {
                            dl->AddCircleFilled(B, 3.f + a * 5.f, C(0.5f), 14);
                            dl->AddCircleFilled(B, 2.f + a * 2.f, ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, a * 0.85f)), 12);
                        }
                    }
                    if (t.life <= 0.f) tracers.erase(tracers.begin() + k); else ++k;
                }
            }

            // hit marker (X at crosshair)
            if (hitTimer > 0.f)
            {
                hitTimer -= dt;
                float a = hitTimer / 0.35f; if (a > 1.f) a = 1.f;   // hold, fade at the end
                const float cx = screenSize.x * 0.5f, cy = screenSize.y * 0.5f;
                const ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(g_config.hitMarkerColor[0], g_config.hitMarkerColor[1], g_config.hitMarkerColor[2], a));
                const ImU32 oc = IM_COL32(0, 0, 0, (int)(a * 200));
                const float g0 = 6.f, g1 = 17.f, th = 2.4f;
                auto arm = [&](float sx, float sy) {
                    dl->AddLine(ImVec2(cx + sx * g1, cy + sy * g1), ImVec2(cx + sx * g0, cy + sy * g0), oc, th + 1.5f);
                    dl->AddLine(ImVec2(cx + sx * g1, cy + sy * g1), ImVec2(cx + sx * g0, cy + sy * g0), col, th);
                };
                arm(-1, -1); arm(1, -1); arm(-1, 1); arm(1, 1);
            }
        }

        // Sound ESP (footstep/noise rings for audible enemies — movement based).
        if (g_config.soundEsp)
        {
            static float phase = 0.f;
            phase += ImGui::GetIO().DeltaTime * 1.8f;
            if (phase > 1.f) phase -= 1.f;

            for (const Player& p : players)
            {
                if (!p.pawn || p.team == localTeam) continue;
                const float vx = *reinterpret_cast<const float*>(p.pawn + kBaseEntity_m_vecVelocity + 0);
                const float vy = *reinterpret_cast<const float*>(p.pawn + kBaseEntity_m_vecVelocity + 4);
                const float speed = std::sqrt(vx * vx + vy * vy);
                const uint32_t flags = *reinterpret_cast<const uint32_t*>(p.pawn + kBaseEntity_m_fFlags);
                if (!(flags & 0x1) || speed < 130.f) continue;   // on ground + louder than walk

                Vec2 feet;
                if (!WorldToScreen(p.origin, feet, *g_viewMatrix)) continue;

                for (int r = 0; r < 2; ++r)
                {
                    float t = phase + r * 0.5f; if (t > 1.f) t -= 1.f;
                    const float rad = 6.f + t * 26.f;
                    const int a = static_cast<int>((1.f - t) * 170.f);
                    const ImU32 sc = ImGui::ColorConvertFloat4ToU32(ImVec4(g_config.soundColor[0], g_config.soundColor[1], g_config.soundColor[2], a / 255.f));
                    dl->AddCircle(ImVec2(feet.x, feet.y), rad, sc, 22, 1.6f);
                }
            }
        }

        // Grenade throw predictor: while holding a grenade, simulate the throw
        // (eye + aim -> velocity, gravity 0.4x, wall bounces) and draw the arc +
        // landing marker. Pure physics + world traces.
        if (g_config.nadeThrow && IsValidPtr(localPawn))
        {
            char wpn[48] = {};
            GetWeaponName(localPawn, wpn, sizeof(wpn));
            const bool holdingNade =
                std::strstr(wpn, "grenade") || std::strstr(wpn, "molotov") ||
                std::strstr(wpn, "flashbang") || std::strstr(wpn, "incgrenade") ||
                std::strstr(wpn, "decoy") || std::strstr(wpn, "smoke") || std::strstr(wpn, "inferno");

            const uintptr_t client = reinterpret_cast<uintptr_t>(GetModuleHandleA("client.dll"));
            if (holdingNade && client)
            {
                const float* va = reinterpret_cast<const float*>(client + kDw_dwViewAngles);
                const float pitch = va[0], yaw = va[1];

                // Throw strength (0..1) as the game maintains it: right-click ~0
                // (underhand lob), left-click ramps to 1 (full), both ~0.5.
                float strength = 1.f;
                {
                    const uintptr_t ws = *reinterpret_cast<uintptr_t*>(localPawn + kBasePlayerPawn_m_pWeaponServices);
                    if (IsValidPtr(ws))
                    {
                        const uint32_t hw = *reinterpret_cast<uint32_t*>(ws + kPlayerWeaponServices_m_hActiveWeapon);
                        const uintptr_t wep = GetEntity(static_cast<int>(hw & 0x7FFF));
                        if (wep) strength = *reinterpret_cast<const float*>(wep + kBaseCSGrenade_m_flThrowStrength);
                    }
                }
                if (strength < 0.f) strength = 0.f; if (strength > 1.f) strength = 1.f;

                // CS grenade throw: clamp pitch, derive velocity from the clamped
                // angle, then scale by throw strength (full=1x, lob≈0.5x).
                const float apitch = (pitch < 0.f) ? (-10.f + pitch * (80.f / 90.f))
                                                   : (-10.f + pitch * (100.f / 90.f));
                const float pr = apitch * 0.01745329f, yr = yaw * 0.01745329f;
                const float cp = std::cos(pr);
                Vec3 fwd{ cp * std::cos(yr), cp * std::sin(yr), -std::sin(pr) };

                const float* vo = reinterpret_cast<const float*>(localPawn + kBaseModelEntity_m_vecViewOffset);
                const Vec3 eye{ localOrigin.x + vo[0], localOrigin.y + vo[1], localOrigin.z + vo[2] };
                const float* lv = reinterpret_cast<const float*>(localPawn + kBaseEntity_m_vecVelocity);

                // CS2 throw velocity: base 750, blended by strength (0.5x..1x).
                const float spd = 750.f * (0.5f + 0.5f * strength);

                Vec3 pos{ eye.x + fwd.x * 16.f, eye.y + fwd.y * 16.f, eye.z + fwd.z * 16.f };
                Vec3 v{ lv[0] * 1.25f + fwd.x * spd, lv[1] * 1.25f + fwd.y * spd, lv[2] * 1.25f + fwd.z * spd };

                const float dt = 1.f / 64.f, g = 800.f * 0.4f, rest = 0.45f;
                const ImU32 tcol = ImGui::ColorConvertFloat4ToU32(ImVec4(g_config.nadeThrowColor[0], g_config.nadeThrowColor[1], g_config.nadeThrowColor[2], g_config.nadeThrowColor[3]));

                Vec2 prev; bool havePrev = WorldToScreen(pos, prev, *g_viewMatrix);
                Vec3 landing = pos; bool landed = false;

                for (int step = 0; step < 320 && !landed; ++step)   // ~5s
                {
                    v.z -= g * dt;
                    Vec3 next{ pos.x + v.x * dt, pos.y + v.y * dt, pos.z + v.z * dt };

                    const float s3[3] = { pos.x, pos.y, pos.z };
                    const float e3[3] = { next.x, next.y, next.z };
                    float oe[3], on[3], frac;
                    if (Trace::Line(s3, e3, localPawn, oe, on, &frac))
                    {
                        pos = Vec3{ oe[0] + on[0] * 0.5f, oe[1] + on[1] * 0.5f, oe[2] + on[2] * 0.5f };
                        const float d = v.x * on[0] + v.y * on[1] + v.z * on[2];
                        v.x = (v.x - 2.f * d * on[0]) * rest;
                        v.y = (v.y - 2.f * d * on[1]) * rest;
                        v.z = (v.z - 2.f * d * on[2]) * rest;
                        if (v.x * v.x + v.y * v.y + v.z * v.z < 3600.f) { landing = pos; landed = true; }
                    }
                    else pos = next;

                    Vec2 sp; const bool ok = WorldToScreen(pos, sp, *g_viewMatrix);
                    if (ok && havePrev)
                    {
                        dl->AddLine(ImVec2(prev.x + 1.f, prev.y + 1.f), ImVec2(sp.x + 1.f, sp.y + 1.f), IM_COL32(0, 0, 0, 160), 2.2f);
                        dl->AddLine(ImVec2(prev.x, prev.y), ImVec2(sp.x, sp.y), tcol, 1.7f);
                    }
                    prev = sp; havePrev = ok;
                }
                if (!landed) landing = pos;

                Vec2 lm;
                if (WorldToScreen(landing, lm, *g_viewMatrix))
                {
                    dl->AddCircleFilled(ImVec2(lm.x, lm.y), 5.f, tcol, 16);
                    dl->AddCircle(ImVec2(lm.x, lm.y), 10.f, tcol, 20, 1.8f);
                }
            }
        }

        // Feed the chams hook: enemy / team / local pawns + dropped items.
        {
            uintptr_t enemies[64]; int ne = 0;
            uintptr_t team[64];    int nt = 0;
            for (const Player& p : players)
            {
                if (!p.pawn || p.pawn == localPawn) continue;
                if (p.team != localTeam) { if (ne < 64) enemies[ne++] = p.pawn; }
                else                     { if (nt < 64) team[nt++] = p.pawn; }
            }
            Chams::UpdateTargets(enemies, ne, team, nt, localPawn, itemPtrs, itemCount, ragdollPtrs, ragdollCount);
        }
    }
}
