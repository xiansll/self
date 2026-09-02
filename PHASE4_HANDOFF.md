# Phase 4 Handoff — Velocity Rage Compatibility Layer

## Objective

Phase 4 exists to make TempleWare structurally compatible with the data/lifecycle surface expected by Velocity's combat subsystem without directly copying operational Rage behaviour.

Velocity's `combat.hpp` is built on `systems::local`, `systems::input`, `systems::prediction`, `systems::entities`, `systems::bones`, `systems::hitboxes`, tracing, and settings/config state. The port therefore needs a compatibility boundary first; a direct `rage.cpp` copy would otherwise bind Velocity code to missing or incompatible TempleWare internals.

## Phase 4 Contract Status

The compile-time compatibility layer is now considered CLOSED for the current port boundary.

Implemented under `source/templeware/compat/`:

- `velocity_rage_compat.h`
  - common math/usercmd aliases
  - local snapshot adapter
  - unified readiness report
  - one port gate
- `velocity_command_compat.h`
  - command begin/end/reset ownership
  - command generation counter
  - no hook acquisition or command mutation
- `velocity_data_compat.h`
  - entity reference/cache shape
  - prediction-state shape
  - bone/skeleton shape
  - hitbox shape
  - rich trace result shape
- `velocity_config_compat.h`
  - isolated, versioned config snapshot/store
  - no direct dependency on TempleWare GUI globals
- `velocity_runtime_compat.h`
  - centralized volatile runtime readiness registry
  - lifecycle epoch
  - one volatile reset point
  - one shutdown point
  - disabled default config publication

## Important Contract vs Runtime Rule

`adapter=1` only means the compatibility surface exists and compiles.

`runtime=1` means a separately validated producer is actively supplying that surface.

These states must never be conflated. A non-null pointer or an existing C++ type is not proof that the corresponding live game object/layout is safe to interpret.

## Current Runtime State

Known-good / available:

- local pawn/controller pointer pair can be observed through the pointer-only fallback
- input interface pointer is present
- prediction object exists as a TempleWare service object
- config-store ownership can be established safely with a disabled default snapshot
- Present-only diagnostic/lifecycle path is stable
- disconnect transition can reset local cache and all volatile compatibility publications from one place

Still blocked / intentionally unproven:

- `sdk_deref_safe`
  - TempleWare local SDK resolver path is unresolved
  - pointer-only locals must not be dereferenced through TempleWare entity wrappers
- live trace backend
  - Trace API exists, but the current required internal functions are unresolved
  - no replacement pattern/signature was introduced
- command runtime
  - lifecycle contract exists
  - active Present diagnostic path does not acquire a live `CUserCmd*`
- entity-cache runtime producer
- bone runtime producer
- hitbox runtime producer
- rich-trace runtime producer

## Velocity Surface Alignment Notes

The data contracts were aligned against Velocity's actual `systems.hpp` / `combat.hpp` surface rather than invented independently:

- entity entries carry pointer, handle/schema/index/type metadata
- prediction state carries flags, velocity/origin state, movement impulses, friction, and stamina
- bones expose position/scale/quaternion-like rotation storage
- hitbox sets use a fixed 20-entry surface
- skeleton snapshots retain up to 128 bones
- rich trace results expose surface/entity/hitbox references, contents, positions, normal, fraction, and solidity/hit state

This alignment is intentionally structural only. It does not implement target selection, aiming, firing, hitchance, penetration, anti-aim, movement automation, or other combat behaviour.

## Runtime Lifecycle Added at Phase 4 Closure

`velocity_runtime_compat.h` now owns volatile readiness state.

On an in-game -> out-of-game transition:

1. `LocalPlayerCache` resets.
2. compatibility command state resets.
3. entity/bone/hitbox/rich-trace readiness resets.
4. runtime epoch increments.
5. config stays published because config ownership is process-lifetime rather than map-lifetime.

On DLL shutdown, volatile runtime and config ownership are both cleared.

## Next Phase Strategy

Do not return to one-header-per-test pacing.

Safe compile-time/refactor work can be batched. Only changes that touch a live runtime producer should get isolated checkpoints.

The next useful work is to build the single TempleWare-owned port/update context that future mechanically adapted Velocity code can consume, while keeping every unproven producer closed behind the existing readiness gate.

Runtime producers should be opened one at a time only after their existing TempleWare source is independently validated. Do not solve blocked producers by guessing new offsets, signatures, vtable indices, call arguments, or resolver targets.

## Acceptance Criteria for Phase 4 Closure

- all compatibility contracts compile
- config contract is process-lifetime owned and disabled by default
- runtime readiness is centralized rather than hardcoded throughout the port
- disconnect clears every volatile compatibility publication
- shutdown clears compatibility ownership
- SDK-unsafe locals remain pointer-only
- trace remains blocked when backend functions are unresolved
- no new gameplay behaviour is introduced by the compatibility layer
