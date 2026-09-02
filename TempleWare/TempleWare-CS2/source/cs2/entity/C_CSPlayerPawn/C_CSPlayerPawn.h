#pragma once
#include "../../../templeware/utils/memory/memorycommon.h"
#include "../../../templeware/utils/math/vector/vector.h"
#include "../../../templeware/utils/schema/schema.h"
#include "../../../cs2/datatypes/utlstronghandle/utlstronghandle.h"
#include "../C_CSWeaponBase/C_CSWeaponBase.h"
#include "../C_BaseEntity/C_BaseEntity.h"
#include "../C_BasePlayerPawn/C_BasePlayerPawn.h"
#include "../../../templeware/utils/math/viewmatrix/viewmatrix.h"
#include "../CPlayer_ItemServices/CPlayer_ItemServices.h"
#include "../C_EconEntity/C_EconEntity.h"

#include <cstdint>

class CGameSceneNode;
class CInButtonStatePB;
class CUserCmd;

enum class BoneID : int {
    ORIGIN = 0,
    PELVIS = 1,
    SPINE0 = 2,
    SPINE1 = 3,
    SPINE2 = 4,
    NECK = 6,
    HEAD = 7,
    CLAVICLE_L = 8,
    SHOULDER_L = 9,
    ELBOW_L = 10,
    HAND_L = 11,
    CLAVICLE_R = 12,
    SHOULDER_R = 13,
    ELBOW_R = 14,
    HAND_R = 15,
    HIP_L = 17,
    KNEE_L = 18,
    FOOT_HEEL_L = 19,
    HIP_R = 20,
    KNEE_R = 21,
    FOOT_HEEL_R = 22,
    CHEST = 23,
    GUN = 24,
    EYE_L = 25,
    EYE_R = 26,
    RANDOM = 27,
    CVJ_BONE = 28,
    FOOT_TOES_L_T = 74,
    FOOT_TOES_R_T = 77,
    FOOT_TOES_L_CT = 81,
    FOOT_TOES_R_CT = 86,
    BONE_MAX = 128
};


struct BoneConnection { BoneID bone1; BoneID bone2; };

namespace Bones
{
    inline const std::vector<BoneConnection> connections = {
        { BoneID::PELVIS, BoneID::SPINE1 },
        { BoneID::SPINE1, BoneID::SPINE2 },
        { BoneID::SPINE2, BoneID::CHEST },
        { BoneID::CHEST, BoneID::NECK },
        { BoneID::NECK,  BoneID::HEAD },
        { BoneID::NECK,           BoneID::SHOULDER_L },
        { BoneID::SHOULDER_L,    BoneID::ELBOW_L },
        { BoneID::ELBOW_L,    BoneID::HAND_L },
        { BoneID::NECK,           BoneID::SHOULDER_R },
        { BoneID::SHOULDER_R,    BoneID::ELBOW_R },
        { BoneID::ELBOW_R,    BoneID::HAND_R },
        { BoneID::PELVIS,         BoneID::HIP_L },
        { BoneID::HIP_L,    BoneID::KNEE_L },
        { BoneID::KNEE_L,    BoneID::FOOT_HEEL_L },
        { BoneID::FOOT_HEEL_L,    BoneID::FOOT_TOES_L_CT },
        { BoneID::PELVIS,         BoneID::HIP_R },
        { BoneID::HIP_R,    BoneID::KNEE_R },
        { BoneID::KNEE_R,    BoneID::FOOT_HEEL_R },
        { BoneID::FOOT_HEEL_R,    BoneID::FOOT_TOES_R_CT },
    };
}

inline bool IsValidPtr(uintptr_t addr)
{
    return addr > 0x10000 && addr < 0x7FFFFFFFFFFF;
}

class CCSPlayerModernJump {
public:
    schema(float, m_flLastLandedFrac, "CCSPlayerModernJump->m_flLastLandedFrac");
    schema(float, m_flLastLandedVelocityX, "CCSPlayerModernJump->m_flLastLandedVelocityX");
    schema(float, m_flLastLandedVelocityY, "CCSPlayerModernJump->m_flLastLandedVelocityY");
    schema(float, m_flLastLandedVelocityZ, "CCSPlayerModernJump->m_flLastLandedVelocityZ");
};

