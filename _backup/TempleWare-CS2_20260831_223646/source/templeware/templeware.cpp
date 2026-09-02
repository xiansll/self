#include "templeware.h"

#include "utils/module/module.h"

#include <iostream>
#include <cstdio>
#include <cwchar>

static void FileLog(const char* msg)
{
    wchar_t path[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, path);
    wcscat_s(path, MAX_PATH, L"TempleWare.log");
    FILE* f = nullptr;
    if (_wfopen_s(&f, path, L"a") == 0 && f)
    {
        fprintf(f, "[init] %s\n", msg);
        fclose(f);
    }
}

void TempleWare::init(HWND& window, ID3D11Device* pDevice, ID3D11DeviceContext* pContext, ID3D11RenderTargetView* mainRenderTargetView) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

    auto printWithPrefix = [&](const char* message) {
        FileLog(message);
        std::cout << "[";
        SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        std::cout << "+";
        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
        std::cout << "] " << message << std::endl;
        };

    printWithPrefix("Initializing modules...");
    modules.init();

    printWithPrefix("Initializing menu...");
    renderer.menu.init(window, pDevice, pContext, mainRenderTargetView);

    printWithPrefix("Initializing schema...");
    schema.init("client.dll", 0);

    printWithPrefix("Initializing Interfaces...");
    interfaces.init();

    printWithPrefix("Initializing visuals...");
    renderer.visuals.init();

    printWithPrefix("Initializing hooks...");
    hooks.init();

    printWithPrefix("Success...");
}
