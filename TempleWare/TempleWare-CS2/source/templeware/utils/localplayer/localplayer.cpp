#include "localplayer.h"
#include "../../../templeware/globals/globals.h"
#include "../../../templeware/interfaces/interfaces.h"
#include "../../../templeware/interfaces/CGameEntitySystem/CGameEntitySystem.h"
#include "../../../templeware/interfaces/IEngineClient/IEngineClient.h"
#include "../../../templeware/utils/filelog/filelog.h"

#include <Windows.h>
#include <chrono>
#include <cstdio>

namespace {
    enum class LocalProvider : int {
        None = 0,
        EntitySystem = 1,
        GameResource = 2,
        Mixed = 3,
        ClientGlobals = 4
    };

    // Same already-existing runtime source used by nerv_bridge.cpp.
    constexpr std::uintptr_t kLocalPlayerPawnOffset = 0x23C6268;
    constexpr std::uintptr_t kLocalPlayerControllerOffset = 0x23A0F30;

    void ReadClientGlobalLocals(C_CSPlayerPawn*& pawn,
                                CCSPlayerController*& controller,
                                std::uintptr_t& clientBase) {
        pawn = nullptr;
        controller = nullptr;
        clientBase = reinterpret_cast<std::uintptr_t>(GetModuleHandleA("client.dll"));
        if (!clientBase)
            return;

        __try {
            pawn = *reinterpret_cast<C_CSPlayerPawn**>(clientBase + kLocalPlayerPawnOffset);
            controller = *reinterpret_cast<CCSPlayerController**>(clientBase + kLocalPlayerControllerOffset);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            pawn = nullptr;
            controller = nullptr;
        }
    }

