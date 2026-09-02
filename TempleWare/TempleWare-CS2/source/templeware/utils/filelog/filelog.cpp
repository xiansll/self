#include "filelog.h"
#include <cstdio>
#include <windows.h>

namespace FileLog {

static wchar_t s_logPath[MAX_PATH] = {};

void Initialize() {
    if (s_logPath[0] != L'\0') return;
    GetTempPathW(MAX_PATH, s_logPath);
    wcscat_s(s_logPath, MAX_PATH, L"TempleWare.log");
}

void Log(const char* msg) {
    if (s_logPath[0] == L'\0') {
        Initialize();
    }
    FILE* f = nullptr;
    if (_wfopen_s(&f, s_logPath, L"a") == 0 && f) {
        fprintf(f, "%s\n", msg);
        fclose(f);
    }
}

} // namespace FileLog