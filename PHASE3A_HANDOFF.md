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

## Root Cause Found

**Why Validation::Initialize() was unreachable before:**
- `Validation::Initialize()` was only called from `H::Hooks::init()` (`source/templeware/hooks/hooks.cpp:163`)
- `H::Hooks::init()` was only called from `TempleWare::init()` (`source/templeware/templeware.cpp:50`)
- `TempleWare::init()` was only called from the **backup** `main.cpp.bak_20260830_164200` (line 130)
- The **active** `main.cpp` did NOT instantiate `TempleWare` and did NOT call `TempleWare::init()`
- Therefore the entire TempleWare foundation (interfaces, hooks, LocalPlayerCache, validation) was never initialized

**Why Phase3A messages did not appear in TempleWare.log:**
- The active runtime (`main.cpp`) used its own private `LogToFile()` function writing to `%TEMP%/TempleWare.log`
- The TempleWare validation logging used `Logger::Log()` which calls `I::ConColorMsg()` (console-only, no file output)
- The TempleWare file logger in `templeware.cpp::FileLog()` was never reached because `TempleWare::init()` was never called
- Two completely separate logging systems existed with no connection between them

## Previous Unreachable Call Graph

```
DllMain (main.cpp:163)
  -> MainThread (main.cpp:139)
      -> kiero::init(D3D11)
      -> kiero::bind(8, hkPresent)
          -> hkPresent (main.cpp:69) [on first Present call]
              -> ImGui/overlay init
              -> Chams::Initialize()
              -> Trace::Initialize()
              -> Icons::Initialize()
              -> Esp::Draw()/Update*()
              -> nerv_bridge::tick()
              -> Gui::Render()
          [TempleWare foundation NEVER reached]
              [Validation::Initialize() NEVER called]
              [H::Hooks::init() NEVER called]
              [hkFrameStageNotify NEVER installed]
              [LocalPlayerCache::update() NEVER called]
```

## Runtime Architecture Fix

**Subsystem Classification:**

| Subsystem | Classification | Notes |
|-----------|----------------|-------|
| modules | REQUIRED FOUNDATION | Now initialized via `initFoundation()` |
| schema | REQUIRED FOUNDATION | Now initialized via `initFoundation()` |
| interfaces | REQUIRED FOUNDATION | Now initialized via `initFoundation()` |
| hooks | REQUIRED FOUNDATION | Now initialized via `initFoundation()` |
| renderer | DUPLICATE | Active in main.cpp (Chams/ESP/Gui); NOT initialized by foundation |
| menu | DUPLICATE | Active in main.cpp (Gui); NOT initialized by foundation |
| visuals | DUPLICATE | Active in main.cpp (Esp); NOT initialized by foundation |
| Present hook | ACTIVE CURRENT | Owned by main.cpp kiero bind |
| ESP | ACTIVE CURRENT | Owned by main.cpp Esp namespace |
| Chams | ACTIVE CURRENT | Owned by main.cpp Chams namespace |
| Trace | ACTIVE CURRENT | Owned by main.cpp Trace namespace |
| nerv_bridge | ACTIVE CURRENT | Owned by main.cpp, fed TW local pointers |
| LocalPlayerCache | REQUIRED FOUNDATION | Now updated from hkFrameStageNotify |
| validation | REQUIRED FOUNDATION | Now initialized and logging to file |

**Fix Applied:**
1. Split `TempleWare::init()` into `initFoundation()` (modules, schema, interfaces, hooks) and `initRenderer()` (menu, visuals)
2. Created reusable `FileLog` utility (`source/templeware/utils/filelog/filelog.h/.cpp`) writing to `%TEMP%/TempleWare.log`
3. Modified active `main.cpp` to:
   - Include `templeware.h` and `filelog.h`
   - Instantiate `TempleWare g_templeWare`
   - Call `FileLog::Initialize()` in `DllMain`
   - Call `g_templeWare.initFoundation()` on first `hkPresent` after D3D11 device ready
   - Replace private `LogToFile()` with `FileLog::Log()`
4. Validation now dual-logs: `Logger::Log()` (console) + `FileLog::Log()` (file)
5. Added one-time milestone markers (see below)

**Duplicate Legacy Systems Deliberately NOT Initialized:**
- `renderer.menu.init()` — main.cpp owns ImGui/Gui
- `renderer.visuals.init()` — main.cpp owns Esp/Chams/Trace/Icons
- `TempleWare::init()` full path — would double-initialize renderer/menu/visuals

## Validation Logging Fix

- Refactored proven file logger from `main.cpp::LogToFile()` into reusable `FileLog` utility
- Validation now writes to same `%TEMP%/TempleWare.log` as active runtime
- All validation messages (including milestones) appear in single unified log
- Console logging via `Logger::Log()` preserved for debug visibility

## Files Changed

