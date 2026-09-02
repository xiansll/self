#pragma once
#include <memory>

#include "..\fnv1a\fnv1a.h"
#include "..\memory\vfunc\vfunc.h"
#include "..\math\utlmemory\utlmemory.h"
#include "..\math\utlvector\utlvector.h"
#include "..\..\..\cs2\datatypes\schema\ISchemaClass\ISchemaClass.h"

#define SCHEMA_ADD_OFFSET(TYPE, NAME, OFFSET)                                                                 \
	[[nodiscard]] inline std::add_lvalue_reference_t<TYPE> NAME() const                                          \
	{                                                                                                         \
		static const std::uint32_t uOffset = OFFSET;                                                          \
		return *reinterpret_cast<std::add_pointer_t<TYPE>>(reinterpret_cast<std::uint8_t*>(const_cast<void*>(static_cast<const void*>(this))) + (uOffset)); \
	}

#define SCHEMA_ADD_POFFSET(TYPE, NAME, OFFSET)                                                               \
	[[nodiscard]] inline std::add_pointer_t<TYPE> NAME() const                                                  \
	{                                                                                                        \
		const static std::uint32_t uOffset = OFFSET;                                                         \
		return reinterpret_cast<std::add_pointer_t<TYPE>>(reinterpret_cast<std::uint8_t*>(const_cast<void*>(static_cast<const void*>(this))) + (uOffset)); \
	}

#define SCHEMA_ARRAY(TYPE, NAME, FIELD) \
    [[nodiscard]] inline TYPE* NAME() const { \
        static const uint32_t uOffset = SchemaFinder::Get(hash_32_fnv1a_const(FIELD)); \
        return reinterpret_cast<TYPE*>(reinterpret_cast<std::uint8_t*>(const_cast<void*>(static_cast<const void*>(this))) + uOffset); \
    }

#define schema(TYPE, NAME, FIELD)  SCHEMA_ADD_OFFSET(TYPE, NAME, SchemaFinder::Get(hash_32_fnv1a_const(FIELD)) + 0u)

#define schema_pfield(TYPE, NAME, FIELD, ADDITIONAL) SCHEMA_ADD_OFFSET(TYPE, NAME, SchemaFinder::Get(hash_32_fnv1a_const(FIELD)) + ADDITIONAL)

#define SCHEMA_ADD_RAW_OFFSET(TYPE, NAME, OFFSET) \
    [[nodiscard]] inline TYPE NAME() const noexcept \
    { \
        return *reinterpret_cast<std::add_pointer_t<TYPE>>( \
            reinterpret_cast<std::uint8_t*>(const_cast<void*>(static_cast<const void*>(this))) + OFFSET); \
    }

#define add_offset_near(_class, _name, _type, _field_name, _offset)              \
[[nodiscard]] inline std::add_lvalue_reference_t<_type> _name() const                  \
{                                                                                \
    static const uint32_t baseOffset = SchemaFinder::Get(                        \
        hash_32_fnv1a_const(_field_name)                                         \
    );                                                                           \
                                                                                  \
    static const uint32_t totalOffset = baseOffset + (_offset);                  \
                                                                                  \
    return *reinterpret_cast<std::add_pointer_t<_type>>(                         \
        reinterpret_cast<uint8_t*>(const_cast<void*>(static_cast<const void*>(this))) + totalOffset                           \
    );                                                                           \
}


class Schema {
public:
    bool init(const char* module_name, int module_type);

    ISchemaSystem* schema_system = nullptr;
};

namespace SchemaFinder {

    [[nodiscard]] std::uint32_t Get(const uint32_t hashed);
    [[nodiscard]] std::uint32_t GetExternal(const char* moduleName, const uint32_t HashedClass, const uint32_t HashedFieldName);
}