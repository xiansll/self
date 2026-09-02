# Phase 2 Status

## Completed
- Added CCollisionProperty class with mins, maxs, solid flags, collision group
- Added base entity classes: C_BasePlayerWeapon, C_BasePlayerPawn, C_BasePlayerController
- Added CPlayer_ItemServices / CCSPlayer_ItemServices for armor/helmet/defuser
- Added C_EconEntity, C_EconItemView, C_AttributeContainer, C_AttributeList for skin/item system
- Enhanced CGameEntitySystem with proper handle resolution, get_local_pawn, get_local_controller, get_base_entity
- Added LocalPlayerCache with snapshot system (controller, pawn, observer, team, alive state)
- Fixed const-correctness in schema accessors
- Resolved circular include issues between C_BaseEntity and C_CSPlayerPawn
- Clean build verified

## Files Changed
- `source/cs2/entity/CCollisionProperty/CCollisionProperty.h` — new collision property class
- `source/cs2/entity/C_BasePlayerWeapon/C_BasePlayerWeapon.h` — new base weapon class
- `source/cs2/entity/C_BasePlayerPawn/C_BasePlayerPawn.h` — new base player pawn class
- `source/cs2/entity/C_BasePlayerController/C_BasePlayerController.h` — new base player controller class
- `source/cs2/entity/CPlayer_ItemServices/CPlayer_ItemServices.h` — new item services classes
- `source/cs2/entity/C_EconEntity/C_EconEntity.h` — new econ entity/item view/attribute classes
- `source/cs2/entity/C_BaseEntity/C_BaseEntity.h` — updated with collision, scene node, owner entity, is_dormant
- `source/cs2/entity/C_BaseEntity/C_BaseEntity.cpp` — new implementation for is_dormant
- `source/cs2/entity/CCSPlayerController/CCSPlayerController.h` — updated with compat aliases and proper inheritance
- `source/cs2/entity/CCSPlayerController/CCSPlayerController.cpp` — updated to use schema accessor
- `source/cs2/entity/C_CSPlayerPawn/C_CSPlayerPawn.h` — updated inheritance, added econ gloves, fixed GetBonePos
- `source/cs2/entity/C_CSPlayerPawn/C_CSPlayerPawn.cpp` — updated to use schema accessors
- `source/templeware/interfaces/CGameEntitySystem/CGameEntitySystem.h` — enhanced with handle resolution, local player access
- `source/templeware/interfaces/interfaces.h` — fixed IGameResourceService usage
- `source/templeware/utils/localplayer/localplayer.h` — new local player snapshot/cache system
- `source/templeware/utils/schema/schema.h` — made schema accessors const-correct
- `source/templeware/features/prediction/prediction.cpp` — fixed m_MovementServices → m_pMovementServices, fixed tick base assignment
- `TempleWare-CS2.vcxproj` — added C_BaseEntity.cpp

## Existing TempleWare Components Reused
- C_BaseEntity, C_CSPlayerPawn, CCSPlayerController — extended with proper inheritance hierarchy
- CCSPlayer_WeaponServices, C_PlayerMovementService, CPlayer_ObserverServices — kept and extended
- CGameEntitySystem, IGameResourceService — enhanced with pattern-based resolution
- Schema system (SchemaFinder, schema macros) — made const-correct, used for all new fields
- Handle system (CBaseHandle) — used throughout for entity references
- Pattern scanning (M::FindPattern, M::scan_absolute) — used for interface/entity resolution
- FNV1a hashing (HASH macro) — used for schema field lookup
- Interfaces (IEngineClient, CCSGOInput, CGlobalVarsBase, CPrediction, INetworkClient, ISceneSystem) — unchanged

## New/Adapted SDK Components
- **CCollisionProperty** — mins, maxs, solid flags, collision group via schema
- **C_BasePlayerWeapon** — clip1/2, next attack ticks, reload state, burst mode
- **C_BasePlayerPawn** — weapon/movement/item/observer/camera services, controller handle, view angle changes
- **C_BasePlayerController** — pawn handle, local player flag, tick base, steam ID, player name, observer pawn/mode
- **CPlayer_ItemServices / CCSPlayer_ItemServices** — helmet, heavy armor, armor value, defuser
- **C_EconEntity** — attribute manager, fallback paint kit/seed/wear/stattrak, update_subclass/skin/weapon_data
- **C_EconItemView** — definition index, entity quality, item ID, attributes, custom name, fallback fields
- **C_AttributeContainer / C_AttributeList** — item attribute storage
- **CGameEntitySystem** — Get by index/handle, get_local_pawn, get_local_controller, get_base_entity (pattern-based)
- **LocalPlayerCache** — snapshot with controller, pawn, observer, team, alive state; thread-safe with shared_mutex

