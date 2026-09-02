#pragma once

#include <cstdint>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <memory>
#include <Windows.h>

namespace mem {

[[nodiscard]] inline std::uintptr_t get_module_base(std::string_view module_name) noexcept {
    return reinterpret_cast<std::uintptr_t>(GetModuleHandleA(module_name.data()));
}

[[nodiscard]] inline std::uintptr_t get_module_export(std::uintptr_t module_base, std::string_view export_name) noexcept {
    return reinterpret_cast<std::uintptr_t>(GetProcAddress(reinterpret_cast<HMODULE>(module_base), export_name.data()));
}

[[nodiscard]] inline std::uintptr_t get_module_export(std::uintptr_t module_base, std::uint16_t ordinal) noexcept {
    return reinterpret_cast<std::uintptr_t>(GetProcAddress(reinterpret_cast<HMODULE>(module_base), MAKEINTRESOURCEA(ordinal)));
}

[[nodiscard]] inline std::uintptr_t get_module_size(std::uintptr_t module_base) noexcept {
    const auto dos_header = reinterpret_cast<const IMAGE_DOS_HEADER*>(module_base);
    if (!dos_header || dos_header->e_magic != IMAGE_DOS_SIGNATURE) return 0;

    const auto nt_headers = reinterpret_cast<const IMAGE_NT_HEADERS*>(module_base + dos_header->e_lfanew);
    if (!nt_headers || nt_headers->Signature != IMAGE_NT_SIGNATURE) return 0;

    return nt_headers->OptionalHeader.SizeOfImage;
}

template <typename T>
[[nodiscard]] inline T read(std::uintptr_t address) noexcept {
    return *reinterpret_cast<T*>(address);
}

template <typename T>
[[nodiscard]] inline std::optional<T> safe_read(std::uintptr_t address) noexcept {
    __try {
        return *reinterpret_cast<T*>(address);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return std::nullopt;
    }
}

template <typename T>
inline void write(std::uintptr_t address, const T& value) noexcept {
    *reinterpret_cast<T*>(address) = value;
}

template <typename T, typename... Args>
[[nodiscard]] inline T call(std::uintptr_t address, Args... args) noexcept {
    return reinterpret_cast<T(__fastcall*)(Args...)>(address)(args...);
}

template <typename T, typename... Args>
[[nodiscard]] inline T call_vfunc(std::uintptr_t instance, std::size_t index, Args... args) noexcept {
    const auto vtable = *reinterpret_cast<std::uintptr_t*>(instance);
    const auto func = *reinterpret_cast<std::uintptr_t*>(vtable + index * sizeof(std::uintptr_t));
    return reinterpret_cast<T(__fastcall*)(std::uintptr_t, Args...)>(func)(instance, args...);
}

[[nodiscard]] inline std::uintptr_t get_vfunc(std::uintptr_t instance, std::size_t index) noexcept {
    const auto vtable = *reinterpret_cast<std::uintptr_t*>(instance);
    if (!vtable) return 0;
    return *reinterpret_cast<std::uintptr_t*>(vtable + index * sizeof(std::uintptr_t));
}

[[nodiscard]] inline std::string read_string(std::uintptr_t address, std::size_t max_length = 256) noexcept {
    if (!address) return {};
    const auto str_ptr = reinterpret_cast<const char*>(address);
    std::size_t len = 0;
    for (; len < max_length && str_ptr[len] != '\0'; ++len);
    return (len > 0) ? std::string(str_ptr, len) : std::string{};
}

namespace pattern {

struct byte_t {
    std::uint8_t byte = 0;
    bool wildcard = false;
};

enum class resolve_op : std::uint8_t {
    direct,
    rel_call,
    rip_relative,
    absolute_ptr
};

struct parsed_t {
    std::size_t byte_count = 0;
    resolve_op op = resolve_op::direct;
    std::size_t op_byte_offset = 0;
    std::int64_t post_offset = 0;
    bool deref_final = false;
};

[[nodiscard]] inline int hex_char_to_int(char c) noexcept {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

[[nodiscard]] inline parsed_t parse_pattern(std::string_view pattern, std::span<byte_t> out_bytes) noexcept {
    parsed_t result;
    if (pattern.empty() || out_bytes.empty()) return result;

    std::size_t count = 0;
    std::size_t idx = 0;

    while (idx < pattern.length() && count < out_bytes.size()) {
        char c = pattern[idx];

        if (c == ' ' || c == '\t') { idx++; continue; }

        if (c == '>') { result.op = resolve_op::rel_call; result.op_byte_offset = count; idx++; continue; }
        if (c == '*') { result.op = resolve_op::rip_relative; result.op_byte_offset = count; idx++; continue; }
        if (c == '^') { result.op = resolve_op::absolute_ptr; result.op_byte_offset = count; idx++; continue; }
        if (c == '~') { result.deref_final = true; idx++; continue; }

        if (c == '+' || c == '-') {
            bool negate = (c == '-');
            idx++;
            std::int64_t val = 0;
            while (idx < pattern.length()) {
                int h = hex_char_to_int(pattern[idx]);
                if (h < 0) break;
                val = (val << 4) | h;
                idx++;
            }
            result.post_offset = negate ? -val : val;
            continue;
        }

        if (c == '?') {
            out_bytes[count++] = {0, true};
            idx++;
            if (idx < pattern.length() && pattern[idx] == '?') idx++;
            continue;
        }

        int high = hex_char_to_int(pattern[idx++]);
        if (idx < pattern.length()) {
            int low = hex_char_to_int(pattern[idx]);
            if (low != -1) {
                out_bytes[count++] = {static_cast<std::uint8_t>((high << 4) | low), false};
                idx++;
            } else {
                out_bytes[count++] = {static_cast<std::uint8_t>(high), false};
            }
        } else {
            out_bytes[count++] = {static_cast<std::uint8_t>(high), false};
        }
    }

    result.byte_count = count;
    return result;
}

} // namespace pattern

[[nodiscard]] inline std::uintptr_t find_pattern(std::uintptr_t module_base, std::string_view pattern_str) noexcept {
    const auto module_size = get_module_size(module_base);
    if (!module_size) return 0;

    constexpr std::size_t MAX_PATTERN_LEN = 128;
    pattern::byte_t parsed[MAX_PATTERN_LEN];
    const auto info = pattern::parse_pattern(pattern_str, parsed);

    if (info.byte_count == 0 || info.byte_count > MAX_PATTERN_LEN) return 0;

    const auto pattern_length = info.byte_count;
    int first_byte_idx = -1;
    for (std::size_t i = 0; i < pattern_length; i++) {
        if (!parsed[i].wildcard) { first_byte_idx = static_cast<int>(i); break; }
    }
    if (first_byte_idx == -1) return 0;

    const std::uint8_t first_byte = parsed[first_byte_idx].byte;
    const auto module_end = module_base + module_size;
    const auto scan_end = (module_size >= 32) ? module_size - 32 : 0;

    for (std::uintptr_t i = 0; i <= scan_end; i += 32) {
        const auto* mem = reinterpret_cast<const std::uint8_t*>(module_base + i);
        for (int j = 0; j < 32; j++) {
            if (mem[j] != first_byte) continue;

            const std::intptr_t candidate_offset = static_cast<std::intptr_t>(i) + j - first_byte_idx;
            if (candidate_offset < 0) continue;

            const std::uintptr_t potential_start = module_base + static_cast<std::uintptr_t>(candidate_offset);
            if (potential_start + pattern_length > module_end) continue;

            bool found = true;
            for (std::size_t k = 0; k < pattern_length; k++) {
                if (parsed[k].wildcard) continue;
                if (*reinterpret_cast<const std::uint8_t*>(potential_start + k) != parsed[k].byte) {
                    found = false; break;
                }
            }
            if (found) {
                std::uintptr_t result = potential_start;

                switch (info.op) {
                    case pattern::resolve_op::direct: break;
                    case pattern::resolve_op::rel_call: {
                        const auto operand_addr = result + info.op_byte_offset + 1;
                        if (operand_addr + 4 > module_end) return 0;
                        const auto rel = *reinterpret_cast<const std::int32_t*>(operand_addr);
                        result = operand_addr + 4 + rel;
                        break;
                    }
                    case pattern::resolve_op::rip_relative: {
                        const auto operand_addr = result + info.op_byte_offset;
                        if (operand_addr + 4 > module_end) return 0;
                        const auto rel = *reinterpret_cast<const std::int32_t*>(operand_addr);
                        result = operand_addr + 4 + rel;
                        break;
                    }
                    case pattern::resolve_op::absolute_ptr: {
                        const auto ptr_addr = result + info.op_byte_offset;
                        if (ptr_addr + sizeof(std::uintptr_t) > module_end) return 0;
                        result = *reinterpret_cast<const std::uintptr_t*>(ptr_addr);
                        break;
                    }
                }

                result += info.post_offset;

                if (info.deref_final) {
                    if (result + sizeof(std::uintptr_t) > module_end || result < module_base) return 0;
                    result = *reinterpret_cast<const std::uintptr_t*>(result);
                }

                return result;
            }
        }
    }
    return 0;
}

[[nodiscard]] inline std::uintptr_t find_pattern_module(std::string_view module_name, std::string_view pattern) noexcept {
    const auto base = get_module_base(module_name);
    return base ? find_pattern(base, pattern) : 0;
}

} // namespace mem