class C_PlayerMovementService {
public:
    schema(float, m_max_speed, "CPlayer_MovementServices->m_flMaxspeed");
    schema(float, m_surface_friction, "CPlayer_MovementServices_Humanoid->m_flSurfaceFriction");
    SCHEMA_ADD_OFFSET(CInButtonStatePB, m_button_state, 0x50);
    schema(uint64_t, m_nQueuedButtonDownMask, "CPlayer_MovementServices->m_nQueuedButtonDownMask");
    schema(uint64_t, m_nQueuedButtonChangeMask, "CPlayer_MovementServices->m_nQueuedButtonChangeMask");
    schema(uint64_t, m_nButtonDoublePressed, "CPlayer_MovementServices->m_nButtonDoublePressed");
    schema(uint64_t, m_nToggleButtonDownMask, "CPlayer_MovementServices->m_nToggleButtonDownMask");
    schema(uint64_t, m_nButtonDownMaskPrev, "CCSPlayer_MovementServices->m_nButtonDownMaskPrev");
    schema(uint64_t, m_flCmdForwardMove, "CCSPlayer_MovementServices->m_flCmdForwardMove");
    schema(CCSPlayerModernJump, m_ModernJump, "CCSPlayer_MovementServices->m_ModernJump");

    void set_prediction_command(CUserCmd* user_cmd) {

        M::CallVFunc<void, 46U>(this, user_cmd);
    }

    void run_command(CUserCmd* user_cmd) {
        M::CallVFunc<void, 32U>(this, user_cmd);
    }

    void reset_prediction_command() {
        M::CallVFunc<void, 47U>(this);
    }
};

class C_CCSPlayerMovementServices : public C_PlayerMovementService {
public:
    schema(float, m_duck_amount, "CCSPlayer_MovementServices->m_flDuckAmount");
    schema(float, m_duck_speed, "CCSPlayer_MovementServices->m_flDuckSpeed");
    schema(bool, m_bDucked, "CCSPlayer_MovementServices->m_bDucked");
    schema(bool, m_bDucking, "CCSPlayer_MovementServices->m_bDucking");
    schema(float, m_stamina, "CCSPlayer_MovementServices->m_flStamina");
    schema(float, m_offset_tick_complete_time, "CCSPlayer_MovementServices->m_flOffsetTickCompleteTime");
    schema(float, m_offset_tick_stashed_speed, "CCSPlayer_MovementServices->m_flOffsetTickStashedSpeed");
    SCHEMA_ARRAY(float, m_arrForceSubtickMoveWhen, "CPlayer_MovementServices->m_arrForceSubtickMoveWhen");
};

class ViewAngleServerChange_t {
public:
	schema(QAngle_t*, qAngle, "ViewAngleServerChange_t->qAngle");
};

class CPlayer_ObserverServices {
public:
    schema(CBaseHandle, m_hObserverTarget, ("CPlayer_ObserverServices->m_hObserverTarget"));
    schema(std::uint8_t, m_iObserverMode, "CPlayer_ObserverServices->m_iObserverMode");
    schema(CBaseHandle, m_hObserverPawn, "CPlayer_ObserverServices->m_hObserverPawn");
};

class C_CSPlayerPawn : public C_BasePlayerPawn {
public:
    schema(bool, m_bIsScoped, "C_CSPlayerPawn->m_bIsScoped");
    schema(float, m_flFlashDuration, "C_CSPlayerPawnBase->m_flFlashDuration");
    schema(int, m_ArmorValue, "C_CSPlayerPawn->m_ArmorValue");
    schema(bool, m_bHasHelmet, "CCSPlayer_ItemServices->m_bHasHelmet");
    schema(bool, m_bHasHeavyArmor, "CCSPlayer_ItemServices->m_bHasHeavyArmor");
    schema(bool, m_bHasDefuser, "CCSPlayer_ItemServices->m_bHasDefuser");
    schema(CBaseHandle, m_hHudModelArms, "C_CSPlayerPawn->m_hHudModelArms");
    schema(bool, m_bNeedToReapplyGloves, "C_CSPlayerPawn->m_bNeedToReApplyGloves");
    schema(bool, m_bHasFemaleVoice, "C_CSPlayerPawn->m_bHasFemaleVoice");
    schema(bool, m_bIsBuyMenuOpen, "C_CSPlayerPawn->m_bIsBuyMenuOpen");
    schema(float, m_flLastSpawnTimeIndex, "C_CSPlayerPawnBase->m_flLastSpawnTimeIndex");
    schema(c_utl_vector<C_EconItemView>, m_EconGloves, "C_CSPlayerPawn->m_EconGloves");

    C_CSPlayerPawn(uintptr_t address);

    C_CSWeaponBase* GetActiveWeapon();
    CCSPlayer_WeaponServices* GetWeaponServices() const;
    Vector_t getPosition() const;
    Vector_t getEyePosition() const;

