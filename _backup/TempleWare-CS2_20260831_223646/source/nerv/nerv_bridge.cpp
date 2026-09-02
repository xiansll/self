#include "nerv_bridge.h"

// nerv backend
#include "main.hpp"
#include "valve/interfaces/vtables/i_mem_alloc.hpp"
#include "features/shared/item_schema.hpp"
#include "features/skin_changer/skin_changer.hpp"
#include "features/glove_changer/glove_changer.hpp"

// TempleWare's ImGui (single shared instance/context)
#include "../../external/imgui/imgui.h"

#include <mutex>

// ---- nerv_log: route nerv's LOG_* to the TempleWare log file ----
void nerv_log(const char* fmt, ...) {
	wchar_t path[MAX_PATH] = {};
	GetTempPathW(MAX_PATH, path);
	wcscat_s(path, MAX_PATH, L"TempleWare.log");
	FILE* f = nullptr;
	if (_wfopen_s(&f, path, L"a") == 0 && f) {
		char buf[512];
		va_list ap; va_start(ap, fmt);
		vsnprintf(buf, sizeof(buf), fmt, ap);
		va_end(ap);
		fprintf(f, "[nerv] %s\n", buf);
		fclose(f);
	}
}

namespace nerv_bridge {

	static bool g_ready = false;
	static bool g_sdk_ready = false;

	void initialize() {
		if (g_ready)
			return;

		__try {
			// One-time SDK resolve (mem alloc, modules, interfaces). The item
			// schema, however, only loads inside a match, so retry it per frame.
			if (!g_sdk_ready) {
				InitMemAlloc();
				g_modules->m_modules.initialize();
				g_interfaces->initialize();
				g_skin_changer->initialize();
				g_sdk_ready = true;
				nerv_log("SDK init done");
			}
			g_item_schema->initialize();
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			nerv_log("initialize() EXCEPTION (sdk_ready=%d)", (int)g_sdk_ready);
			return;
		}

		if (g_item_schema->is_initialized() && !g_ready) {
			g_ready = true;
			nerv_log("initialize() done ready=1 knives=%d gloves=%d weapons=%d",
				(int)g_item_schema->knives.size(),
				(int)g_item_schema->gloves.size(),
				(int)g_item_schema->weapons.size());
		}
	}

	bool is_initialized() {
		return g_ready && g_item_schema->is_initialized();
	}

	void force_update() {
		g_skin_changer->should_update = true;
		g_glove_changer->should_update = true;
	}

	// TempleWare's FrameStageNotify in this build delivers FRAME_RENDER_END == 6
	// reliably every frame (its own ESP/aim run there); stage 7 does not arrive,
	// so we must NOT gate on 7. nerv's run()/init originally keyed on 7, so we
	// init on any frame and spoof stage 7 into run() to satisfy its gate.
	static constexpr int kApplyStage = 6; // FRAME_RENDER_END

	void on_frame(int stage, bool menu_open) {
		static bool s_probe = false;
		if (!s_probe) { s_probe = true; nerv_log("on_frame first call, stage=%d", stage); }

		if (!g_ready) {
			// item schema only becomes available in a match; keep trying each
			// frame until it succeeds (cheap: initialize() early-outs when ready).
			initialize();
			if (!g_ready)
				return;
		}

		if (stage != kApplyStage)
			return;

		__try {
			if (!g_interfaces || !g_interfaces->m_entity_system)
				return;

			g_nctx->m_local_pawn = g_interfaces->m_entity_system->get_local_pawn();
			g_nctx->m_local_controller = g_interfaces->m_entity_system->get_local_controller();

			// While the menu is open, keep forcing re-apply so edits take
			// effect immediately; when closed it self-heals on paint mismatch.
			if (menu_open)
				force_update();

			g_skin_changer->run(7);
			g_glove_changer->run(7);
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			nerv_log("on_frame EXCEPTION stage=%d", stage);
		}
	}

	void tick(bool menu_open) {
		static bool s_probe = false;
		if (!s_probe) { s_probe = true; nerv_log("tick first call"); }

		if (!g_ready) {
			initialize();
			if (!g_ready)
				return;
		}

		__try {
			if (!g_interfaces || !g_interfaces->m_entity_system)
				return;

			g_nctx->m_local_pawn = g_interfaces->m_entity_system->get_local_pawn();
			g_nctx->m_local_controller = g_interfaces->m_entity_system->get_local_controller();
			if (!g_nctx->m_local_pawn)
				return; // not in a live match yet

			if (menu_open)
				force_update();

			g_skin_changer->run(7);
			g_glove_changer->run(7);
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			nerv_log("tick EXCEPTION");
		}
	}

