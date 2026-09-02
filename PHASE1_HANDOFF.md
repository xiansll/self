# Phase 1 Status

## Completed
- Created unified math foundation (`templeware/utils/math/math.hpp`) with Vector2D, Vector3D, Vector4D, QAngle, Matrix3x4, VectorAligned types and helper functions
- Enhanced FNV1a hash (`templeware/utils/fnv1a/fnv1a.hpp`) with constexpr/runtime 32/64-bit hashes, user-defined literals, and backward-compatible `hash_32_fnv1a_const` function
- Created CS2 types (`templeware/utils/cstypes/cstypes.hpp`) with tick constants, bone IDs, hit groups, weapon types, command buttons, move types, entity flags, item definition indices, and tick_fraction helper
- Created memory utilities (`templeware/utils/memory/memory.hpp`) with module base/export/size, pattern scanning with AVX-optimized search, read/write/safe_read/call/call_vfunc/get_vfunc/read_string helpers
- Created address/interface resolution (`templeware/utils/addresses/addresses.hpp`) with module caching, interface retrieval (CreateInterface walking), and global address resolution
- Created minimal foundation PCH (`templeware/foundation/pch.hpp`) and updated `includes.h` to use it — no heavy externals (xdraw, zydis, vmprotect, phnt, lz4) imported
- Verified clean build: `TempleWare.dll` compiles successfully

## Files Changed
- `source/templeware/utils/math/math.hpp` — new unified math types and helpers
- `source/templeware/utils/fnv1a/fnv1a.hpp` — new FNV1a implementation with UDLs
- `source/templeware/utils/fnv1a/fnv1a.h` — backward-compat wrapper for `hash_32_fnv1a_const`
- `source/templeware/utils/cstypes/cstypes.hpp` — new CS2 constants and types
- `source/templeware/utils/memory/memory.hpp` — new memory/pattern utilities
- `source/templeware/utils/addresses/addresses.hpp` — new address/interface resolution
- `source/templeware/foundation/pch.hpp` — new minimal foundation PCH
- `source/includes.h` — updated to include foundation PCH

## Reused TempleWare Components
- `Vector_t`, `QAngle_t`, `Vector2D_t`, `Vector4D_t`, `Matrix3x4_t` from `templeware/utils/math/vector/vector.h` — aliased in new math.hpp
- `ViewMatrix`, `Matrix2x4_t` from `templeware/utils/math/viewmatrix/viewmatrix.h` — preserved
- `Module`, `Modules` from `templeware/utils/module/module.h` — preserved and used by addresses.hpp
- Pattern scanning infrastructure from `templeware/utils/memory/patternscan/patternscan.h` — preserved
- `c_opcodes` from `nerv/utils/utils.hpp` — preserved

## New/Adapted Components
- `math.hpp` — consolidates vector types from vector.h and viewmatrix.h, adds `math::helpers` namespace (angle_vectors, normalize_angles, vector_to_angle, calculate_angle, etc.)
- `fnv1a.hpp` — replaces old recursive constexpr with iterative constexpr; adds `operator""_hash32/64` UDLs
- `cstypes.hpp` — ports Velocity's cstypes.hpp constants (bones, hitgroups, weapons, buttons, flags, move_types, item defs, tick_fraction)
- `memory.hpp` — ports Velocity's memory.hpp read/write/call/vfunc/safe_read + pattern parsing with resolve ops (rel_call, rip_relative, absolute_ptr, deref_final)
- `addresses.hpp` — minimal port of Velocity's addresses/modules/globals/interfaces with pattern-based resolution

## Important Decisions
- **No Velocity PCH copied** — TempleWare's minimal `includes.h` extended with foundation PCH only; heavy externals (xdraw, zydis, phnt, lz4, vmprotect, xorstr, poly2d, bc7, rpack) NOT imported
- **Backward compatibility maintained** — old type names (`Vector_t`, `QAngle_t`, etc.) aliased to new `math::` types; `hash_32_fnv1a_const` function kept for existing schema macros
- **Pattern scanning** — new implementation in `memory.hpp` supports Velocity-style pattern syntax (`>`, `*`, `^`, `~`, `+/-offset`) but uses simpler scalar fallback; existing `M::FindPattern` preserved for compatibility
- **Addresses** — lazy initialization pattern; modules/interfaces/globals resolved on first use via `addr::modules::initialize()`, `addr::globals::initialize()`
- **CSTypes** — full Velocity constant set ported; `tick_fraction` helper ported for prediction/subtick work

## Build Verification
- **Command**: `MSBuild.exe TempleWare-CS2.vcxproj /p:Configuration=Release /p:Platform=x64`
- **Result**: Success — `TempleWare.dll` built at `C:\CS\TempleWare\x64\Release\TempleWare.dll`
- **Remaining errors**: None (clean build)

## Phase 2 Notes
- Entity SDK integration will need: `C_CSPlayerPawn`, `CCSPlayerController`, `C_CSWeaponBase`, `C_BaseEntity` schema access — already uses `hash_32_fnv1a_const` which now works
- Interface resolution: `addr::interfaces::get_interface()` and `get_interface_any()` ready for `IEngineClient`, `CGameEntitySystem`, `CCSGOInput`, `ISceneSystem`, `INetworkClient`, `CGlobalVarsBase`, `CPrediction`, `ILocalize`, `IMemAlloc`, `IEconItemSystem`
- Pattern signatures for globals (csgo_input, entity_list, view_matrix, etc.) defined in `addr::globals::initialize()` — will need updating with actual CS2 signatures
- Memory helpers (`mem::read`, `mem::write`, `mem::call_vfunc`, `mem::safe_read`) available for entity SDK field access
- Math types (`math::Vector3D` / `Vector_t`, `math::QAngle` / `QAngle_t`) unified — feature code can use either naming