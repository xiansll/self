#include "schema.h"
#include "schema_offsets_table.h"

// Field offsets are now a static table generated from the current CS2 schema
// dump (Antigravity output). No runtime schema-system traversal is needed,
// which also removes the version-fragile struct offsets / vtable indices that
// were crashing under newer schemasystem.dll builds.

bool Schema::init(const char* module_name, int module_type)
{
    // Nothing to initialize; offsets are compile-time constants.
    return true;
}

std::uint32_t SchemaFinder::Get(const uint32_t hashedName)
{
    for (std::size_t i = 0; i < g_schemaOffsetCount; i++)
    {
        if (g_schemaOffsets[i].hash == hashedName)
            return g_schemaOffsets[i].offset;
    }
    return 0U;
}

std::uint32_t SchemaFinder::GetExternal(const char* moduleName, const uint32_t HashedClass, const uint32_t HashedFieldName)
{
    return 0U;
}