    uintptr_t getAddress() const;
    int getHealth() const;
    uint8_t getTeam() const;
    Vector_t getViewOffset() const;

    bool is_alive() const;
    bool has_helmet() const;
    bool has_heavy_armor() const;
    bool has_defuser() const;

private:
    uintptr_t address;
};

class c_transform
{
public:
    Vector_t position;
    Vector4D_t rotate;
};

class C_Model;

class c_perm_model_ext_part
{
public:
    c_transform m_Transform;
    const char* m_Name;
    int32_t m_nParent;
private:
    [[maybe_unused]] uint8_t __pad002c[0x4];
public:
    C_StrongHandle< C_Model > m_refModel;
};

struct HitboxData_t {
    Vector_t m_vMins;
    Vector_t m_vMaxs;
    Vector_t m_vCenter;
    float m_flRadius;
    int m_nHitboxIndex;
    int m_nShapeType;
    bool m_bMultipoint;
};

class C_Hitbox
{
public:
    char* m_name;
    char* m_surface_property;
    char* m_bone_name;
    Vector_t m_vec_min;
    Vector_t m_vec_maxs;
    float m_shape_radius;
    uint32_t m_bone_hash;
    int32_t m_hitgroup;
    uint8_t m_shape_type;
    bool m_translation_only;
    std::uint32_t m_crc;
    char pad1[4];
    std::uint16_t m_hitbox_index;
    char pad_003E[0x22];
};

class C_HitboxSets
{
public:
    std::byte pad_0x0000[0x20];
    uint32_t m_nNameHash;
    std::byte pad_0x0024[0x4];
    __int32 m_nHitboxCount;
    std::byte pad_0x002C[0x4];
    C_Hitbox* m_hitbox;
    std::byte pad_0x0038[0x18];
};

class C_RenderMesh
{
public:
    char pad_0000[0x168];
    C_HitboxSets* m_hitboxsets;
    int32_t m_nHitboxSets;
};

class C_RenderMeshes
{
public:
    C_RenderMesh* m_pMeshes;
};


class c_quaternion_storage
{
public:
    float x, y, z, w;
};

struct ModelSkeletonData_t
{
public:
    c_utl_vector< const char* > m_boneName;
    char pad[0x8];
    c_utl_vector< int16_t > m_nParent;
    char pad2[0x8];
    c_utl_vector< float > m_boneSphere;
    char pad3[0x8];
    c_utl_vector< std::uint32_t > m_nFlag;
    char pad4[0x8];
    c_utl_vector< Vector_t > m_bonePosParent;
    char pad5[0x8];
    c_utl_vector< c_quaternion_storage > m_boneRotParent;
    char pad6[0x8];
    c_utl_vector< float > m_boneScaleParent;
    char pad7[0x8];
};

class C_PermModelData
{
public:
    const char* name;
    uint32_t flags;
    Vector_t hull_min;
    Vector_t hull_max;
    Vector_t view_min;
    Vector_t view_max;
    float mass;
    Vector_t eye_position;
    float max_eye_deflection;
    char* surface_property;
    c_utl_vector<c_perm_model_ext_part> ext_parts;
    c_utl_vector<C_RenderMesh*>  ref_meshes;
    char pad2[0x8];
    c_utl_vector<uint64_t> mesh_group_mask;
    char pad3[0x8];
    c_utl_vector<uint64_t> phys_group_mask;
    char pad5[0x30];
    c_utl_vector<void*> anim_groups;
    char pad9[0x70];
    ModelSkeletonData_t model_skeleton;
};

class C_Model
{
public:
    void* vtable;
    C_PermModelData perm_model_data;

    C_Hitbox* get_hitbox(int index)
    {
        auto model_data = &perm_model_data;

        if (perm_model_data.ref_meshes.m_size <= 0)
            return nullptr;

        auto mesh = perm_model_data.ref_meshes.element(0);
        if (!mesh)
            return nullptr;

        auto hitboxsets = mesh->m_hitboxsets;
        if (!hitboxsets)
            return nullptr;

        if (hitboxsets->m_nHitboxCount <= 0 || index > hitboxsets->m_nHitboxCount)
            return nullptr;

        auto& hitbox = hitboxsets->m_hitbox;
        return &hitbox[index];
    }

    const char* get_bone_name(int index)
    {
        if (perm_model_data.model_skeleton.m_boneName.count() <= 0 || perm_model_data.model_skeleton.m_boneName.count() > index)
            return "root";

        return perm_model_data.model_skeleton.m_boneName.element(index);
    }

