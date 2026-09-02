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
#include "templeware/utils/validation/phase3c_validation.h"

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
            ImGui::GetIO().IniFilename = nullptr;
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

    // Present is the proven-safe Phase 3 runtime validation path. The suspect
    // FrameStageNotify detour stays disabled. Phase 3C is resolver-gated: no
    // pointer-only local is dereferenced through TempleWare wrappers here.
    if (foundationInit)
    {
        static bool s_presentDiagnosticLogged = false;
        static bool s_phase3bActiveLogged = false;
        static bool s_providerOkLogged = false;
        static bool s_controllerOkLogged = false;
        static bool s_pawnChangeLogged = false;
        static bool s_lifecycleOkLogged = false;
        static bool s_phase3cActiveLogged = false;
        static bool s_wasInGame = false;
        static std::uintptr_t s_lastPawn = 0;

        if (!s_presentDiagnosticLogged)
        {
            FileLog::Log("[Validation] PRESENT DIAGNOSTIC FIRST CALL");
            s_presentDiagnosticLogged = true;
        }

        if (!s_phase3bActiveLogged)
        {
            FileLog::Log("[P3B] ACTIVE - LOCAL RUNTIME/LIFECYCLE VALIDATION");
            s_phase3bActiveLogged = true;
        }

        if (!s_phase3cActiveLogged)
        {
            FileLog::Log("[P3C] ACTIVE - SDK RESOLVER GATE (NO DEEP DEREF)");
            s_phase3cActiveLogged = true;
        }

        const bool inGame = I::EngineClient &&
            I::EngineClient->connected() && I::EngineClient->in_game();

        if (inGame)
        {
            // LocalPlayerCache owns provider selection. This remains pointer-only
            // when the selected source is not proven safe for TempleWare wrapper
            // dereferences.
            g_local_player_cache->update();
            const LocalPlayerSnapshot snapshot = g_local_player_cache->get();
            Validation::OnLocalPlayerCacheUpdate(snapshot);

            if (!s_providerOkLogged && snapshot.pawn && snapshot.controller)
            {
                char buf[256];
                std::snprintf(buf, sizeof(buf),
                    "[P3B] LOCAL PROVIDER OK pawn=%p controller=%p sdk_safe=%d",
                    reinterpret_cast<void*>(snapshot.pawn),
                    reinterpret_cast<void*>(snapshot.controller),
                    snapshot.sdk_deref_safe ? 1 : 0);
                FileLog::Log(buf);
                s_providerOkLogged = true;
            }

            if (!s_controllerOkLogged && snapshot.controller)
            {
                FileLog::Log("[P3B] CONTROLLER OK");
                s_controllerOkLogged = true;
            }

            if (snapshot.pawn)
            {
                if (!s_pawnChangeLogged && s_lastPawn && snapshot.pawn != s_lastPawn)
                {
                    char buf[256];
                    std::snprintf(buf, sizeof(buf),
                        "[P3B] PAWN CHANGE OK old=%p new=%p",
                        reinterpret_cast<void*>(s_lastPawn),
                        reinterpret_cast<void*>(snapshot.pawn));
                    FileLog::Log(buf);
                    s_pawnChangeLogged = true;
                }
                s_lastPawn = snapshot.pawn;
            }

            // Resolver-only P3C checkpoint. It reruns only when the selected
            // pawn/controller pair changes and never enables S3-S7 by itself.
            Phase3C::Run(snapshot);

            if (snapshot.sdk_deref_safe)
            {
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
            }
            else if (snapshot.pawn || snapshot.controller)
            {
                static bool s_pointerOnlyLogged = false;
                if (!s_pointerOnlyLogged)
                {
                    FileLog::Log("[Validation] POINTER-ONLY LOCAL FOUND - NORMAL DEEP SDK DEREF SKIPPED");
                    s_pointerOnlyLogged = true;
                }
            }

            Validation::LogPeriodicSummary(0);
        }
        else if (s_wasInGame)
        {
            // Validate the cache lifecycle without re-enabling a game lifecycle
            // hook: Present observes the in-game -> out-of-game transition and
            // performs the reset directly.
            g_local_player_cache->reset();
            Validation::OnLocalPlayerCacheReset();
            const LocalPlayerSnapshot resetSnapshot = g_local_player_cache->get();

            const bool cleanReset =
                resetSnapshot.pawn == 0 &&
                resetSnapshot.controller == 0 &&
                resetSnapshot.observer_pawn == 0 &&
                resetSnapshot.observer_controller == 0;

            if (cleanReset)
            {
                if (!s_lifecycleOkLogged)
                {
                    FileLog::Log("[P3B] LIFECYCLE OK - CACHE RESET CLEAN");
                    s_lifecycleOkLogged = true;
                }
            }
            else
            {
                FileLog::Log("[P3B] LIFECYCLE FAILED - CACHE NOT CLEAN AFTER RESET");
            }

            s_lastPawn = 0;
        }

        s_wasInGame = inGame;
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    Gui::MaybeRebuildFont();
    ImGui::NewFrame();

    Esp::Draw();
    Esp::DrawOverlay();
    Esp::UpdateAim();
    Esp::UpdateTrigger();
    Esp::UpdateMisc();
    Esp::UpdateSkins();
    nerv_bridge::tick(g_showMenu, (void*)g_ctx->local_pawn, (void*)g_ctx->local_controller);

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
