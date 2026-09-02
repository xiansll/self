#pragma once
// Bridge between TempleWare and the ported Andromeda inventory-changer.
// Only this TU mixes the (minimal) TempleWare hooking helpers with Andromeda's
// SDK, so the two SDKs never meet elsewhere.

namespace andro_bridge {
	// Resolve Andromeda interfaces/schema and install the EquipItemInLoadout
	// hook. Safe to call repeatedly; heavy work runs once. SEH-guarded.
	void initialize();

	bool is_ready();

	// Per-frame apply (Andromeda OnFrameStageNotify stage 6). Call from the
	// reliable Present path. SEH-guarded; lazily initializes.
	void tick();

	// Render the inventory/skin picker UI (ImGui). Call inside an existing
	// ImGui window/child region from gui.cpp PageInventory.
	void draw_ui();
}
