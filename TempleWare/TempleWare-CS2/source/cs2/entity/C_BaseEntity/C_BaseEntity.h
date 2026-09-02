#pragma once
#include <cstdint>
#include "../C_EntityInstance/C_EntityInstance.h"
#include "../../../templeware/utils/memory/memorycommon.h"
#include "../../../templeware/utils/math/vector/vector.h"
#include "../../../../source/templeware/utils/schema/schema.h"
#include "../../../../source/templeware/utils/memory/vfunc/vfunc.h"
#include "../handle.h"
#include "../CCollisionProperty/CCollisionProperty.h"

class CGameSceneNode;
class CCSWeaponBaseVData;

class C_BaseEntity : public CEntityInstance
{
public:
    schema(int, m_iMaxHealth, "C_BaseEntity->m_iMaxHealth");
    schema(CBaseHandle, m_hPawn, "CBasePlayerController->m_hPawn");
    schema(std::int32_t, m_iHealth, "C_BaseEntity->m_iHealth");
    schema(std::uint8_t, m_iTeamNum, "C_BaseEntity->m_iTeamNum");
    schema(std::uint32_t, m_fFlags, "C_BaseEntity->m_fFlags");
    schema(std::int32_t, m_MoveType, "C_BaseEntity->m_MoveType");
    schema(CBaseHandle, m_hController, "C_BasePlayerPawn->m_hController");
    schema(Vector_t, m_vecAbsVelocity, "C_BaseEntity->m_vecAbsVelocity");
    schema(Vector_t, m_vecVelocity, "C_BaseEntity->m_vecVelocity");
    schema(Vector_t, m_vecBaseVelocity, "C_BaseEntity->m_vecBaseVelocity");
    schema(float, m_flSimulationTime, "C_BaseEntity->m_flSimulationTime");
    schema(int, m_nSimulationTick, "C_BaseEntity->m_nSimulationTick");
    schema(CCollisionProperty*, m_pCollision, "C_BaseEntity->m_pCollision");
    schema(CGameSceneNode*, m_pGameSceneNode, "C_BaseEntity->m_pGameSceneNode");
    schema(CBaseHandle, m_hOwnerEntity, "C_BaseEntity->m_hOwnerEntity");
    schema(CBaseHandle, m_hEffectEntity, "C_BaseEntity->m_hEffectEntity");
    [[nodiscard]] inline std::add_lvalue_reference_t<CCSWeaponBaseVData*> m_pWeaponData() {
        static const uint32_t baseOffset = SchemaFinder::Get(hash_32_fnv1a_const("C_BaseEntity->m_nSubclassID")); static const uint32_t totalOffset = baseOffset + (0x8); return *reinterpret_cast<std::add_pointer_t<CCSWeaponBaseVData*>>(reinterpret_cast<uint8_t*>(this) + totalOffset);
    };
    schema(int, m_nSubclassID, "C_BaseEntity->m_nSubclassID");
    schema(float, m_flGravity, "C_BaseEntity->m_flGravity");
    schema(int, m_nWaterLevel, "C_BaseEntity->m_nWaterLevel");
    schema(float, m_flMaxspeed, "C_BaseEntity->m_flMaxspeed");
    schema(int, m_lifeState, "C_BaseEntity->m_lifeState");
    schema(bool, m_bDeadFlag, "C_BaseEntity->m_bDeadFlag");
    schema(float, m_flDeathTime, "C_BaseEntity->m_flDeathTime");

    bool IsBasePlayer()
    {
        SchemaClassInfoData_t* pClassInfo;
        dump_class_info(&pClassInfo);
        if (pClassInfo == nullptr)
            return false;

        return hash_32_fnv1a_const(pClassInfo->szName) == hash_32_fnv1a_const("C_CSPlayerPawn");
    }

    bool IsViewmodelAttachment()
    {
        SchemaClassInfoData_t* pClassInfo;
        dump_class_info(&pClassInfo);
        if (pClassInfo == nullptr)
            return false;

        return hash_32_fnv1a_const(pClassInfo->szName) == hash_32_fnv1a_const("C_CS2HudModelArms");
    }

    bool IsViewmodel()
    {
        SchemaClassInfoData_t* pClassInfo;
        dump_class_info(&pClassInfo);
        if (pClassInfo == nullptr)
            return false;

        return hash_32_fnv1a_const(pClassInfo->szName) == hash_32_fnv1a_const("C_CS2HudModelWeapon");
    }

    bool IsPlayerController()
    {
        SchemaClassInfoData_t* _class = nullptr;
        dump_class_info(&_class);
        if (!_class)
            return false;

        const uint32_t hash = hash_32_fnv1a_const(_class->szName);

        return (hash == hash_32_fnv1a_const("CCSPlayerController"));
    }

    bool is_alive() const {
        return m_iHealth() > 0;
    }

    int get_life_state() const {
        return m_lifeState();
    }

    bool is_dormant() const;
};