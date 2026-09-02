#include "andro_bridge.h"

// TempleWare's ImGui FIRST — defines IMGUI_VERSION so the Andromeda headers
// below skip their own (ABI-different) ImGui and use this one.
#include "../../external/imgui/imgui.h"

#include <windows.h>
#include <cstdarg>
#include <cstdio>
#include <string>

// Shim for Andromeda's launcher helper (DllLauncher.cpp not ported): the config
// system uses GetDllDir() only to locate its JSON. Point it at %TEMP%.
std::string& GetDllDir() {
	static std::string dir = [] {
		char p[MAX_PATH] = {};
		GetTempPathA(MAX_PATH, p);
		return std::string(p);
	}();
	return dir;
}

// TempleWare helpers (minimal — pattern scan + MinHook)
#include "../templeware/utils/memory/patternscan/patternscan.h"
#include "../../external/kiero/minhook/include/MinHook.h"

// Andromeda backend (angle-includes resolve via source/andro include root)
#include <CS2/CSDK_Loader.hpp>
#include <Common/DevLog.hpp>
#include <CS2/Hook/Hook_EquipItemInLoadout.hpp>
#include <AndromedaClient/Features/CInventoryChanger/CInventoryChanger.hpp>
#include <AndromedaClient/Features/CInventoryChanger/CInventoryItemsManager.hpp>

// TempleWare's ImGui (single shared context)
#include "../../external/imgui/imgui.h"

static void AndroLog(const char* fmt, ...) {
	wchar_t path[MAX_PATH] = {};
	GetTempPathW(MAX_PATH, path);
	wcscat_s(path, MAX_PATH, L"TempleWare.log");
	FILE* f = nullptr;
	if (_wfopen_s(&f, path, L"a") == 0 && f) {
		char buf[512];
		va_list ap; va_start(ap, fmt);
		vsnprintf(buf, sizeof(buf), fmt, ap);
		va_end(ap);
		fprintf(f, "[andro] %s\n", buf);
		fclose(f);
	}
}

namespace andro_bridge {

	static bool g_ready = false;

	// Actual work — uses C++ objects, so NO __try here (kept out of the
	// SEH-guarded wrappers below to satisfy MSVC C2712).
	static void init_impl() {
		AndroLog("init_impl: begin");
		GetDevLog()->Init();
		AndroLog("init_impl: DevLog init ok");

		if (!GetSDK_Loader()->LoadSDK()) {
			AndroLog("LoadSDK failed");
			return;
		}
		AndroLog("init_impl: LoadSDK ok");

		// Install the EquipItemInLoadout hook via TempleWare's MinHook.
		MH_Initialize(); // no-op if already initialized by kiero
		auto* addr = M::FindPattern("client.dll",
			"48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 89 54 24 ? 57 41 54 41 55 41 56 41 57 48 83 EC ? 0F B7 FA");
		if (addr) {
			MH_CreateHook(reinterpret_cast<LPVOID>(addr), &Hook_EquipItemInLoadout,
				reinterpret_cast<LPVOID*>(&EquipItemInLoadout_o));
			MH_EnableHook(reinterpret_cast<LPVOID>(addr));
			AndroLog("EquipItemInLoadout hook installed @ %p", (void*)addr);
		} else {
			AndroLog("EquipItemInLoadout pattern not found (equip-side skins disabled)");
		}

		g_ready = true;
		AndroLog("initialize() done ready=1");
	}

	static void tick_impl() {
		GetInventoryChanger()->OnFrameStageNotify(6);
	}

	void initialize() {
		if (g_ready)
			return;
		__try { init_impl(); }
		__except (EXCEPTION_EXECUTE_HANDLER) { AndroLog("initialize() EXCEPTION"); }
	}

	bool is_ready() { return g_ready; }

	void tick() {
		static bool s_probe = false;
		if (!s_probe) { s_probe = true; AndroLog("tick: first call"); }

		if (!g_ready) {
			initialize();
			if (!g_ready)
				return;
		}
		__try { tick_impl(); }
		__except (EXCEPTION_EXECUTE_HANDLER) { AndroLog("tick EXCEPTION"); }
	}

