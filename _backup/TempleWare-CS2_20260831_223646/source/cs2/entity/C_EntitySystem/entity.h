#pragma once

#include "../../../templeware/globals/globals.h"
#include <unordered_map>

#define ENTITY_ALL 1
#define ENTITY_ENEMIES_ONLY 2
#define ENTITY_TEAMMATES_ONLY 3

using player_controller_pair = std::pair<CCSPlayerController*, C_CSPlayerPawn*>;

class C_EntitySystem {
    std::unordered_map<uint64_t, std::vector<CEntityInstance*>> entities;
public:
    void initialize();

    void level_init();
    void level_shutdown();
    void add_entity(CEntityInstance*, int);
    void remove_entity(CEntityInstance*, int);

    std::vector<CEntityInstance*> get(const char*);
    std::vector<C_CSPlayerPawn*> get_players(int);
    std::vector<player_controller_pair> get_players_with_controller(int);
};

inline const auto g_entitySystem = std::make_unique<C_EntitySystem>();