#include "hooks.h"
#include <iostream>
#include <atomic>

#include "../../../external/kiero/minhook/include/MinHook.h"

#include "../../templeware/utils/memory/Interface/Interface.h"
#include "../utils/memory/patternscan/patternscan.h"
#include "../utils/memory/gaa/gaa.h"

#include "../players/hook/playerHook.h"

#include "../../nerv/nerv_bridge.h"

extern bool g_showMenu; // defined in main.cpp (global scope)

#include "../features/visuals/visuals.h"
#include "../features/chams/chams.h"

#include "../../cs2/datatypes/cutlbuffer/cutlbuffer.h"
#include "../../cs2/datatypes/keyvalues/keyvalues.h"

#include "../../cs2/entity/C_Material/C_Material.h"
#include "../../cs2/entity/C_EntitySystem/entity.h"

#include "../config/config.h"
#include "../interfaces/interfaces.h"
#include "../features/aim/aim.h"
#include "../features/movement/movement.h"
#include "../features/rage/antiaim.h"
#include "../features/prediction/prediction.h"
#include "../features/skinchanger/skinchanger.h"
#include "../utils/localplayer/localplayer.h"
#include "../utils/validation/validation.h"

namespace {
    // The active Phase3A path installs only FrameStageNotify for lifecycle and
    // diagnostic validation. This flag prevents that validation-only hook from
    // accidentally executing unrelated legacy feature callbacks.
    std::atomic<bool> s_validationOnlyMode{false};
}

void* H::hkOnAddEntity(void* a1, CEntityInstance* entity_instance, int handle)
{
	static auto original = H::OnAddEntity.GetOriginal();

	g_entitySystem->add_entity(entity_instance, handle);

	return original(a1, entity_instance, handle);
}

void* H::hkOnRemoveEntity(void* a1, CEntityInstance* entity_instance, int handle)
{
	static auto original = H::OnRemoveEntity.GetOriginal();

	g_entitySystem->remove_entity(entity_instance, handle);

	return original(a1, entity_instance, handle);
}

void __fastcall H::hkFrameStageNotify(void* a1, int stage)
{
	static auto original = FrameStageNotify.GetOriginal();
	if (!original)
		return;

	static std::atomic<bool> s_firstCallLogged{false};
	if (!s_firstCallLogged.exchange(true)) {
		Validation::LogFramestageFirstCall();
	}

	original(a1, stage);
	if (!I::EngineClient || !I::EntitySystem)
		return;
	if (!I::EngineClient->connected() || !I::EngineClient->in_game())
		return;

	g_ctx->local_pawn = I::EntitySystem->get_local_pawn();
	g_ctx->local_controller = I::EntitySystem->get_base_entity<CCSPlayerController>(I::EngineClient->get_local_player());

	g_local_player_cache->update();
	Validation::OnLocalPlayerCacheUpdate(g_local_player_cache->get());

	if (g_ctx->local_pawn) {
		Validation::OnSceneNodeChainCheck(g_ctx->local_pawn);
		Validation::OnEntityIdentityCheck(g_ctx->local_pawn);
	}
	if (g_ctx->local_controller) {
		Validation::OnEntityIdentityCheck(g_ctx->local_controller);
	}

	Validation::LogPeriodicSummary(I::GlobalVars ? I::GlobalVars->m_tick_count : 0);

	// Phase3A validation-only runtime must not activate unrelated legacy
	// gameplay/feature callbacks merely to validate lifecycle plumbing.
	if (s_validationOnlyMode.load(std::memory_order_relaxed))
		return;

	// Legacy/full path only.
	// features::skinchanger::OnFrameStageNotify();
	nerv_bridge::on_frame(stage, ::g_showMenu);

	if (stage == FRAME_RENDER_END && g_ctx->local_pawn) {
		Esp::cache();
		Aimbot();
	}
}

