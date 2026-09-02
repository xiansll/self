#include "../../main.hpp"

void* get_entity_by_index(int index) {
	if (!g_interfaces->m_entity_system)
		return nullptr;
	return g_interfaces->m_entity_system->get_base_entity(index);
}

#define CHECK(name, arg) \
    if (arg == nullptr) { \
        LOG_ERROR(xorstr_("[-] Failed to get: %s"), name); \
        /* MessageBoxA removed (deadlock risk vs render hook) */ \
        return; \
    }

void c_interfaces::initialize()
{
	const char* client_dll = g_modules->m_modules.client_dll.get_name();

	// m_csgo_input / m_input_system are only needed for movement/aim, NOT for
	// the skin changer. Their signatures can be stale on some builds, so we
	// resolve them best-effort and DO NOT abort init if they fail.
	using get_input_t = i_csgo_input * (__fastcall*)();
	get_input_t get_input = reinterpret_cast<get_input_t>(g_opcodes->scan_absolute(client_dll, xorstr_("E8 ? ? ? ? 48 8B 56 ? 48 8B C8 E8 ? ? ? ? 4C 89 7E"), 0x1));
	if (get_input) m_csgo_input = get_input();
	else LOG_ERROR(xorstr_("[-] Input unresolved (non-fatal, not needed for skins)"));

	m_entity_system = *reinterpret_cast<i_entity_system**>(g_opcodes->scan_absolute(client_dll, xorstr_("48 8B 0D ? ? ? ? 48 89 7C 24 ? 8B FA C1 EB"), 0x3));
	CHECK(xorstr_("Entity System"), m_entity_system);

	m_schema_system = get_interface<i_schema_system>(&g_modules->m_modules.schemasystem_dll, xorstr_("SchemaSystem_001"));
	CHECK(xorstr_("Schema System"), m_schema_system);

	m_input_system = get_interface(&g_modules->m_modules.input_system, xorstr_("InputSystemVersion001"));
	if (!m_input_system) LOG_ERROR(xorstr_("[-] Input System unresolved (non-fatal)"));

	m_source2_client = get_interface<c_source2_client>(&g_modules->m_modules.client_dll, xorstr_("Source2Client002"));
	CHECK(xorstr_("Source2Client"), m_source2_client);

	m_file_system = get_interface<i_file_system>(&g_modules->m_modules.filesystem_stdio, xorstr_("VFileSystem017"));
	CHECK(xorstr_("FileSystem"), m_file_system);
}