	// ---- ported from nerv menu.cpp draw_skins_tab() ----
	void draw_skins_ui() {
		if (!is_initialized()) {
			ImGui::TextDisabled("Item system not ready. Enter a match, then it loads automatically.");
			return;
		}

		ImGui::Text("Knife Changer");
		ImGui::Separator();
		ImGui::Checkbox("Enabled##knife", &g_cfg->knife_changer.m_enabled);

		if (g_cfg->knife_changer.m_enabled) {
			static int last_knife = 0;

			if (!g_item_schema->knife_names_cstr.empty()) {
				ImGui::Combo("Knife Model", &g_cfg->knife_changer.m_knife,
					g_item_schema->knife_names_cstr.data(),
					(int)g_item_schema->knife_names_cstr.size());
			} else {
				ImGui::TextDisabled("Loading knives...");
			}

			uint16_t selected_knife = 0;
			if (g_cfg->knife_changer.m_knife < (int)g_item_schema->knives.size())
				selected_knife = g_item_schema->knives[g_cfg->knife_changer.m_knife].definition_index;

			if (last_knife != g_cfg->knife_changer.m_knife) {
				g_cfg->knife_changer.m_paint_kit = 0;
				last_knife = g_cfg->knife_changer.m_knife;
			}

			auto& knife_skins = g_item_schema->get_paint_kit_names_for_item(selected_knife);
			if (!knife_skins.empty()) {
				ImGui::Combo("Knife Skin", &g_cfg->knife_changer.m_paint_kit,
					knife_skins.data(), (int)knife_skins.size());
			}

			ImGui::SliderFloat("Wear##knife", &g_cfg->knife_changer.m_wear, 0.0f, 1.0f, "%.4f");
			ImGui::InputInt("Seed##knife", &g_cfg->knife_changer.m_seed, 0, 0);
			ImGui::InputText("Custom Name##knife", g_cfg->knife_changer.m_custom_name,
				sizeof(g_cfg->knife_changer.m_custom_name));
		}

		ImGui::Spacing();
		ImGui::Text("Glove Changer");
		ImGui::Separator();
		ImGui::Checkbox("Enabled##glove", &g_cfg->glove_changer.m_enabled);

		if (g_cfg->glove_changer.m_enabled) {
			static int last_glove = 0;

			if (!g_item_schema->glove_names_cstr.empty()) {
				ImGui::Combo("Glove Model", &g_cfg->glove_changer.m_glove,
					g_item_schema->glove_names_cstr.data(),
					(int)g_item_schema->glove_names_cstr.size());
			} else {
				ImGui::TextDisabled("Loading gloves...");
			}

			uint16_t selected_glove = 0;
			if (g_cfg->glove_changer.m_glove < (int)g_item_schema->gloves.size())
				selected_glove = g_item_schema->gloves[g_cfg->glove_changer.m_glove].definition_index;

			if (last_glove != g_cfg->glove_changer.m_glove) {
				auto& glove_skins = g_item_schema->get_paint_kit_names_for_item(selected_glove);
				g_cfg->glove_changer.m_paint_kit = (glove_skins.size() > 1) ? 1 : 0;
				last_glove = g_cfg->glove_changer.m_glove;
			}

			auto& glove_skins = g_item_schema->get_paint_kit_names_for_item(selected_glove);
			if (!glove_skins.empty()) {
				ImGui::Combo("Glove Skin", &g_cfg->glove_changer.m_paint_kit,
					glove_skins.data(), (int)glove_skins.size());
			}

			ImGui::SliderFloat("Wear##glove", &g_cfg->glove_changer.m_wear, 0.0f, 1.0f, "%.4f");
			ImGui::InputInt("Seed##glove", &g_cfg->glove_changer.m_seed, 0, 0);
		}

		ImGui::Spacing();
		ImGui::Text("Skin Changer");
		ImGui::Separator();
		ImGui::Checkbox("Enabled##skin", &g_cfg->skin_changer.m_enabled);

		if (g_cfg->skin_changer.m_enabled) {
			if (!g_item_schema->weapon_names_cstr.empty()) {
				ImGui::Combo("Weapon", &g_cfg->skin_changer.m_selected_weapon,
					g_item_schema->weapon_names_cstr.data(),
					(int)g_item_schema->weapon_names_cstr.size());
			}

			uint16_t selected_weapon_def = 0;
			if (g_cfg->skin_changer.m_selected_weapon < (int)g_item_schema->weapons.size())
				selected_weapon_def = g_item_schema->weapons[g_cfg->skin_changer.m_selected_weapon].definition_index;

			if (selected_weapon_def > 0) {
				int config_index = c_config::skin_changer_t::get_config_index(selected_weapon_def);
				auto& weapon_skin = g_cfg->skin_changer.weapon_skins[config_index];

				auto& weapon_skins = g_item_schema->get_paint_kit_names_for_item(selected_weapon_def);
				if (!weapon_skins.empty()) {
					ImGui::Combo("Skin##weapon_skin", &weapon_skin.paint_kit,
						weapon_skins.data(), (int)weapon_skins.size());
				}

				ImGui::SliderFloat("Wear##weapon", &weapon_skin.wear, 0.0f, 1.0f, "%.4f");
				ImGui::InputInt("Seed##weapon", &weapon_skin.seed, 0, 0);
				ImGui::InputText("Name##weapon_name", weapon_skin.custom_name,
					sizeof(weapon_skin.custom_name));

				if (ImGui::Button("Apply##skin_apply"))
					g_skin_changer->should_update = true;
			}
		}
	}
}
