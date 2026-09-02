#pragma once
#include <cstdint>
#include "../../../../source/templeware/utils/schema/schema.h"
#include "../../../../source/templeware/utils/math/vector/vector.h"

class CCollisionProperty {
public:
    schema(Vector_t, m_vecMins, "CCollisionProperty->m_vecMins");
    schema(Vector_t, m_vecMaxs, "CCollisionProperty->m_vecMaxs");
    schema(std::uint16_t, m_usSolidFlags, "CCollisionProperty->m_usSolidFlags");
    schema(std::uint8_t, m_CollisionGroup, "CCollisionProperty->m_CollisionGroup");
    schema(std::uint8_t, m_nSolidType, "CCollisionProperty->m_nSolidType");
    schema(std::uint8_t, m_triggerBloat, "CCollisionProperty->m_triggerBloat");
    schema(std::uint32_t, m_collisionMask, "CCollisionProperty->m_collisionMask");
    schema(Vector_t, m_vecSpecifiedSurroundingMins, "CCollisionProperty->m_vecSpecifiedSurroundingMins");
    schema(Vector_t, m_vecSpecifiedSurroundingMaxs, "CCollisionProperty->m_vecSpecifiedSurroundingMaxs");
};