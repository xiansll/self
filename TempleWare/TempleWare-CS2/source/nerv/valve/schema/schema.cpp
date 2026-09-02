#include "../../main.hpp"

using schema_key_value_map_t = std::unordered_map<unsigned long long, short>;
using schema_table_map_t = std::unordered_map<unsigned long long, schema_key_value_map_t>;

const char* dll_files[2] = { "client.dll", "animationsystem.dll" };

bool init_schema_fields_for_class(schema_table_map_t& table_map, const char* class_name, uint64_t class_key) {
    for (const char* dll_file : dll_files) {
        c_schema_type_scope* type_scope = g_interfaces->m_schema_system->find_type_scope_for_module(dll_file);
        if (!type_scope)
            continue;

        c_schema_class_info* class_info = type_scope->find_declared_class(class_name);
        if (!class_info)
            continue;

        uint16_t fields_size = class_info->get_fields_size();
        c_schema_class_field* fields = class_info->get_fields();
        if (!fields)
            continue;

        auto& key_value_map = table_map[class_key];

        for (uint16_t i = 0; i < fields_size; ++i) {
            c_schema_class_field& field = fields[i];
            if (!field.m_name)
                continue;
            key_value_map.emplace(fnv1a::hash_64(field.m_name), field.m_offset);
        }

        return true;
    }

    table_map.emplace(class_key, schema_key_value_map_t{});
    return false;
}

// Delegate to TempleWare's static schema-offset table (generated from the cs2
// dump). nerv's runtime schema-system traversal used struct offsets that are
// wrong on this build (every field resolved to garbage). The table is keyed by
// fnv1a32(class) ^ fnv1a32(field) — identical to TempleWare's SchemaOffset().
namespace SchemaFinder { std::uint32_t Get(const std::uint32_t hashedName); }

static constexpr std::uint32_t tw_h32(const char* s, std::uint32_t v = 0x811c9dc5u) {
    return s[0] == '\0' ? v : tw_h32(s + 1, (v ^ std::uint32_t((unsigned char)s[0])) * 0x1000193u);
}

int16_t schema_get_offset(const char* class_name, const char* key_name) {
    return static_cast<int16_t>(SchemaFinder::Get(tw_h32(class_name) ^ tw_h32(key_name)));
}
