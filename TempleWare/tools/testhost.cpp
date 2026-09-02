#include <windows.h>
#include <cstdio>

int main()
{
    const wchar_t* dllPath = L"C:\\Dev\\CS2\\TempleWare-CS2-1.1.5\\x64\\Release\\TempleWare.dll";
    printf("[testhost] Loading %ls ...\n", dllPath);

    HMODULE h = LoadLibraryW(dllPath);
    if (h)
    {
        printf("[testhost] LoadLibrary OK. base=%p\n", h);
        printf("[testhost] waiting 6s for MainThread/DllMain to run ...\n");
        Sleep(6000);
        FreeLibrary(h);
        printf("[testhost] FreeLibrary done.\n");
        return 0;
    }
    else
    {
        printf("[testhost] LoadLibrary FAILED. err=%lu\n", GetLastError());
        return 1;
    }
}
