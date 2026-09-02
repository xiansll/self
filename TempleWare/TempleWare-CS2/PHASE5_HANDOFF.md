# Phase 5 handoff — owner binding boundary

## Status

TempleWare now has a non-operational integration boundary that keeps runtime
ownership, lifecycle, provider registration, config publication, menu timing,
and feature dispatch outside feature implementations.

This phase deliberately does **not** activate the legacy CreateMove hook, invent
new signatures/offsets, open the currently blocked SDK wrapper gate, or mark an
unvalidated runtime producer ready.

## What is already wired

### Frame lane

`main.cpp` updates the proven local/port context and then calls:

```cpp
VelocityRageCompat::FeatureIntegration::on_frame();
```

Every registered `IIntegratedFeature` therefore receives a
`feature_runtime_context` on the existing Present path.

### Menu lane

When the TempleWare menu is visible, `main.cpp` calls:

```cpp
VelocityRageCompat::FeatureIntegration::on_menu();
```

Every registered feature gets `on_menu()` timing automatically. TempleWare does
not know or implement the feature-specific controls.

### Reset/shutdown lane

Disconnect/map-exit invokes:

```cpp
VelocityRageCompat::FeatureIntegration::reset_volatile();
```

Process shutdown invokes:

```cpp
VelocityRageCompat::FeatureIntegration::shutdown();
```

Registered features and bound providers therefore have centralized reset and
shutdown ownership.

### Command lane

The command lifecycle contract is implemented but intentionally dormant. No
CreateMove/game hook is installed by Phase 5. A separately validated owner-side
command source can dispatch one already-owned command with:

```cpp
VelocityRageCompat::FeatureIntegration::dispatch_command(user_cmd);
```

The command is published only for that dispatch scope and then cleared.

## The one owner-editable registration file

All future process-lifetime registrations belong in:

```text
source/templeware/compat/velocity_owner_bindings.h
```

`OwnerBindings::install()` is already called by `main.cpp`. Its default body is
empty, so the current build cannot accidentally enable a provider or feature.

Typical owner-side registration shape:

```cpp
g_provider_bindings.bind_entities(&your_entity_provider);
g_provider_bindings.bind_bones(&your_bone_provider);
g_provider_bindings.bind_hitboxes(&your_hitbox_provider);
g_provider_bindings.bind_rich_trace(&your_trace_provider);

FeatureIntegration::bind_config_refresh(&your_config_publish_function);
g_feature_registry.register_feature(&your_feature);
```

Bindings do not imply readiness. Each provider's `ready()` remains authoritative.

## Provider contracts

Implement only the interfaces required by a feature:

- `IEntityProvider`
- `IBoneProvider`
- `IHitboxProvider`
- `IRichTraceProvider`

All are declared in:

```text
source/templeware/compat/velocity_provider_bindings.h
```

They expose read-only compatibility data (`entity_ref`, `skeleton_snapshot`,
`hitbox_set`, `rich_trace_result`) plus `ready()`, `generation()`, and `reset()`.
Provider readiness is mirrored into the existing P4/P5 runtime readiness system
only when the provider itself reports ready.

## Feature contract

Owner features implement:

```cpp
class MyFeature final : public VelocityRageCompat::IIntegratedFeature {
public:
    const char* integration_name() const noexcept override {
        return "MyFeature";
    }

    void on_frame(const VelocityRageCompat::feature_runtime_context& ctx) noexcept override {
        // Read-only frame work. Check ctx.compat / ctx.providers first.
    }

    void on_command(const VelocityRageCompat::feature_runtime_context& ctx) noexcept override {
        // Runs only when the owner explicitly dispatches a validated command.
    }

    void on_menu() noexcept override {
        // Owner-defined menu/config controls.
    }

    void on_reset() noexcept override {
        // Clear volatile feature state.
    }
};
```

The feature does not need to discover local player state, own provider lifecycle,
or find its own menu/update/reset entry points.

## Context mapping for a future reference-code adaptation

Reference dependency | TempleWare integration source
--- | ---
local snapshot | `ctx.port.local`
compat/runtime readiness | `ctx.compat`
entities | `ctx.providers.entities`
bones | `ctx.providers.bones`
hitboxes | `ctx.providers.hitboxes`
rich trace | `ctx.providers.rich_trace`
command | `ctx.command` (command lane only)
config snapshot | `ctx.config`
reset | `IIntegratedFeature::on_reset()`
menu timing | `IIntegratedFeature::on_menu()`

## Current hard blockers remain closed

The latest runtime validation established that:

- local resolver provenance is working;
- current basic entity wrapper/schema interpretation is not semantically proven;
- current TraceRay/FilterInit resolver expressions do not resolve;
- no entity/bone/hitbox/rich-trace provider has been independently validated;
- no command source is activated by this phase.

Do not turn those states into `ready=true` merely to open the port gate.

## Expected P5C/P5D diagnostic markers

A build containing this handoff should log:

```text
[P5C] FEATURE INTEGRATION READY - frame/menu/reset wired; command lane dormant until owner dispatch
[P5D] OWNER BINDING SLOT READY - default bindings empty
[P5C] STATUS features=0 ... command=0 entities=0 bones=0 hitboxes=0 rich_trace=0 ...
```

On disconnect:

```text
[P5C] FEATURE/PROVIDER VOLATILE RESET CLEAN
```

On process shutdown:

```text
[P5D] OWNER BINDING SLOT UNINSTALLED
[P5C] FEATURE INTEGRATION SHUTDOWN CLEAN
```

## What remains for the owner

At this point the architectural integration work is intentionally reduced to
owner bindings:

1. Implement a provider only when its underlying TempleWare data path is proven.
2. Bind it in `velocity_owner_bindings.h`.
3. Implement/register an `IIntegratedFeature`.
4. Publish feature config through the config refresh callback.
5. If command-time behavior is needed, call `dispatch_command(user_cmd)` from the
   owner's separately validated command source.

`main.cpp` should not require feature-specific changes for those steps.
