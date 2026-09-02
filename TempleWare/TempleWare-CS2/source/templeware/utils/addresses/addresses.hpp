#pragma once

#include <cstdint>
#include <string_view>
#include <optional>
#include "memory.hpp"

namespace addr {

namespace modules {
    inline std::uintptr_t client = 0;
    inline std::uintptr_t engine2 = 0;
    inline std::uintptr_t server = 0;
    inline std::uintptr_t scenesystem = 0;
    inline std::uintptr_t materialsystem2 = 0;
    inline std::uintptr_t rendersystemdx11 = 0;
    inline std::uintptr_t panorama = 0;
    inline std::uintptr_t schemasystem = 0;
    inline std::uintptr_t inputsystem = 0;
    inline std::uintptr_t soundsystem = 0;
    inline std::uintptr_t tier0 = 0;
    inline std::uintptr_t particles = 0;
    inline std::uintptr_t resourcesystem = 0;
    inline std::uintptr_t localize = 0;
    inline std::uintptr_t filesystem_stdio = 0;
    inline std::uintptr_t vphysics2 = 0;

    inline bool initialize() noexcept {
        client = mem::get_module_base("client.dll");
        engine2 = mem::get_module_base("engine2.dll");
        server = mem::get_module_base("server.dll");
        scenesystem = mem::get_module_base("scenesystem.dll");
        materialsystem2 = mem::get_module_base("materialsystem2.dll");
        rendersystemdx11 = mem::get_module_base("rendersystemdx11.dll");
        panorama = mem::get_module_base("panorama.dll");
        schemasystem = mem::get_module_base("schemasystem.dll");
        inputsystem = mem::get_module_base("inputsystem.dll");
        soundsystem = mem::get_module_base("soundsystem.dll");
        tier0 = mem::get_module_base("tier0.dll");
        particles = mem::get_module_base("particles.dll");
        resourcesystem = mem::get_module_base("resourcesystem.dll");
        localize = mem::get_module_base("localize.dll");
        filesystem_stdio = mem::get_module_base("filesystem_stdio.dll");
        vphysics2 = mem::get_module_base("vphysics2.dll");

        return client && engine2 && server && scenesystem && materialsystem2 &&
               rendersystemdx11 && panorama && schemasystem && inputsystem &&
               soundsystem && tier0 && particles && resourcesystem && localize &&
               filesystem_stdio && vphysics2;
    }
} // namespace modules

namespace interfaces {
    struct c_convar {
        const char* name = nullptr;
        c_convar* next = nullptr;
        char pad_0[0x10];
        const char* description = nullptr;
        std::uint32_t type = 0;
        std::uint32_t registered = 0;
        std::uint32_t flags = 0;
        char pad_1[0x24];

        union {
            bool b;
            short i16;
            int i32;
            std::int64_t i64;
            float fl;
            double db;
            const char* sz;
        } value, default_value;

        template <typename T>
        [[nodiscard]] T get() const noexcept {
            if constexpr (std::is_same_v<T, bool>) return value.b;
            else if constexpr (std::is_same_v<T, short>) return value.i16;
            else if constexpr (std::is_same_v<T, int>) return value.i32;
            else if constexpr (std::is_same_v<T, std::int64_t>) return value.i64;
            else if constexpr (std::is_same_v<T, float>) return value.fl;
            else return T{};
        }
    };

    struct c_engine_cvar {
        char pad[0x48];
        struct cvar_container_t {
            c_convar* cvar = nullptr;
            std::int16_t unknown = 0;
            std::int16_t next_index = 0;
            std::int32_t unknown2 = 0;
        }* container = nullptr;

        [[nodiscard]] c_convar* find(std::uint32_t name_hash) noexcept;
        bool unlock_all() noexcept;
    };

    inline c_engine_cvar* cvar = nullptr;

