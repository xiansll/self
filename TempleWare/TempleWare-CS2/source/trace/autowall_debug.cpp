#include "autowall_debug.h"
#include "autowall.h"
#include "trace.h"
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdint>

namespace {

// Offsets mirrored from esp.cpp — entity system access
constexpr uintptr_t kEntityListOffset   = 0x10;
constexpr uintptr_t kEntityEntryStride  = 0x70;
constexpr uintptr_t kEntityEntryEntity  = 0x00;
constexpr uintptr_t kEntityEntryHandle  = 0x10;

constexpr uintptr_t kBaseEntity_m_pGameSceneNode = 0x330;
constexpr uintptr_t kGameSceneNode_m_vecOrigin   = 0x80;
constexpr uintptr_t kBaseEntity_m_iHealth        = 0x34C;
constexpr uintptr_t kBaseEntity_m_iTeamNum       = 0x3E7;
constexpr uintptr_t kBaseModelEntity_m_vecViewOffset = 0xE78;
constexpr uintptr_t kBaseEntity_m_pEntity        = 0x10;
constexpr uintptr_t kIdentity_m_designerName     = 0x20;
constexpr uintptr_t kDw_dwLocalPlayerPawn        = 0x23C6268;

// Bone access
constexpr uintptr_t kSkeletonInstance_m_modelState = 0x140;
constexpr uintptr_t kModelState_boneArray          = 0xB0;
constexpr uintptr_t kBoneData_stride               = 0x20;
constexpr int       kBone_Head                     = 6;

// Weapon vdata
constexpr uintptr_t kBasePlayerPawn_m_pWeaponServices  = 0x1208;
constexpr uintptr_t kPlayerWeaponServices_m_hActiveWeapon = 0x60;
constexpr uintptr_t kBaseEntity_m_nSubclassID          = 0x380;
constexpr uintptr_t kWeaponVData_m_nDamage     = 0x828;
constexpr uintptr_t kWeaponVData_m_flPenetration   = 0x834;
constexpr uintptr_t kWeaponVData_m_flRange         = 0x838;
constexpr uintptr_t kWeaponVData_m_flRangeModifier = 0x83C;
constexpr uintptr_t kWeaponVData_m_flArmorRatio    = 0x830;
constexpr uintptr_t kWeaponVData_m_flHeadshotMult  = 0x82C;

bool IsValidPtr(uintptr_t p) { return p >= 0x10000 && p < 0x0000FFFFFFFFFFFFull; }

void LogLine(const char* msg)
{
    wchar_t path[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, path);
    wcscat_s(path, MAX_PATH, L"TempleWare.log");
    FILE* f = nullptr;
    if (_wfopen_s(&f, path, L"a") == 0 && f) {
        fprintf(f, "[autowall] %s\n", msg);
        fclose(f);
    }
}

uintptr_t GetEntitySystem()
{
    HMODULE client = GetModuleHandleA("client.dll");
    if (!client) return 0;

    // CGameEntitySystem** is at client + dwEntityList (known pattern)
    // We use the same global that ESP resolves
    static uintptr_t s_cached = 0;
    if (s_cached) {
        uintptr_t val = *reinterpret_cast<uintptr_t*>(s_cached);
        if (IsValidPtr(val)) return val;
        s_cached = 0;
    }

    // Resolve via pattern: 48 8B 0D ?? ?? ?? ?? 48 85 C9 74 ?? E8 ?? ?? ?? ?? 48 8B D8 EB
    auto base = reinterpret_cast<const uint8_t*>(client);
    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)client;
    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)(base + dos->e_lfanew);
    size_t size = nt->OptionalHeader.SizeOfImage;

    const uint8_t sig[] = {0x48, 0x8B, 0x0D};
    for (size_t i = 0; i < size - 20; ++i) {
        if (base[i] == 0x48 && base[i+1] == 0x8B && base[i+2] == 0x0D) {
            uintptr_t rva = (uintptr_t)(base + i);
            int32_t disp = *reinterpret_cast<const int32_t*>(base + i + 3);
            uintptr_t global = rva + 7 + disp;
            if (!IsValidPtr(global)) continue;
            uintptr_t val = *reinterpret_cast<uintptr_t*>(global);
            if (!IsValidPtr(val)) continue;
            // Verify: at val + kEntityListOffset there should be a valid chunk ptr
            uintptr_t chunk0 = *reinterpret_cast<uintptr_t*>(val + kEntityListOffset);
            if (IsValidPtr(chunk0)) {
                s_cached = global;
                return val;
            }
        }
    }
    return 0;
}

