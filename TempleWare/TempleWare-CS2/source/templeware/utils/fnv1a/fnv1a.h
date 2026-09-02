#pragma once
#include "fnv1a.hpp"

inline constexpr uint32_t hash_32_fnv1a_const(const char* const str, const uint32_t value = ::fnv1a::offset_basis_32) noexcept {
    return ::fnv1a::hash_32_const(str, value);
}

#define HASH(str) ::fnv1a::hash_32_const(str)
#define HASH_RT(str) ::fnv1a::hash_32(str)

constexpr uint32_t val_32_const = ::fnv1a::offset_basis_32;
constexpr uint32_t prime_32_const = ::fnv1a::prime_32;