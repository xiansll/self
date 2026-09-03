#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>

#include <atomic>
#include <cstdio>
#include <cstring>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"
#include "kiero/kiero.h"

#include "../../TempleWare-CS2/source/esp/esp.h"
#include "../../TempleWare-CS2/source/icons/icons.h"
#include "../../TempleWare-CS2/source/chams/chams.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

using PresentFn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT);
using ResizeBuffersFn = HRESULT(__stdcall*)(
    IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

extern LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND window, UINT message, WPARAM wParam, LPARAM lParam);

namespace CleanGui
{
    HMODULE module = nullptr;
    PresentFn originalPresent = nullptr;
    ResizeBuffersFn originalResizeBuffers = nullptr;

    HWND window = nullptr;
    WNDPROC originalWndProc = nullptr;
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    ID3D11RenderTargetView* renderTarget = nullptr;

    std::atomic<bool> initialized{false};
    std::atomic<bool> unloadRequested{false};
    std::atomic<bool> guiShutdownComplete{false};

    bool menuOpen = true;
    int selectedPage = 0;
    bool showStatusPanel = true;
    bool testToggle = false;
    bool espModuleReady = false;
    bool iconModuleReady = false;
    float uiScale = 1.0f;
    ImVec4 accent = ImVec4(1.00f, 0.38f, 0.08f, 1.00f);