uintptr_t GetEntity(uintptr_t entitySystem, int index)
{
    if (!entitySystem || index < 0 || index > 0x7FFE) return 0;
    int chunk = index >> 9;
    if (chunk > 0x3F) return 0;
    uintptr_t chunkPtr = *reinterpret_cast<uintptr_t*>(entitySystem + kEntityListOffset + (uintptr_t)chunk * 8);
    if (!IsValidPtr(chunkPtr)) return 0;
    uintptr_t entry = chunkPtr + (uintptr_t)(index & 0x1FF) * kEntityEntryStride;
    uint32_t handle = *reinterpret_cast<uint32_t*>(entry + kEntityEntryHandle);
    if ((handle & 0x7FFF) != (uint32_t)index) return 0;
    uintptr_t ent = *reinterpret_cast<uintptr_t*>(entry + kEntityEntryEntity);
    return IsValidPtr(ent) ? ent : 0;
}

bool GetOrigin(uintptr_t pawn, float out[3])
{
    uintptr_t sceneNode = *reinterpret_cast<uintptr_t*>(pawn + kBaseEntity_m_pGameSceneNode);
    if (!IsValidPtr(sceneNode)) return false;
    const float* o = reinterpret_cast<const float*>(sceneNode + kGameSceneNode_m_vecOrigin);
    out[0] = o[0]; out[1] = o[1]; out[2] = o[2];
    return std::isfinite(out[0]) && std::isfinite(out[1]) && std::isfinite(out[2]);
}

bool GetEyePos(uintptr_t pawn, float out[3])
{
    float origin[3];
    if (!GetOrigin(pawn, origin)) return false;
    const float* vo = reinterpret_cast<const float*>(pawn + kBaseModelEntity_m_vecViewOffset);
    out[0] = origin[0] + vo[0];
    out[1] = origin[1] + vo[1];
    out[2] = origin[2] + vo[2];
    return true;
}

bool GetHeadPos(uintptr_t pawn, float out[3])
{
    uintptr_t sceneNode = *reinterpret_cast<uintptr_t*>(pawn + kBaseEntity_m_pGameSceneNode);
    if (!IsValidPtr(sceneNode)) return false;
    uintptr_t boneArray = *reinterpret_cast<uintptr_t*>(sceneNode + kSkeletonInstance_m_modelState + kModelState_boneArray);
    if (!IsValidPtr(boneArray)) return false;
    const float* bone = reinterpret_cast<const float*>(boneArray + kBone_Head * kBoneData_stride);
    out[0] = bone[0]; out[1] = bone[1]; out[2] = bone[2];
    return std::isfinite(out[0]) && std::isfinite(out[1]) && std::isfinite(out[2]);
}

bool IsDesignerName(uintptr_t ent, const char* name)
{
    uintptr_t id = *reinterpret_cast<uintptr_t*>(ent + kBaseEntity_m_pEntity);
    if (!IsValidPtr(id)) return false;
    const char* n = *reinterpret_cast<const char* const*>(id + kIdentity_m_designerName);
    if (!IsValidPtr((uintptr_t)n)) return false;
    return strcmp(n, name) == 0;
}

struct WeaponData
{
    float damage;
    float penetration;
    float range;
    float range_modifier;
    float armor_ratio;
    float headshot_mult;
    bool  valid;
};

WeaponData GetWeaponData(uintptr_t pawn, uintptr_t entitySystem)
{
    WeaponData w{};
    uintptr_t ws = *reinterpret_cast<uintptr_t*>(pawn + kBasePlayerPawn_m_pWeaponServices);
    if (!IsValidPtr(ws)) return w;
    uint32_t hActive = *reinterpret_cast<uint32_t*>(ws + kPlayerWeaponServices_m_hActiveWeapon);
    uintptr_t weapon = GetEntity(entitySystem, (int)(hActive & 0x7FFF));
    if (!weapon) return w;
    uintptr_t vdata = *reinterpret_cast<uintptr_t*>(weapon + kBaseEntity_m_nSubclassID + 0x8);
    if (!IsValidPtr(vdata)) return w;
    w.damage = (float)*reinterpret_cast<int32_t*>(vdata + kWeaponVData_m_nDamage);
    w.penetration = *reinterpret_cast<float*>(vdata + kWeaponVData_m_flPenetration);
    w.range = *reinterpret_cast<float*>(vdata + kWeaponVData_m_flRange);
    w.range_modifier = *reinterpret_cast<float*>(vdata + kWeaponVData_m_flRangeModifier);
    w.armor_ratio = *reinterpret_cast<float*>(vdata + kWeaponVData_m_flArmorRatio);
    w.headshot_mult = *reinterpret_cast<float*>(vdata + kWeaponVData_m_flHeadshotMult);
    w.valid = w.damage > 0 && w.penetration > 0 && w.range > 0;
    return w;
}

} // namespace

