# Phase 2C Status — Runtime Wiring and Validation Prep

## Runtime Wiring Completed

- **LocalPlayerCache wired into FrameStageNotify**: `g_local_player_cache->update()` called once per frame in `hkFrameStageNotify` after `g_ctx->local_pawn/controller` are updated (hooks.cpp:64)
- **LocalPlayerCache reset on level shutdown**: `g_local_player_cache->reset()` called in `hkOnLevelShutdown` (hooks.cpp:101)
- **Added missing include**: `<shared_mutex>` in localplayer.h for C++17 `std::shared_mutex`/`shared_lock`
- **Added localplayer.cpp/.h to vcxproj**: Both compile and link now work

## Files Changed

| Path | Purpose |
|------|---------|
| `source/templeware/hooks/hooks.cpp` | Call `g_local_player_cache->update()` in `hkFrameStageNotify` after local entity pointers set; call `reset()` in `hkOnLevelShutdown` |
| `source/templeware/utils/localplayer/localplayer.h` | Add `<shared_mutex>` include |
| `TempleWare-CS2.vcxproj` | Add `source/templeware/utils/localplayer/localplayer.cpp` to ClCompile; add `localplayer.h` to ClInclude |

## LocalPlayerCache Lifecycle

- **Update location**: `H::hkFrameStageNotify` (hooks.cpp:51-73), called once per frame after:
  1. `original(a1, stage)` — original engine frame stage
  2. Connection/in-game check
  3. `g_ctx->local_pawn = I::EntitySystem->get_local_pawn()`
  4. `g_ctx->local_controller = I::EntitySystem->get_base_entity<CCSPlayerController>(I::EngineClient->get_local_player())`
- **Reset location**: `H::hkOnLevelShutdown` (hooks.cpp:94-103), called on level shutdown before skinchanger shutdown
- **Ordering requirements**: Must run after local pawn/controller pointers are valid; before any consumer (ESP, aimbot, prediction) reads the snapshot. Current placement satisfies this.
- **Thread safety**: `shared_mutex` protects snapshot reads via `get()`; `unique_lock` in `update()`/`reset()` ensures exclusive writes.

## Handle Validation Status

| Component | Status | Notes |
|-----------|--------|-------|
| `CBaseHandle` packing (15-bit index + serial) | **STATICALLY VERIFIED** | Matches Source 2 entity limits |
| `CEntityIdentity::index` schema offset (0x10) | **STATICALLY VERIFIED** | Uses `SCHEMA_ADD_OFFSET` |
| `CEntityIdentity::get_index()` / `get_serial_number()` | **STATICALLY VERIFIED** | Correct bitmask/shift per `CBaseHandle` layout |
| `CEntityInstance::handle()` construction | **REQUIRES RUNTIME VALIDATION** | Applies undocumented `serial - (flags & 1)` adjustment |
| `CGameEntitySystem::Get(handle)` serial comparison | **REQUIRES RUNTIME VALIDATION** | Compares `hHandle.serial` vs `entity->handle().serial`; consistency depends on `CEntityInstance::handle()` adjustment matching engine's stored handle serial |

**Critical assumption**: `CEntityInstance::handle()` subtracts `(flags & 1)` from the raw serial read from `CEntityIdentity`. This is not documented in any local source. If the engine stores handles without this adjustment, serial validation will incorrectly reject valid entities. No fix possible without runtime evidence.

## Deferred Econ Functionality

| Method | Status | Phase 3 Dependency |
|--------|--------|-------------------|
| `C_EconEntity::update_subclass()` | **FUNCTIONAL** (pattern-scanned) | No |
| `C_EconEntity::update_skin()` | **STILL BLOCKED** (stubbed no-op) | No |
| `C_EconEntity::update_weapon_data()` | **STILL BLOCKED** (stubbed no-op) | No |