    const char* get_hitbox_name(int index)
    {
        auto hitbox = get_hitbox(index);

        if (!hitbox)
            return nullptr;

        return hitbox->m_name;
    }
    int get_max_hitboxes()
    {
        if (perm_model_data.ref_meshes.count() <= 0)
            return -1;

        auto meshes = perm_model_data.ref_meshes.element(0);
        if (!meshes)
            return -1;

        auto hithoxsets = meshes->m_hitboxsets;
        if (!hithoxsets)
            return -1;

        return hithoxsets[0].m_nHitboxCount;
    }

    uint32_t get_hitboxes_num()
    {
        using fnHitboxNum = uint32_t(__fastcall*)(void*);
        static auto HitboxNum = reinterpret_cast<fnHitboxNum>(M::scan_absolute("client.dll", "E8 ? ? ? ? 85 C0 7E ? 83 7F 20 00", 0x1));

        return HitboxNum(this);
    }

    uint32_t get_bone_flags(std::uint32_t index)
    {
        static auto fn = reinterpret_cast<uint32_t(__fastcall*)(void*, uint32_t)>(M::scan("client.dll", "85 D2 78 ? 3B 91 ? ? ? ? 7D ? 48 8B 81 ? ? ? ? 48 63 D2 8B 04 90"));

        return fn(this, index);
    }

    std::int32_t get_bone_parent(std::uint32_t index)
    {
        static auto fn = reinterpret_cast<uint32_t(__fastcall*)(void*, uint32_t)>(M::scan("client.dll", "85 D2 78 ? 3B 91 ? ? ? ? 7D ? 48 8B 81 ? ? ? ? 48 63 D2 0F BF 04 50"));

        return fn(this, index);
    }
public:
    MEM_PAD(0x70);
    std::int32_t m_nRendermeshCount;
    MEM_PAD(0x4);
    C_RenderMeshes* m_pRenderMeshes;
};

struct alignas(16) C_BoneData {
    Vector_t m_pos;
    float m_scale;
    Vector4D_t m_rot;
};

class C_ModelState {
public:
    schema(C_StrongHandle<C_Model>, m_model, "CModelState->m_hModel");

    SCHEMA_ADD_OFFSET(C_BoneData*, get_bone_data, 0x80);
};

class CSkeletonInstance;
class CGameSceneNode
{
public:
    schema(CEntityInstance*, GetOwner, "CGameSceneNode->m_pOwner");

    schema(Vector_t, GetAbsOrigin, "CGameSceneNode->m_vecAbsOrigin");
    schema(Vector_t, GetVecOrigin, "CGameSceneNode->m_vecOrigin");
    schema(Vector_t, GetRenderOrigin, "CGameSceneNode->m_vecOrigin");
    schema(QAngle_t, GetAngleRotation, "CGameSceneNode->m_angRotation");
    schema(QAngle_t, GetAbsAngleRotation, "CGameSceneNode->m_angAbsRotation");
    schema(CGameSceneNode*, GetChild, "CGameSceneNode->m_pChild");
    schema(CGameSceneNode*, GetNextSibling, "CGameSceneNode->m_pNextSibling");
    schema(bool, IsDormant, "CGameSceneNode->m_bDormant");

    CSkeletonInstance* GetSkeletonInstance()
    {
        return M::CallVFunc<CSkeletonInstance*, 13U>(this);
    }
};

class CSkeletonInstance : public CGameSceneNode
{
public:
    char pad_003[412];
    int m_bone_count;
    char pad_002[24];
    int m_mask;
    char pad_001[4];
    Matrix2x4_t* m_bone_cache;

    schema(C_ModelState, m_model_state, "CSkeletonInstance->m_modelState");
    schema(uint8_t, m_hitbox_set, "CSkeletonInstance->m_nHitboxSet");

    void calc_world_space_bones(std::uint32_t bone_mask) {
        static const auto fn = reinterpret_cast<void(__fastcall*)(void*, unsigned int)>(M::scan("client.dll", "40 55 56 57 41 54 41 55 41 56 41 57 48 81 EC D0"));
        return fn(this, bone_mask);
    }
};

inline Vector_t GetBonePos(C_CSPlayerPawn* pawn, BoneID boneID)
{
    if (!pawn)
        return {};

    auto scene = pawn->m_pGameSceneNode();
    if (!scene)
        return {};

    if (!scene->GetSkeletonInstance())
    {
        printf("[TempleWare] Failed the GetSkeletonInstance ! \n");
        return {};
    }

    uintptr_t boneArray = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(scene) + 0x140 + 0x80);
    if (!IsValidPtr(boneArray))
        return {};

    return *reinterpret_cast<Vector_t*>(boneArray + static_cast<int>(boneID) * 0x20);
}