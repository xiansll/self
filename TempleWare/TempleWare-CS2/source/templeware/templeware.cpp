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

bool TempleWare::initFoundation() {
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
        return false;
    }
    Validation::LogInterfacesReady();

    // Phase3A regression-isolation mode: initialize diagnostics only.
    // FrameStageNotify is deliberately NOT installed here because the previous
    // runtime test showed a camera regression while the hook never produced a
    // FIRST CALL milestone. Present is already a proven live callback, so the
    // active runtime performs the validation health checks from hkPresent.
    Validation::Initialize();
    FileLogInit("[Validation] FRAMESTAGE HOOK BYPASSED - PRESENT DIAGNOSTIC MODE");
    FileLogInit("[init] Foundation diagnostic init complete");
    return true;
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
    // calls initFoundation() directly and therefore does not initialize either
    // the legacy hook group or the duplicate renderer/menu stack.
    if (!initFoundation())
        return;

    Validation::LogHookInitBegin();
    FileLogInit("[init] Initializing full legacy hooks...");
    hooks.init();

    initRenderer(window, pDevice, pContext, mainRenderTargetView);
    FileLogInit("[init] Success...");
}