namespace AutowallDebug
{
    void Tick()
    {
        if (!Trace::Ready()) return;

        static DWORD s_lastTick = 0;
        DWORD now = GetTickCount();
        if (s_lastTick != 0 && (now - s_lastTick) < 2000) return;
        s_lastTick = now;

        __try
        {
            HMODULE client = GetModuleHandleA("client.dll");
            if (!client) return;

            uintptr_t localPawn = *reinterpret_cast<uintptr_t*>((uintptr_t)client + kDw_dwLocalPlayerPawn);
            if (!IsValidPtr(localPawn)) return;

            int localHealth = *reinterpret_cast<int32_t*>(localPawn + kBaseEntity_m_iHealth);
            if (localHealth <= 0) return;

            int localTeam = *reinterpret_cast<uint8_t*>(localPawn + kBaseEntity_m_iTeamNum);

            float eyePos[3];
            if (!GetEyePos(localPawn, eyePos)) return;

            uintptr_t entitySystem = GetEntitySystem();
            if (!entitySystem) return;

            WeaponData wpn = GetWeaponData(localPawn, entitySystem);
            if (!wpn.valid) return;

            // Find first alive enemy
            for (int i = 1; i <= 64; ++i)
            {
                uintptr_t ent = GetEntity(entitySystem, i);
                if (!ent || ent == localPawn) continue;
                if (!IsDesignerName(ent, "cs_player_controller")) continue;

                // Get pawn handle from controller
                uint32_t hPawn = *reinterpret_cast<uint32_t*>(ent + 0x6BC); // m_hPlayerPawn
                uintptr_t pawn = GetEntity(entitySystem, (int)(hPawn & 0x7FFF));
                if (!pawn || !IsValidPtr(pawn)) continue;

                int team = *reinterpret_cast<uint8_t*>(pawn + kBaseEntity_m_iTeamNum);
                if (team == localTeam) continue;

                int hp = *reinterpret_cast<int32_t*>(pawn + kBaseEntity_m_iHealth);
                if (hp <= 0) continue;

                float headPos[3];
                if (!GetHeadPos(pawn, headPos)) continue;

                float origin[3];
                if (!GetOrigin(pawn, origin)) continue;

                // Build autowall input
                Autowall::PenetrationInput input{};
                memcpy(input.origin, eyePos, sizeof(float) * 3);
                memcpy(input.target, headPos, sizeof(float) * 3);
                input.attacker = localPawn;
                input.target_entity = pawn;
                input.weapon_damage = wpn.damage;
                input.weapon_penetration = wpn.penetration;
                input.weapon_range = wpn.range;
                input.weapon_range_modifier = wpn.range_modifier;
                input.weapon_armor_ratio = wpn.armor_ratio;
                input.headshot_multiplier = wpn.headshot_mult;
                input.hitgroup = 1; // head
                input.target_has_helmet = true;
                input.target_has_armor = true;
                input.target_armor_value = 100;

                auto result = Autowall::SimulateBullet(input);

                // Also do a simple LOS check for comparison
                float dummyEnd[3], dummyN[3], frac = 1.f;
                bool losBlocked = Trace::Line(eyePos, headPos, localPawn, dummyEnd, dummyN, &frac);

                float dx = headPos[0] - eyePos[0];
                float dy = headPos[1] - eyePos[1];
                float dz = headPos[2] - eyePos[2];
                float dist = sqrtf(dx*dx + dy*dy + dz*dz);

                char buf[512];
                snprintf(buf, sizeof(buf),
                    "TEST player=%d dist=%.0f los_blocked=%d frac=%.3f | "
                    "hit=%d pen=%d surfaces=%d dmg=%.1f total_dist=%.0f | "
                    "wpn_dmg=%.0f wpn_pen=%.1f wpn_range=%.0f",
                    i, dist, losBlocked ? 1 : 0, frac,
                    result.did_hit ? 1 : 0, result.did_penetrate ? 1 : 0,
                    result.surfaces_penetrated, result.damage, result.total_distance,
                    wpn.damage, wpn.penetration, wpn.range);
                LogLine(buf);

                break; // only test first enemy found
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
}
