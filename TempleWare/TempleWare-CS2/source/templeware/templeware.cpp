#include "templeware.h"

#include "utils/module/module.h"

#include <iostream>
#include <cstdio>
#include <cwchar>

#include "utils/filelog/filelog.h"

static void FileLogInit(const char* msg)
{
    FileLog::Log(msg);
}

void TempleWare::initFoundation() {
    Validation::LogFoundationInitBegin();
    FileLogInit("[init] Initializing modules...");
    modules.init();

    FileLogInit("[init] Initializing schema...");
    schema.init("client.dll", 0);

    FileLogInit("[init] Initializing Interfaces...");
    const bool interfacesReady = interfaces.init();
    if (!interfacesReady) {
        Validation::LogInterfacesFailed();
        FileLogInit("[init] Foundation init stopped: interfaces failed");
        return;
    }
    Validation::LogInterfacesReady();

    Validation::LogHookInitBegin();
    FileLogInit("[init] Initializing Phase3A lifecycle validation hook...");
    if (!hooks.initValidation()) {
        FileLogInit("[init] Foundation init stopped: FrameStageNotify hook failed");
        return;
    }

    FileLogInit("[init] Foundation validation init complete");
}

void TempleWare::initRenderer(HWND& window, ID3D11Device* pDevice, ID3D11DeviceContext* pContext, ID3D11RenderTargetView* mainRenderTargetView) {
    FileLogInit("[init] Initializing menu...");
    renderer.menu.init(window, pDevice, pContext, mainRenderTargetView);

    FileLogInit("[init] Initializing visuals...");
    renderer.visuals.init();

    FileLogInit("[init] Renderer init complete");
}

void TempleWare::init(HWND& window, ID3D11Device* pDevice, ID3D11DeviceContext* pContext, ID3D11RenderTargetView* mainRenderTargetView) {
    // Legacy/full path retained for compatibility. The active Phase3A runtime
    // calls initFoundation() directly and therefore does not initialize the
    // duplicate renderer/menu stack.
    initFoundation();
    initRenderer(window, pDevice, pContext, mainRenderTargetView);

    FileLogInit("[init] Success...");
}
