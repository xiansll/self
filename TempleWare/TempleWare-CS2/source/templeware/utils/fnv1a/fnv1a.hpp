#pragma once

#include <cstdint>
#include <string_view>

namespace fnv1a {

constexpr std::uint32_t offset_basis_32 = 0x811c9dc5;
constexpr std::uint32_t prime_32 = 0x1000193;

constexpr std::uint64_t offset_basis_64 = 0xcbf29ce484222325ull;
constexpr std::uint64_t prime_64 = 0x100000001b3ull;

constexpr std::uint32_t hash_32_const(std::string_view str, std::uint32_t value = offset_basis_32) noexcept {
    for (char c : str) {
        value ^= static_cast<std::uint32_t>(c);
        value *= prime_32;
    }
    return value;
}

constexpr std::uint64_t hash_64_const(std::string_view str, std::uint64_t value = offset_basis_64) noexcept {
    for (char c : str) {
        value ^= static_cast<std::uint64_t>(c);
        value *= prime_64;
    }
    return value;
}

inline std::uint32_t hash_32(std::string_view str) noexcept {
    std::uint32_t hash = offset_basis_32;
    for (char c : str) {
        hash ^= static_cast<std::uint32_t>(c);
        hash *= prime_32;
    }
    return hash;
}

inline std::uint64_t hash_64(std::string_view str) noexcept {
    std::uint64_t hash = offset_basis_64;
    for (char c : str) {
        hash ^= static_cast<std::uint64_t>(c);
        hash *= prime_64;
    }
    return hash;
}

inline std::uint32_t hash_32_cstr(const char* str) noexcept {
    std::uint32_t hash = offset_basis_32;
    while (*str) {
        hash ^= static_cast<std::uint32_t>(*str++);
        hash *= prime_32;
    }
    return hash;
}

inline std::uint64_t hash_64_cstr(const char* str) noexcept {
    std::uint64_t hash = offset_basis_64;
    while (*str) {
        hash ^= static_cast<std::uint64_t>(*str++);
        hash *= prime_64;
    }
    return hash;
}

} // namespace fnv1a

consteval std::uint32_t operator""_hash32(const char* str, std::size_t len) noexcept {
    return fnv1a::hash_32_const(std::string_view(str, len));
}

consteval std::uint64_t operator""_hash64(const char* str, std::size_t len) noexcept {
    return fnv1a::hash_64_const(std::string_view(str, len));
}

#define HASH_32(str) ::fnv1a::hash_32_const(str)
#define HASH_64(str) ::fnv1a::hash_64_const(str)
#define HASH_RT_32(str) ::fnv1a::hash_32(str)
#define HASH_RT_64(str) ::fnv1a::hash_64(str)