    void Log(const char* message) noexcept
    {
        if (!message)
            return;

        char path[MAX_PATH]{};
        const DWORD modulePathLength =
            GetModuleFileNameA(module, path, MAX_PATH);

        if (modulePathLength > 0 && modulePathLength < MAX_PATH) {
            char* slash = std::strrchr(path, '\\');
            if (slash) {
                const size_t remaining =
                    MAX_PATH - static_cast<size_t>((slash + 1) - path);
                std::snprintf(
                    slash + 1,
                    remaining,
                    "%s",
                    "TempleWare-Clean.log");
            }
            else {
                std::snprintf(
                    path,
                    sizeof(path),
                    "%s",
                    "TempleWare-Clean.log");
            }
        }
        else {
            char tempPath[MAX_PATH]{};
            const DWORD tempLength = GetTempPathA(MAX_PATH, tempPath);
            if (!tempLength || tempLength >= MAX_PATH)
                return;

            std::snprintf(
                path,
                sizeof(path),
                "%s%s",
                tempPath,
                "TempleWare-Clean.log");
        }

        HANDLE file = CreateFileA(
            path,
            FILE_APPEND_DATA,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

        if (file == INVALID_HANDLE_VALUE)
            return;

        SYSTEMTIME time{};
        GetLocalTime(&time);

        char line[768]{};
        const int length = std::snprintf(
            line,
            sizeof(line),
            "[%02u:%02u:%02u.%03u] %s\r\n",
            time.wHour,
            time.wMinute,
            time.wSecond,
            time.wMilliseconds,
            message);

        if (length > 0) {
            DWORD written = 0;
            WriteFile(file, line, static_cast<DWORD>(length), &written, nullptr);
        }

        CloseHandle(file);
    }

    bool ShouldTraceEspFrame() noexcept
    {
        static ULONGLONG lastTrace = 0;
        const ULONGLONG now = GetTickCount64();

        if (now - lastTrace < 100)
            return false;

        lastTrace = now;
        return true;
    }

    void RunEspFrame()
    {
        const bool trace = ShouldTraceEspFrame();

        if (trace) Log("[ESP-10] Esp::Draw BEGIN");
        Esp::Draw();
        if (trace) Log("[ESP-11] Esp::Draw END");

        if (trace) Log("[ESP-20] Esp::DrawOverlay BEGIN");
        Esp::DrawOverlay();
        if (trace) Log("[ESP-21] Esp::DrawOverlay END");

        if (trace) Log("[ESP-30] Esp::UpdateMisc BEGIN");
        Esp::UpdateMisc();
        if (trace) Log("[ESP-31] Esp::UpdateMisc END");

        if (trace) Log("[ESP-40] Esp::UpdateSkins BEGIN");
        Esp::UpdateSkins();
        if (trace) Log("[ESP-41] Esp::UpdateSkins END");
    }

    void ApplyStyle()
    {
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowPadding = ImVec2(18.0f, 16.0f);
        style.FramePadding = ImVec2(10.0f, 7.0f);
        style.ItemSpacing = ImVec2(10.0f, 9.0f);
        style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
        style.WindowRounding = 8.0f;
        style.ChildRounding = 6.0f;
        style.FrameRounding = 5.0f;
        style.ScrollbarRounding = 6.0f;
        style.GrabRounding = 5.0f;
        style.WindowBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;

        ImVec4* colors = style.Colors;
        colors[ImGuiCol_Text] = ImVec4(0.93f, 0.94f, 0.96f, 1.00f);
        colors[ImGuiCol_TextDisabled] = ImVec4(0.47f, 0.49f, 0.54f, 1.00f);
        colors[ImGuiCol_WindowBg] = ImVec4(0.055f, 0.060f, 0.075f, 0.98f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.075f, 0.080f, 0.100f, 1.00f);
        colors[ImGuiCol_PopupBg] = ImVec4(0.065f, 0.070f, 0.088f, 1.00f);
        colors[ImGuiCol_Border] = ImVec4(0.16f, 0.17f, 0.21f, 1.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.105f, 0.110f, 0.135f, 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.145f, 0.150f, 0.180f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.18f, 0.18f, 0.21f, 1.00f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.055f, 0.060f, 0.075f, 1.00f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.055f, 0.060f, 0.075f, 1.00f);
        colors[ImGuiCol_CheckMark] = accent;
        colors[ImGuiCol_SliderGrab] = accent;
        colors[ImGuiCol_SliderGrabActive] =
            ImVec4(accent.x, accent.y + 0.08f, accent.z, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.105f, 0.110f, 0.135f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.16f, 0.165f, 0.195f, 1.00f);
        colors[ImGuiCol_ButtonActive] = accent;
        colors[ImGuiCol_Header] = ImVec4(accent.x, accent.y, accent.z, 0.35f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(accent.x, accent.y, accent.z, 0.55f);
        colors[ImGuiCol_HeaderActive] = ImVec4(accent.x, accent.y, accent.z, 0.75f);
        colors[ImGuiCol_Separator] = ImVec4(0.16f, 0.17f, 0.21f, 1.00f);
    }

    bool CreateRenderTarget(IDXGISwapChain* swapChain)
    {
        if (renderTarget)
            return true;

        ID3D11Texture2D* backBuffer = nullptr;
        const HRESULT bufferResult = swapChain->GetBuffer(
            0,
            __uuidof(ID3D11Texture2D),
            reinterpret_cast<void**>(&backBuffer));

        if (FAILED(bufferResult) || !backBuffer)
            return false;

        const HRESULT targetResult =
            device->CreateRenderTargetView(backBuffer, nullptr, &renderTarget);
        backBuffer->Release();
        return SUCCEEDED(targetResult) && renderTarget;
    }

    void ReleaseRenderTarget()
    {
        if (renderTarget) {
            renderTarget->Release();
            renderTarget = nullptr;
        }
    }

    bool Initialize(IDXGISwapChain* swapChain)
    {
        if (initialized.load())
            return true;

        if (!swapChain)
            return false;

        if (FAILED(swapChain->GetDevice(
                __uuidof(ID3D11Device),
                reinterpret_cast<void**>(&device))) ||
            !device) {
            Log("[CLEAN] GetDevice failed");
            return false;
        }

        device->GetImmediateContext(&context);
        if (!context) {
            device->Release();
            device = nullptr;
            Log("[CLEAN] GetImmediateContext failed");
            return false;
        }

        DXGI_SWAP_CHAIN_DESC description{};
        if (FAILED(swapChain->GetDesc(&description)) ||
            !description.OutputWindow) {
            context->Release();
            context = nullptr;
            device->Release();
            device = nullptr;
            Log("[CLEAN] swap-chain description failed");
            return false;
        }

        window = description.OutputWindow;
        originalWndProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrA(
                window,
                GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(
                    +[](HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
                        -> LRESULT
                    {
                        if (message == WM_KEYUP && wParam == VK_INSERT) {
                            menuOpen = !menuOpen;
                            Log(menuOpen
                                ? "[CLEAN] menu opened"
                                : "[CLEAN] menu closed");
                            return 0;
                        }

                        if (message == WM_KEYUP && wParam == VK_END) {
                            unloadRequested.store(true);
                            return 0;
                        }

                        if (menuOpen &&
                            ImGui_ImplWin32_WndProcHandler(
                                hwnd, message, wParam, lParam)) {
                            return 1;
                        }

                        return CallWindowProcA(
                            originalWndProc,
                            hwnd,
                            message,
                            wParam,
                            lParam);
                    })));

        if (!originalWndProc) {
            context->Release();
            context = nullptr;
            device->Release();
            device = nullptr;
            Log("[CLEAN] WndProc hook failed");
            return false;
        }

        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;

        ApplyStyle();

        const bool win32Ready = ImGui_ImplWin32_Init(window);
        const bool dx11Ready =
            win32Ready && ImGui_ImplDX11_Init(device, context);
        const bool targetReady =
            dx11Ready && CreateRenderTarget(swapChain);

        if (!win32Ready || !dx11Ready || !targetReady) {
            if (dx11Ready)
                ImGui_ImplDX11_Shutdown();
            if (win32Ready)
                ImGui_ImplWin32_Shutdown();

            ImGui::DestroyContext();
            ReleaseRenderTarget();

            if (window && originalWndProc && IsWindow(window)) {
                SetWindowLongPtrA(
                    window,
                    GWLP_WNDPROC,
                    reinterpret_cast<LONG_PTR>(originalWndProc));
            }

            originalWndProc = nullptr;
            window = nullptr;

            context->Release();
            context = nullptr;
            device->Release();
            device = nullptr;

            Log("[CLEAN] ImGui initialization failed");
            return false;
        }

        Log("[ESP-00] Icons::Initialize BEGIN");
        iconModuleReady = Icons::Initialize(device, context);
        Log(iconModuleReady
            ? "[ESP-01] Icons::Initialize END ready=1"
            : "[ESP-01] Icons::Initialize END ready=0");

        Log("[ESP-02] Esp::Initialize BEGIN");
        espModuleReady = Esp::Initialize();
        Log(espModuleReady
            ? "[ESP-03] Esp::Initialize END ready=1"
            : "[ESP-03] Esp::Initialize END ready=0");

        initialized.store(true);
        Log("[CLEAN] ImGui ready");
        return true;
    }

    bool PageButton(const char* label, int page)
    {
        const bool selected = selectedPage == page;
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, accent);
            ImGui::PushStyleColor(
                ImGuiCol_ButtonHovered,
                ImVec4(accent.x, accent.y, accent.z, 0.90f));
        }

        const bool clicked = ImGui::Button(label, ImVec2(126.0f, 38.0f));

        if (selected)
            ImGui::PopStyleColor(2);

        if (clicked)
            selectedPage = page;

        return clicked;
    }

