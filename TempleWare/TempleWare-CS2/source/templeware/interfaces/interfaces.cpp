#include "interfaces.h"
#include "CGameEntitySystem/CGameEntitySystem.h"

// @used: I::Get<template>
#include "..\..\templeware\utils\memory\Interface\Interface.h"
bool I::Interfaces::init()
{
    const HMODULE tier0_base = GetModuleHandleA("tier0.dll");
    if (!tier0_base)
        return false;

    bool success = true;

    // interfaces
    EngineClient = I::Get<IEngineClient>(("engine2.dll"), "Source2EngineToClient001");
    success &= (EngineClient != nullptr);

    GameEntity = I::Get<IGameResourceService>(("engine2.dll"), "GameResourceServiceClientV001");
    success &= (GameEntity != nullptr);

    Input = *reinterpret_cast<CCSGOInput**>(M::GetAbsoluteAddress(M::FindPattern("client", ("48 8B 0D ? ? ? ? 8B D3 E8 ? ? ? ? ? ? ? ? F2 0F 11 45")), 0x3));
    success &= (Input != nullptr);

    SceneSystem = I::Get<ISceneSystem>(("scenesystem.dll"), "SceneSystem_002");
    success &= (SceneSystem != nullptr);

    EntitySystem = *reinterpret_cast<I_EntitySystem**>(M::GetAbsoluteAddress(M::FindPattern("client", ("48 8B 0D ? ? ? ? 48 89 7C 24 ? 8B FA C1 EB")), 0x3));
    success &= (EntitySystem != nullptr);

    NetworkClient = I::Get<I_NetworkClientService>(("engine2.dll"), "NetworkClientService_001");
    success &= (NetworkClient != nullptr);

    Prediction = I::Get<CPrediction>("client.dll", "Source2ClientPrediction001");
    success &= (Prediction != nullptr);

    Localize = I::Get<void>("localize.dll", "Localize_001");
    success &= (Localize != nullptr);

    // exports
    ConstructUtlBuffer = reinterpret_cast<decltype(ConstructUtlBuffer)>(GetProcAddress(tier0_base, "??0CUtlBuffer@@QEAA@HHW4BufferFlags_t@0@@Z"));
    EnsureCapacityBuffer = reinterpret_cast<decltype(EnsureCapacityBuffer)>(GetProcAddress(tier0_base, "?EnsureCapacity@CUtlBuffer@@QEAAXH@Z"));
    PutUtlString = reinterpret_cast<decltype(PutUtlString)>(GetProcAddress(tier0_base, "?PutString@CUtlBuffer@@QEAAXPEBD@Z"));
    CreateMaterial = reinterpret_cast<decltype(CreateMaterial)>(M::FindPattern("materialsystem2.dll", "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 48 89 7C 24 ? 41 56 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 8B F2")); // ??_7CMaterialSystem2@@6B@ Index 29
    LoadKeyValues = reinterpret_cast<decltype(LoadKeyValues)>(GetProcAddress(tier0_base, "?LoadKV3@@YA_NPEAVKeyValues3@@PEAVCUtlString@@PEBDAEBUKV3ID_t@@2I@Z"));
    ConMsg = reinterpret_cast<decltype(ConMsg)>(GetProcAddress(tier0_base, "?ConMsg@@YAXPEBDZZ"));
    ConColorMsg = reinterpret_cast<decltype(ConColorMsg)>(GetProcAddress(tier0_base, "?ConColorMsg@@YAXAEBVColor@@PEBDZZ"));

    // return status
    return success;
}
