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
| `systems::input::usercmd` | Type available as `CUserCmd` | Use compatibility alias; runtime command pipeline still gated |
| `systems::local::snapshot` | Shape available as `LocalPlayerSnapshot` | Use `VelocityRageCompat::local_state` adapter |
| Local pawn/controller lifecycle | Pointer-only provider works | Keep provenance gate; do not deep-deref until `sdk_deref_safe` is proven |
| Prediction object | Existing `C_Prediction` exists | Treat as partial until its runtime contract is validated for the port |
| Basic line trace | Existing `Trace` service exists | Rich Velocity trace result/filter adapter still missing |
| Entity cache / lookup | TempleWare has entity systems, but API differs | Build an adapter; do not make Velocity depend directly on TempleWare internals |
| Bones | Existing project has bone-related code, but no validated Velocity-compatible service contract yet | Add a dedicated adapter after SDK layout is proven |
| Hitboxes | No validated Velocity-compatible adapter yet | Add a dedicated query/result adapter |
| Rage settings | TempleWare config schema does not match Velocity Rage settings | Add a separate config adapter; keep it isolated from runtime behaviour |
| CreateMove lifecycle | Legacy hook exists but is not part of the current safe Present diagnostic path | Add a lifecycle adapter only after Phase 3 runtime blockers are resolved |

## Current Runtime Blocker

Phase 3C currently treats the live local pawn/controller as pointer-only because the TempleWare SDK local resolver path is not proven. A non-null fallback pointer is not sufficient evidence that TempleWare entity wrapper methods can safely interpret the object.

Therefore compatibility work may proceed at the type/adapter level, but any adapter that requires entity layout interpretation remains closed until the SDK resolver/provenance gate is green.

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

Required later:

- expose the current command through one TempleWare-owned adapter
- define init/update/reset ownership
- keep command acquisition separate from feature behaviour
- validate map/disconnect/respawn transitions before any ported combat module consumes it

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
6. command pipeline adapter exists
7. entity cache adapter exists
8. bone adapter exists
9. hitbox adapter exists
10. rich trace adapter exists
11. Rage config adapter exists

Only after that should the Velocity Rage source be mechanically adapted against the compatibility interfaces.

## Rule For Future Port Work

Do not solve a missing Velocity dependency by scattering direct game access throughout Rage code. Add or fix the corresponding TempleWare compatibility adapter first, validate it independently, then let the port consume that adapter.
