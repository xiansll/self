// Minimal TempleWare skeleton: manual map + ImGui empty overlay.
// No features, no game hooks (except the D3D11 Present hook for the overlay).
#include <windows.h>
#include <d3d11.h>
#include <cstdio>

#include "../external/imgui/imgui.h"
#include "../external/imgui/imgui_impl_dx11.h"
#include "../external/imgui/imgui_impl_win32.h"
#include "../external/kiero/kiero.h"
#include "../external/kiero/minhook/include/MinHook.h"
#include "esp/esp.h"
#include "chams/chams.h"
#include "gui/gui.h"
#include "trace/trace.h"
#include "icons/icons.h"
#include "nerv/nerv_bridge.h"
#include "templeware/globals/d3d11_globals.h"

typedef HRESULT(__stdcall* Present)(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static HMODULE g_hModule = nullptr;

Present oPresent;
bool g_showMenu = true;
HWND window = NULL;
WNDPROC oWndProc;
ID3D11Device* g_pDevice = NULL;
ID3D11DeviceContext* g_pContext = NULL;
ID3D11RenderTargetView* mainRenderTargetView;

// D3D11 Globals for skin changer
ID3D11Device* D3D11::g_pDevice = nullptr;
ID3D11DeviceContext* D3D11::g_pContext = nullptr;

static void LogToFile(const char* msg)
{
    wchar_t path[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, path);
    wcscat_s(path, MAX_PATH, L"TempleWare.log");
    FILE* f = nullptr;
    if (_wfopen_s(&f, path, L"a") == 0 && f)
    {
        fprintf(f, "%s\n", msg);
        fclose(f);
    }
}

LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    // Insert tusu: menuyu ac/kapa
    if (uMsg == WM_KEYDOWN && wParam == VK_INSERT)
    {
        g_showMenu = !g_showMenu;
        return true;
    }

    // Menu acikken input'u ImGui'ye ver, kapaliyken oyuna gecir
    if (g_showMenu && ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
        return true;

    return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

bool init = false;
HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
    if (!init)
    {
        if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&g_pDevice)))
        {
            g_pDevice->GetImmediateContext(&g_pContext);
            DXGI_SWAP_CHAIN_DESC sd;
            pSwapChain->GetDesc(&sd);
            window = sd.OutputWindow;

            ID3D11Texture2D* pBackBuffer = nullptr;
            pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
            g_pDevice->CreateRenderTargetView(pBackBuffer, NULL, &mainRenderTargetView);
            pBackBuffer->Release();

            oWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)WndProc);

            ImGui::CreateContext();
            ImGui::GetIO().IniFilename = nullptr;   // no persisted window drift
            ImGui_ImplWin32_Init(window);
            ImGui_ImplDX11_Init(g_pDevice, g_pContext);

            init = true;
            LogToFile("overlay init complete");

            if (Chams::Initialize())
                LogToFile("chams init ok");
            else
                LogToFile("chams init failed (see esp log)");

            Trace::Initialize();
            Icons::Initialize(g_pDevice, g_pContext);
        }
        else
        {
            return oPresent(pSwapChain, SyncInterval, Flags);
        }
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    Gui::MaybeRebuildFont();   // crisp font atlas at current UI scale (before NewFrame)
    ImGui::NewFrame();

    Esp::Draw();
    Esp::DrawOverlay();
    Esp::UpdateAim();
    Esp::UpdateTrigger();
    Esp::UpdateMisc();
    Esp::UpdateSkins();
    nerv_bridge::tick(g_showMenu);   // nerv skin engine, driven from Present (reliable every frame)

    static float g_menuAnim = 0.f;
    {
        float dt = ImGui::GetIO().DeltaTime;
        if (dt <= 0.f) dt = 1.f / 60.f;
        const float tgt = g_showMenu ? 1.f : 0.f;
        const float step = 14.f * dt;
        g_menuAnim += (tgt - g_menuAnim) * (step < 1.f ? step : 1.f);
    }
    if (g_menuAnim > 0.01f)
        Gui::Render(g_menuAnim);

    ImGui::Render();
    g_pContext->OMSetRenderTargets(1, &mainRenderTargetView, NULL);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    return oPresent(pSwapChain, SyncInterval, Flags);
}

DWORD WINAPI MainThread(LPVOID lpReserved)
{
    LogToFile("MainThread started");

    bool init_hook = false;
    do
    {
        if (!init_hook)
        {
            if (kiero::init(kiero::RenderType::D3D11) == kiero::Status::Success)
            {
                kiero::bind(8, (void**)&oPresent, hkPresent);
                init_hook = true;
                LogToFile("kiero bind(8) done");
            }
        }
        Sleep(100);
    } while (!GetAsyncKeyState(VK_END));

    kiero::shutdown();
    FreeLibraryAndExitThread(g_hModule, EXIT_SUCCESS);
    return TRUE;
}

BOOL WINAPI DllMain(HMODULE hMod, DWORD dwReason, LPVOID lpReserved)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        g_hModule = hMod;
        DisableThreadLibraryCalls(hMod);
        LogToFile("DLL_PROCESS_ATTACH");
        CreateThread(nullptr, 0, MainThread, hMod, 0, nullptr);
    }
    return TRUE;
}
