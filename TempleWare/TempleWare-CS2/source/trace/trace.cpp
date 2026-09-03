#include "trace.h"
#include <windows.h>
#include <cstring>
#include <cstdio>

#include <CS2/SDK/SDK.hpp>
#include <CS2/SDK/Update/GameTrace.hpp>
#include <CS2/SDK/FunctionListSDK.hpp>

namespace {
void LogLine(const char* m){wchar_t p[MAX_PATH]={};GetTempPathW(MAX_PATH,p);wcscat_s(p,MAX_PATH,L"TempleWare.log");FILE* f=nullptr;if(_wfopen_s(&f,p,L"a")==0&&f){fprintf(f,"[trace] %s\n",m);fclose(f);}}

bool g_ready = false;
Trace::ResolverDiagnostics g_lastDiagnostics{};

std::uintptr_t GetResolvedTraceShapeAddress() noexcept
{
    auto* functions = GetFunctionList();
    if (!functions) return 0;
    return reinterpret_cast<std::uintptr_t>(
        functions->IGamePhysicsQuery_TraceShape.GetFunction());
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

        const bool clientLoaded = GetModuleHandleA("client.dll") != nullptr;
        auto* pPhysicsWorld = SDK::Pointers::CVPhys2World();
        const std::uintptr_t traceShape = GetResolvedTraceShapeAddress();

        g_ready = clientLoaded && pPhysicsWorld && *pPhysicsWorld && traceShape;

        g_lastDiagnostics.client_loaded = clientLoaded;
        g_lastDiagnostics.ready = g_ready;
        // Keep legacy field names for callers that already consume this struct:
        // manager   -> resolved physics-world object
        // trace_ray -> resolved TraceShape backend address
        g_lastDiagnostics.manager = reinterpret_cast<std::uintptr_t>(
            (pPhysicsWorld && *pPhysicsWorld) ? *pPhysicsWorld : nullptr);
        g_lastDiagnostics.trace_ray = traceShape;
        g_lastDiagnostics.filter_init = 0;

        char b[192];
        std::snprintf(b, sizeof(b),
            "client=%d physics_world=%p trace_shape=%p ready=%d",
            clientLoaded ? 1 : 0,
            (void*)((pPhysicsWorld && *pPhysicsWorld) ? *pPhysicsWorld : nullptr),
            (void*)traceShape,
            g_ready ? 1 : 0);
        LogLine(b);
        return g_ready;
    }

    bool Ready() { return g_ready; }

    bool DiagnoseAndRetryExistingResolvers()
    {
        const bool clientLoaded = GetModuleHandleA("client.dll") != nullptr;
        auto* pPhysicsWorld = SDK::Pointers::CVPhys2World();
        const std::uintptr_t traceShape = GetResolvedTraceShapeAddress();

        g_ready = clientLoaded && pPhysicsWorld && *pPhysicsWorld && traceShape;

        g_lastDiagnostics.client_loaded = clientLoaded;
        g_lastDiagnostics.ready = g_ready;
        g_lastDiagnostics.manager = reinterpret_cast<std::uintptr_t>(
            (pPhysicsWorld && *pPhysicsWorld) ? *pPhysicsWorld : nullptr);
        g_lastDiagnostics.trace_ray = traceShape;
        g_lastDiagnostics.filter_init = 0;
        g_lastDiagnostics.fresh_manager = g_lastDiagnostics.manager;
        g_lastDiagnostics.fresh_trace_ray = traceShape;
        g_lastDiagnostics.fresh_filter_init = 0;

        char b[256];
        std::snprintf(b, sizeof(b),
            "diag client=%d physics_world=%p trace_shape=%p ready=%d",
            clientLoaded ? 1 : 0,
            (void*)((pPhysicsWorld && *pPhysicsWorld) ? *pPhysicsWorld : nullptr),
            (void*)traceShape,
            g_ready ? 1 : 0);
        LogLine(b);

        if (!clientLoaded)
            LogLine("diag blocker: client.dll is not loaded");
        else if (!pPhysicsWorld)
            LogLine("diag blocker: CVPhys2World pattern did not resolve");
        else if (!*pPhysicsWorld)
            LogLine("diag blocker: physics world pointer is null");
        else if (!traceShape)
            LogLine("diag blocker: IGamePhysicsQuery::TraceShape did not resolve");
        else if (g_ready)
            LogLine("diag ready: trace system initialized via CFunctionList integration");
        else
            LogLine("diag blocker: unknown error in trace system initialization");

        return g_ready;
    }

    ResolverDiagnostics GetResolverDiagnostics()
    {
        return g_lastDiagnostics;
    }

    bool IsVisible(const float start[3], const float end[3], uintptr_t target, uintptr_t skip)
    {
        if (!g_ready) return false;
        return DoTrace(start, end, target, skip);
    }

    bool Line(const float start[3], const float end[3], uintptr_t skip,
              float outEnd[3], float outNormal[3], float* outFrac)
    {
        if (!g_ready)
        {
            if (outEnd) { outEnd[0] = end[0]; outEnd[1] = end[1]; outEnd[2] = end[2]; }
            if (outNormal) { outNormal[0] = 0; outNormal[1] = 0; outNormal[2] = 1; }
            if (outFrac) *outFrac = 1.f;
            return false;
        }
        return DoTraceLine(start, end, skip, outEnd, outNormal, outFrac);
    }
}
