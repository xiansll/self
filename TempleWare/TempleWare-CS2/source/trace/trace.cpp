#include "trace.h"
#include <windows.h>
#include <cstring>
#include <cstdio>

#include <CS2/SDK/SDK.hpp>
#include <CS2/SDK/Update/GameTrace.hpp>
#include <CS2/SDK/FunctionListSDK.hpp>

namespace {
void LogLine(const char* m){wchar_t p[MAX_PATH]={};GetTempPathW(MAX_PATH,p);wcscat_s(p,MAX_PATH,L"TempleWare-Clean.log");FILE* f=nullptr;if(_wfopen_s(&f,p,L"a")==0&&f){fprintf(f,"[trace] %s\n",m);fclose(f);}}

bool g_ready = false;
Trace::ResolverDiagnostics g_lastDiagnostics{};

bool ShouldTraceVisibility()
{
    static ULONGLONG last = 0;
    const ULONGLONG now = GetTickCount64();
    if (now - last < 250) return false;
    last = now;
    return true;
}

bool ShouldTraceLine()
{
    static ULONGLONG last = 0;
    const ULONGLONG now = GetTickCount64();
    if (now - last < 250) return false;
    last = now;
    return true;
}

std::uintptr_t GetResolvedTraceShapeAddress() noexcept
{
    auto* functions = GetFunctionList();
    if (!functions) return 0;
    return reinterpret_cast<std::uintptr_t>(
        functions->IGamePhysicsQuery_TraceShape.GetFunction());
}

std::uintptr_t GetResolvedFilterConstructorAddress() noexcept
{
    auto* functions = GetFunctionList();
    if (!functions) return 0;
    return reinterpret_cast<std::uintptr_t>(
        functions->CTraceFilter_Constructor.GetFunction());
}

// Inner workers hold the C++ objects (which require unwinding) so the SEH
// wrappers below contain no unwindable objects (avoids C2712).
__declspec(noinline) bool DoTraceInner(IVPhysics2World** pPhysicsWorld, const float* start, const float* end, uintptr_t target, uintptr_t skip)
{
    Vector3 vecStart(start[0], start[1], start[2]);
    Vector3 vecEnd(end[0], end[1], end[2]);

    CTraceFilter filter(0x1c3003, (C_CSPlayerPawn*)skip, 4, 7);
    Ray_t ray{};
    CGameTrace trace{};

    if (!IGamePhysicsQuery_TraceShape(pPhysicsWorld, ray, vecStart, vecEnd, &filter, &trace))
        return false;

    if (trace.pHitEntity && (uintptr_t)trace.pHitEntity == target) return true;
    if (trace.flFraction > 0.97f) return true;
    return false;
}

__declspec(noinline) bool DoTrace(const float* start, const float* end, uintptr_t target, uintptr_t skip)
{
    if (!g_ready) return false;

    auto* pPhysicsWorld = SDK::Pointers::CVPhys2World();
    if (!pPhysicsWorld || !*pPhysicsWorld) return false;

    __try
    {
        return DoTraceInner(pPhysicsWorld, start, end, target, skip);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

__declspec(noinline) bool DoTraceLineInner(IVPhysics2World** pPhysicsWorld, const float* start, const float* end, uintptr_t skip,
                                           float* oe, float* on, float* of)
{
    Vector3 vecStart(start[0], start[1], start[2]);
    Vector3 vecEnd(end[0], end[1], end[2]);

    CTraceFilter filter(0x1c3003, (C_CSPlayerPawn*)skip, 4, 7);
    Ray_t ray{};
    CGameTrace trace{};

    if (!IGamePhysicsQuery_TraceShape(pPhysicsWorld, ray, vecStart, vecEnd, &filter, &trace)) {
        if (of) *of = 1.f;
        return false;
    }

    if (oe) { oe[0] = trace.vecEnd.m_x; oe[1] = trace.vecEnd.m_y; oe[2] = trace.vecEnd.m_z; }
    if (on) { on[0] = trace.vecNormal.m_x; on[1] = trace.vecNormal.m_y; on[2] = trace.vecNormal.m_z; }
    if (of) *of = trace.flFraction;
    return trace.flFraction < 0.999f;
}

__declspec(noinline) bool DoTraceLine(const float* start, const float* end, uintptr_t skip,
                                      float* oe, float* on, float* of)
{
    if (!g_ready) {
        if (oe) { oe[0] = end[0]; oe[1] = end[1]; oe[2] = end[2]; }
        if (on) { on[0] = 0; on[1] = 0; on[2] = 1; }
        if (of) *of = 1.f;
        return false;
    }

    auto* pPhysicsWorld = SDK::Pointers::CVPhys2World();
    if (!pPhysicsWorld || !*pPhysicsWorld) {
        if (of) *of = 1.f;
        return false;
    }

    __try
    {
        return DoTraceLineInner(pPhysicsWorld, start, end, skip, oe, on, of);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { if (of) *of = 1.f; return false; }
}
} // namespace

namespace Trace
{
    bool Initialize()
    {
        if (g_ready) return true;

        LogLine("[TRACE-100] Internal Initialize BEGIN");

        const bool clientLoaded = GetModuleHandleA("client.dll") != nullptr;
        LogLine(clientLoaded
            ? "[TRACE-101] client.dll READY"
            : "[TRACE-101] client.dll WAITING");

        auto* functions = GetFunctionList();

        LogLine("[TRACE-102] TraceShape resolve BEGIN");
        if (functions && !GetResolvedTraceShapeAddress())
            functions->IGamePhysicsQuery_TraceShape.Search(true);
        const std::uintptr_t traceShape = GetResolvedTraceShapeAddress();
        LogLine(traceShape
            ? "[TRACE-103] TraceShape resolve END ready=1"
            : "[TRACE-103] TraceShape resolve END ready=0");

        LogLine("[TRACE-104] Filter resolve BEGIN");
        if (functions && !GetResolvedFilterConstructorAddress())
            functions->CTraceFilter_Constructor.Search(true);
        const std::uintptr_t filterConstructor =
            GetResolvedFilterConstructorAddress();
        LogLine(filterConstructor
            ? "[TRACE-105] Filter resolve END ready=1"
            : "[TRACE-105] Filter resolve END ready=0");

        LogLine("[TRACE-106] Physics world resolve BEGIN");
        auto* pPhysicsWorld = SDK::Pointers::CVPhys2World();
        LogLine((pPhysicsWorld && *pPhysicsWorld)
            ? "[TRACE-107] Physics world resolve END ready=1"
            : "[TRACE-107] Physics world resolve END ready=0");

        g_ready =
            clientLoaded &&
            pPhysicsWorld &&
            *pPhysicsWorld &&
            traceShape &&
            filterConstructor;

        g_lastDiagnostics.client_loaded = clientLoaded;
        g_lastDiagnostics.ready = g_ready;
        g_lastDiagnostics.manager = reinterpret_cast<std::uintptr_t>(
            (pPhysicsWorld && *pPhysicsWorld) ? *pPhysicsWorld : nullptr);
        g_lastDiagnostics.trace_ray = traceShape;
        g_lastDiagnostics.filter_init = filterConstructor;
        g_lastDiagnostics.fresh_manager = g_lastDiagnostics.manager;
        g_lastDiagnostics.fresh_trace_ray = traceShape;
        g_lastDiagnostics.fresh_filter_init = filterConstructor;

        char b[256];
        std::snprintf(
            b,
            sizeof(b),
            "[TRACE-108] Internal Initialize END ready=%d world=%p trace=%p filter=%p",
            g_ready ? 1 : 0,
            reinterpret_cast<void*>(g_lastDiagnostics.manager),
            reinterpret_cast<void*>(traceShape),
            reinterpret_cast<void*>(filterConstructor));
        LogLine(b);

        return g_ready;
    }

    bool Ready() { return g_ready; }

    bool DiagnoseAndRetryExistingResolvers()
    {
        LogLine("[TRACE-120] Retry resolve BEGIN");

        const bool clientLoaded = GetModuleHandleA("client.dll") != nullptr;
        auto* functions = GetFunctionList();

        std::uintptr_t traceShape = GetResolvedTraceShapeAddress();
        if (!traceShape && clientLoaded && functions)
        {
            LogLine("[TRACE-121] TraceShape retry BEGIN");
            functions->IGamePhysicsQuery_TraceShape.Search(true);
            traceShape = GetResolvedTraceShapeAddress();
            LogLine(traceShape
                ? "[TRACE-122] TraceShape retry END ready=1"
                : "[TRACE-122] TraceShape retry END ready=0");
        }

        std::uintptr_t filterConstructor =
            GetResolvedFilterConstructorAddress();
        if (!filterConstructor && clientLoaded && functions)
        {
            LogLine("[TRACE-123] Filter retry BEGIN");
            functions->CTraceFilter_Constructor.Search(true);
            filterConstructor = GetResolvedFilterConstructorAddress();
            LogLine(filterConstructor
                ? "[TRACE-124] Filter retry END ready=1"
                : "[TRACE-124] Filter retry END ready=0");
        }

        LogLine("[TRACE-125] Physics world retry BEGIN");
        auto* pPhysicsWorld = SDK::Pointers::CVPhys2World();
        LogLine((pPhysicsWorld && *pPhysicsWorld)
            ? "[TRACE-126] Physics world retry END ready=1"
            : "[TRACE-126] Physics world retry END ready=0");

        g_ready =
            clientLoaded &&
            pPhysicsWorld &&
            *pPhysicsWorld &&
            traceShape &&
            filterConstructor;

        g_lastDiagnostics.client_loaded = clientLoaded;
        g_lastDiagnostics.ready = g_ready;
        g_lastDiagnostics.manager = reinterpret_cast<std::uintptr_t>(
            (pPhysicsWorld && *pPhysicsWorld) ? *pPhysicsWorld : nullptr);
        g_lastDiagnostics.trace_ray = traceShape;
        g_lastDiagnostics.filter_init = filterConstructor;
        g_lastDiagnostics.fresh_manager = g_lastDiagnostics.manager;
        g_lastDiagnostics.fresh_trace_ray = traceShape;
        g_lastDiagnostics.fresh_filter_init = filterConstructor;

        char b[256];
        std::snprintf(
            b,
            sizeof(b),
            "[TRACE-127] Retry resolve END ready=%d world=%p trace=%p filter=%p",
            g_ready ? 1 : 0,
            reinterpret_cast<void*>(g_lastDiagnostics.manager),
            reinterpret_cast<void*>(traceShape),
            reinterpret_cast<void*>(filterConstructor));
        LogLine(b);

        return g_ready;
    }

    ResolverDiagnostics GetResolverDiagnostics()
    {
        return g_lastDiagnostics;
    }

    bool ThrottledRetry()
    {
        if (g_ready) return true;

        static DWORD lastRetryTick = 0;
        static int retryCount = 0;

        const DWORD now = GetTickCount();
        if (lastRetryTick != 0 && (now - lastRetryTick) < 500)
            return false;

        lastRetryTick = now;
        ++retryCount;

        char begin[128];
        std::snprintf(
            begin,
            sizeof(begin),
            "[TRACE-130] Retry attempt=%d BEGIN",
            retryCount);
        LogLine(begin);

        const bool result = DiagnoseAndRetryExistingResolvers();

        char end[128];
        std::snprintf(
            end,
            sizeof(end),
            "[TRACE-131] Retry attempt=%d END ready=%d",
            retryCount,
            result ? 1 : 0);
        LogLine(end);

        return result;
    }

    bool IsVisible(const float start[3], const float end[3], uintptr_t target, uintptr_t skip)
    {
        const bool log = ShouldTraceVisibility();
        if (log) LogLine("[TRACE-200] IsVisible BEGIN");

        const bool result =
            g_ready && DoTrace(start, end, target, skip);

        if (log) LogLine(result
            ? "[TRACE-201] IsVisible END result=1"
            : "[TRACE-201] IsVisible END result=0");
        return result;
    }

    bool Line(const float start[3], const float end[3], uintptr_t skip,
              float outEnd[3], float outNormal[3], float* outFrac)
    {
        const bool log = ShouldTraceLine();
        if (log) LogLine("[TRACE-210] Line BEGIN");

        if (!g_ready)
        {
            if (outEnd) {
                outEnd[0] = end[0];
                outEnd[1] = end[1];
                outEnd[2] = end[2];
            }
            if (outNormal) {
                outNormal[0] = 0;
                outNormal[1] = 0;
                outNormal[2] = 1;
            }
            if (outFrac) *outFrac = 1.f;
            if (log) LogLine("[TRACE-211] Line END ready=0");
            return false;
        }

        const bool result =
            DoTraceLine(start, end, skip, outEnd, outNormal, outFrac);

        if (log) LogLine(result
            ? "[TRACE-211] Line END hit=1"
            : "[TRACE-211] Line END hit=0");
        return result;
    }
}
