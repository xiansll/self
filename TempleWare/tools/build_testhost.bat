@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
cd /d C:\Dev\CS2\TempleWare-CS2-1.1.5
cl /nologo /EHsc /MD tools\testhost.cpp /Fe:x64\Release\testhost.exe
