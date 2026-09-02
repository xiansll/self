#include "localplayer.h"
#include "../../../templeware/globals/globals.h"
#include "../../../templeware/interfaces/interfaces.h"
#include "../../../templeware/interfaces/CGameEntitySystem/CGameEntitySystem.h"
#include "../../../templeware/interfaces/IEngineClient/IEngineClient.h"

void LocalPlayerCache::update() {
    std::unique_lock lock(m_mutex);

    if (!I::EngineClient || !I::EngineClient->connected() || !I::EngineClient->in_game()) {
        m_snapshot = {};
        m_is_deathmatch.store(false);
        m_is_in_cinematic.store(false);
        m_is_in_time_freeze.store(false);
        return;
    }

    C_CSPlayerPawn* local_pawn = I::GameEntity->Instance->get_local_pawn();
    void* local_controller_void = I::GameEntity->Instance->get_local_controller();
    CCSPlayerController* local_controller = reinterpret_cast<CCSPlayerController*>(local_controller_void);

    m_snapshot.controller = reinterpret_cast<std::uintptr_t>(local_controller);
    m_snapshot.pawn = reinterpret_cast<std::uintptr_t>(local_pawn);

    if (local_pawn) {
        m_snapshot.team = local_pawn->m_iTeamNum();
        m_snapshot.is_alive = local_pawn->is_alive();
        m_snapshot.is_team_mode = true;
    } else {
        m_snapshot.team = 0;
        m_snapshot.is_alive = false;
        m_snapshot.is_team_mode = false;
    }

    if (local_controller) {
        if (auto observer_pawn_handle = local_controller->m_hObserverPawn(); observer_pawn_handle.valid()) {
            if (auto observer_pawn = I::GameEntity->Instance->Get<C_CSPlayerPawn>(observer_pawn_handle)) {
                m_snapshot.observer_pawn = reinterpret_cast<std::uintptr_t>(observer_pawn);
                if (auto observer_controller_handle = observer_pawn->m_hController(); observer_controller_handle.valid()) {
                    if (auto observer_controller = I::GameEntity->Instance->Get<CCSPlayerController>(observer_controller_handle)) {
                        m_snapshot.observer_controller = reinterpret_cast<std::uintptr_t>(observer_controller);
                    }
                }
            }
        }
        m_snapshot.view_team = local_controller->m_iDesiredTeam();
    }

    m_is_deathmatch.store(false);
    m_is_in_cinematic.store(false);
    m_is_in_time_freeze.store(false);
}

void LocalPlayerCache::reset() {
    std::unique_lock lock(m_mutex);
    m_snapshot = {};
    m_is_deathmatch.store(false);
    m_is_in_cinematic.store(false);
    m_is_in_time_freeze.store(false);
}