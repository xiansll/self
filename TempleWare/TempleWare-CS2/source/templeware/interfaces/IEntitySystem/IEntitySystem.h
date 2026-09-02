#pragma once

#include "../../globals/globals.h"

class C_BaseEntity;

class I_EntitySystem {
private:
	using LocalPawnFn = C_CSPlayerPawn * (__fastcall*)(int);
	using LocalControllerFn = void* (__fastcall*)(int);

	static LocalPawnFn local_pawn_resolver() {
		static auto fn = reinterpret_cast<LocalPawnFn>(
			M::scan_absolute("client.dll", "E8 ? ? ? ? 48 8B F0 48 85 C0 74 ? 48 8D 15 ? ? ? ? B9", 0x1)
		); // existing resolver; diagnostic refactor only
		return fn;
	}

	static LocalControllerFn local_controller_resolver() {
		static auto fn = reinterpret_cast<LocalControllerFn>(
			M::patternScan("client", "48 83 EC ? 83 F9 ? 75 ? 48 8B 0D ? ? ? ? 48 8D 54 24 ? 48 8B 01 FF 90 ? ? ? ? 8B 08 48 63 C1 48 8D 0D ? ? ? ? 48 8B 04 C1 48 83 C4 ? C3 CC CC CC CC CC CC CC CC CC CC CC CC CC 48 83 EC ? 83 F9")
		);
		return fn;
	}

public:
	template <class C = C_BaseEntity>
	C* get_base_entity(int index) {
		static auto get_client_entity = reinterpret_cast<C * (__fastcall*)(I_EntitySystem*, int)>(
			M::patternScan("client", "4C 8D 49 ? 81 FA")
			);

		if (!get_client_entity)
			return nullptr;
		return get_client_entity(this, index);
	}

	C_CSPlayerPawn* get_local_pawn() {
		auto fn = local_pawn_resolver();
		return fn ? fn(0) : nullptr;
	}

	void* get_local_controller() {
		auto fn = local_controller_resolver();
		return fn ? fn(-1) : nullptr;
	}

	// Phase 3C diagnostic-only accessors. These expose only whether the existing
	// resolver addresses were found; they do not add or change signatures and do
	// not perform any extra entity dereferences.
	[[nodiscard]] void* diagnostic_local_pawn_resolver() const {
		return reinterpret_cast<void*>(local_pawn_resolver());
	}

	[[nodiscard]] void* diagnostic_local_controller_resolver() const {
		return reinterpret_cast<void*>(local_controller_resolver());
	}

	int get_highest_entity_index() {
		return *reinterpret_cast<int*>(reinterpret_cast<std::uintptr_t>(this) + 0x20A0);
	}
};