void* __fastcall H::hkLevelInit(__int64 a1, __int64 a2) {
	const auto original = H::LevelInit.GetOriginal();

	static void* g_pPVS = (void*)M::getAbsoluteAddress(M::patternScan("engine2", "48 8D 0D ? ? ? ? 33 D2 FF 50"), 0x3);
	M::vfunc<void*, 6U, void>(g_pPVS, false);

	static const auto globalAddr = reinterpret_cast<CGlobalVarsBase**>(M::ResolveRelativeAddress(M::FindPattern("client", ("48 8B 0D ? ? ? ? 4C 8D 05 ? ? ? ? 48 85 D2")), 0x3, 0x7));
	I::GlobalVars = *globalAddr;

	g_entitySystem->level_init();

	features::skinchanger::Initialize();

	nerv_bridge::initialize();
	nerv_bridge::force_update();

	return original(a1, a2);
}

void* __fastcall H::hkOnLevelShutdown(void* a1, const char* map_name)
{
	static auto original = H::LevelShutdown.GetOriginal();

	g_entitySystem->level_shutdown();

	features::skinchanger::Shutdown();

	g_local_player_cache->reset();
	Validation::OnLocalPlayerCacheReset();

	return original(a1, map_name);
}

void __fastcall H::hkCreateMove(CCSGOInput* rcx, int slot, bool active)
{
	static auto original = CreateMove.GetOriginal();
	original(rcx, slot, active);

	C_CSPlayerPawn* pLocalPawn = g_ctx->local_pawn;
	if (!pLocalPawn || pLocalPawn->m_iHealth() <= 0)
		return;

	CCSPlayerController* pLocalController = I::GameEntity->Instance->Get<CCSPlayerController>(pLocalPawn->m_hController().index());
	if (!pLocalController)
		return;

	CUserCmd* user_cmd = I::Input->get_user_cmd(pLocalController);

	Vector_t viewAngle = { user_cmd->csgoUserCmd.mutable_base()->viewangles().x(), user_cmd->csgoUserCmd.mutable_base()->viewangles().y(), user_cmd->csgoUserCmd.mutable_base()->viewangles().z() };

	g_movement->OnCreateMove(user_cmd, viewAngle);
	g_antiaim->OnCreateMove(user_cmd);

	g_prediction->Start(user_cmd);
	{
		// Add ur ragebot / no spread etc...
	}
	g_prediction->End(user_cmd);

	g_movement->MovementFix(user_cmd, viewAngle);

	if (user_cmd->csgoUserCmd.mutable_base()->forwardmove() > 0.f)
		user_cmd->nButtons.nValue |= IN_FORWARD;
	else if (user_cmd->csgoUserCmd.mutable_base()->forwardmove() < 0.f)
		user_cmd->nButtons.nValue |= IN_BACK;

	if (user_cmd->csgoUserCmd.mutable_base()->leftmove() > 0.f)
		user_cmd->nButtons.nValue |= IN_MOVELEFT;
	else if (user_cmd->csgoUserCmd.mutable_base()->leftmove() < 0.f)
		user_cmd->nButtons.nValue |= IN_MOVERIGHT;
}

bool H::Hooks::initValidation() {
	Validation::Initialize();
	s_validationOnlyMode.store(true, std::memory_order_relaxed);

	void* frameStageTarget = (void*)M::patternScan("client", ("48 89 5C 24 ? 48 89 6C 24 ? 57 48 83 EC 40 48 8B F9 33 ED"));
	const bool installed = FrameStageNotify.Add(frameStageTarget, &hkFrameStageNotify);
	if (installed) {
		Validation::LogFramestageHookInstalled();
	} else {
		Validation::LogFramestageHookFailed();
	}

	return installed;
}

