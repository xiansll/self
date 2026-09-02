# Velocity Rage -> TempleWare Compatibility Plan

## Goal

Prepare TempleWare so the Velocity combat/rage subsystem can be mechanically adapted through explicit TempleWare-owned contracts instead of direct copy/paste.

This plan covers architecture, dependency mapping, compile/runtime validation, ownership, and lifecycle safety. It does not enable Rage behaviour and does not add new game signatures/offsets.

## Why Direct Copy/Paste Does Not Work

Velocity `combat.hpp` is built on a broad `systems` layer rather than being a self-contained Rage class. Its combat code consumes, among other things:

- `systems::local::snapshot`
- `systems::input::usercmd`
- vector/angle math types
- prediction state
- entity lookup/cache
- bones and skeleton data
- hitbox queries
- tracing results/filters
- shared combat state
- configuration/settings state
- a CreateMove-style command lifecycle

TempleWare therefore needs a stable compatibility boundary before any mechanical source adaptation is attempted.

## Current TempleWare Mapping

| Velocity dependency | TempleWare compatibility status | Runtime status |
|---|---|---|
| vector/angle types | contract available | compile-time only |
| `systems::input::usercmd` | `CUserCmd` alias + command lifecycle contract | no active command producer on Present path |
| `systems::local::snapshot` | `local_state` adapter | pointer pair available, SDK dereference safety blocked |
| prediction object/state | object exists + prediction-state POD contract | semantic runtime proof not yet opened for the port |
| entities | entity reference/cache POD contract | producer closed |
| bones | bone/skeleton POD contract | producer closed |
| hitboxes | hitbox POD contract | producer closed |
| tracing | basic TempleWare API + rich trace POD contract | backend currently unresolved, producer closed |
| Rage settings ownership | isolated versioned config store | disabled default snapshot published safely |
| port/update context | single read-only TempleWare-owned context | active on Present diagnostic path |

## Phase 4 — Compatibility Contracts

Phase 4 compile-time compatibility is CLOSED.

Implemented:

- `source/templeware/compat/velocity_rage_compat.h`
- `source/templeware/compat/velocity_command_compat.h`
- `source/templeware/compat/velocity_data_compat.h`
- `source/templeware/compat/velocity_config_compat.h`
- `source/templeware/compat/velocity_runtime_compat.h`

### Contract vs Runtime Rule

A contract existing does not prove its live producer.

Examples:

- entity adapter can be `1` while entity runtime is `0`
- bone adapter can be `1` while bone runtime is `0`
- config contract can be `1` while a real GUI/settings translation is absent

This separation is deliberate and prevents non-null pointers or readable memory from being treated as semantic SDK proof.

## Central Runtime Lifecycle

`velocity_runtime_compat.h` owns the compatibility runtime epoch and volatile readiness states.

Volatile state:

- command publication
- entity-cache readiness
- bone readiness
- hitbox readiness
- rich-trace readiness

On an in-game -> out-of-game transition, all volatile compatibility state is reset from one place. Config remains published because its ownership is process-lifetime rather than map-lifetime.

At DLL shutdown, volatile state and config ownership are both cleared.

## Config Runtime

The compatibility config store now receives a disabled default snapshot after foundation initialization.

This proves only:

- store ownership
- publication lifecycle
- generation tracking

It does not translate existing TempleWare aim/anti-aim settings and does not enable gameplay behaviour.

## Phase 5A — Read-only Port Context

Implemented by `source/templeware/compat/velocity_port_context.h`.

This provides one TempleWare-owned snapshot for future mechanically adapted Velocity consumers containing:

- adapted local state
- in-game state
- current compatibility gate state
- frame sequence
- runtime epoch
- command generation
- config generation

The context is updated from the existing Present diagnostic path and reset on disconnect/shutdown. It does not acquire game data or run feature callbacks.

## Current Runtime Blockers

### 1. SDK-safe local entity interpretation

The active local provider can expose pawn/controller pointer values, but the TempleWare SDK local resolver path is still unresolved. Therefore `sdk_deref_safe` remains false and pointer-only locals must not be interpreted through TempleWare wrappers.

### 2. Trace backend

The TempleWare Trace API contract exists, but its current runtime backend is not ready when required internal functions fail to resolve. No replacement pattern/signature has been introduced by the compatibility work.

### 3. Command producer

The command lifecycle contract exists, but the safe Present diagnostic path has no live `CUserCmd*` producer. Runtime command readiness remains false.

### 4. Entity/bone/hitbox/rich-trace producers

POD contracts exist, but no producer is marked ready until its TempleWare source is independently validated.

## Acceleration Rule

Do not return to one-header-per-build pacing.

Batch these together:

- compile-time contract alignment
- ownership refactors
- logging cleanup
- context aggregation
- docs/handoff work

Isolate only changes that open or modify a live runtime producer. That keeps crashes attributable without forcing a build for every small architecture edit.

## Port Gate

The final port gate remains closed until all required runtime surfaces are proven:

1. local pawn/controller pair available
2. SDK dereference safety proven
3. input/prediction runtime contract proven for the port
4. trace runtime ready
5. command runtime source proven
6. entity-cache runtime producer proven
7. bone runtime producer proven
8. hitbox runtime producer proven
9. rich-trace runtime producer proven
10. config ownership published

The gate opening means only that compatibility prerequisites are proven. It does not itself execute Rage behaviour.

## Rule For Future Work

Do not solve a missing Velocity dependency by scattering direct game access throughout ported combat code. Add or validate the corresponding TempleWare compatibility producer first, publish it through the central runtime boundary, then let future adapted code consume only the compatibility context/contracts.
