#pragma once
#include "econ_item_system.h"
#include "../../config/config.h"
#include "../../globals/globals.h"
#include "../../utils/schema/schema.h"
#include "../../interfaces/interfaces.h"

namespace features::skinchanger {

class Guns {
public:
    void OnFrameStageNotify();

private:
    void Apply(std::uintptr_t weapon, std::uintptr_t iv, std::uint32_t handle, std::uint32_t active_handle, std::uintptr_t pawn, const Config::skin_changer_t::applied_skin* skin, std::uint32_t account_id);
    void RebuildPaint(std::uintptr_t weapon, std::uint32_t handle, std::uint32_t active_handle, std::uintptr_t pawn, const EconItemSystem::PaintKit* pk);
    void UpdateViewModel(std::uintptr_t pawn, const EconItemSystem::PaintKit* pk);
    [[nodiscard]] std::uintptr_t FindHudModelWeapon(std::uintptr_t pawn);
    void ClearHudIcon(std::uintptr_t iv);
    void ScheduleHudClear(std::uintptr_t iv);
    void ProcessHudClear();

    std::uint32_t m_last_active_handle{};
    std::uintptr_t m_tracked_pawn{};
    std::unordered_map<std::uint32_t, int> m_applied_weapons{};
    std::uintptr_t m_pending_hud_iv{};
    std::chrono::steady_clock::time_point m_hud_clear_time{};
};

class Knives {
public:
    void OnFrameStageNotify();

private:
    struct OriginalState {
        std::uint16_t def_index{};
        std::uint32_t id_high{};
        std::uint32_t id_low{};
        std::uint32_t account_id{};
        bool initialized{};
        int paint_kit{};
        int seed{};
        float wear{};
        int stattrak{};
        bool captured{};
    };

    void CaptureOriginal(std::uintptr_t weapon, std::uintptr_t iv);
    void Apply(std::uintptr_t weapon, std::uintptr_t iv, const EconItemSystem::ItemDef* def, const Config::skin_changer_t::applied_skin* skin, std::uint32_t account_id, std::uintptr_t active_weapon, std::uintptr_t pawn);
    void Restore(std::uintptr_t weapon, std::uintptr_t iv, std::uintptr_t active_weapon, std::uintptr_t pawn);
    void UpdateModel(std::uintptr_t weapon, std::uintptr_t iv, std::uint16_t def_index);
    void UpdateViewModel(std::uintptr_t pawn, const EconItemSystem::PaintKit* pk);
    [[nodiscard]] std::uintptr_t FindHudModelWeapon(std::uintptr_t pawn);
    void RebuildPaint(std::uintptr_t weapon, std::uintptr_t active_weapon, std::uintptr_t pawn, const EconItemSystem::PaintKit* pk);
    void ClearHudIcon(std::uintptr_t iv);
    void ScheduleHudClear(std::uintptr_t iv);
    void ProcessHudClear();

    [[nodiscard]] static std::uint32_t MakeSubclassToken(std::int16_t def_index);

    OriginalState m_original{};
    std::uint32_t m_last_active_handle{};
    std::uintptr_t m_tracked_pawn{};
    bool m_overridden{};
    std::uintptr_t m_pending_hud_iv{};
    std::chrono::steady_clock::time_point m_hud_clear_time{};
};

class Gloves {
public:
    void OnFrameStageNotify();
};

class Agents {
public:
    void OnFrameStageNotify();

private:
    void CycleWeaponOwners(std::uintptr_t pawn);

    std::string m_original_model{};
    std::uintptr_t m_tracked_pawn{};
    std::uintptr_t m_applied_handle{};
    std::int16_t m_applied_def{};
    bool m_overridden{};
    int m_tracked_team{};
};

inline Guns g_guns;
inline Knives g_knives;
inline Gloves g_gloves;
inline Agents g_agents;

} // namespace features::skinchanger