#include "entity.h"

void C_EntitySystem::initialize()
{
    int highest_idx = I::EntitySystem->get_highest_entity_index();

    for (int i = 0; i <= highest_idx; i++) {
        CEntityInstance* entity = I::EntitySystem->get_base_entity(i);

        if (!entity)
            continue;

        const char* class_name = entity->get_entity_class_name();
        entities[fnv1a::hash_64(class_name)].emplace_back(entity);
    }
}

void C_EntitySystem::level_init() {
    entities.clear();
    initialize();
}

void C_EntitySystem::level_shutdown() {
    entities.clear();
}

void C_EntitySystem::add_entity(CEntityInstance* entity, int index) {
    const char* className = entity->get_entity_class_name();

    entities[fnv1a::hash_64(className)].emplace_back(entity);
}

void C_EntitySystem::remove_entity(CEntityInstance* entity, int index) {
    const char* class_name = entity->get_entity_class_name();

    auto it = entities.find(fnv1a::hash_64(class_name));
    if (it == entities.end())
        return;

    std::vector<CEntityInstance*>& entities = it->second;

    for (auto iter = entities.begin(); iter != entities.end(); ++iter) {
        if ((*iter) == entity) {
            entities.erase(iter);

            break;
        }
    }
}

std::vector<CEntityInstance*> C_EntitySystem::get(const char* name) {
    return entities[fnv1a::hash_64(name)];
}

std::vector<C_CSPlayerPawn*> C_EntitySystem::get_players(int index) {
    std::vector<C_CSPlayerPawn*> enteties;

    C_CSPlayerPawn* local_player = g_ctx->local_pawn;
    if (!local_player)
        return enteties;

    int local_team_num = local_player->m_iTeamNum();

    std::vector<CEntityInstance*> entities = get("C_CSPlayerPawn");
    for (CEntityInstance* entity : entities) {
        C_CSPlayerPawn* player_pawn = reinterpret_cast<C_CSPlayerPawn*>(entity);

        int team_num = player_pawn->m_iTeamNum();

        if (index == ENTITY_ALL || ((index == ENTITY_ENEMIES_ONLY && local_team_num != team_num) || (index == ENTITY_TEAMMATES_ONLY && local_team_num == team_num && local_player != player_pawn)))
            enteties.emplace_back(player_pawn);
    }

    return enteties;
}

std::vector<player_controller_pair> C_EntitySystem::get_players_with_controller(int index) {
    std::vector<player_controller_pair> return_enteties;

    C_CSPlayerPawn* local_player = g_ctx->local_pawn;
    if (!local_player)
        return return_enteties;

    int local_team_num = local_player->m_iTeamNum();

    std::vector<CEntityInstance*> entities = get("CCSPlayerController");
    for (auto entity : entities)
    {
        CCSPlayerController* controller = reinterpret_cast<CCSPlayerController*>(entity);
        if (!controller)
            continue;

        C_CSPlayerPawn* player_pawn = reinterpret_cast<C_CSPlayerPawn*>(I::EntitySystem->get_base_entity(controller->m_hPawn().index()));
        if (!player_pawn)
            continue;

        int team_num = player_pawn->m_iTeamNum();

        if (index == ENTITY_ALL || ((index == ENTITY_ENEMIES_ONLY && local_team_num != team_num) || (index == ENTITY_TEAMMATES_ONLY && local_team_num == team_num && local_player != player_pawn)))
            return_enteties.emplace_back(std::make_pair(controller, player_pawn));
    }

    return return_enteties;
}