    void LogProviderState(LocalProvider provider,
                          bool resolverPairProven,
                          bool wrapperSemanticsProven,
                          bool sdkDerefSafe,
                          bool deepGraphSafe,
                          C_CSPlayerPawn* entitySystemPawn,
                          CCSPlayerController* entitySystemController,
                          C_CSPlayerPawn* gameResourcePawn,
                          CCSPlayerController* gameResourceController,
                          C_CSPlayerPawn* clientGlobalPawn,
                          CCSPlayerController* clientGlobalController,
                          std::uintptr_t clientBase) {
        using namespace std::chrono;
        static steady_clock::time_point lastLog{};
        static LocalProvider lastProvider = LocalProvider::None;
        static bool lastResolverPairProven = false;
        static bool lastWrapperSemanticsProven = false;
        static bool lastSdkDerefSafe = false;
        static bool lastDeepGraphSafe = false;

        const auto now = steady_clock::now();
        const bool providerChanged = provider != lastProvider ||
            resolverPairProven != lastResolverPairProven ||
            wrapperSemanticsProven != lastWrapperSemanticsProven ||
            sdkDerefSafe != lastSdkDerefSafe ||
            deepGraphSafe != lastDeepGraphSafe;
        const bool intervalElapsed = lastLog.time_since_epoch().count() == 0 ||
            duration_cast<milliseconds>(now - lastLog).count() >= 1000;

        if (!providerChanged && !intervalElapsed)
            return;

        char buf[1024];
        std::snprintf(buf, sizeof(buf),
            "[Validation] LocalProvider: selected=%d resolver_proven=%d wrapper_proven=%d sdk_safe=%d deep_safe=%d I::EntitySystem=%p es_pawn=%p es_ctrl=%p GameEntity=%p GameEntity.Instance=%p gr_pawn=%p gr_ctrl=%p client=%p cg_pawn=%p cg_ctrl=%p",
            static_cast<int>(provider),
            resolverPairProven ? 1 : 0,
            wrapperSemanticsProven ? 1 : 0,
            sdkDerefSafe ? 1 : 0,
            deepGraphSafe ? 1 : 0,
            static_cast<void*>(I::EntitySystem),
            static_cast<void*>(entitySystemPawn),
            static_cast<void*>(entitySystemController),
            static_cast<void*>(I::GameEntity),
            I::GameEntity ? static_cast<void*>(I::GameEntity->Instance) : nullptr,
            static_cast<void*>(gameResourcePawn),
            static_cast<void*>(gameResourceController),
            reinterpret_cast<void*>(clientBase),
            static_cast<void*>(clientGlobalPawn),
            static_cast<void*>(clientGlobalController));
        FileLog::Log(buf);

        lastProvider = provider;
        lastResolverPairProven = resolverPairProven;
        lastWrapperSemanticsProven = wrapperSemanticsProven;
        lastSdkDerefSafe = sdkDerefSafe;
        lastDeepGraphSafe = deepGraphSafe;
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

    C_CSPlayerPawn* clientGlobalPawn = nullptr;
    CCSPlayerController* clientGlobalController = nullptr;
    std::uintptr_t clientBase = 0;
    ReadClientGlobalLocals(clientGlobalPawn, clientGlobalController, clientBase);

    C_CSPlayerPawn* local_pawn = entitySystemPawn ? entitySystemPawn :
        (gameResourcePawn ? gameResourcePawn : clientGlobalPawn);
    CCSPlayerController* local_controller = entitySystemController ? entitySystemController :
        (gameResourceController ? gameResourceController : clientGlobalController);

    const bool usedEntitySystem =
        (local_pawn && local_pawn == entitySystemPawn) ||
        (local_controller && local_controller == entitySystemController);
    const bool usedGameResource =
        (local_pawn && local_pawn == gameResourcePawn && !entitySystemPawn) ||
        (local_controller && local_controller == gameResourceController && !entitySystemController);
    const bool usedClientGlobals =
        (local_pawn && local_pawn == clientGlobalPawn && !entitySystemPawn && !gameResourcePawn) ||
        (local_controller && local_controller == clientGlobalController && !entitySystemController && !gameResourceController);

    const int sourcesUsed = static_cast<int>(usedEntitySystem) +
        static_cast<int>(usedGameResource) + static_cast<int>(usedClientGlobals);

    LocalProvider provider = LocalProvider::None;
    if (sourcesUsed > 1)
        provider = LocalProvider::Mixed;
    else if (usedEntitySystem)
        provider = LocalProvider::EntitySystem;
    else if (usedGameResource)
        provider = LocalProvider::GameResource;
    else if (usedClientGlobals)
        provider = LocalProvider::ClientGlobals;

    // Resolver proof is based on independent address parity with the existing
    // pointer-only reference source. It intentionally does not imply wrapper
    // layout safety.
    const bool resolverPairProven =
        entitySystemPawn && entitySystemController &&
        clientGlobalPawn && clientGlobalController &&
        entitySystemPawn == clientGlobalPawn &&
        entitySystemController == clientGlobalController;

    // P3D can publish only the schema-backed basic wrapper semantic gate after
    // repeated, exception-free read-only probes. Deeper identity/scene/skeleton
    // traversal and controller fields not present in the static schema table stay
    // closed until independently proven.
    const bool wrapperSemanticsProven =
        resolverPairProven && LocalPlayerTrust::basic_wrapper_semantics_proven();
    const bool sdkDerefSafe = wrapperSemanticsProven;
    const bool deepGraphSafe = false;

    LogProviderState(provider,
                     resolverPairProven,
                     wrapperSemanticsProven,
                     sdkDerefSafe,
                     deepGraphSafe,
                     entitySystemPawn,
                     entitySystemController,
                     gameResourcePawn,
                     gameResourceController,
                     clientGlobalPawn,
                     clientGlobalController,
                     clientBase);

    m_snapshot.controller = reinterpret_cast<std::uintptr_t>(local_controller);
    m_snapshot.pawn = reinterpret_cast<std::uintptr_t>(local_pawn);
    m_snapshot.observer_pawn = 0;
    m_snapshot.observer_controller = 0;
    m_snapshot.sdk_resolver_pair_proven = resolverPairProven;
    m_snapshot.sdk_wrapper_semantics_proven = wrapperSemanticsProven;
    m_snapshot.sdk_deref_safe = sdkDerefSafe;
    m_snapshot.sdk_deep_graph_safe = deepGraphSafe;

    // P3D validates exactly these basic pawn reads before this gate can open.
    if (local_pawn && sdkDerefSafe) {
        m_snapshot.team = local_pawn->m_iTeamNum();
        m_snapshot.is_alive = local_pawn->is_alive();
        m_snapshot.is_team_mode = true;
    } else {
        m_snapshot.team = 0;
        m_snapshot.is_alive = false;
        m_snapshot.is_team_mode = false;
    }

    // view_team currently has no backing entry in TempleWare's static schema
    // table, so do not call m_iDesiredTeam() merely because the basic wrapper
    // gate is open. Observer handles likewise belong to the unproven deep graph.
    m_snapshot.view_team = 0;

    if (local_controller && deepGraphSafe) {
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
