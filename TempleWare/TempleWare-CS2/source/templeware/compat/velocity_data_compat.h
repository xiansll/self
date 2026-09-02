#pragma once

// Velocity-facing data contracts for the future combat port.
//
// These are POD-style compatibility shapes only. They mirror the data surface
// consumed by Velocity's systems/combat headers closely enough that future
// adapters can translate TempleWare data into one stable contract. This file
// does not scan memory, resolve entities, read bones/hitboxes, perform traces,
// or dereference game pointers.

#include <array>
#include <cstddef>
#include <cstdint>

#include "../utils/math/vector/vector.h"

namespace VelocityRageCompat {

enum class entity_type : std::uint8_t {
    unknown,
    player,
    item,
    projectile
};

struct entity_ref {
    std::uintptr_t ptr{};
    std::uint32_t handle{};
    std::uint32_t schema_hash{};
    std::int16_t index{-1};
    entity_type type{entity_type::unknown};

    [[nodiscard]] bool valid() const noexcept {
        return ptr != 0;
    }
};

// Mirrors the non-behavioural state Velocity prediction consumers expect.
// No TempleWare entity read is performed here.
struct prediction_state {
    std::uint32_t flags{};
    Vector_t networked_velocity{};
    Vector_t velocity{};
    Vector_t origin{};
    Vector_t networked_origin{};
    Vector_t last_movement_impulses{};
    float surface_friction{};
    float stamina{};
    bool valid{};
};

struct bone_pose {
    Vector_t position{};
    float scale{};
    std::array<float, 4> rotation{};
};

struct skeleton_snapshot {
    static constexpr std::size_t k_max_bones = 128;

    std::array<bone_pose, k_max_bones> bones{};
    std::size_t count{};
    bool valid{};
};

struct hitbox_entry {
    int index{-1};
    int bone{-1};
    Vector_t mins{};
    Vector_t maxs{};
    float radius{};
    std::uint8_t shape_type{};
    bool translation_only{};
};

struct hitbox_set {
    static constexpr std::size_t k_max_hitboxes = 20;

    std::array<hitbox_entry, k_max_hitboxes> entries{};
    int count{};
    bool valid{};

    [[nodiscard]] const hitbox_entry* begin() const noexcept {
        return entries.data();
    }

    [[nodiscard]] const hitbox_entry* end() const noexcept {
        const int safe_count = count < 0 ? 0 : (count > static_cast<int>(k_max_hitboxes) ? static_cast<int>(k_max_hitboxes) : count);
        return entries.data() + safe_count;
    }
};

struct rich_trace_result {
    std::uintptr_t surface{};
    std::uintptr_t hit_entity{};
    std::uintptr_t hitbox_data{};
    std::uint32_t contents{};
    Vector_t start{};
    Vector_t end{};
    Vector_t normal{};
    Vector_t position{};
    float fraction{1.0f};
    bool all_solid{};
    bool hit{};
};

// Compile-time availability only. Runtime producers remain deliberately absent
// until their TempleWare-side ownership and validation are proven separately.
struct data_contracts {
    static constexpr bool entity_cache = true;
    static constexpr bool prediction_state = true;
    static constexpr bool bones = true;
    static constexpr bool hitboxes = true;
    static constexpr bool rich_trace = true;
};

} // namespace VelocityRageCompat
