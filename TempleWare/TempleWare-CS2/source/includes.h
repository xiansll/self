#pragma once

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>

#include <cstdint>
#include <cstddef>
#include <cmath>
#include <numbers>
#include <string>
#include <string_view>
#include <optional>
#include <memory>
#include <vector>
#include <array>
#include <span>
#include <algorithm>
#include <functional>

#include "../external/kiero/kiero.h"
#include "../external/imgui/imgui.h"
#include "../external/imgui/imgui_impl_win32.h"
#include "../external/imgui/imgui_impl_dx11.h"

#include "templeware/foundation/pch.hpp"

typedef HRESULT(__stdcall* Present)(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
typedef LRESULT(CALLBACK* WNDPROC)(HWND, UINT, WPARAM, LPARAM);
typedef uintptr_t PTR;