    void DrawOverview()
    {
        ImGui::TextColored(accent, "CLEAN RUNTIME");
        ImGui::TextWrapped(
            "Clean overlay + the complete ESP module are active.");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("Overlay status");
        ImGui::SameLine(170.0f);
        ImGui::TextColored(
            ImVec4(0.35f, 0.90f, 0.55f, 1.00f),
            "ONLINE");

        const Esp::Stats& stats = Esp::GetStats();

        ImGui::Text("ESP module");
        ImGui::SameLine(170.0f);
        ImGui::TextColored(
            espModuleReady
                ? ImVec4(0.35f, 0.90f, 0.55f, 1.00f)
                : ImVec4(0.95f, 0.65f, 0.25f, 1.00f),
            espModuleReady ? "READY" : "WAITING");

        ImGui::Text("Entity / matrix");
        ImGui::SameLine(170.0f);
        ImGui::Text(
            "%s / %s",
            stats.entitySystemReady ? "READY" : "WAIT",
            stats.viewMatrixReady ? "READY" : "WAIT");

        ImGui::Text("Players / enemies");
        ImGui::SameLine(170.0f);
        ImGui::Text(
            "%d / %d",
            stats.playersFound,
            stats.enemiesDrawn);

        ImGui::Spacing();
        ImGui::Checkbox("Show status panel", &showStatusPanel);
        ImGui::Checkbox("UI-only test toggle", &testToggle);
    }

