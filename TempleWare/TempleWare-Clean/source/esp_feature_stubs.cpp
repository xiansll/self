#include "../../TempleWare-CS2/source/trace/trace.h"

namespace Trace
{
    bool Initialize()
    {
        return false;
    }

    bool Ready()
    {
        return false;
    }

    bool DiagnoseAndRetryExistingResolvers()
    {
        return false;
    }

    ResolverDiagnostics GetResolverDiagnostics()
    {
        return {};
    }

    bool ThrottledRetry()
    {
        return true;
    }

    bool IsVisible(
        const float[3],
        const float[3],
        uintptr_t,
        uintptr_t)
    {
        return false;
    }

    bool Line(
        const float[3],
        const float end[3],
        uintptr_t,
        float outEnd[3],
        float outNormal[3],
        float* outFraction)
    {
        if (outEnd) {
            outEnd[0] = end[0];
            outEnd[1] = end[1];
            outEnd[2] = end[2];
        }

        if (outNormal) {
            outNormal[0] = 0.0f;
            outNormal[1] = 0.0f;
            outNormal[2] = 1.0f;
        }

        if (outFraction)
            *outFraction = 1.0f;

        return false;
    }
}
