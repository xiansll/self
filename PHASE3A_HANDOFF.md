# Phase 3A Status — Runtime Validation Harness

## Diagnostics Added

- **Validation harness module** (`source/templeware/utils/validation/validation.h/.cpp`)
  - Thread-safe counters for all validation categories
  - Rate-limited logging (configurable tick interval, default 64 ticks)
  - State-change and error logging (no per-frame spam)

- **LocalPlayerCache lifecycle validation**
  - `OnLocalPlayerCacheUpdate()` — logs snapshot contents (pawn/controller/observer addresses, team, alive state)
  - `OnLocalPlayerCacheReset()` — logs reset events
  - Integrated into `hkFrameStageNotify` (after pointer updates) and `hkOnLevelShutdown`

- **Entity handle resolution validation**
  - `OnEntityHandleLookup()` — compares handle serial vs resolved entity's serial
  - Logs mismatches with full index/serial details for both handle and entity
  - Integrated into `CGameEntitySystem::Get(handle)`

- **Entity identity validation**
  - `OnEntityIdentityCheck()` — verifies entity index, serial, schema name, handle consistency
  - Checks `CEntityIdentity::valid()`, `get_index()`, `get_serial_number()`, `GetSchemaName()`
  - Verifies handle reconstruction matches identity
  - Called for local pawn and controller each frame

- **Scene-node chain validation**
  - `OnSceneNodeChainCheck()` — validates pawn → scene node → skeleton instance → bone cache
  - Logs pointer availability at each level
  - Called for local pawn each frame

- **VTable call validation** (instrumentation points ready)
  - `OnVTableCall()` — logs vtable index, this pointer, success/failure
  - Not yet hooked into specific call sites (see Remaining Assumptions)

- **Pattern resolution validation** (instrumentation points ready)
  - `OnPatternResolution()` — logs pattern name, resolved address, success/failure
  - Not yet hooked into specific pattern scans (see Remaining Assumptions)

## Files Changed

| Path | Purpose |
|------|---------|
| `source/templeware/utils/validation/validation.h` | Validation harness interface — counters, rate limiter, function declarations |
| `source/templeware/utils/validation/validation.cpp` | Implementation — logging, counters, rate limiting |
| `source/templeware/hooks/hooks.cpp` | Added validation calls in `hkFrameStageNotify`, `hkOnLevelShutdown`, `Hooks::init` |
| `source/templeware/interfaces/CGameEntitySystem/CGameEntitySystem.h` | Added handle lookup validation in `Get(handle)` |
| `source/templeware/interfaces/CGameEntitySystem/CGameEntitySystem.h` | Added validation.h include |
| `TempleWare-CS2.vcxproj` | Added validation.cpp/.h to build |

## Runtime Checks Available

| Check | Trigger | Logs On |
|-------|---------|---------|
| LocalPlayerCache update | Every frame (in-game) | Snapshot contents, rate-limited |
| LocalPlayerCache reset | Level shutdown | Reset event |
| Handle lookup | Every `Get(handle)` call | Mismatches (always), OK (rate-limited) |
| Entity identity | Every frame (local pawn/controller) | Index, serial, schema name, handle match |
| Scene-node chain | Every frame (local pawn) | Pointer chain, bone cache, bone count |
| Periodic summary | Every 64 ticks (configurable) | All counters aggregated |

## Expected Log Output

```
[Validation] Diagnostic harness initialized
[Validation] LocalPlayerCache update #1: pawn=0x... controller=0x... observer_pawn=0x0 observer_ctrl=0x0 team=2 alive=1 valid=1
[Validation] SceneNodeChain: scene=0x... skeleton=0x... bone_cache=0x... bone_count=128
[Validation] EntityIdentity: idx=1 serial=5 valid=1 schema='C_CSPlayerPawn' handle_match=1
[Validation] Handle OK: idx=1 serial=5 resolved=0x...
[Validation] Summary: cache_updates=64 resets=0 handle_lookups=128 handle_mismatches=0 identity_checks=128 identity_mismatches=0 scene_checks=64 scene_failures=0 vtable_calls=0 vtable_failures=0 pattern_resolutions=0 pattern_failures=0
[Validation] LocalPlayerCache reset
```

## Runtime Results

**PENDING** — Requires in-game testing with live CS2 instance. No runtime logs available at this time.

## Build Verification

- **Command**: `MSBuild.exe TempleWare-CS2.vcxproj /p:Configuration=Release /p:Platform=x64`
- **Result**: Success — `TempleWare.dll` built at `C:\CS\TempleWare\x64\Release\TempleWare.dll`
- **Warnings**: 0
- **Errors**: 0 (clean build)

## Remaining Assumptions

| Assumption | Status | Notes |
|------------|--------|-------|
| Handle serial adjustment `(flags & 1)` in `CEntityInstance::handle()` | **REQUIRES RUNTIME VALIDATION** | Undocumented; validation logs will reveal mismatches |
| `CEntityIdentity::index` schema offset (0x10) | **REQUIRES RUNTIME VALIDATION** | Static offset; may differ by game version |
| `CGameSceneNode::GetSkeletonInstance()` vfunc index 13 | **REQUIRES RUNTIME VALIDATION** | Hardcoded; will log NULL if wrong |
| Bone cache offset (0x140 + 0x80 in `GetBonePos`) | **REQUIRES RUNTIME VALIDATION** | Not schema-based; will log NULL if wrong |
| `get_local_pawn` / `get_local_controller` patterns | **REQUIRES RUNTIME VALIDATION** | Pattern-scanned; validation logs NULL pointers |
| Movement services vtable indices (32/46/47) | **REQUIRES RUNTIME VALIDATION** | Not yet instrumented; ready for hooking |
| Prediction vtable calls | **REQUIRES RUNTIME VALIDATION** | Not yet instrumented |
| Trace system patterns (manager/ray/filter) | **REQUIRES RUNTIME VALIDATION** | Not yet instrumented |
| Input system patterns (`get_user_cmd` chain) | **REQUIRES RUNTIME VALIDATION** | Not yet instrumented |

## Ready For Next Phase

**PENDING RUNTIME TEST**

The validation harness is fully implemented, integrated, and builds cleanly. All Phase 3 runtime dependencies have instrumentation points ready. The next step is to run the module in-game and collect actual log output to classify each assumption as VALID, NULL/UNAVAILABLE, INCONSISTENT, or NOT TESTED. Do not proceed to feature implementation (Phase 3B) until runtime results are analyzed.