	// ---- UI ----
	static void scan_impl()  { GetInventoryItemsManager()->ScanAllItems(); }
	static void scan_guarded() {
		__try { scan_impl(); }
		__except (EXCEPTION_EXECUTE_HANDLER) { AndroLog("ScanAllItems EXCEPTION"); }
	}

	static uint64_t g_lastAdded = 0;
	static void add_impl(uint16_t defIdx, int paintKit, float wear, int seed, int statTrak) {
		g_lastAdded = GetInventoryItemsManager()->AddSelectedSkinToInventory(defIdx, paintKit, wear, true, seed, statTrak);
		AndroLog("Add&Equip def=%u paint=%d wear=%.4f seed=%d -> id=%llu",
			(unsigned)defIdx, paintKit, wear, seed, (unsigned long long)g_lastAdded);
	}
	static void add_guarded(uint16_t defIdx, int paintKit, float wear, int seed, int statTrak) {
		__try { add_impl(defIdx, paintKit, wear, seed, statTrak); }
		__except (EXCEPTION_EXECUTE_HANDLER) { AndroLog("AddSkin EXCEPTION"); }
	}

	void draw_ui() {
		if (!g_ready) {
			ImGui::TextDisabled("Inventory system not ready. Enter a match, then it loads.");
			return;
		}

		auto* mgr = GetInventoryItemsManager();
		auto& items = mgr->GetDumpedItems();

		if (items.empty()) {
			if (ImGui::Button("Scan Items"))
				scan_guarded();
			ImGui::SameLine();
			ImGui::TextDisabled("No items scanned yet.");
			return;
		}

		// category: combo index 0..3 maps to EDumpedItemType_t 1..4 (weapon/knife/glove/agent)
		static int catFilter = 0;
		ImGui::Combo("Category", &catFilter, "Weapon\0Knife\0Glove\0Agent\0");
		const int wantType = catFilter + 1;

		static int selItem = -1;
		const char* itemPreview = (selItem >= 0 && selItem < (int)items.size())
			? items[selItem].m_DisplayName.c_str() : "Select item...";
		if (ImGui::BeginCombo("Item", itemPreview)) {
			for (int i = 0; i < (int)items.size(); ++i) {
				if ((int)items[i].m_ItemType != wantType) continue;
				if (ImGui::Selectable(items[i].m_DisplayName.c_str(), selItem == i))
					selItem = i;
			}
			ImGui::EndCombo();
		}

		if (selItem >= 0 && selItem < (int)items.size()) {
			auto& it = items[selItem];
			static int selSkin = -1;
			const char* skinPreview = (selSkin >= 0 && selSkin < (int)it.m_DumpedSkins.size())
				? it.m_DumpedSkins[selSkin].m_DisplayName.c_str() : "Select skin...";
			if (ImGui::BeginCombo("Skin", skinPreview)) {
				for (int s = 0; s < (int)it.m_DumpedSkins.size(); ++s)
					if (ImGui::Selectable(it.m_DumpedSkins[s].m_DisplayName.c_str(), selSkin == s))
						selSkin = s;
				ImGui::EndCombo();
			}

			static float wear = 0.0001f;
			static int seed = 0;
			static int statTrak = -1;
			ImGui::SliderFloat("Wear", &wear, 0.0f, 1.0f, "%.4f");
			ImGui::InputInt("Seed", &seed);
			ImGui::InputInt("StatTrak (-1 = off)", &statTrak);

			if (ImGui::Button("Add & Equip") && selSkin >= 0 && selSkin < (int)it.m_DumpedSkins.size()) {
				const int paintKit = it.m_DumpedSkins[selSkin].m_ID;
				add_guarded(it.m_DefIdx, paintKit, wear, seed, statTrak);
			}
			if (g_lastAdded != 0) {
				ImGui::SameLine();
				ImGui::TextDisabled("Added id=%llu", (unsigned long long)g_lastAdded);
			}
		}

		ImGui::Spacing();
		ImGui::Separator();
		auto& added = mgr->GetAddedItems();
		ImGui::Text("Equipped: %d item(s)", (int)added.size());
	}
}
