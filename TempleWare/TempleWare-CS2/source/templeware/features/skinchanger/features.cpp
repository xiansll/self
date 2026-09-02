#include "features.h"
#include <chrono>
#include <thread>
#include <cstdarg>
#include <cstdio>
#include "../../utils/math/vector/vector.h"
#include "../../utils/fnv1a/fnv1a.h"
#include "../../utils/memory/patternscan/patternscan.h"
#include "../../utils/memory/vfunc/vfunc.h"
#include "../../interfaces/interfaces.h"
#include "../../../cs2/entity/C_CSPlayerPawn/C_CSPlayerPawn.h"
#include "../../../cs2/entity/C_BaseEntity/C_BaseEntity.h"

namespace features::skinchanger {

// Helper to get schema offset
inline std::uint32_t SchemaOffset(const char* className, const char* fieldName) {
    return SchemaFinder::Get(hash_32_fnv1a_const(className) ^ hash_32_fnv1a_const(fieldName));
}

// Pattern scan helper
inline std::uintptr_t FindPattern(const char* module, const char* pattern) {
    return reinterpret_cast<std::uintptr_t>(M::FindPattern(module, pattern));
}

// Helper to get entity by handle
template <typename T = C_BaseEntity>
inline T* GetEntityFromHandle(const CBaseHandle& handle) {
    if (!handle.valid()) return nullptr;
    return I::GameEntity->Instance->Get<T>(handle);
}

template <typename T = C_BaseEntity>
inline T* GetEntityFromHandle(uint32_t handle) {
    if (!handle) return nullptr;
    return I::GameEntity->Instance->Get<T>(handle);
}

// Helper to get entity by index
template <typename T = C_BaseEntity>
inline T* GetEntityByIndex(int index) {
    if (index < 0 || index > 0x7FFE) return nullptr;
    return I::EntitySystem->get_base_entity<T>(index);
}

// ---------------------------------------------------------------------------
//  Real client.dll routines for the skin apply/rebuild path. Signatures and
//  virtual indices ported from a known-working reference build. The previous
//  code resolved every one of these with a single identical placeholder
//  signature that matched nothing, so nothing rebuilt. Resolved once, lazily.
// ---------------------------------------------------------------------------
static void SkLog(const char* fmt, ...) {
    wchar_t path[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, path);
    wcscat_s(path, MAX_PATH, L"TempleWare.log");
    FILE* f = nullptr;
    if (_wfopen_s(&f, path, L"a") == 0 && f) {
        char buf[512];
        va_list ap; va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        fprintf(f, "[skins] %s\n", buf);
        fclose(f);
    }
}

namespace sc_real {
    // c_base_entity::set_model(const char* path)
    inline void SetModel(std::uintptr_t entity, const char* path) {
        static auto fn = reinterpret_cast<void(__fastcall*)(void*, const char*)>(
            M::FindPattern("client.dll", "40 53 48 83 EC ? 48 8B D9 4C 8B C2 48 8B 0D ? ? ? ? 48 8D 54 24 40"));
        if (fn && entity && path) fn(reinterpret_cast<void*>(entity), path);
    }

    // c_game_scene_node::set_mesh_group_mask(uint64 mask)
    inline void SetMeshGroupMask(std::uintptr_t sceneNode, std::uint64_t mask) {
        static auto fn = reinterpret_cast<void(__fastcall*)(void*, std::uint64_t)>(
            M::FindPattern("client.dll", "48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC ? 48 8D 99 ? ? ? ? 48 8B 71"));
        if (fn && sceneNode) fn(reinterpret_cast<void*>(sceneNode), mask);
    }

    // c_econ_entity::update_subclass() — caller sets m_nSubclassID first
    inline void UpdateSubclassFn(std::uintptr_t econEntity) {
        static auto fn = reinterpret_cast<void(__fastcall*)(void*)>(
            M::FindPattern("client.dll", "4C 8B DC 53 48 81 EC ? ? ? ? 48 8B 41"));
        if (fn && econEntity) fn(reinterpret_cast<void*>(econEntity));
    }

    // Global skin/model regeneration, called once after a batch of updates
    inline void RegenerateSkins() {
        static auto fn = reinterpret_cast<void(__fastcall*)()>(
            M::FindPattern("client.dll", "48 83 EC ? E8 ? ? ? ? 48 85 C0 0F 84 ? ? ? ? 48 8B 10"));
        if (fn) fn();
    }

    // c_hud::find_hud_element(name)
    inline std::uintptr_t FindHudElement(const char* name) {
        static auto fn = reinterpret_cast<std::uintptr_t(__fastcall*)(const char*)>(
            M::FindPattern("client.dll", "4C 8B DC 53 48 83 EC ? 48 8B 05"));
        return fn ? fn(name) : 0;
    }

