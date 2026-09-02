# Phase 3C — Full Staged SDK Validation

## Scope

Phase 3C is a diagnostic-only runtime suite driven from the already-proven Present path. It does not install the suspect FrameStageNotify detour, change existing resolver signatures, add new game signatures/offsets, write game state, or activate gameplay features.

The suite executes once for each new local pawn/controller pair (for example after spawn/respawn). Potentially unsafe TempleWare wrapper calls are surrounded by stage markers and SEH guards so a runtime fault is attributed to the exact call.

## Stage order

1. `S1 resolver-addresses`
   - Existing local pawn resolver address
   - Existing local controller resolver address
2. `S2 local getter semantics`
   - `I_EntitySystem::get_local_pawn()`
   - `I_EntitySystem::get_local_controller()`
   - provider alias check
3. `S3 pawn basic wrapper compatibility`
   - `m_iTeamNum()`
   - `m_iHealth()`
   - `is_alive()`
4. `S4 controller basic wrapper compatibility`
   - `m_iDesiredTeam()`
   - `m_hObserverPawn()`
5. `S5 entity identity validation`
   - pawn identity
   - controller identity
6. `S6 scene chain validation`
   - pawn -> scene node -> skeleton -> bone cache
7. `S7 observer handle validation`
   - state-dependent; SKIP is expected when no observer pawn is active

Each risky operation logs `ENTER` before execution and `PASS` or `FAIL` afterwards. An access violation is logged with its exception code instead of intentionally proceeding through dependent checks.

## Completion marker

Each run ends with:

`[P3C][COMPLETE] PASS/FAIL - run=N result=... logical_failures=X exceptions=Y skips=Z`

A `SKIP` is not automatically a failure because some checks (notably observer-handle validation) depend on the player's current state.

## Known baseline entering Phase 3C

Phase 3B runtime testing established that the pointer-only local provider supplies stable non-null pawn/controller pointers, follows pawn replacement, and resets the cache cleanly on the in-game -> out-of-game transition. The TempleWare SDK local getters were still returning null, so Phase 3C explicitly separates resolver-call failure from wrapper/layout compatibility.

## Runtime status

PENDING USER BUILD/RUNTIME TEST after the full-suite commit. Do not mark Phase 3C passed until the complete staged trace is observed in a fresh runtime log.
