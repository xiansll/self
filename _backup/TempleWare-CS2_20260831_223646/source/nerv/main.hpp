#pragma once
// Slim main.hpp for the ported nerv skin subsystem inside TempleWare.
// Deliberately omits nerv's imgui/minhook/hooks/menu/directx — only the
// backend (interfaces/modules/schema/classes/utils) needed to resolve the
// game and apply skins. Kept self-contained so it does not clash with
// TempleWare's own SDK (I::/M::/g_ctx live in the global namespace there).

#include "config.hpp"

#include <Windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <memory>
#include <string>
#include <vector>

// --- macro shims (nerv originals came from xor.hpp / console.hpp) ---
#ifndef xorstr_
#define xorstr_(s) (s)
#endif
#ifndef WINCALL
#define WINCALL(func) func
#endif

void nerv_log(const char* fmt, ...);
#define LOG(fmt, ...)         nerv_log(fmt, __VA_ARGS__)
#define LOG_ERROR(fmt, ...)   nerv_log(fmt, __VA_ARGS__)
#define LOG_WARNING(fmt, ...) nerv_log(fmt, __VA_ARGS__)
#define LOG_SUCCESS(fmt, ...) nerv_log(fmt, __VA_ARGS__)
#define LOG_INFO(fmt, ...)    nerv_log(fmt, __VA_ARGS__)

#include "sdk/includes/hash.hpp"
#include "sdk/typedefs/vec_t.hpp"
#include "sdk/vfunc/vfunc.hpp"
#include "utils/utils.hpp"
#include "valve/modules/modules.hpp"
#include "valve/interfaces/interfaces.hpp"
#include "valve/schema/schema.hpp"

// nerv per-frame context, renamed g_ctx -> g_nctx to avoid clashing with
// TempleWare's own global g_ctx (different type). Populated each frame by the
// bridge from TempleWare's FrameStageNotify hook.
class c_user_cmd;
struct nerv_globals_t {
	c_user_cmd* m_user_cmd;
	void* m_local_pawn;
	void* m_local_controller;
};
inline const auto g_nctx = std::make_unique<nerv_globals_t>();
