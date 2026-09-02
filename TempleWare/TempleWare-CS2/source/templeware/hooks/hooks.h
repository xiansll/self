#pragma once
#include "includeHooks.h"
#include "../../cs2/entity/C_AggregateSceneObject/C_AggregateSceneObject.h"
#include "../../cs2/entity/C_CSPlayerPawn/C_CSPlayerPawn.h"
#include "../../cs2/datatypes/cutlbuffer/cutlbuffer.h"
#include "../../cs2/datatypes/keyvalues/keyvalues.h"
#include "../../cs2/entity/C_Material/C_Material.h"
#include "../interfaces/CCSGOInput/CCSGOInput.h"
#include "../../cs2/entity/C_Material/C_Material.h"

// Forward declaration
class CMeshData;
class CEntityIdentity;

namespace H {
	void __fastcall hkFrameStageNotify(void* a1, int stage);
	void* __fastcall hkLevelInit(__int64 a1, __int64 a2);
	void* __fastcall hkOnLevelShutdown(void* a1, const char* map_name);
	void __fastcall hkChamsObject(void* pAnimatableSceneObjectDesc, void* pDx11, CMeshData* arrMeshDraw, int nDataCount, void* pSceneView, void* pSceneLayer, void* pUnk, void* pUnk2);
	void __fastcall hkRenderFlashbangOverlay(void* a1, void* a2, void* a3, void* a4, void* a5);
	void __fastcall hkCreateMove(CCSGOInput* rcx, int slot, bool active);
	void* hkOnAddEntity(void* a1, CEntityInstance* entity_instance, int handle);
	void* hkOnRemoveEntity(void* a1, CEntityInstance* entity_instance, int handle);
	inline float g_flActiveFov;
	float hkGetRenderFov(void* rcx);

	// World
	void* __fastcall hkUpdateAggregateSceneObject(void* a1, void* a2, c_aggregate_object_array* a3);
	void __fastcall hkUpdateLightObject(__int64 a1, __int64 a2, __int64 a3);

	inline CInlineHookObj<decltype(&hkChamsObject)> DrawArray = { };
	inline CInlineHookObj<decltype(&hkFrameStageNotify)> FrameStageNotify = { };
	inline CInlineHookObj<decltype(&hkGetRenderFov)> GetRenderFov = { };
	inline CInlineHookObj<decltype(&hkLevelInit)> LevelInit = { };
	inline CInlineHookObj<decltype(&hkOnLevelShutdown)> LevelShutdown = { };
	inline CInlineHookObj<decltype(&hkRenderFlashbangOverlay)> RenderFlashBangOverlay = { };
	inline CInlineHookObj<decltype(&hkCreateMove)> CreateMove = { };
	inline CInlineHookObj<decltype(&hkUpdateAggregateSceneObject)> UpdateAggregateSceneObject = { };
	inline CInlineHookObj<decltype(&hkUpdateLightObject)> UpdateLightObject = { };
	inline CInlineHookObj<decltype(&hkOnAddEntity)> OnAddEntity = { };
	inline CInlineHookObj<decltype(&hkOnRemoveEntity)> OnRemoveEntity = { };

	// inline hooks
	inline int  oGetWeaponData;
	inline void* (__fastcall* ogGetBaseEntity)(void*, int);

	class Hooks {
	public:
		// Phase3A-safe path: installs only the lifecycle hook needed for
		// LocalPlayerCache/runtime validation. Returns true only if the
		// FrameStageNotify hook is actually active.
		bool initValidation();

		// Legacy/full hook initialization path.
		void init();
	};
}
