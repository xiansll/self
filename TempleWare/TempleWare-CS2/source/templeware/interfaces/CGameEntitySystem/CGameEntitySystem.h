#pragma once

#include <cstdint>
#include "../../../cs2/entity/handle.h"
#include "../../../templeware/utils/memory/memorycommon.h"
#include "../../../templeware/utils/math/vector/vector.h"
#include "../../../../source/templeware/utils/schema/schema.h"
#include "../../../../source/templeware/utils/memory/vfunc/vfunc.h"
#include "../../../../source/templeware/utils/memory/patternscan/patternscan.h"

#include "../../../cs2/entity/C_BaseEntity/C_BaseEntity.h"
#include "../../../../source/templeware/utils/validation/validation.h"

class C_CSPlayerPawn;
class CCSPlayerController;

class CGameEntitySystem
{
public:
    template <typename T = C_BaseEntity>
    T* Get(int nIndex)
    {
        return reinterpret_cast<T*>(this->GetEntityByIndex(nIndex));
    }

    /// GetClientEntityFromHandle
    template <typename T = C_BaseEntity>
    T* Get(const CBaseHandle hHandle)
    {
        if (!hHandle.valid())
            return nullptr;

        T* pEntity = reinterpret_cast<T*>(this->GetEntityByIndex(hHandle.index()));
        if (!pEntity)
        {
            Validation::OnEntityHandleLookup(hHandle, nullptr, CBaseHandle());
            return nullptr;
        }

        CBaseHandle entityHandle = pEntity->handle();
        if (entityHandle.serial_number() != hHandle.serial_number())
        {
            Validation::OnEntityHandleLookup(hHandle, pEntity, entityHandle);
            return nullptr;
        }

        Validation::OnEntityHandleLookup(hHandle, pEntity, entityHandle);
        return pEntity;
    }

    int GetHighestEntityIndex()
    {
        return *reinterpret_cast<int*>(reinterpret_cast<std::uintptr_t>(this) + 0x2090);
    }

    C_CSPlayerPawn* get_entity(int index)
    {
        __int64 v2; // rcx
        __int64 v3; // r8
        __int64 result{}; // rax

        if ((unsigned int)index <= 0x7FFE
            && (unsigned int)(index >> 9) <= 0x3F
            && (v2 = *(std::uintptr_t*)(std::uintptr_t(this) + 8 * (index >> 9) + 16)) != 0
            && (v3 = 120 * (index & 0x1FF), v3 + v2)
            && (*(std::uintptr_t*)(v3 + v2 + 16) & 0x7FFF) == index)
        {
            result = *(std::uintptr_t*)(v3 + v2);
        }
        return reinterpret_cast<C_CSPlayerPawn*>(result);
    }

    C_CSPlayerPawn* get_local_pawn() {
        static auto fn = reinterpret_cast<C_CSPlayerPawn* (__fastcall*)(int)>(
            M::scan_absolute("client.dll", "E8 ? ? ? ? 48 8B F0 48 85 C0 74 ? 48 8D 15 ? ? ? ? B9", 0x1)
        );
        return fn ? fn(-1) : nullptr;
    }

    void* get_local_controller() {
        static auto fn = reinterpret_cast<void* (__fastcall*)(int)>(
            M::patternScan("client", "48 83 EC ? 83 F9 ? 75 ? 48 8B 0D ? ? ? ? 48 8D 54 24 ? 48 8B 01 FF 90 ? ? ? ? 8B 08 48 63 C1 48 8D 0D ? ? ? ? 48 8B 04 C1 48 83 C4 ? C3 CC CC CC CC CC CC CC CC CC CC CC CC CC 48 83 EC ? 83 F9")
        );
        return fn ? fn(-1) : nullptr;
    }

    template <typename T = C_BaseEntity>
    T* get_base_entity(int index) {
        static auto get_client_entity = reinterpret_cast<T* (__fastcall*)(CGameEntitySystem*, int)>(
            M::patternScan("client", "4C 8D 49 ? 81 FA")
        );

        if (!get_client_entity)
            return nullptr;
        return get_client_entity(this, index);
    }

private:
    void* GetEntityByIndex(int nIndex);
};

class IGameResourceService
{
public:
    MEM_PAD(0x58);
    CGameEntitySystem* Instance;
};