void H::Hooks::init() {
	// Full/legacy path. Reuse the validated lifecycle hook setup, then explicitly
	// leave validation-only mode before installing unrelated legacy hooks.
	if (!FrameStageNotify.IsHooked()) {
		initValidation();
	}
	s_validationOnlyMode.store(false, std::memory_order_relaxed);

	oGetWeaponData = *reinterpret_cast<int*>(M::patternScan("client", ("48 8B 81 ? ? ? ? 85 D2 78 ? 48 83 FA ? 73 ? F3 0F 10 84 90 ? ? ? ? C3 F3 0F 10 80 ? ? ? ? C3 CC CC CC CC")) + 0x3);
	ogGetBaseEntity = reinterpret_cast<decltype(ogGetBaseEntity)>(M::patternScan("client", ("4C 8D 49 10 81 FA FE 7F 00 00 ? ? 8B CA C1 F9 09 83 F9 3F ? ? 48 63 C1 4D"))); // String: Found no entity at %d.\n and Press Double Click on v4 and then on Return.

	if (I::Input)
	{
		using CreateMoveFn = void(__fastcall*)(CCSGOInput*, int, bool);

		auto createMoveFunc = M::GetVFunc<CreateMoveFn>(I::Input, 5);

		if (createMoveFunc)
		{
			if (CreateMove.Add(reinterpret_cast<void*>(createMoveFunc),
				reinterpret_cast<void*>(&hkCreateMove)))
			{
			}
		}
	}

	// DrawArray.Add((void*)M::patternScan("scenesystem", ("48 8B C4 53 57 41 54 48 81 EC D0 00 00 00 49 63 F9 49")), &chams::hook);
	GetRenderFov.Add((void*)M::patternScan("client", "40 53 48 83 EC ? 48 8B D9 E8 ? ? ? ? 48 85 C0 74 ? 48 8B C8 48 83 C4"), &hkGetRenderFov);
	LevelInit.Add((void*)M::patternScan("client", "48 89 74 24 ? 57 48 83 EC ? 48 8B 0D ? ? ? ? 48 8B FA"), &hkLevelInit);
	LevelShutdown.Add((void*)M::patternScan("client", "48 83 EC ? 48 8B 0D ? ? ? ? 48 8D 15 ? ? ? ? 45 33 C9 45 33 C0 48 8B 01 FF 50 ? 48 85 C0 74 ? 48 8B 0D ? ? ? ? 48 8B D0 4C 8B 01 41 FF 50 ? 48 83 C4"), &hkOnLevelShutdown);
	RenderFlashBangOverlay.Add((void*)M::patternScan("client", ("85 D2 0F 88 ? ? ? ? 48 89 4C 24 ? 55 56")), &hkRenderFlashbangOverlay);

	// Entity
	OnAddEntity.Add((void*)M::patternScan("client", "48 89 74 24 ? 57 48 83 EC ? 41 B9 ? ? ? ? 41 8B C0 41 23 C1 48 8B F2 41 83 F8 ? 48 8B F9 44 0F 45 C8 41 81 F9 ? ? ? ? 73 ? FF 81"), &hkOnAddEntity);
	OnRemoveEntity.Add((void*)M::patternScan("client", "48 89 74 24 ? 57 48 83 EC ? 41 B9 ? ? ? ? 41 8B C0 41 23 C1 48 8B F2 41 83 F8 ? 48 8B F9 44 0F 45 C8 41 81 F9 ? ? ? ? 73 ? FF 89"), &hkOnRemoveEntity);

	// World
	UpdateAggregateSceneObject.Add((void*)M::patternScan("scenesystem", "48 8B C4 48 89 50 ? 48 89 48 ? 55 53 56 57 41 54 41 55 41 56 41 57 48 8D A8 ? ? ? ? 48 81 EC ? ? ? ? 0F 29 70"), &hkUpdateAggregateSceneObject);
	UpdateLightObject.Add((void*)M::FindPattern("scenesystem", "48 89 54 24 ? 55 57 41 56 48 83 EC"), &hkUpdateLightObject);

	MH_EnableHook(MH_ALL_HOOKS);
}