**Call sites**: `nerv/features/skin_changer/skin_changer.cpp` calls all three for weapon/knife skin changes. TempleWare's `features/skinchanger/features.cpp` has its own working implementations (`sc_real::UpdateSkin` via vfunc 110, `UpdateWeaponData` via vfunc 195) but is NOT wired into frame loop (commented out in hooks.cpp:68).

**Decision**: Econ methods deferred. Phase 3 (bones/hitboxes/tracing/input) does not depend on skin changer. The `nerv` skin changer will silently fail to update skins; no crash risk from no-op stubs.

## Phase 3 Runtime Dependencies

| Dependency | Classification | Evidence |
|------------|----------------|----------|
| **Entity system access** (`I::GameEntity->Instance->Get()`) | **REQUIRES RUNTIME VALIDATION** | Pattern-scanned `GetEntityByIndex` hook; handle serial validation unproven |
| **Scene node access** (`C_BaseEntity::m_pGameSceneNode()`) | **REQUIRES RUNTIME VALIDATION** | Schema offset; `CGameSceneNode` layout assumed |
| **Skeleton/bone access** (`CGameSceneNode::GetSkeletonInstance()` vfunc 13, `CSkeletonInstance::m_bone_cache`) | **REQUIRES RUNTIME VALIDATION** | Hardcoded vfunc index 13; bone cache offset 0x140+0x80 in `GetBonePos` |
| **Model/hitbox access** (`C_ModelState::get_bone_data` offset 0x80, `C_Model::get_hitbox`) | **REQUIRES RUNTIME VALIDATION** | Hardcoded offsets; `C_Model` layout reverse-engineered |
| **Vtable calls** (movement services 32/46/47, prediction, input) | **REQUIRES RUNTIME VALIDATION** | All hardcoded indices; may change on game update |
| **Pattern-resolved globals** (`get_local_pawn`, `get_local_controller`, `get_base_entity`, trace manager) | **REQUIRES RUNTIME VALIDATION** | Patterns unvalidated; brittle across versions |
| **Input system** (`CCSGOInput::GetViewAngles`, `SetViewAngle`, `get_user_cmd`) | **REQUIRES RUNTIME VALIDATION** | Pattern-scanned functions; `get_user_cmd` uses 3 chained pattern scans |
| **Trace system** (`Trace::Initialize` patterns for manager/ray/filter) | **REQUIRES RUNTIME VALIDATION** | Custom pattern scanner in trace.cpp; patterns unvalidated |

## Build Verification

- **Command**: `MSBuild.exe TempleWare-CS2.vcxproj /p:Configuration=Release /p:Platform=x64`
- **Result**: Success — `TempleWare.dll` built at `C:\CS\TempleWare\x64\Release\TempleWare.dll`
- **Warnings**: 0
- **Errors**: 0 (clean build)

## Remaining Blockers

1. **Entity handle serial adjustment** (`flags & 1`) — undocumented; cannot be proven correct from local code; requires runtime validation against live engine
2. **All vtable indices** (movement services 32/46/47, prediction, skeleton instance 13, input, trace) — hardcoded; no static guarantee
3. **All pattern scans** (entity system, local player, input, trace, econ) — unvalidated; may match wrong functions or break on update
4. **Bone cache offset** (0x140 + 0x80 in `GetBonePos`) — reverse-engineered; not schema-based
5. **`nerv` skin changer non-functional** — `update_skin`/`update_weapon_data` stubbed; TempleWare skinchanger not wired

## Ready For Phase 3

**YES** — with caveats.

The core entity SDK, handle system, local player cache, and frame lifecycle wiring are in place and build cleanly. Phase 3 (bones/hitboxes/tracing/input) can proceed using the existing infrastructure. All Phase 3 dependencies are classified as **REQUIRES RUNTIME VALIDATION** — they compile and are architecturally integrated, but their runtime correctness depends on binary-version-specific values (vtable indices, pattern matches, memory layouts) that can only be verified in-game.

The handle serial validation and econ stubs are documented blockers that do not prevent Phase 3 work but must be resolved before feature-complete release.