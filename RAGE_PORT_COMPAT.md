# Velocity Rage -> TempleWare Compatibility Plan

## Goal

Prepare TempleWare so the Velocity combat/rage subsystem can be ported through explicit adapters instead of direct copy/paste.

This document is about architecture, dependency mapping, compile/runtime validation, and lifecycle safety. It does not enable Rage behaviour and it does not add new game signatures/offsets.

## What Velocity Rage Depends On

The Velocity `combat.hpp` / Rage declarations are not self-contained. They depend on a broad `systems` layer, including:

- `systems::local::snapshot`
- `systems::input::usercmd`
- vector/angle math types
- prediction state
- entity lookup/cache
- bones and skeleton data
- hitbox queries
- tracing results/filters
- shared combat state and lag-record containers
- configuration types
- a CreateMove-style command lifecycle

Velocity therefore cannot be dropped into TempleWare as a single `rage.cpp` file.

## TempleWare Mapping

| Velocity dependency | TempleWare status | Port action |
|---|---|---|
| `math::vector2/vector3` | Available | Use `VelocityRageCompat::vector2/vector3` aliases |
| `systems::input::usercmd` | Type available as `CUserCmd` | Use compatibility alias; runtime command source remains gated |
| `systems::local::snapshot` | Shape available as `LocalPlayerSnapshot` | Use `VelocityRageCompat::local_state` adapter |
| Local pawn/controller lifecycle | Pointer-only provider works | Keep provenance gate; do not deep-deref until `sdk_deref_safe` is proven |
| Prediction object | Existing `C_Prediction` exists | Treat as partial until its runtime contract is validated for the port |
| Basic line trace | Existing `Trace` service exists, runtime currently unresolved | Rich trace DTO contract exists; runtime producer remains gated |
| Entity cache / lookup | DTO contract exists | Runtime producer remains gated until SDK-safe access is proven |
| Bones | DTO contract exists | Runtime producer remains gated until SDK-safe access is proven |
| Hitboxes | DTO contract exists | Runtime producer remains gated until SDK-safe access is proven |
| Rage settings | Isolated config contract exists | TempleWare->Velocity translation/runtime publication remains gated |
| CreateMove lifecycle | Non-operational lifecycle contract exists | Runtime command acquisition stays disabled until a separately validated source is available |

## Current Runtime Blockers

Phase 3C currently treats the live local pawn/controller as pointer-only because the TempleWare SDK local resolver path is not proven. A non-null fallback pointer is not sufficient evidence that TempleWare entity wrapper methods can safely interpret the object.

The current trace service also exposes an API contract but is not runtime-ready when its required internal functions fail to resolve. Compatibility code must keep `trace_runtime=false` in that state rather than treating API presence as proof of a working trace backend.

Therefore compatibility work may proceed at the type/adapter level, but any adapter that requires entity layout interpretation, an active command source, live tracing, or live config translation remains closed until its runtime gate is green.

## Phase 4 Compatibility Checkpoints

### P4A - Type/runtime contract

Implemented by `source/templeware/compat/velocity_rage_compat.h`.

It provides:

- common type aliases
- a stable local-state adapter
- a runtime readiness report
- a single compatibility gate
- `[P4COMPAT]` logging without game-state writes or deep pointer dereferences

### P4B - Command lifecycle adapter

Contract implemented by `source/templeware/compat/velocity_command_compat.h`.

It provides:

- one TempleWare-owned location for a future validated `CUserCmd*`
- explicit begin/end/reset ownership
- a generation counter for lifecycle diagnostics
- separate `command_pipeline_adapter` and `command_runtime` readiness states
- no CreateMove hook installation, command acquisition, or command mutation

The contract is considered present at compile time, but `command_runtime` intentionally remains false until a safe command source is wired and independently validated.

### P4C - Data service adapters

Contract layer implemented by `source/templeware/compat/velocity_data_compat.h`.

It provides non-operational compatibility shapes for:

- entity references
- bone poses and skeleton snapshots
- hitbox entries and hitbox sets
- rich trace results

The compile-time adapter flags are separate from runtime producer flags. This means `entities/bones/hitboxes/rich_trace` may report adapter=1 while the corresponding runtime state remains 0. No live entity, bone, hitbox, or trace acquisition was added by this checkpoint.

### P4D - Config adapter

Contract implemented by `source/templeware/compat/velocity_config_compat.h`.

It provides:

- an isolated `rage_config_snapshot`
- version/revision metadata for future translation diagnostics
- a TempleWare-owned publish/get/reset store
- separate compile-time `rage_config_adapter` and runtime `rage_config_runtime` readiness
- no direct dependency on GUI globals and no gameplay behaviour

The config contract may therefore report adapter=1 while runtime config remains 0 until a validated TempleWare->Velocity translation publishes a snapshot.

## Port Gate

The compatibility gate stays BLOCKED until all of these are true:

1. local pawn/controller pair is available
2. TempleWare SDK dereference safety is proven
3. input runtime is available
4. prediction runtime is available
5. trace runtime is available
6. command lifecycle contract exists
7. command runtime source is proven
8. entity cache contract exists and its runtime producer is proven
9. bone contract exists and its runtime producer is proven
10. hitbox contract exists and its runtime producer is proven
11. rich trace contract exists and its runtime producer is proven
12. Rage config contract exists
13. Rage config runtime translation/publication is proven

Only after that should the Velocity Rage source be mechanically adapted against the compatibility interfaces.

## Rule For Future Port Work

Do not solve a missing Velocity dependency by scattering direct game access throughout Rage code. Add or fix the corresponding TempleWare compatibility adapter first, validate it independently, then let the port consume that adapter.
