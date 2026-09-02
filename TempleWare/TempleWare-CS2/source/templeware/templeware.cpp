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
    interfaces.init();
    Validation::LogInterfacesReady();

    Validation::LogHookInitBegin();
    FileLogInit("[init] Initializing hooks...");
    hooks.init();

    FileLogInit("[init] Foundation init complete");
}

void TempleWare::initRenderer(HWND& window, ID3D11Device* pDevice, ID3D11DeviceContext* pContext, ID3D11RenderTargetView* mainRenderTargetView) {
    FileLogInit("[init] Initializing menu...");
    renderer.menu.init(window, pDevice, pContext, mainRenderTargetView);

    FileLogInit("[init] Initializing visuals...");
    renderer.visuals.init();

    FileLogInit("[init] Renderer init complete");
}

void TempleWare::init(HWND& window, ID3D11Device* pDevice, ID3D11DeviceContext* pContext, ID3D11RenderTargetView* mainRenderTargetView) {
    initFoundation();
    initRenderer(window, pDevice, pContext, mainRenderTargetView);

    FileLogInit("[init] Success...");
}
