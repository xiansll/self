#pragma once

// Velocity-facing data contracts for the future combat port.
//
// This file intentionally contains only POD-style compatibility shapes and
// compile-time capability markers. It does not scan memory, resolve entities,
// read bones/hitboxes, perform traces, or dereference game pointers.

#include <array>
#include <cstddef>
#include <cstdint>

#include "../utils/math/vector/vector.h"

namespace VelocityRageCompat {

struct entity_ref {
    std::uintptr_t ptr{};
    std::uint32_t handle{};
    int index{-1};

    [[nodiscard]] bool valid() const noexcept {
        return ptr != 0;
    }
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
    int hitgroup{-1};
    Vector_t mins{};
    Vector_t maxs{};
    float radius{};
    std::uint8_t shape_type{};
    bool translation_only{};
};

struct hitbox_set {
    static constexpr std::size_t k_max_hitboxes = 20;

    std::array<hitbox_entry, k_max_hitboxes> entries{};
    std::size_t count{};
    bool valid{};
};

struct rich_trace_result {
    Vector_t start{};
    Vector_t end{};
    Vector_t normal{};
    Vector_t position{};
    std::uintptr_t hit_entity{};
    std::uint32_t contents{};
    float fraction{1.0f};
    bool all_solid{};
    bool hit{};
};

// Compile-time availability only. Runtime producers remain deliberately absent
// until their TempleWare-side ownership and validation are proven separately.
struct data_contracts {
    static constexpr bool entity_cache = true;
    static constexpr bool bones = true;
    static constexpr bool hitboxes = true;
    static constexpr bool rich_trace = true;
};

} // namespace VelocityRageCompat