    [[nodiscard]] inline std::uintptr_t get_interface(std::string_view module_name, std::string_view interface_name) noexcept {
        const auto module_base = mem::get_module_base(module_name);
        if (!module_base) return 0;

        const auto create_interface = mem::get_module_export(module_base, "CreateInterface");
        if (!create_interface) return 0;

        struct interface_reg_t {
            std::uintptr_t (*create_fn)();
            const char* name;
            interface_reg_t* next;
        };

        const auto interface_list_offset = *reinterpret_cast<const std::int32_t*>(create_interface + 3);
        const auto interface_regs = *reinterpret_cast<interface_reg_t**>(create_interface + 7 + interface_list_offset);

        for (auto current = interface_regs; current; current = current->next) {
            if (!current->name || !current->create_fn) continue;
            if (std::string_view(current->name) == interface_name) {
                return current->create_fn();
            }
        }
        return 0;
    }

    [[nodiscard]] inline std::uintptr_t get_interface_any(std::string_view interface_name) noexcept {
        static constexpr std::string_view known_modules[] = {
            "client.dll", "engine2.dll", "server.dll", "scenesystem.dll",
            "materialsystem2.dll", "rendersystemdx11.dll", "panorama.dll",
            "schemasystem.dll", "inputsystem.dll", "soundsystem.dll",
            "tier0.dll", "particles.dll", "resourcesystem.dll",
            "localize.dll", "filesystem_stdio.dll", "vphysics2.dll"
        };

        for (auto module : known_modules) {
            if (auto result = get_interface(module, interface_name)) return result;
        }
        return 0;
    }
} // namespace interfaces

namespace globals {
    inline std::uintptr_t csgo_input = 0;
    inline std::uintptr_t entity_list = 0;
    inline std::uintptr_t local_player_controller = 0;
    inline std::uintptr_t global_vars = 0;
    inline std::uintptr_t view_matrix = 0;
    inline std::uintptr_t game_rules = 0;
    inline std::uintptr_t game_entity_system = 0;
    inline std::uintptr_t weapon_recoil_data = 0;
    inline std::uintptr_t game_event_manager = 0;
    inline std::uintptr_t game_trace_manager = 0;
    inline std::uintptr_t mem_alloc = 0;
    inline std::uintptr_t swap_chain = 0;
    inline std::uintptr_t prediction_seed = 0;
    inline std::uintptr_t simulation_player = 0;
    inline std::uintptr_t prediction_player = 0;
    inline std::uintptr_t planted_c4 = 0;
    inline std::uintptr_t item_system = 0;
    inline std::uintptr_t net_client = 0;

    inline bool initialize() noexcept {
        using namespace modules;

        csgo_input = mem::find_pattern_module("client.dll", "48 8B 0D ? ? ? ? 48 85 C9 74 0A 48 8B 01 FF 50 ? C3");
        entity_list = mem::find_pattern_module("client.dll", "48 8B 0D ? ? ? ? 48 85 C9 74 0A 48 8B 01 FF 50 ? C3");
        local_player_controller = mem::find_pattern_module("client.dll", "48 8B 0D ? ? ? ? 48 85 C9 74 0A 48 8B 01 FF 50 ? C3");
        global_vars = mem::find_pattern_module("client.dll", "48 8B 05 ? ? ? ? 48 85 C0 74 0A 48 8B 00");
        view_matrix = mem::find_pattern_module("client.dll", "48 8D 0D ? ? ? ? E8 ? ? ? ? 48 8B C8");
        game_rules = mem::find_pattern_module("client.dll", "48 8B 0D ? ? ? ? 48 85 C9 74 0A 48 8B 01");
        game_entity_system = mem::find_pattern_module("client.dll", "48 8B 0D ? ? ? ? 48 85 C9 74 0A 48 8B 01");
        weapon_recoil_data = mem::find_pattern_module("client.dll", "48 8B 0D ? ? ? ? 48 85 C9 74 0A 48 8B 01");
        game_event_manager = mem::find_pattern_module("engine2.dll", "48 8B 0D ? ? ? ? 48 85 C9 74 0A 48 8B 01");
        game_trace_manager = mem::find_pattern_module("engine2.dll", "48 8B 0D ? ? ? ? 48 85 C9 74 0A 48 8B 01");

        if (const auto ptr = mem::get_module_export(tier0, "g_pMemAlloc")) {
            mem_alloc = mem::read<std::uintptr_t>(ptr);
        }

        return csgo_input && entity_list && local_player_controller && global_vars &&
               view_matrix && game_rules && game_entity_system && weapon_recoil_data &&
               game_event_manager && game_trace_manager && mem_alloc;
    }
} // namespace globals

} // namespace addr