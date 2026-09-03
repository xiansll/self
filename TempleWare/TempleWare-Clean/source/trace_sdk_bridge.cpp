#include <CS2/SDK/SDK.hpp>
#include <CS2/SDK/CFunctionList.hpp>
#include <Common/MemoryEngine.hpp>

namespace
{
    CFunctionList g_cleanFunctionList{};
}

CFunctionList* GetFunctionList()
{
    return &g_cleanFunctionList;
}

namespace SDK
{
    IVPhysics2World** Pointers::g_ppIVPhysics2World = nullptr;

    auto Pointers::CVPhys2World() -> IVPhysics2World**
    {
        if (!g_ppIVPhysics2World)
        {
            const auto address = reinterpret_cast<uintptr_t>(
                FindPattern(
                    CLIENT_DLL,
                    XorStr(
                        "48 8B 1D ? ? ? ? 48 8B 01 FF 90 ? ? ? ? 4C 8B 0B 4C 8D 44 24 ? 48 8B C8")));

            if (!address)
                return nullptr;

            g_ppIVPhysics2World =
                *GetPtrAddress<IVPhysics2World***>(address);
        }

        return g_ppIVPhysics2World;
    }
}
