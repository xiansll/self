# Velocity Rage -> TempleWare Compatibility Plan

## Goal

Prepare TempleWare so a future reference subsystem can be adapted through explicit TempleWare-owned contracts instead of direct copy/paste.

The compatibility work covers architecture, dependency mapping, ownership, lifecycle, config/menu timing, provider registration, and compile/runtime validation. It does not add operational aiming/firing/anti-aim behavior and does not invent new game signatures/offsets.

## Why Direct Copy/Paste Does Not Work

Velocity `combat.hpp` is built on a broad `systems` layer rather than being a self-contained feature class. Its consumers depend on local state, command ownership, prediction, entities, bones, hitboxes, tracing, config, and lifecycle.

TempleWare therefore needs a stable runtime boundary before owner-side feature adaptation.

## Current TempleWare Mapping

| Reference dependency | TempleWare compatibility surface | Current runtime status |
|---|---|---|
| vector/angle types | aliases/contracts | ready at compile time |
| user command | `CUserCmd` + scoped command lifecycle | dormant until owner dispatch |
| local snapshot | `local_state` / port context | pointer provenance proven |
| basic SDK wrapper semantics | staged P3D gate | blocked by incompatible current schema interpretation |
| prediction | existing object + POD contract | object present; feature proof remains owner-side |
| entities | `IEntityProvider` + `entity_ref` | unbound/not-ready |
| bones | `IBoneProvider` + `skeleton_snapshot` | unbound/not-ready |
| hitboxes | `IHitboxProvider` + `hitbox_set` | unbound/not-ready |
| rich tracing | `IRichTraceProvider` + `rich_trace_result` | unbound/not-ready |
| basic TempleWare trace backend | existing `Trace` API | current TraceRay/FilterInit resolvers unresolved |
| config | versioned store + refresh callback | disabled default published; owner translator unbound |
| frame context | `port_context_snapshot` + `feature_runtime_context` | wired on Present |
| menu timing | `IIntegratedFeature::on_menu()` | wired when menu is visible |
| reset/shutdown | central feature/provider lifecycle | wired |
| feature registration | `feature_registry` | wired; empty by default |
| owner registrations | `velocity_owner_bindings.h` | wired; empty by default |

## Phase 4 — Compatibility Contracts

Phase 4 compile-time compatibility is CLOSED.

Implemented:

- `source/templeware/compat/velocity_rage_compat.h`
- `source/templeware/compat/velocity_command_compat.h`
- `source/templeware/compat/velocity_data_compat.h`
- `source/templeware/compat/velocity_config_compat.h`
- `source/templeware/compat/velocity_runtime_compat.h`

A contract existing never implies that its live producer is valid.

## Phase 5A — Read-only Port Context

Implemented by `source/templeware/compat/velocity_port_context.h`.

The context carries adapted local state, in-game state, gate state, frame sequence, runtime epoch, command generation, and config generation. It owns no game-data acquisition.

## Phase 5B — Runtime blocker diagnosis

### Local resolver

The local pawn/controller resolver pair is now proven by module health, fresh scan, cached-address parity, fresh-call parity, and normal cached getter parity. The earlier one-shot null-cache failure was repaired.

### Basic wrapper semantics

Resolver provenance does not imply object-layout semantics. The current P3D read-only probe returns nonsensical basic pawn fields while the pointer pair itself remains stable. Therefore `sdk_deref_safe` stays false and deeper wrapper/entity graph traversal remains closed.

No offset has been guessed or replaced to force this gate open.

### Trace backend

The existing trace manager resolves, but the current TraceRay and FilterInit resolver expressions do not match the loaded client module. Fresh retry returns the same null result, so this is not treated as an init-order/cache problem.

No replacement signature has been introduced.

## Phase 5C — Provider + Feature Integration Runtime

Implemented by:

- `source/templeware/compat/velocity_provider_bindings.h`
- `source/templeware/compat/velocity_feature_integration.h`

### Provider contracts

Owner-supplied process-lifetime providers can implement and bind:

- `IEntityProvider`
- `IBoneProvider`
- `IHitboxProvider`
- `IRichTraceProvider`

Bindings do not imply readiness. Each provider's own `ready()` result remains authoritative and is mirrored into the central compatibility runtime only on actual state transitions.

### Feature contract

Owner features implement `IIntegratedFeature` and receive:

- `on_frame(feature_runtime_context)` from the existing Present lane
- `on_command(feature_runtime_context)` only from explicit owner command dispatch
- `on_menu()` while the menu is visible
- `on_reset()` on volatile runtime reset
- `on_shutdown()` on process shutdown

The feature runtime context contains the port snapshot, compatibility readiness, config snapshot, provider bindings/readiness, and a scoped command pointer only during command dispatch.

### Command ownership

`FeatureIntegration::dispatch_command(CUserCmd*)` is implemented as a scoped publication/dispatch/clear operation, but TempleWare does not call it automatically and no legacy CreateMove hook is activated.

## Phase 5D — Single Owner Binding Slot

Implemented by `source/templeware/compat/velocity_owner_bindings.h`.

`main.cpp` calls `OwnerBindings::install()` after the integration runtime initializes. The default implementation binds nothing.

Future owner-side registrations therefore have one stable location:

```cpp
g_provider_bindings.bind_entities(&your_entity_provider);
g_provider_bindings.bind_bones(&your_bone_provider);
g_provider_bindings.bind_hitboxes(&your_hitbox_provider);
g_provider_bindings.bind_rich_trace(&your_trace_provider);
FeatureIntegration::bind_config_refresh(&your_config_publish_function);
g_feature_registry.register_feature(&your_feature);
```

Menu/frame/reset/shutdown plumbing does not need feature-specific edits in `main.cpp` after registration.

## Current Runtime Blockers

1. `sdk_deref_safe` remains false because current basic wrapper/schema semantics are not proven.
2. TempleWare `Trace::Ready()` remains false because current TraceRay/FilterInit resolver expressions do not resolve.
3. No owner command source calls `FeatureIntegration::dispatch_command()` yet.
4. Entity/bone/hitbox/rich-trace providers are intentionally unbound until their data sources are independently validated.

These blockers must remain closed rather than being converted to `ready=true` just to open the port gate.

## Port Gate

The compatibility port gate still requires proven local state, SDK safety, trace runtime, command runtime, entity/bone/hitbox/rich-trace runtime producers, input/prediction availability, and config ownership.

Opening that gate means only that prerequisites are available. It does not execute a feature by itself.

## Final Owner Workflow

Architecture is now reduced to owner bindings:

1. Implement only the provider interfaces required by the owner feature.
2. Bind process-lifetime provider instances in `velocity_owner_bindings.h`.
3. Implement/register an `IIntegratedFeature`.
4. Publish translated config through the config refresh callback.
5. If command-time work is needed, call `FeatureIntegration::dispatch_command(user_cmd)` from the owner's separately validated command source.

Detailed examples and lifecycle expectations are documented in `TempleWare/TempleWare-CS2/PHASE5_HANDOFF.md`.

## Rule For Future Work

Do not scatter direct game access throughout owner feature code. Resolve/validate data in a TempleWare-owned producer, bind the producer once, and consume it through `feature_runtime_context`.