    // c_hud::clear_hud_weapon_icon(hud_weapons, index, unk) — resolved from a CALL
    inline void ClearHudWeaponIcon(std::uintptr_t hudWeapons, std::int32_t index, std::int64_t unk) {
        static auto fn = []() -> std::int64_t(__fastcall*)(std::uintptr_t, std::int32_t, std::int64_t) {
            auto call = M::FindPattern("client.dll", "E8 ? ? ? ? 8B F8 C6 84 24");
            if (!call) return nullptr;
            const auto rel = *reinterpret_cast<std::int32_t*>(reinterpret_cast<std::uintptr_t>(call) + 1);
            return reinterpret_cast<std::int64_t(__fastcall*)(std::uintptr_t, std::int32_t, std::int64_t)>(
                reinterpret_cast<std::uintptr_t>(call) + 5 + rel);
        }();
        if (fn && hudWeapons) fn(hudWeapons, index, unk);
    }

    // virtuals on c_econ_entity
    inline void UpdateSkin(std::uintptr_t weapon, bool force) {
        M::GetVFunc<void(__fastcall*)(void*, bool)>(reinterpret_cast<void*>(weapon), 110)(reinterpret_cast<void*>(weapon), force);
    }
    inline void UpdateWeaponData(std::uintptr_t weapon) {
        M::GetVFunc<void*(__fastcall*)(void*)>(reinterpret_cast<void*>(weapon), 195)(reinterpret_cast<void*>(weapon));
    }
}

// Guns implementation
void Guns::OnFrameStageNotify() {
    ProcessHudClear();

    C_CSPlayerPawn* localPawn = g_ctx->local_pawn;
    CCSPlayerController* localController = g_ctx->local_controller;
    if (!localPawn || !localController) return;

    const auto weaponServices = localPawn->GetWeaponServices();
    if (!weaponServices) return;

    const auto weaponsBase = reinterpret_cast<std::uintptr_t>(weaponServices) + SchemaOffset("CPlayer_WeaponServices", "m_hMyWeapons");
    const auto weaponsSize = *reinterpret_cast<int*>(weaponsBase);
    const auto weaponsData = *reinterpret_cast<std::uintptr_t*>(weaponsBase + 0x8);

    if (!weaponsData || weaponsSize <= 0) return;

    const auto steamId = *reinterpret_cast<std::uintptr_t*>(reinterpret_cast<std::uintptr_t>(localController) + SchemaOffset("CBasePlayerController", "m_steamID"));
    const auto accountId = static_cast<std::uint32_t>(steamId & 0xffffffff);
    const auto activeHandle = *reinterpret_cast<std::uint32_t*>(reinterpret_cast<std::uintptr_t>(weaponServices) + SchemaOffset("CPlayer_WeaponServices", "m_hActiveWeapon"));

    if (m_tracked_pawn != reinterpret_cast<std::uintptr_t>(localPawn)) {
        m_applied_weapons.clear();
        m_last_active_handle = 0;
        m_tracked_pawn = reinterpret_cast<std::uintptr_t>(localPawn);
    }

    bool didApply = false;
    for (auto i = 0; i < weaponsSize; ++i) {
        const auto handle = *reinterpret_cast<std::uint32_t*>(weaponsData + i * sizeof(std::uint32_t));
        C_CSWeaponBase* weapon = GetEntityFromHandle<C_CSWeaponBase>(handle);
        if (!weapon) continue;

        const auto iv = reinterpret_cast<std::uintptr_t>(weapon) + SchemaOffset("C_EconEntity", "m_AttributeManager") + SchemaOffset("C_AttributeContainer", "m_Item");
        const auto currentDefIndex = *reinterpret_cast<std::uint16_t*>(iv + SchemaOffset("C_EconItemView", "m_iItemDefinitionIndex"));
        const auto currentDef = g_econ_item_system.FindDef(static_cast<std::int16_t>(currentDefIndex));

        if (!currentDef || currentDef->category != EconItemSystem::ItemCategory::Gun) continue;

        const auto skinIt = Config::skin_changer.skins.find(currentDefIndex);
        if (skinIt == Config::skin_changer.skins.end()) continue;

        const auto& skin = skinIt->second;
        const auto appliedIt = m_applied_weapons.find(handle);

        if (appliedIt != m_applied_weapons.end() && appliedIt->second == skin.paint_kit_id) continue;

        const auto subclassIdPtr = reinterpret_cast<std::uintptr_t>(weapon) + SchemaOffset("C_BaseEntity", "m_nSubclassID") + 0x8;
        if (!*reinterpret_cast<std::uintptr_t*>(subclassIdPtr)) continue;

        SkLog("gun apply def=%d paint=%d handle=%u", (int)currentDefIndex, skin.paint_kit_id, handle);
        Apply(reinterpret_cast<std::uintptr_t>(weapon), iv, handle, activeHandle, reinterpret_cast<std::uintptr_t>(localPawn), &skin, accountId);
        m_applied_weapons[handle] = skin.paint_kit_id;
        didApply = true;
    }

    if (didApply) sc_real::RegenerateSkins();

    if (activeHandle != m_last_active_handle) {
        m_last_active_handle = activeHandle;

        C_CSWeaponBase* activeWeapon = GetEntityFromHandle<C_CSWeaponBase>(activeHandle);
        if (activeWeapon) {
            const auto iv = reinterpret_cast<std::uintptr_t>(activeWeapon) + SchemaOffset("C_EconEntity", "m_AttributeManager") + SchemaOffset("C_AttributeContainer", "m_Item");
            const auto defIndex = *reinterpret_cast<std::uint16_t*>(iv + SchemaOffset("C_EconItemView", "m_iItemDefinitionIndex"));
            const auto def = g_econ_item_system.FindDef(static_cast<std::int16_t>(defIndex));

            if (def && def->category == EconItemSystem::ItemCategory::Gun) {
                const auto paintKitId = *reinterpret_cast<int*>(reinterpret_cast<std::uintptr_t>(activeWeapon) + SchemaOffset("C_EconEntity", "m_nFallbackPaintKit"));
                const auto pk = g_econ_item_system.FindPaintKit(paintKitId);
                UpdateViewModel(reinterpret_cast<std::uintptr_t>(localPawn), pk);
            }
        }
    }
}

void Guns::Apply(std::uintptr_t weapon, std::uintptr_t iv, std::uint32_t handle, std::uint32_t activeHandle, std::uintptr_t pawn, const Config::skin_changer_t::applied_skin* skin, std::uint32_t accountId) {
    m_pending_hud_iv = 0;

    *reinterpret_cast<std::uint32_t*>(iv + SchemaOffset("C_EconItemView", "m_iItemIDHigh")) = 0xF0000000;
    *reinterpret_cast<std::uint32_t*>(iv + SchemaOffset("C_EconItemView", "m_iItemIDLow")) = 0x10;
    *reinterpret_cast<std::uint32_t*>(iv + SchemaOffset("C_EconItemView", "m_iAccountID")) = accountId;
    *reinterpret_cast<bool*>(iv + SchemaOffset("C_EconItemView", "m_bInitialized")) = true;

    *reinterpret_cast<int*>(weapon + SchemaOffset("C_EconEntity", "m_nFallbackPaintKit")) = skin->paint_kit_id;
    *reinterpret_cast<int*>(weapon + SchemaOffset("C_EconEntity", "m_nFallbackSeed")) = skin->seed;
    *reinterpret_cast<float*>(weapon + SchemaOffset("C_EconEntity", "m_flFallbackWear")) = skin->wear;
    *reinterpret_cast<int*>(weapon + SchemaOffset("C_EconEntity", "m_nFallbackStatTrak")) = skin->stattrak ? 0 : -1;

    const auto pk = g_econ_item_system.FindPaintKit(skin->paint_kit_id);

    RebuildPaint(weapon, handle, activeHandle, pawn, pk);
    ScheduleHudClear(iv);
}

void Guns::RebuildPaint(std::uintptr_t weapon, std::uint32_t handle, std::uint32_t activeHandle, std::uintptr_t pawn, const EconItemSystem::PaintKit* pk) {
    // Mesh group mask on the weapon's own scene node (guns: old model -> 2 else 1)
    const std::uint64_t meshMask = (pk && pk->legacy_model) ? 2ull : 1ull;
    const auto sceneNode = *reinterpret_cast<std::uintptr_t*>(weapon + SchemaOffset("C_BaseEntity", "m_pGameSceneNode"));
    if (sceneNode) sc_real::SetMeshGroupMask(sceneNode, meshMask);

    // Rebuild the weapon model/skin from the fallback fields we just wrote.
    sc_real::UpdateSkin(weapon, true);
    sc_real::UpdateWeaponData(weapon);

    // Mirror onto the active view model so the first-person weapon updates too.
    if (handle == activeHandle) {
        UpdateViewModel(pawn, pk);
    }
}

void Guns::UpdateViewModel(std::uintptr_t pawn, const EconItemSystem::PaintKit* pk) {
    const std::uint64_t meshMask = (pk && pk->legacy_model) ? 2ull : 1ull;

    C_CSPlayerPawn* pawnPtr = reinterpret_cast<C_CSPlayerPawn*>(pawn);
    const auto armsHandle = pawnPtr->m_hHudModelArms();
    if (!armsHandle.valid()) return;

    C_CSPlayerPawn* arms = GetEntityFromHandle<C_CSPlayerPawn>(armsHandle);
    if (!arms) return;

    const auto armsSceneNode = arms->m_pGameSceneNode();
    if (!armsSceneNode) return;

    // Each child scene node is a HUD view model; refresh its mesh mask/skin.
    for (auto child = armsSceneNode->GetChild(); child && reinterpret_cast<std::uintptr_t>(child) > 0x10000; child = child->GetNextSibling()) {
        sc_real::SetMeshGroupMask(reinterpret_cast<std::uintptr_t>(child), meshMask);

        const auto owner = child->GetOwner();
        if (owner && reinterpret_cast<std::uintptr_t>(owner) > 0x10000) {
            const auto ownerAddr = reinterpret_cast<std::uintptr_t>(owner);
            sc_real::UpdateSkin(ownerAddr, true);
            sc_real::UpdateWeaponData(ownerAddr);
        }
    }
}

std::uintptr_t Guns::FindHudModelWeapon(std::uintptr_t pawn) {
    return 0; // superseded by the scene-node walk in UpdateViewModel
}

void Guns::ClearHudIcon(std::uintptr_t iv) {
    const auto cached = *reinterpret_cast<std::uintptr_t*>(iv + 0x200);
    if (cached) {
        *reinterpret_cast<std::uintptr_t*>(iv + 0x200) = 0;
    }

    const auto hud = sc_real::FindHudElement("HudWeaponSelection");
    if (!hud) return;

    const auto widget = hud - 0x98;
    const auto rowCount = *reinterpret_cast<int*>(widget + 80);
    if (rowCount <= 0 || rowCount > 64) return;

    const auto rowArray = *reinterpret_cast<std::uintptr_t*>(widget + 88);
    if (!rowArray) return;

    const auto rowPanel = *reinterpret_cast<std::uintptr_t*>(rowArray + 8);
    if (!rowPanel || !*reinterpret_cast<std::uintptr_t*>(rowPanel)) return;

    for (int i = rowCount - 1; i >= 0; --i)
        sc_real::ClearHudWeaponIcon(widget, i, 0);
}

void Guns::ScheduleHudClear(std::uintptr_t iv) {
    if (m_pending_hud_iv) {
        ClearHudIcon(m_pending_hud_iv);
    }

    ClearHudIcon(iv);

    m_pending_hud_iv = iv;
    m_hud_clear_time = std::chrono::steady_clock::now();
}

void Guns::ProcessHudClear() {
    if (!m_pending_hud_iv) return;

    const auto elapsed = std::chrono::steady_clock::now() - m_hud_clear_time;
    if (elapsed < std::chrono::milliseconds(250)) return;

    ClearHudIcon(m_pending_hud_iv);
    m_pending_hud_iv = 0;
}

// Knives implementation
std::uint32_t Knives::MakeSubclassToken(std::int16_t def_index) {
    const auto s = std::to_string(def_index);
    const char* str = s.c_str();
    int len = static_cast<int>(s.length());

    constexpr auto m = 0x5bd1e995u;
    constexpr auto r = 24;

    auto h = 0x31415926u ^ len;
    auto i = 0;

    while (len >= 4) {
        auto k = static_cast<std::uint32_t>(
            (str[i] >= 'A' && str[i] <= 'Z' ? str[i] + 32 : str[i]) |
            ((str[i + 1] >= 'A' && str[i + 1] <= 'Z' ? str[i + 1] + 32 : str[i + 1]) << 8) |
            ((str[i + 2] >= 'A' && str[i + 2] <= 'Z' ? str[i + 2] + 32 : str[i + 2]) << 16) |
            ((str[i + 3] >= 'A' && str[i + 3] <= 'Z' ? str[i + 3] + 32 : str[i + 3]) << 24)
        );

        k *= m;
        k ^= k >> r;
        k *= m;

        h *= m;
        h ^= k;

        i += 4;
        len -= 4;
    }

    switch (len) {
    case 3: h ^= static_cast<std::uint32_t>(str[i + 2] >= 'A' && str[i + 2] <= 'Z' ? str[i + 2] + 32 : str[i + 2]) << 16; [[fallthrough]];
    case 2: h ^= static_cast<std::uint32_t>(str[i + 1] >= 'A' && str[i + 1] <= 'Z' ? str[i + 1] + 32 : str[i + 1]) << 8; [[fallthrough]];
    case 1: h ^= static_cast<std::uint32_t>(str[i] >= 'A' && str[i] <= 'Z' ? str[i] + 32 : str[i]); h *= m;
    }

    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return h;
}

void Knives::OnFrameStageNotify() {
    ProcessHudClear();

    C_CSPlayerPawn* localPawn = g_ctx->local_pawn;
    CCSPlayerController* localController = g_ctx->local_controller;
    if (!localPawn || !localController) return;

    const auto weaponServices = localPawn->GetWeaponServices();
    if (!weaponServices) return;

    const Config::skin_changer_t::applied_skin* selectedSkin = nullptr;
    const EconItemSystem::ItemDef* selectedKnifeDef = nullptr;

    for (const auto& [defIdx, skin] : Config::skin_changer.skins) {
        const auto def = g_econ_item_system.FindDef(defIdx);
        if (!def || def->category != EconItemSystem::ItemCategory::Knife) continue;

        selectedSkin = &skin;
        selectedKnifeDef = def;
        break;
    }

    const auto weaponsBase = reinterpret_cast<std::uintptr_t>(weaponServices) + SchemaOffset("CPlayer_WeaponServices", "m_hMyWeapons");
    const auto weaponsSize = *reinterpret_cast<int*>(weaponsBase);
    const auto weaponsData = *reinterpret_cast<std::uintptr_t*>(weaponsBase + 0x8);

    if (!weaponsData || weaponsSize <= 0) return;

    const auto activeHandle = *reinterpret_cast<std::uint32_t*>(reinterpret_cast<std::uintptr_t>(weaponServices) + SchemaOffset("CPlayer_WeaponServices", "m_hActiveWeapon"));
    C_CSWeaponBase* activeWeapon = GetEntityFromHandle<C_CSWeaponBase>(activeHandle);

    if (m_tracked_pawn != reinterpret_cast<std::uintptr_t>(localPawn)) {
        m_original = {};
        m_overridden = false;
        m_last_active_handle = 0;
        m_tracked_pawn = reinterpret_cast<std::uintptr_t>(localPawn);
    }

    for (auto i = 0; i < weaponsSize; ++i) {
        const auto handle = *reinterpret_cast<std::uint32_t*>(weaponsData + i * sizeof(std::uint32_t));
        C_CSWeaponBase* weapon = GetEntityFromHandle<C_CSWeaponBase>(handle);
        if (!weapon) continue;

        const auto iv = reinterpret_cast<std::uintptr_t>(weapon) + SchemaOffset("C_EconEntity", "m_AttributeManager") + SchemaOffset("C_AttributeContainer", "m_Item");
        const auto currentDefIndex = *reinterpret_cast<std::uint16_t*>(iv + SchemaOffset("C_EconItemView", "m_iItemDefinitionIndex"));
        const auto currentDef = g_econ_item_system.FindDef(static_cast<std::int16_t>(currentDefIndex));

        if (!currentDef || currentDef->category != EconItemSystem::ItemCategory::Knife) continue;

        const auto subclassIdPtr = reinterpret_cast<std::uintptr_t>(weapon) + SchemaOffset("C_BaseEntity", "m_nSubclassID") + 0x8;
        if (!*reinterpret_cast<std::uintptr_t*>(subclassIdPtr)) continue;

        if (!selectedKnifeDef) {
            Restore(reinterpret_cast<std::uintptr_t>(weapon), iv, reinterpret_cast<std::uintptr_t>(activeWeapon), reinterpret_cast<std::uintptr_t>(localPawn));
            break;
        }

        if (!m_overridden) {
            CaptureOriginal(reinterpret_cast<std::uintptr_t>(weapon), iv);
        }

        const auto targetToken = MakeSubclassToken(selectedKnifeDef->def_index);
        const auto currentSubclass = *reinterpret_cast<std::uint32_t*>(reinterpret_cast<std::uintptr_t>(weapon) + SchemaOffset("C_BaseEntity", "m_nSubclassID"));
        const auto currentPk = *reinterpret_cast<int*>(reinterpret_cast<std::uintptr_t>(weapon) + SchemaOffset("C_EconEntity", "m_nFallbackPaintKit"));

        if (currentSubclass == targetToken && currentPk == selectedSkin->paint_kit_id) break;

        const auto steamId = *reinterpret_cast<std::uintptr_t*>(reinterpret_cast<std::uintptr_t>(localController) + SchemaOffset("CBasePlayerController", "m_steamID"));
        const auto accountId = static_cast<std::uint32_t>(steamId & 0xffffffff);

        SkLog("knife apply def=%d paint=%d", (int)selectedKnifeDef->def_index, selectedSkin->paint_kit_id);
        Apply(reinterpret_cast<std::uintptr_t>(weapon), iv, selectedKnifeDef, selectedSkin, accountId, reinterpret_cast<std::uintptr_t>(activeWeapon), reinterpret_cast<std::uintptr_t>(localPawn));
        sc_real::RegenerateSkins();
        break;
    }

    if (activeHandle != m_last_active_handle) {
        m_last_active_handle = activeHandle;

        if (m_overridden && activeWeapon) {
            const auto iv = reinterpret_cast<std::uintptr_t>(activeWeapon) + SchemaOffset("C_EconEntity", "m_AttributeManager") + SchemaOffset("C_AttributeContainer", "m_Item");
            const auto defIndex = *reinterpret_cast<std::uint16_t*>(iv + SchemaOffset("C_EconItemView", "m_iItemDefinitionIndex"));
            const auto def = g_econ_item_system.FindDef(static_cast<std::int16_t>(defIndex));

            if (def && def->category == EconItemSystem::ItemCategory::Knife) {
                const auto paintKitId = *reinterpret_cast<int*>(reinterpret_cast<std::uintptr_t>(activeWeapon) + SchemaOffset("C_EconEntity", "m_nFallbackPaintKit"));
                const auto pk = g_econ_item_system.FindPaintKit(paintKitId);
                UpdateViewModel(reinterpret_cast<std::uintptr_t>(localPawn), pk);
            }
        }
    }
}

void Knives::CaptureOriginal(std::uintptr_t weapon, std::uintptr_t iv) {
    if (m_original.captured) return;

    m_original.def_index = *reinterpret_cast<std::uint16_t*>(iv + SchemaOffset("C_EconItemView", "m_iItemDefinitionIndex"));
    m_original.id_high = *reinterpret_cast<std::uint32_t*>(iv + SchemaOffset("C_EconItemView", "m_iItemIDHigh"));
    m_original.id_low = *reinterpret_cast<std::uint32_t*>(iv + SchemaOffset("C_EconItemView", "m_iItemIDLow"));
    m_original.account_id = *reinterpret_cast<std::uint32_t*>(iv + SchemaOffset("C_EconItemView", "m_iAccountID"));
    m_original.initialized = *reinterpret_cast<bool*>(iv + SchemaOffset("C_EconItemView", "m_bInitialized"));
    m_original.paint_kit = *reinterpret_cast<int*>(weapon + SchemaOffset("C_EconEntity", "m_nFallbackPaintKit"));
    m_original.seed = *reinterpret_cast<int*>(weapon + SchemaOffset("C_EconEntity", "m_nFallbackSeed"));
    m_original.wear = *reinterpret_cast<float*>(weapon + SchemaOffset("C_EconEntity", "m_flFallbackWear"));
    m_original.stattrak = *reinterpret_cast<int*>(weapon + SchemaOffset("C_EconEntity", "m_nFallbackStatTrak"));
    m_original.captured = true;
}

void Knives::Apply(std::uintptr_t weapon, std::uintptr_t iv, const EconItemSystem::ItemDef* def, const Config::skin_changer_t::applied_skin* skin, std::uint32_t accountId, std::uintptr_t activeWeapon, std::uintptr_t pawn) {
    m_pending_hud_iv = 0;

    *reinterpret_cast<std::uint16_t*>(iv + SchemaOffset("C_EconItemView", "m_iItemDefinitionIndex")) = static_cast<std::uint16_t>(def->def_index);
    *reinterpret_cast<std::uint32_t*>(iv + SchemaOffset("C_EconItemView", "m_iItemIDHigh")) = 0xF0000000;
    *reinterpret_cast<std::uint32_t*>(iv + SchemaOffset("C_EconItemView", "m_iItemIDLow")) = 0x10;
    *reinterpret_cast<std::uint32_t*>(iv + SchemaOffset("C_EconItemView", "m_iAccountID")) = accountId;
    *reinterpret_cast<bool*>(iv + SchemaOffset("C_EconItemView", "m_bInitialized")) = true;

    *reinterpret_cast<int*>(weapon + SchemaOffset("C_EconEntity", "m_nFallbackPaintKit")) = skin->paint_kit_id;
    *reinterpret_cast<int*>(weapon + SchemaOffset("C_EconEntity", "m_nFallbackSeed")) = skin->seed;
    *reinterpret_cast<float*>(weapon + SchemaOffset("C_EconEntity", "m_flFallbackWear")) = skin->wear;
    *reinterpret_cast<int*>(weapon + SchemaOffset("C_EconEntity", "m_nFallbackStatTrak")) = skin->stattrak ? 0 : -1;

    const auto pk = g_econ_item_system.FindPaintKit(skin->paint_kit_id);

    UpdateModel(weapon, iv, static_cast<std::uint16_t>(def->def_index));
    RebuildPaint(weapon, activeWeapon, pawn, pk);
    ScheduleHudClear(iv);

    m_overridden = true;
}

void Knives::Restore(std::uintptr_t weapon, std::uintptr_t iv, std::uintptr_t activeWeapon, std::uintptr_t pawn) {
    if (!m_overridden || !m_original.captured) return;

    m_pending_hud_iv = 0;

    *reinterpret_cast<std::uint16_t*>(iv + SchemaOffset("C_EconItemView", "m_iItemDefinitionIndex")) = m_original.def_index;
    *reinterpret_cast<std::uint32_t*>(iv + SchemaOffset("C_EconItemView", "m_iItemIDHigh")) = m_original.id_high;
    *reinterpret_cast<std::uint32_t*>(iv + SchemaOffset("C_EconItemView", "m_iItemIDLow")) = m_original.id_low;
    *reinterpret_cast<std::uint32_t*>(iv + SchemaOffset("C_EconItemView", "m_iAccountID")) = m_original.account_id;
    *reinterpret_cast<bool*>(iv + SchemaOffset("C_EconItemView", "m_bInitialized")) = m_original.initialized;

    *reinterpret_cast<int*>(weapon + SchemaOffset("C_EconEntity", "m_nFallbackPaintKit")) = m_original.paint_kit;
    *reinterpret_cast<int*>(weapon + SchemaOffset("C_EconEntity", "m_nFallbackSeed")) = m_original.seed;
    *reinterpret_cast<float*>(weapon + SchemaOffset("C_EconEntity", "m_flFallbackWear")) = m_original.wear;
    *reinterpret_cast<int*>(weapon + SchemaOffset("C_EconEntity", "m_nFallbackStatTrak")) = m_original.stattrak;

    const auto pk = g_econ_item_system.FindPaintKit(m_original.paint_kit);

    UpdateModel(weapon, iv, m_original.def_index);
    RebuildPaint(weapon, activeWeapon, pawn, pk);
    ScheduleHudClear(iv);

    m_overridden = false;
}

void Knives::UpdateModel(std::uintptr_t weapon, std::uintptr_t iv, std::uint16_t defIndex) {
    // Set the subclass id (hash of the decimal def index) then invoke the real
    // update-subclass routine, then swap the world/view model to the knife's.
    const auto token = MakeSubclassToken(static_cast<std::int16_t>(defIndex));
    *reinterpret_cast<std::uint32_t*>(weapon + SchemaOffset("C_BaseEntity", "m_nSubclassID")) = token;
    sc_real::UpdateSubclassFn(weapon);

    const auto def = g_econ_item_system.FindDef(static_cast<std::int16_t>(defIndex));
    if (def && !def->model_player.empty()) {
        sc_real::SetModel(weapon, def->model_player.c_str());
        SkLog("knife set_model def=%d model=%s", (int)defIndex, def->model_player.c_str());
    } else {
        SkLog("knife UpdateModel def=%d NO MODEL PATH", (int)defIndex);
    }
}

void Knives::RebuildPaint(std::uintptr_t weapon, std::uintptr_t activeWeapon, std::uintptr_t pawn, const EconItemSystem::PaintKit* pk) {
    // Knife mesh mask (knives: old model -> 1 else 2)
    const std::uint64_t meshMask = (pk && pk->legacy_model) ? 1ull : 2ull;
    const auto sceneNode = *reinterpret_cast<std::uintptr_t*>(weapon + SchemaOffset("C_BaseEntity", "m_pGameSceneNode"));
    if (sceneNode) sc_real::SetMeshGroupMask(sceneNode, meshMask);

    sc_real::UpdateSkin(weapon, true);
    sc_real::UpdateWeaponData(weapon);

    if (weapon == activeWeapon) {
        UpdateViewModel(pawn, pk);
    }
}

void Knives::UpdateViewModel(std::uintptr_t pawn, const EconItemSystem::PaintKit* pk) {
    const std::uint64_t meshMask = (pk && pk->legacy_model) ? 1ull : 2ull;

    C_CSPlayerPawn* pawnPtr = reinterpret_cast<C_CSPlayerPawn*>(pawn);
    const auto armsHandle = pawnPtr->m_hHudModelArms();
    if (!armsHandle.valid()) return;

    C_CSPlayerPawn* arms = GetEntityFromHandle<C_CSPlayerPawn>(armsHandle);
    if (!arms) return;

    const auto armsSceneNode = arms->m_pGameSceneNode();
    if (!armsSceneNode) return;

    for (auto child = armsSceneNode->GetChild(); child && reinterpret_cast<std::uintptr_t>(child) > 0x10000; child = child->GetNextSibling()) {
        sc_real::SetMeshGroupMask(reinterpret_cast<std::uintptr_t>(child), meshMask);

        const auto owner = child->GetOwner();
        if (owner && reinterpret_cast<std::uintptr_t>(owner) > 0x10000) {
            const auto ownerAddr = reinterpret_cast<std::uintptr_t>(owner);
            sc_real::UpdateSkin(ownerAddr, true);
            sc_real::UpdateWeaponData(ownerAddr);
        }
    }
}

std::uintptr_t Knives::FindHudModelWeapon(std::uintptr_t pawn) {
    return 0; // superseded by the scene-node walk in UpdateViewModel
}

void Knives::ClearHudIcon(std::uintptr_t iv) {
    const auto cached = *reinterpret_cast<std::uintptr_t*>(iv + 0x200);
    if (cached) {
        *reinterpret_cast<std::uintptr_t*>(iv + 0x200) = 0;
    }

    const auto hud = sc_real::FindHudElement("HudWeaponSelection");
    if (!hud) return;

    const auto widget = hud - 0x98;
    const auto rowCount = *reinterpret_cast<int*>(widget + 80);
    if (rowCount <= 0 || rowCount > 64) return;

    const auto rowArray = *reinterpret_cast<std::uintptr_t*>(widget + 88);
    if (!rowArray) return;

    const auto rowPanel = *reinterpret_cast<std::uintptr_t*>(rowArray + 8);
    if (!rowPanel || !*reinterpret_cast<std::uintptr_t*>(rowPanel)) return;

    for (int i = rowCount - 1; i >= 0; --i)
        sc_real::ClearHudWeaponIcon(widget, i, 0);
}

void Knives::ScheduleHudClear(std::uintptr_t iv) {
    if (m_pending_hud_iv) {
        ClearHudIcon(m_pending_hud_iv);
    }

    ClearHudIcon(iv);

    m_pending_hud_iv = iv;
    m_hud_clear_time = std::chrono::steady_clock::now();
}

void Knives::ProcessHudClear() {
    if (!m_pending_hud_iv) return;

    const auto elapsed = std::chrono::steady_clock::now() - m_hud_clear_time;
    if (elapsed < std::chrono::milliseconds(250)) return;

    ClearHudIcon(m_pending_hud_iv);
    m_pending_hud_iv = 0;
}

// Gloves implementation (placeholder)
void Gloves::OnFrameStageNotify() {
    // TODO: Implement glove changing
}

// Agents implementation
void Agents::OnFrameStageNotify() {
    C_CSPlayerPawn* localPawn = g_ctx->local_pawn;
    if (!localPawn) return;

    const auto team = localPawn->getTeam();
    const auto selectedDefIndex = (team == 3) ? Config::skin_changer.ct_agent : (team == 2) ? Config::skin_changer.t_agent : static_cast<std::int16_t>(0);

    const EconItemSystem::ItemDef* selected = nullptr;
    if (selectedDefIndex != 0) {
        selected = g_econ_item_system.FindDef(selectedDefIndex);
    }

    if (m_tracked_pawn != reinterpret_cast<std::uintptr_t>(localPawn)) {
        m_original_model.clear();
        m_overridden = false;
        m_applied_handle = 0;
        m_applied_def = 0;
        m_tracked_team = 0;
        m_tracked_pawn = reinterpret_cast<std::uintptr_t>(localPawn);
    }

    const auto gameSceneNode = localPawn->m_pGameSceneNode();
    if (!gameSceneNode) return;

    const auto modelState = reinterpret_cast<std::uintptr_t>(gameSceneNode) + SchemaOffset("CSkeletonInstance", "m_modelState");

    if (!selected || selected->model_player.empty()) {
        if (m_overridden && !m_original_model.empty()) {
            sc_real::SetModel(reinterpret_cast<std::uintptr_t>(localPawn), m_original_model.c_str());

            CycleWeaponOwners(reinterpret_cast<std::uintptr_t>(localPawn));

            m_applied_handle = 0;
            m_applied_def = 0;
            m_tracked_team = 0;
            m_overridden = false;
        }
        return;
    }

    if (!m_overridden && m_original_model.empty()) {
        const auto modelNamePtr = *reinterpret_cast<std::uintptr_t*>(modelState + SchemaOffset("CModelState", "m_ModelName"));
        if (modelNamePtr) {
            m_original_model = *reinterpret_cast<const char**>(modelNamePtr);
        }
    }

    const auto currentHandle = *reinterpret_cast<std::uintptr_t*>(modelState + SchemaOffset("CModelState", "m_hModel"));
    const auto selectionMatches = (m_applied_def == selected->def_index);
    const auto teamMatches = (m_tracked_team == team);
    const auto handleMatches = (m_applied_handle != 0 && currentHandle == m_applied_handle);

    if (m_overridden && selectionMatches && teamMatches && handleMatches) return;

    if (m_overridden && !teamMatches) {
        m_original_model.clear();
        m_overridden = false;

        const auto modelNamePtr = *reinterpret_cast<std::uintptr_t*>(modelState + SchemaOffset("CModelState", "m_ModelName"));
        if (modelNamePtr) {
            m_original_model = *reinterpret_cast<const char**>(modelNamePtr);
        }
    }

    sc_real::SetModel(reinterpret_cast<std::uintptr_t>(localPawn), selected->model_player.c_str());

    const auto collision = reinterpret_cast<std::uintptr_t>(localPawn) + SchemaOffset("C_BaseModelEntity", "m_Collision");
    *reinterpret_cast<Vector_t*>(collision + SchemaOffset("CCollisionProperty", "m_vecMins")) = Vector_t(-16.0f, -16.0f, 0.0f);
    *reinterpret_cast<Vector_t*>(collision + SchemaOffset("CCollisionProperty", "m_vecMaxs")) = Vector_t(16.0f, 16.0f, 72.0f);

    CycleWeaponOwners(reinterpret_cast<std::uintptr_t>(localPawn));

    m_applied_handle = *reinterpret_cast<std::uintptr_t*>(modelState + SchemaOffset("CModelState", "m_hModel"));
    m_applied_def = selected->def_index;
    m_tracked_team = team;
    m_overridden = true;
}

void Agents::CycleWeaponOwners(std::uintptr_t pawn) {
    C_CSPlayerPawn* pawnPtr = reinterpret_cast<C_CSPlayerPawn*>(pawn);
    const auto weaponServices = pawnPtr->GetWeaponServices();
    if (!weaponServices) return;

    const auto weaponsBase = reinterpret_cast<std::uintptr_t>(weaponServices) + SchemaOffset("CPlayer_WeaponServices", "m_hMyWeapons");
    const auto weaponsSize = *reinterpret_cast<int*>(weaponsBase);
    const auto weaponsData = *reinterpret_cast<std::uintptr_t*>(weaponsBase + 0x8);

    if (!weaponsData || weaponsSize <= 0) return;

    for (auto i = 0; i < weaponsSize; ++i) {
        const auto handle = *reinterpret_cast<std::uint32_t*>(weaponsData + i * sizeof(std::uint32_t));
        C_CSWeaponBase* weapon = GetEntityFromHandle<C_CSWeaponBase>(handle);
        if (!weapon) continue;

        const auto ownerOff = SchemaOffset("C_BaseEntity", "m_hOwnerEntity");
        const auto savedOwner = *reinterpret_cast<std::uint32_t*>(reinterpret_cast<std::uintptr_t>(weapon) + ownerOff);

        *reinterpret_cast<std::uint32_t*>(reinterpret_cast<std::uintptr_t>(weapon) + ownerOff) = 0xFFFFFFFF;
        *reinterpret_cast<std::uint32_t*>(reinterpret_cast<std::uintptr_t>(weapon) + ownerOff) = savedOwner;
    }
}

} // namespace features::skinchanger