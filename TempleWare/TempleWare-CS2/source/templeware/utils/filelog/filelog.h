#pragma once

#include <cstdio>
#include <windows.h>

namespace FileLog {

void Initialize();
void Log(const char* msg);

} // namespace FileLog