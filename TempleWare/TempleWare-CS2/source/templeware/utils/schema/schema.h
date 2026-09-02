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

// Schema-backed accessors deliberately do not permanently cache a zero lookup.
// Some accessors can be touched before the schema source is usable; freezing that
// first zero would make every later read behave like `this + 0`. Once a non-zero
// field offset is observed it is cached exactly as before.
#define SCHEMA_ARRAY(TYPE, NAME, FIELD) \
    [[nodiscard]] inline TYPE* NAME() const { \
        static std::uint32_t uOffset = 0U; \
        if (uOffset == 0U) \
            uOffset = SchemaFinder::Get(hash_32_fnv1a_const(FIELD)); \
        return reinterpret_cast<TYPE*>(reinterpret_cast<std::uint8_t*>(const_cast<void*>(static_cast<const void*>(this))) + uOffset); \
    }

#define schema(TYPE, NAME, FIELD) \
    [[nodiscard]] inline std::add_lvalue_reference_t<TYPE> NAME() const \
    { \
        static std::uint32_t uOffset = 0U; \
        if (uOffset == 0U) \
            uOffset = SchemaFinder::Get(hash_32_fnv1a_const(FIELD)); \
        return *reinterpret_cast<std::add_pointer_t<TYPE>>( \
            reinterpret_cast<std::uint8_t*>(const_cast<void*>(static_cast<const void*>(this))) + uOffset); \
    }

#define schema_pfield(TYPE, NAME, FIELD, ADDITIONAL) \
    [[nodiscard]] inline std::add_lvalue_reference_t<TYPE> NAME() const \
    { \
        static std::uint32_t uBaseOffset = 0U; \
        if (uBaseOffset == 0U) \
            uBaseOffset = SchemaFinder::Get(hash_32_fnv1a_const(FIELD)); \
        const std::uint32_t uOffset = uBaseOffset + static_cast<std::uint32_t>(ADDITIONAL); \
        return *reinterpret_cast<std::add_pointer_t<TYPE>>( \
            reinterpret_cast<std::uint8_t*>(const_cast<void*>(static_cast<const void*>(this))) + uOffset); \
    }

#define SCHEMA_ADD_RAW_OFFSET(TYPE, NAME, OFFSET) \
    [[nodiscard]] inline TYPE NAME() const noexcept \
    { \
        return *reinterpret_cast<std::add_pointer_t<TYPE>>( \
            reinterpret_cast<std::uint8_t*>(const_cast<void*>(static_cast<const void*>(this))) + OFFSET); \
    }

#define add_offset_near(_class, _name, _type, _field_name, _offset)              \
[[nodiscard]] inline std::add_lvalue_reference_t<_type> _name() const                  \
{                                                                                \
    static std::uint32_t baseOffset = 0U;                                        \
    if (baseOffset == 0U)                                                        \
        baseOffset = SchemaFinder::Get(hash_32_fnv1a_const(_field_name));        \
                                                                                  \
    const std::uint32_t totalOffset = baseOffset + static_cast<std::uint32_t>(_offset); \
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