    void DrawVisuals()
    {
        ImGui::TextColored(accent, "VISUALS");

        Esp::Config& config = Esp::g_config;

        ImGui::Checkbox("Enable ESP", &config.enabled);
        ImGui::Checkbox("Box", &config.box);
        ImGui::Checkbox("Name", &config.name);
        ImGui::Checkbox("Health bar", &config.healthBar);
        ImGui::Checkbox("Skeleton", &config.skeleton);
        ImGui::Checkbox("Weapon", &config.weapon);
        ImGui::Checkbox("Ammo bar", &config.ammoBar);
        ImGui::Checkbox("Flags", &config.flags);
        ImGui::Checkbox("Distance", &config.distance);
        ImGui::Checkbox("Team ESP", &config.teamEsp);
        ImGui::Checkbox("Snapline", &config.snapline);
        ImGui::Checkbox("Head circle", &config.headCircle);
        ImGui::Checkbox("Off-screen arrows", &config.offArrows);
        ImGui::Checkbox("Item ESP", &config.itemEsp);
        ImGui::Checkbox("Bomb ESP", &config.bombEsp);
        ImGui::Checkbox("Grenade ESP", &config.nadeEsp);
        ImGui::Checkbox("Spectator list", &config.specList);
        ImGui::Checkbox("Watermark", &config.watermark);
        ImGui::Checkbox("Crosshair", &config.crosshair);

        ImGui::ColorEdit4("Box color", config.boxColor);
    }

    void DrawSettings()
    {
        ImGui::TextColored(accent, "SETTINGS");
        ImGui::SliderFloat("Interface scale", &uiScale, 0.85f, 1.20f, "%.2f");
        ImGui::TextColored(
            ImVec4(0.62f, 0.64f, 0.70f, 1.00f),
            "Scale control is visual-only in this first clean build.");

        float color[3] = {accent.x, accent.y, accent.z};
        if (ImGui::ColorEdit3("Accent", color)) {
            accent.x = color[0];
            accent.y = color[1];
            accent.z = color[2];
            ApplyStyle();
        }

        ImGui::Spacing();
        ImGui::Text("INSERT  Open / close menu");
        ImGui::Text("END     Unload clean DLL");
    }

    void DrawMenu()
    {
        if (!menuOpen)
            return;

        ImGui::SetNextWindowSize(ImVec2(690.0f, 440.0f), ImGuiCond_Once);
        ImGui::SetNextWindowPos(
            ImVec2(70.0f, 70.0f),
            ImGuiCond_Once);

        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoSavedSettings;

        if (!ImGui::Begin("TempleWare - Clean Base", &menuOpen, flags)) {
            ImGui::End();
            return;
        }

        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImVec2 position = ImGui::GetWindowPos();
        const ImVec2 size = ImGui::GetWindowSize();
        draw->AddRectFilled(
            position,
            ImVec2(position.x + size.x, position.y + 4.0f),
            ImGui::ColorConvertFloat4ToU32(accent));

        ImGui::TextColored(accent, "TEMPLEWARE");
        ImGui::SameLine();
        ImGui::TextColored(
            ImVec4(0.62f, 0.64f, 0.70f, 1.00f),
            "/ CLEAN BASE");
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::BeginChild(
            "navigation",
            ImVec2(148.0f, 326.0f),
            true);

        PageButton("Overview", 0);
        PageButton("Visuals", 1);
        PageButton("Settings", 2);

        ImGui::SetCursorPosY(268.0f);
        ImGui::TextColored(
            ImVec4(0.35f, 0.90f, 0.55f, 1.00f),
            "OVERLAY ONLINE");
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild(
            "content",
            ImVec2(0.0f, 326.0f),
            true);

        if (selectedPage == 0)
            DrawOverview();
        else if (selectedPage == 1)
            DrawVisuals();
        else
            DrawSettings();

        if (showStatusPanel) {
            ImGui::SetCursorPosY(270.0f);
            ImGui::Separator();
            ImGui::Text(
                "FPS %.0f   |   Frame %.2f ms",
                ImGui::GetIO().Framerate,
                ImGui::GetIO().Framerate > 0.0f
                    ? 1000.0f / ImGui::GetIO().Framerate
                    : 0.0f);
        }

        ImGui::EndChild();
        ImGui::End();
    }