## Entity Relationships
- **Controller → Pawn**: `CCSPlayerController::m_hPawn()` (CBaseHandle) → `CGameEntitySystem::Get(handle)`
- **Pawn → Controller**: `C_BasePlayerPawn::m_hController()` (CBaseHandle)
- **Pawn → Weapon**: `C_BasePlayerPawn::m_pWeaponServices()->m_hActiveWeapon()` → `CGameEntitySystem::Get(handle)`
- **Pawn → Weapon Services**: `C_BasePlayerPawn::m_pWeaponServices()` (CCSPlayer_WeaponServices*)
- **Pawn → Movement Services**: `C_BasePlayerPawn::m_pMovementServices()` (C_PlayerMovementService*)
- **Pawn → Item Services**: `C_BasePlayerPawn::m_pItemServices()` (CPlayer_ItemServices*)
- **Pawn → Scene Node**: `C_BaseEntity::m_pGameSceneNode()` (CGameSceneNode*) → skeleton/bones
- **Weapon → Weapon Data**: `C_CSWeaponBase::m_pWeaponData()` (CCSWeaponBaseVData*) via subclass ID
- **Entity Identity**: `CEntityInstance::m_pEntityIdentity()` → `CEntityIdentity::index()` / `get_serial_number()`

## Interfaces/Globals Available
- **IEngineClient** — `I::EngineClient` (maxClients, in_game, connected, get_local_player, get_screen_size)
- **CGameEntitySystem** — `I::GameEntity->Instance` (Get by index/handle, get_local_pawn/controller)
- **CCSGOInput** — `I::Input` (GetViewAngles, SetViewAngle, get_user_cmd)
- **CGlobalVarsBase** — `I::GlobalVars` (curtime, tickcount, frametime, interval_per_tick)
- **CPrediction** — `I::Prediction` (in_prediction, first_prediction, update)
- **INetworkClient** — `I::NetworkClient->get_network_client()` (tick info, prediction state, latency)
- **ISceneSystem** — `I::SceneSystem` (light_data_queue)
- **ILocalize** — `I::Localize` (via CreateInterface)
- **Schema System** — `SchemaFinder::Get(hash)` for compile-time schema offsets

## Important Decisions
- **Schema accessors are const** — all `schema()` macros generate const member functions; write access requires direct memory writes via SchemaFinder offset
- **No circular includes** — C_BaseEntity forward declares CGameSceneNode; implementation in C_BaseEntity.cpp
- **Compat aliases** — CCSPlayerController provides `IsLocalPlayer()`, `m_bPawnIsAlive()`, `m_sSanitizedPlayerName()`, `m_nTickBase()` calling base schema accessors
- **Local player caching** — LocalPlayerCache::update() should be called once per frame; get() returns thread-safe snapshot
- **Econ entity update** — C_EconEntity::update_subclass/skin/weapon_data use pattern-scanned virtual calls

## Build Verification
- **Command**: `MSBuild.exe TempleWare-CS2.vcxproj /p:Configuration=Release /p:Platform=x64`
- **Result**: Success — `TempleWare.dll` built at `C:\CS\TempleWare\x64\Release\TempleWare.dll`
- **Remaining errors**: None (clean build)

## Phase 3 Notes
- **Bones/Hitboxes**: CGameSceneNode → CSkeletonInstance → C_ModelState → C_BoneData/C_Model available; GetBonePos() in C_CSPlayerPawn.h works
- **Tracing**: INetworkClient and CGameSceneNode collision property available; need trace_ray/trace_hull implementations
- **Input**: CCSGOInput::GetViewAngles/SetViewAngle and get_user_cmd working; CUserCmd with protobuf fields defined
- **Local entities**: LocalPlayerCache provides cached controller/pawn/observer; update per frame
- **Schema offsets**: All fields use compile-time schema offsets via SchemaFinder; no runtime schema traversal needed