| Path | Purpose |
|------|---------|
| `source/templeware/utils/filelog/filelog.h` | New reusable file logger interface |
| `source/templeware/utils/filelog/filelog.cpp` | New reusable file logger implementation |
| `source/templeware/templeware.h` | Added `initFoundation()` and `initRenderer()` declarations |
| `source/templeware/templeware.cpp` | Split init into foundation/renderer; use FileLog utility |
| `source/main.cpp` | Call `initFoundation()` from hkPresent; use FileLog; instantiate TempleWare |
| `source/templeware/utils/validation/validation.h` | Added milestone logging function declarations |
| `source/templeware/utils/validation/validation.cpp` | Added milestone logging; dual-log to FileLog + Logger |
| `source/templeware/hooks/hooks.cpp` | Added `FRAMESTAGE HOOK INSTALLED` and `FRAMESTAGE FIRST CALL` milestones |
| `TempleWare-CS2.vcxproj` | Added filelog.cpp/.h to build |

## Exact Startup Path After Fix

```
DllMain (main.cpp)
  -> MainThread
      -> kiero::init(D3D11)
      -> kiero::bind(8, hkPresent)
          -> hkPresent (first call)
              -> GetDevice/GetImmediateContext/CreateRenderTargetView
              -> ImGui/Win32/DX11 init
              -> FileLog::Log("overlay init complete")
              -> Chams::Initialize()
              -> Trace::Initialize()
              -> Icons::Initialize()
              -> g_templeWare.initFoundation()  [NEW]
                  -> Validation::LogFoundationInitBegin()
                  -> modules.init()
                  -> schema.init("client.dll", 0)
                  -> interfaces.init()
                  -> Validation::LogInterfacesReady()
                  -> Validation::LogHookInitBegin()
                  -> hooks.init()
                      -> Validation::Initialize() -> [Validation] PHASE3A BUILD ACTIVE
                      -> FrameStageNotify hook installed
                      -> Validation::LogFramestageHookInstalled()
                      -> MH_EnableHook(MH_ALL_HOOKS)
              -> foundationInit = true
          -> hkPresent (subsequent calls)
              -> Esp/Chams/Trace/nerv_bridge/Gui tick
              -> FrameStageNotify fires (from hooks.init)
                  -> original(a1, stage)
                  -> Validation::LogFramestageFirstCall() [first in-game call only]
                  -> g_ctx->local_pawn = get_local_pawn()
                  -> g_ctx->local_controller = get_base_entity<CCSPlayerController>(get_local_player())
                  -> g_local_player_cache->update()
                  -> Validation::OnLocalPlayerCacheUpdate() -> [Validation] CACHE UPDATE FIRST CALL
                  -> Validation::OnSceneNodeChainCheck()
                  -> Validation::OnEntityIdentityCheck()
                  -> Validation::LogPeriodicSummary()
```

## Build Verification

- **Command**: `MSBuild.exe TempleWare-CS2.vcxproj /p:Configuration=Release /p:Platform=x64` (via VS2022 MSBuild at `C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe`)
- **Result**: Success — `TempleWare.dll` built at `C:\CS\TempleWare\x64\Release\TempleWare.dll`
- **Warnings**: 0
- **Errors**: 0 (clean build)

## Expected Milestone Order

1. `[Validation] PHASE3A BUILD ACTIVE` — from `Validation::Initialize()` in `hooks.init()`
2. `[Validation] FOUNDATION INIT BEGIN` — from `Validation::LogFoundationInitBegin()` at start of `initFoundation()`
3. `[Validation] INTERFACES READY` — after `interfaces.init()` completes
4. `[Validation] HOOK INIT BEGIN` — before `hooks.init()` starts
5. `[Validation] FRAMESTAGE HOOK INSTALLED` — after `FrameStageNotify.Add()` succeeds
6. `[Validation] FRAMESTAGE FIRST CALL` — first time `hkFrameStageNotify` executes in-game
7. `[Validation] CACHE UPDATE FIRST CALL` — first time `OnLocalPlayerCacheUpdate()` runs

Each milestone executes exactly once (guarded by `std::atomic<bool>` exchange).

## Runtime Results

**PENDING NEW RUNTIME TEST**

Do NOT claim Phase3A runtime validation passed until an actual new TempleWare.log is supplied with the above milestone sequence.

## Next Phase Notes

- Phase 3B can proceed once runtime logs confirm:
  - All 7 milestones appear in order
  - `LocalPlayerCache` updates show valid pawn/controller pointers
  - `EntityIdentity` checks show matching handle/index/serial
  - `SceneNodeChain` shows valid skeleton/bone_cache pointers
  - No `HANDLE MISMATCH` or `identity_mismatches` in steady state
- Handle serial adjustment `(flags & 1)` remains REQUIRES RUNTIME VALIDATION
- All vtable indices and pattern scans remain REQUIRES RUNTIME VALIDATION