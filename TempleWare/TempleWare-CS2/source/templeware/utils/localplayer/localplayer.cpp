#include "localplayer.h"
#include "../../../templeware/globals/globals.h"
#include "../../../templeware/interfaces/interfaces.h"
#include "../../../templeware/interfaces/CGameEntitySystem/CGameEntitySystem.h"
#include "../../../templeware/interfaces/IEngineClient/IEngineClient.h"
#include "../../../templeware/utils/filelog/filelog.h"

#include <chrono>
#include <cstdio>

namespace {
    enum class LocalProvider : int {
        None = 0,
        EntitySystem = 1,
        GameResource = 2,
        Mixed = 3
    };

    void LogProviderState(LocalProvider provider,
                          C_CSPlayerPawn* entitySystemPawn,
                          CCSPlayerController* entitySystemController,
                          C_CSPlayerPawn* gameResourcePawn,
                          CCSPlayerController* gameResourceController) {
        using namespace std::chrono;
        static steady_clock::time_point lastLog{};
        static LocalProvider lastProvider = LocalProvider::None;

        const auto now = steady_clock::now();
        const bool providerChanged = provider != lastProvider;
        const bool intervalElapsed = lastLog.time_since_epoch().count() == 0 ||
            duration_cast<milliseconds>(now - lastLog).count() >= 1000;

        if (!providerChanged && !intervalElapsed)
            return;

        char buf[512];
        std::snprintf(buf, sizeof(buf),
            "[Validation] LocalProvider: selected=%d I::EntitySystem=%p es_pawn=%p es_ctrl=%p GameEntity=%p GameEntity.Instance=%p gr_pawn=%p gr_ctrl=%p",
            static_cast<int>(provider),
            static_cast<void*>(I::EntitySystem),
            static_cast<void*>(entitySystemPawn),
            static_cast<void*>(entitySystemController),
            static_cast<void*>(I::GameEntity),
            I::GameEntity ? static_cast<void*>(I::GameEntity->Instance) : nullptr,
            static_cast<void*>(gameResourcePawn),
            static_cast<void*>(gameResourceController));
        FileLog::Log(buf);

        lastProvider = provider;
        lastLog = now;
    }
}

void LocalPlayerCache::update() {
    std::unique_lock lock(m_mutex);

    if (!I::EngineClient || !I::EngineClient->connected() || !I::EngineClient->in_game()) {
        m_snapshot = {};
        m_is_deathmatch.store(false);
        m_is_in_cinematic.store(false);
        m_is_in_time_freeze.store(false);
        return;
    }

    C_CSPlayerPawn* entitySystemPawn = nullptr;
    CCSPlayerController* entitySystemController = nullptr;
    if (I::EntitySystem) {
        entitySystemPawn = I::EntitySystem->get_local_pawn();
        entitySystemController = reinterpret_cast<CCSPlayerController*>(I::EntitySystem->get_local_controller());
    }

    C_CSPlayerPawn* gameResourcePawn = nullptr;
    CCSPlayerController* gameResourceController = nullptr;
    if (I::GameEntity && I::GameEntity->Instance) {
        gameResourcePawn = I::GameEntity->Instance->get_local_pawn();
        gameResourceController = reinterpret_cast<CCSPlayerController*>(I::GameEntity->Instance->get_local_controller());
    }

    // Prefer the independently resolved EntitySystem path already present in
    // TempleWare. Fall back per-pointer to the GameResourceService path. This
    // avoids treating one broken local accessor as proof that the entire cache
    // is unavailable, while still keeping both existing providers observable.
    C_CSPlayerPawn* local_pawn = entitySystemPawn ? entitySystemPawn : gameResourcePawn;
    CCSPlayerController* local_controller = entitySystemController ? entitySystemController : gameResourceController;

    LocalProvider provider = LocalProvider::None;
    if (local_pawn || local_controller) {
        const bool usedEntitySystem =
            (local_pawn && local_pawn == entitySystemPawn) ||
            (local_controller && local_controller == entitySystemController);
        const bool usedGameResource =
            (local_pawn && local_pawn == gameResourcePawn && !entitySystemPawn) ||
            (local_controller && local_controller == gameResourceController && !entitySystemController);

        if (usedEntitySystem && usedGameResource)
            provider = LocalProvider::Mixed;
        else if (usedEntitySystem)
            provider = LocalProvider::EntitySystem;
        else if (usedGameResource)
            provider = LocalProvider::GameResource;
    }

    LogProviderState(provider,
                     entitySystemPawn,
                     entitySystemController,
                     gameResourcePawn,
                     gameResourceController);

    m_snapshot.controller = reinterpret_cast<std::uintptr_t>(local_controller);
    m_snapshot.pawn = reinterpret_cast<std::uintptr_t>(local_pawn);
    m_snapshot.observer_pawn = 0;
    m_snapshot.observer_controller = 0;

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
            C_CSPlayerPawn* observer_pawn = nullptr;
            if (I::GameEntity && I::GameEntity->Instance)
                observer_pawn = I::GameEntity->Instance->Get<C_CSPlayerPawn>(observer_pawn_handle);
            if (!observer_pawn && I::EntitySystem)
                observer_pawn = I::EntitySystem->get_base_entity<C_CSPlayerPawn>(observer_pawn_handle.index());

            if (observer_pawn) {
                m_snapshot.observer_pawn = reinterpret_cast<std::uintptr_t>(observer_pawn);
                if (auto observer_controller_handle = observer_pawn->m_hController(); observer_controller_handle.valid()) {
                    CCSPlayerController* observer_controller = nullptr;
                    if (I::GameEntity && I::GameEntity->Instance)
                        observer_controller = I::GameEntity->Instance->Get<CCSPlayerController>(observer_controller_handle);
                    if (!observer_controller && I::EntitySystem)
                        observer_controller = I::EntitySystem->get_base_entity<CCSPlayerController>(observer_controller_handle.index());

                    if (observer_controller)
                        m_snapshot.observer_controller = reinterpret_cast<std::uintptr_t>(observer_controller);
                }
            }
        }
        m_snapshot.view_team = local_controller->m_iDesiredTeam();
    } else {
        m_snapshot.view_team = 0;
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