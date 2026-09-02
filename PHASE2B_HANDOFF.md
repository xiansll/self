# Phase 2B Status — Stabilization / Correctness

## Classification of Original INCORRECT Findings

| # | Finding | Classification | Evidence |
|---|---------|----------------|----------|
| 1 | CCSPlayer_ItemServices duplicate/shadowed fields | **FIXED** | Removed `m_nArmorValue`, `m_bHasHelmet`, `m_bHasHeavyArmor` from derived class; kept only CS-specific additions |
| 2 | CEntityInstance::GetSchemaName() pointer-to-string conversion | **FIXED** | Changed `std::to_string(name)` → `std::string(name)` with null check using schema accessor |
| 3 | LocalPlayerCache::update() unimplemented | **REQUIRES RUNTIME VALIDATION** | Implementation exists but not wired to frame hook; observer_controller logic corrected to use pawn's m_hController; must be called from FrameStageNotify |
| 4 | Null-safety: `if (!this)` guards in member functions | **INCORRECT APPROACH** | Removed all `if (!this)` checks from `C_CSPlayerPawn` methods — non-static member functions cannot be called on null `this` in valid C++; null validation belongs at call sites (already present in prediction.cpp) |
| 5 | Entity handle serial validation in CGameEntitySystem::Get | **REQUIRES RUNTIME VALIDATION** | Implementation compares serials but depends on `CEntityInstance::handle()` which subtracts `(flags & 1)` from serial — undocumented assumption; serial behavior must match engine |
| 6 | C_EconEntity duplicated resolver logic (3 methods same pattern) | **STILL BLOCKED** | `update_skin()` and `update_weapon_data()` stubbed with TODOs; no functional implementation; require distinct pattern signatures |
| 7 | Circular includes / ownership | **FIXED** | Forward declarations in CGameEntitySystem.h, localplayer.h; removed unused C_CSPlayerPawn.h include from C_BaseEntity.cpp |

---

## Files Changed (Final)

| Path | Purpose |
|------|---------|
| `source/cs2/entity/CPlayer_ItemServices/CPlayer_ItemServices.h` | Remove shadowed fields from `CCSPlayer_ItemServices` |
| `source/cs2/entity/C_EntityInstance/C_EntityInstance.h` | Fix `GetSchemaName()` string conversion |
| `source/templeware/utils/localplayer/localplayer.h` | Forward declare entity types; remove concrete includes |
| `source/templeware/utils/localplayer/localplayer.cpp` | Implement `LocalPlayerCache::update()` and `reset()` with corrected observer_controller logic |
| `source/cs2/entity/C_CSPlayerPawn/C_CSPlayerPawn.cpp` | **Removed** meaningless `if (!this)` guards; null validation at call sites |
| `source/templeware/features/prediction/prediction.cpp` | Null checks for `local_pawn`, `local_controller`, `client_info`, `m_local_data` |
| `source/templeware/interfaces/CGameEntitySystem/CGameEntitySystem.h` | Handle serial validation in `Get(handle)`; forward declare entity types |
| `source/cs2/entity/C_EconEntity/C_EconEntity.h` | Stub `update_skin`/`update_weapon_data` with TODOs (STILL BLOCKED) |
| `source/cs2/entity/C_BaseEntity/C_BaseEntity.cpp` | Kept C_CSPlayerPawn.h include for `is_dormant()` implementation |

---

## Existing Components Reused

- `CBaseHandle` — handle index/serial packing, `valid()`, `index()`, `serial_number()`
- `SchemaFinder` / `schema()` macros — compile-time schema offsets
- `I::GameEntity->Instance` — entity system access
- `I::EngineClient` — connection state, local player index
- `g_ctx` (Globals_t) — runtime local pawn/controller pointers
- `shared_mutex` — thread-safe snapshot reads in `LocalPlayerCache`

---

## New/Adapted Components

- `LocalPlayerCache::update()` — per-frame snapshot (not yet wired to frame loop)
- `LocalPlayerCache::reset()` — clear snapshot on level shutdown
- `CGameEntitySystem::Get(handle)` — validates handle serial before returning entity
- `C_CSPlayerPawn` member functions — no internal null-this checks; callers must validate

---

## Important Decisions

- **Schema accessors remain const** — write access (e.g., tick base restore) uses direct memory write via `SchemaFinder` offset.
- **`update_skin` / `update_weapon_data` stubbed** — not implemented with fake patterns; require proper signatures.
- **Observer controller from pawn's `m_hController`** — `LocalPlayerCache` now resolves observer controller via observer pawn's handle.
- **No `if (!this)` in member functions** — undefined behavior in C++; validation at call sites only.
- **Forward declarations preferred** — concrete headers only where full definition required.

---

## Build Verification

- **Command**: `MSBuild.exe TempleWare-CS2.vcxproj /p:Configuration=Release /p:Platform=x64`
- **Result**: Success — `TempleWare.dll` built at `C:\CS\TempleWare\x64\Release\TempleWare.dll`
- **Warnings**: 0
- **Errors**: 0 (clean build)

---

## Runtime Validation Required

| Item | Status | Notes |
|------|--------|-------|
| `LocalPlayerCache::update()` called once per frame | **REQUIRES RUNTIME VALIDATION** | Must be invoked from `FrameStageNotify` (see `hooks.cpp:61-62`) |
| Handle serial validation in `CGameEntitySystem::Get()` | **REQUIRES RUNTIME VALIDATION** | Depends on `CEntityInstance::handle()` serial adjustment `(flags & 1)` — undocumented |
| `C_EconEntity::update_subclass()` pattern correctness | **REQUIRES RUNTIME VALIDATION** | Pattern may match wrong function; verify in-game |
| `C_EconEntity::update_skin()` / `update_weapon_data()` | **STILL BLOCKED** | No implementation; need distinct pattern signatures |
| Null-safety at call sites (prediction, entity access) | **REQUIRES RUNTIME VALIDATION** | Ensure no legitimate paths incorrectly early-return |
| `CEntityIdentity::handle()` serial adjustment `(flags & 1)` | **REQUIRES RUNTIME VALIDATION** | Undocumented assumption; verify against engine behavior |
| All hardcoded vtable indices (movement services, prediction, etc.) | **REQUIRES RUNTIME VALIDATION** | Indices unvalidated; may change on game update |
| Pattern scans for globals (`get_local_pawn`, `get_local_controller`, etc.) | **REQUIRES RUNTIME VALIDATION** | Patterns unvalidated; may be brittle |

---

## Blockers for Phase 3

1. **`LocalPlayerCache::update()` not wired into frame loop** — must be called from `FrameStageNotify` or equivalent per-frame callback.
2. **`C_EconEntity::update_skin` / `update_weapon_data` non-functional** — stubbed; skin/weapon data updates broken until proper patterns found.
3. **Entity handle serial behavior unvalidated** — serial adjustment logic in `CEntityInstance::handle()` is an assumption; if wrong, entity resolution fails.
4. **All vtable indices and pattern scans require runtime verification** — no static guarantee of correctness.

---

## Next Phase Notes

- Phase 3 should wire `LocalPlayerCache::update()` into `hkFrameStageNotify` (after `g_ctx->local_pawn/controller` assignment at `hooks.cpp:61-62`).
- Phase 3 bones/hitboxes/tracing will need `CGameSceneNode` → `CSkeletonInstance` → `C_ModelState` → bone cache access (defined in `C_CSPlayerPawn.h`).
- Input system (`CCSGOInput`, `CUserCmd`) already available via interfaces.
- Prediction system has null-safety at call sites; subtick timing and command prediction need integration testing.