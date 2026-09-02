#pragma once

#include "../../globals/globals.h"

class C_BaseEntity;

class I_EntitySystem {
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

		static auto fn = reinterpret_cast<C_CSPlayerPawn * (__fastcall*)(int)>(M::scan_absolute("client.dll", "E8 ? ? ? ? 48 8B F0 48 85 C0 74 ? 48 8D 15 ? ? ? ? B9", 0x1)); // x-ref to "Deathcam.Review_Start"; last call, struct call->mov->test->jz
		return fn(0);
	}

	void* get_local_controller() {

		static auto fn = reinterpret_cast<void* (__fastcall*)(int)>(
			M::patternScan("client", "48 83 EC ? 83 F9 ? 75 ? 48 8B 0D ? ? ? ? 48 8D 54 24 ? 48 8B 01 FF 90 ? ? ? ? 8B 08 48 63 C1 48 8D 0D ? ? ? ? 48 8B 04 C1 48 83 C4 ? C3 CC CC CC CC CC CC CC CC CC CC CC CC CC 48 83 EC ? 83 F9")
			);
		if (fn)
			return fn(-1);
		return nullptr;
	}

	int get_highest_entity_index() {
		return *reinterpret_cast<int*>(reinterpret_cast<std::uintptr_t>(this) + 0x20A0);
	}
};