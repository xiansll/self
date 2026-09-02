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
#include "templeware/globals/globals.h"
#include "templeware/templeware.h"
#include "templeware/utils/filelog/filelog.h"
#include "templeware/utils/localplayer/localplayer.h"
#include "templeware/utils/validation/validation.h"

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

TempleWare g_templeWare;

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
bool foundationInit = false;
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
            FileLog::Log("overlay init complete");

            if (Chams::Initialize())
                FileLog::Log("chams init ok");
            else
                FileLog::Log("chams init failed (see esp log)");

            Trace::Initialize();
            Icons::Initialize(g_pDevice, g_pContext);
        }
        else
        {
            return oPresent(pSwapChain, SyncInterval, Flags);
        }
    }

    if (!foundationInit)
    {
        foundationInit = g_templeWare.initFoundation();
    }

    // Phase3A regression-isolation path. Present is already proven to execute,
    // so use it to validate the local-player/entity plumbing without installing
    // the suspect FrameStageNotify detour.
    if (foundationInit)
    {
        static bool s_presentDiagnosticLogged = false;
        if (!s_presentDiagnosticLogged)
        {
            FileLog::Log("[Validation] PRESENT DIAGNOSTIC FIRST CALL");
            s_presentDiagnosticLogged = true;
        }

        if (I::EngineClient && I::GameEntity && I::GameEntity->Instance &&
            I::EngineClient->connected() && I::EngineClient->in_game())
        {
            g_local_player_cache->update();
            const LocalPlayerSnapshot snapshot = g_local_player_cache->get();
            Validation::OnLocalPlayerCacheUpdate(snapshot);

            if (snapshot.pawn)
            {
                auto* pawn = reinterpret_cast<C_CSPlayerPawn*>(snapshot.pawn);
                Validation::OnSceneNodeChainCheck(pawn);
                Validation::OnEntityIdentityCheck(reinterpret_cast<CEntityInstance*>(pawn));
            }

            if (snapshot.controller)
            {
                Validation::OnEntityIdentityCheck(reinterpret_cast<CEntityInstance*>(snapshot.controller));
            }

            Validation::LogPeriodicSummary(0);
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
    nerv_bridge::tick(g_showMenu, (void*)g_ctx->local_pawn, (void*)g_ctx->local_controller);   // feed TW's working local pointers

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
    FileLog::Log("MainThread started");

    bool init_hook = false;
    do
    {
        if (!init_hook)
        {
            if (kiero::init(kiero::RenderType::D3D11) == kiero::Status::Success)
            {
                kiero::bind(8, (void**)&oPresent, hkPresent);
                init_hook = true;
                FileLog::Log("kiero bind(8) done");
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
        FileLog::Initialize();
        FileLog::Log("DLL_PROCESS_ATTACH");
        CreateThread(nullptr, 0, MainThread, hMod, 0, nullptr);
    }
    return TRUE;
}
