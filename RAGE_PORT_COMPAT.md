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
| Basic line trace | Existing `Trace` service exists, runtime currently unresolved | Rich Velocity trace result/filter adapter still missing |
| Entity cache / lookup | TempleWare has entity systems, but API differs | Build an adapter; do not make Velocity depend directly on TempleWare internals |
| Bones | Existing project has bone-related code, but no validated Velocity-compatible service contract yet | Add a dedicated adapter after SDK layout is proven |
| Hitboxes | No validated Velocity-compatible adapter yet | Add a dedicated query/result adapter |
| Rage settings | TempleWare config schema does not match Velocity Rage settings | Add a separate config adapter; keep it isolated from runtime behaviour |
| CreateMove lifecycle | Non-operational lifecycle contract now exists | Runtime command acquisition stays disabled until a separately validated source is available |

## Current Runtime Blockers

Phase 3C currently treats the live local pawn/controller as pointer-only because the TempleWare SDK local resolver path is not proven. A non-null fallback pointer is not sufficient evidence that TempleWare entity wrapper methods can safely interpret the object.

The current trace service also exposes an API contract but is not runtime-ready when its required internal functions fail to resolve. Compatibility code must keep `trace_runtime=false` in that state rather than treating API presence as proof of a working trace backend.

Therefore compatibility work may proceed at the type/adapter level, but any adapter that requires entity layout interpretation, an active command source, or live tracing remains closed until its runtime gate is green.

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

It now provides:

- one TempleWare-owned location for a future validated `CUserCmd*`
- explicit begin/end/reset ownership
- a generation counter for lifecycle diagnostics
- separate `command_pipeline_adapter` and `command_runtime` readiness states
- no CreateMove hook installation, command acquisition, or command mutation

The contract is considered present at compile time, but `command_runtime` intentionally remains false until a safe command source is wired and independently validated.

### P4C - Data service adapters

Required later:

- entity lookup/cache contract
- bone data contract
- hitbox contract
- rich trace result contract

Each adapter should be independently testable and must not rely on unproven pointer-only wrapper dereferences.

### P4D - Config adapter

Required later:

- isolate Velocity-facing settings from TempleWare's existing config structs
- keep UI/config serialization independent from combat execution
- use a translation layer rather than making ported code read GUI globals directly

## Port Gate

The compatibility gate stays BLOCKED until all of these are true:

1. local pawn/controller pair is available
2. TempleWare SDK dereference safety is proven
3. input runtime is available
4. prediction runtime is available
5. trace runtime is available
6. command lifecycle contract exists
7. command runtime source is proven
8. entity cache adapter exists
9. bone adapter exists
10. hitbox adapter exists
11. rich trace adapter exists
12. Rage config adapter exists

Only after that should the Velocity Rage source be mechanically adapted against the compatibility interfaces.

## Rule For Future Port Work

Do not solve a missing Velocity dependency by scattering direct game access throughout Rage code. Add or fix the corresponding TempleWare compatibility adapter first, validate it independently, then let the port consume that adapter.