    void ShutdownGui()
    {
        if (!initialized.exchange(false)) {
            guiShutdownComplete.store(true);
            return;
        }

        if (window && originalWndProc && IsWindow(window)) {
            SetWindowLongPtrA(
                window,
                GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(originalWndProc));
        }

        Log("[ESP-90] ESP shutdown BEGIN");
        Chams::Shutdown();
        Log("[ESP-91] ESP shutdown END");

        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        ReleaseRenderTarget();

        if (context) {
            context->Release();
            context = nullptr;
        }

        if (device) {
            device->Release();
            device = nullptr;
        }

        window = nullptr;
        originalWndProc = nullptr;
        guiShutdownComplete.store(true);
        Log("[CLEAN] ImGui shutdown complete");
    }

    HRESULT __stdcall PresentHook(
        IDXGISwapChain* swapChain,
        UINT syncInterval,
        UINT flags)
    {
        if (unloadRequested.load()) {
            ShutdownGui();
            return originalPresent(swapChain, syncInterval, flags);
        }

        if (!Initialize(swapChain))
            return originalPresent(swapChain, syncInterval, flags);

        if (!renderTarget && !CreateRenderTarget(swapChain))
            return originalPresent(swapChain, syncInterval, flags);

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        RunEspFrame();
        DrawMenu();

        ImGui::Render();
        context->OMSetRenderTargets(1, &renderTarget, nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        return originalPresent(swapChain, syncInterval, flags);
    }

    HRESULT __stdcall ResizeBuffersHook(
        IDXGISwapChain* swapChain,
        UINT bufferCount,
        UINT width,
        UINT height,
        DXGI_FORMAT format,
        UINT swapChainFlags)
    {
        ReleaseRenderTarget();
        return originalResizeBuffers(
            swapChain,
            bufferCount,
            width,
            height,
            format,
            swapChainFlags);
    }

    DWORD WINAPI WorkerThread(void*)
    {
        Log("[CLEAN] worker started");

        while (!unloadRequested.load()) {
            const kiero::Status::Enum status =
                kiero::init(kiero::RenderType::D3D11);

            if (status == kiero::Status::Success ||
                status == kiero::Status::AlreadyInitializedError) {
                break;
            }

            Sleep(250);
        }

        if (unloadRequested.load()) {
            FreeLibraryAndExitThread(module, 0);
            return 0;
        }

        if (kiero::bind(
                8,
                reinterpret_cast<void**>(&originalPresent),
                reinterpret_cast<void*>(&PresentHook)) !=
            kiero::Status::Success) {
            Log("[CLEAN] Present hook failed");
            kiero::shutdown();
            FreeLibraryAndExitThread(module, 1);
            return 1;
        }

        const bool resizeHooked =
            kiero::bind(
                13,
                reinterpret_cast<void**>(&originalResizeBuffers),
                reinterpret_cast<void*>(&ResizeBuffersHook)) ==
            kiero::Status::Success;

        Log(resizeHooked
            ? "[CLEAN] Present + ResizeBuffers hooks ready"
            : "[CLEAN] Present ready; ResizeBuffers hook unavailable");

        while (!unloadRequested.load())
            Sleep(50);

        for (int i = 0;
             i < 100 && initialized.load() && !guiShutdownComplete.load();
             ++i) {
            Sleep(20);
        }

        if (initialized.load())
            ShutdownGui();

        Sleep(100);
        kiero::shutdown();
        Log("[CLEAN] unloaded");
        FreeLibraryAndExitThread(module, 0);
        return 0;
    }
}

BOOL WINAPI DllMain(HMODULE module, DWORD reason, void*)
{
    if (reason == DLL_PROCESS_ATTACH) {
        CleanGui::module = module;
        DisableThreadLibraryCalls(module);

        HANDLE thread = CreateThread(
            nullptr,
            0,
            CleanGui::WorkerThread,
            nullptr,
            0,
            nullptr);

        if (thread)
            CloseHandle(thread);
    }

    return TRUE;
}
