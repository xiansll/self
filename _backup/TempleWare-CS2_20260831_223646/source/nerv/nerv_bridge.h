#pragma once
// Thin bridge between TempleWare and the ported nerv skin subsystem.
// TempleWare code (gui.cpp, hooks.cpp) includes ONLY this header, never nerv
// internals, so the two SDKs never meet in the same translation unit.

namespace nerv_bridge {
	// Resolve nerv modules/interfaces/schema and build the item schema.
	// Safe to call repeatedly; only does the heavy work once.
	void initialize();

	bool is_initialized();

	// Drive the per-frame skin/glove apply. Call from TempleWare's
	// FrameStageNotify hook. `stage` is the raw stage int; `menu_open`
	// forces re-apply while the user is tweaking the UI.
	void on_frame(int stage, bool menu_open);

	// Drive init + skin/glove apply from the D3D11 Present hook (runs every
	// frame regardless of FrameStageNotify, which is unreliable in this build).
	void tick(bool menu_open);

	// Force a re-apply on the next frame (e.g. on level init / round start).
	void force_update();

	// Render the skins UI (knife/glove/weapon) with ImGui. Call inside an
	// existing ImGui window/child region from PageInventory.
	void draw_skins_ui();
}
