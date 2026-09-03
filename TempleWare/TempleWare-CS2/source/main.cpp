// Minimal TempleWare skeleton: manual map + ImGui empty overlay.
// No features, no game hooks (except the D3D11 Present hook for the overlay).
#include <windows.h>
#include <d3d11.h>
#include <cstdio>
#include <cstring>

#include "../external/imgui/imgui.h"
#include "../external/imgui/imgui_impl_dx11.h"
#include "../external/imgui/imgui_impl_win32.h"
#include "../external/kiero/kiero.h"
#include "../external/kiero/minhook/include/MinHook.h"
#include "esp/esp.h"
#include "chams/chams.h"
#include "gui/gui.h"
#include "gui/nexus_redesign.h"
#include "trace/trace.h"
#include "trace/autowall_debug.h"
#include "icons/icons.h"
#include "nerv/nerv_bridge.h"
#include "templeware/globals/d3d11_globals.h"
#include "templeware/globals/globals.h"
#include "templeware/templeware.h"
#include "templeware/utils/filelog/filelog.h"
#include "templeware/utils/localplayer/localplayer.h"
#include "templeware/utils/validation/validation.h"
#include "templeware/utils/validation/phase3c_validation.h"
#include "templeware/utils/validation/phase3d_validation.h"
#include "templeware/compat/velocity_rage_compat.h"
#include "templeware/compat/velocity_port_context.h"
#include "templeware/compat/velocity_feature_integration.h"
#include "templeware/compat/velocity_owner_bindings.h"
#include "templeware/rage/rage_dryrun.h"
#include "templeware/rage/rage_dryrun_providers.h"
#include "templeware/rage/rage_live_providers.h"
#include "templeware/rage/rage_execution.h"
#include "templeware/rage/rage_cmd_execution.h"
#include "templeware/hooks/hooks.h"

typedef HRESULT(__stdcall* Present)(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static HMODULE g_hModule = nullptr;

Present oPresent;
bool g_showMenu = true;
HWND window = NULL;
WNDPROC oWndProc;
ID3D11Device* g_pDevice = NULL;
ID3D11DeviceContext* g_pContext = NULL;
ID3D11RenderTargetView* mainRenderTargetView;

// D3D11 Globals for skin changer
ID3D11Device* D3D11::g_pDevice = nullptr;
ID3D11DeviceContext* D3D11::g_pContext = nullptr;

TempleWare g_templeWare;

LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    // Insert tusu: menuyu ac/kapa
    if (uMsg == WM_KEYDOWN && wParam == VK_INSERT)
    {
        g_showMenu = !g_showMenu;
        return true;
    }

    // Menu acikken input'u ImGui'ye ver, kapaliyken oyuna gecir
    if (g_showMenu && ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
        return true;

    return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

bool init = false;
bool foundationInit = false;
HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
    if (!init)
    {
        if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&g_pDevice)))
        {
            g_pDevice->GetImmediateContext(&g_pContext);
            DXGI_SWAP_CHAIN_DESC sd;
            pSwapChain->GetDesc(&sd);
            window = sd.OutputWindow;

            ID3D11Texture2D* pBackBuffer = nullptr;
            pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
            g_pDevice->CreateRenderTargetView(pBackBuffer, NULL, &mainRenderTargetView);
            pBackBuffer->Release();

            oWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)WndProc);

            ImGui::CreateContext();
            ImGui::GetIO().IniFilename = nullptr;
            ImGui_ImplWin32_Init(window);
            ImGui_ImplDX11_Init(g_pDevice, g_pContext);

            init = true;
            FileLog::Log("overlay init complete");

            if (Chams::Initialize())
                FileLog::Log("chams init ok");
            else
                FileLog::Log("chams init failed (see esp log)");

            Trace::Initialize();
            Icons::Initialize(g_pDevice, g_pContext);
        }
        else
        {
            return oPresent(pSwapChain, SyncInterval, Flags);
        }
    }

    if (!foundationInit)
    {
        foundationInit = g_templeWare.initFoundation();
        if (foundationInit)
        {
            // Establish process-lifetime ownership for compatibility config with
            // a disabled default snapshot. This proves only the config lifecycle;
            // it does not enable or execute any gameplay behaviour.
            VelocityRageCompat::initialize_non_gameplay_defaults();
            FileLog::Log("[P4COMPAT] NON-GAMEPLAY DEFAULT CONFIG PUBLISHED");
            FileLog::Log("[P5A] READ-ONLY PORT CONTEXT ACTIVE");

            // P5C owns only feature registration/context/provider lifecycle.
            // Its command lane remains dormant until the owner explicitly calls
            // dispatch_command() from a separately validated command source.
            VelocityRageCompat::FeatureIntegration::initialize();

            // P5D gives the owner one stable file/entry point for all future
            // provider, config translator, and feature registrations.
            VelocityRageCompat::OwnerBindings::install();

            // Bind live providers into the rage pipeline hub.
            RageDryRun::Live::bind();
            FileLog::Log("[P6LIVE] PROVIDERS BOUND, RAGE CONFIG ACTIVE");

            // Initialize no-spread (lazy pattern scan for game functions).
            RageDryRun::NoSpread::Initialize();

            // Install CreateMove hook for CUserCmd-based execution
            // (silent aim, attack via command, proper server-side angles).
            if (I::Input)
            {
                using CreateMoveFn = void(__fastcall*)(CCSGOInput*, int, bool);
                auto createMoveFunc = M::GetVFunc<CreateMoveFn>(I::Input, 5);
                if (createMoveFunc && !H::CreateMove.IsHooked())
                {
                    if (H::CreateMove.Add(reinterpret_cast<void*>(createMoveFunc),
                        reinterpret_cast<void*>(&H::hkCreateMove)))
                        FileLog::Log("[P6CMD] CREATEMOVE HOOK INSTALLED");
                    else
                        FileLog::Log("[P6CMD] CREATEMOVE HOOK FAILED");
                }
            }
            else
            {
                FileLog::Log("[P6CMD] I::Input NULL - CREATEMOVE NOT AVAILABLE");
            }

            // Trace::Initialize() runs very early with the overlay. Re-run only
            // the exact existing resolver expressions after foundation init so
            // we can distinguish an early-null timing problem from a true miss.
            const bool traceReadyAfterRetry = Trace::DiagnoseAndRetryExistingResolvers();
            const Trace::ResolverDiagnostics traceDiag = Trace::GetResolverDiagnostics();
            char traceBuf[384];
            std::snprintf(traceBuf, sizeof(traceBuf),
                "[P5B] TRACE RESOLVER DIAG ready=%d client=%d mgr=%p ray=%p filt=%p fresh_ray=%p fresh_filt=%p",
                traceReadyAfterRetry ? 1 : 0,
                traceDiag.client_loaded ? 1 : 0,
                reinterpret_cast<void*>(traceDiag.manager),
                reinterpret_cast<void*>(traceDiag.trace_ray),
                reinterpret_cast<void*>(traceDiag.filter_init),
                reinterpret_cast<void*>(traceDiag.fresh_trace_ray),
                reinterpret_cast<void*>(traceDiag.fresh_filter_init));
            FileLog::Log(traceBuf);
        }
    }

    // Throttled trace retry: 500ms intervals, 8s total cap.
    if (foundationInit && !Trace::Ready())
        Trace::ThrottledRetry();

    // Autowall debug: read-only test, 2s throttle, logs results
    if (foundationInit && Trace::Ready())
        AutowallDebug::Tick();

    // Present is the proven-safe Phase 3 runtime validation path. The suspect
    // FrameStageNotify detour stays disabled. Phase 3C proves resolver provenance;
    // Phase 3D separately proves a small basic wrapper surface.
    if (foundationInit)
    {
        static bool s_presentDiagnosticLogged = false;
        static bool s_phase3bActiveLogged = false;
        static bool s_providerOkLogged = false;
        static bool s_controllerOkLogged = false;
        static bool s_pawnChangeLogged = false;
        static bool s_lifecycleOkLogged = false;
        static bool s_phase3cActiveLogged = false;
        static bool s_phase3dActiveLogged = false;
        static bool s_wasInGame = false;
        static std::uintptr_t s_lastPawn = 0;

        if (!s_presentDiagnosticLogged)
        {
            FileLog::Log("[Validation] PRESENT DIAGNOSTIC FIRST CALL");
            s_presentDiagnosticLogged = true;
        }

        if (!s_phase3bActiveLogged)
        {
            FileLog::Log("[P3B] ACTIVE - LOCAL RUNTIME/LIFECYCLE VALIDATION");
            s_phase3bActiveLogged = true;
        }

        if (!s_phase3cActiveLogged)
        {
            FileLog::Log("[P3C] ACTIVE - SDK RESOLVER GATE (NO DEEP DEREF)");
            s_phase3cActiveLogged = true;
        }

        if (!s_phase3dActiveLogged)
        {
            FileLog::Log("[P3D] ACTIVE - BASIC WRAPPER SEMANTIC VALIDATION");
            s_phase3dActiveLogged = true;
        }

        const bool inGame = I::EngineClient &&
            I::EngineClient->connected() && I::EngineClient->in_game();

        if (inGame)
        {
            g_local_player_cache->update();
            const LocalPlayerSnapshot snapshot = g_local_player_cache->get();
            Validation::OnLocalPlayerCacheUpdate(snapshot);

            if (!s_providerOkLogged && snapshot.pawn && snapshot.controller)
            {
                char buf[320];
                std::snprintf(buf, sizeof(buf),
                    "[P3B] LOCAL PROVIDER OK pawn=%p controller=%p resolver=%d wrapper=%d sdk_safe=%d deep_safe=%d",
                    reinterpret_cast<void*>(snapshot.pawn),
                    reinterpret_cast<void*>(snapshot.controller),
                    snapshot.sdk_resolver_pair_proven ? 1 : 0,
                    snapshot.sdk_wrapper_semantics_proven ? 1 : 0,
                    snapshot.sdk_deref_safe ? 1 : 0,
                    snapshot.sdk_deep_graph_safe ? 1 : 0);
                FileLog::Log(buf);
                s_providerOkLogged = true;
            }

            if (!s_controllerOkLogged && snapshot.controller)
            {
                FileLog::Log("[P3B] CONTROLLER OK");
                s_controllerOkLogged = true;
            }

            if (snapshot.pawn)
            {
                if (!s_pawnChangeLogged && s_lastPawn && snapshot.pawn != s_lastPawn)
                {
                    char buf[256];
                    std::snprintf(buf, sizeof(buf),
                        "[P3B] PAWN CHANGE OK old=%p new=%p",
                        reinterpret_cast<void*>(s_lastPawn),
                        reinterpret_cast<void*>(snapshot.pawn));
                    FileLog::Log(buf);
                    s_pawnChangeLogged = true;
                }
                s_lastPawn = snapshot.pawn;
            }

            Phase3C::Run(snapshot);
            Phase3D::Run(snapshot);

            // Compatibility readiness is diagnostic-only. Runtime producers stay
            // closed until their own checkpoint proves them independently.
            VelocityRageCompat::log_readiness(snapshot);

            // P5A creates one read-only TempleWare-owned context for future port
            // consumers. It only aggregates already-owned compatibility state.
            VelocityRageCompat::g_port_context.update(snapshot, true);

            // P5C dispatches only the read-only frame lane. With no registered
            // features/providers this is a no-op apart from readiness diagnostics.
            VelocityRageCompat::FeatureIntegration::on_frame();

            // Basic wrapper proof is intentionally insufficient to open identity,
            // scene-node, or skeleton traversal. Those remain on a separate gate.
            if (snapshot.sdk_deep_graph_safe)
            {
                if (snapshot.pawn)
                {
                    auto* pawn = reinterpret_cast<C_CSPlayerPawn*>(snapshot.pawn);
                    Validation::OnSceneNodeChainCheck(pawn);
                    Validation::OnEntityIdentityCheck(reinterpret_cast<CEntityInstance*>(pawn));
                }

                if (snapshot.controller)
                {
                    Validation::OnEntityIdentityCheck(reinterpret_cast<CEntityInstance*>(snapshot.controller));
                }
            }
            else if (snapshot.sdk_deref_safe)
            {
                static bool s_basicWrapperOnlyLogged = false;
                if (!s_basicWrapperOnlyLogged)
                {
                    FileLog::Log("[Validation] BASIC WRAPPER SAFE - DEEP IDENTITY/SCENE GRAPH STILL SKIPPED");
                    s_basicWrapperOnlyLogged = true;
                }
            }
            else if (snapshot.pawn || snapshot.controller)
            {
                static bool s_pointerOnlyLogged = false;
                if (!s_pointerOnlyLogged)
                {
                    FileLog::Log("[Validation] POINTER-ONLY/RESOLVER-ONLY LOCAL - WRAPPER DEREF SKIPPED");
                    s_pointerOnlyLogged = true;
                }
            }

            Validation::LogPeriodicSummary(0);
        }
        else if (s_wasInGame)
        {
            // Reset local state, owner-supplied feature/provider state, then the
            // central compatibility publications. Bindings themselves are kept so
            // validated process-lifetime providers do not need re-registration.
            g_local_player_cache->reset();
            VelocityRageCompat::FeatureIntegration::reset_volatile();
            VelocityRageCompat::reset_volatile_runtime();
            VelocityRageCompat::g_port_context.reset_volatile();
            FileLog::Log("[P4COMPAT] VOLATILE RUNTIME RESET");
            Validation::OnLocalPlayerCacheReset();
            const LocalPlayerSnapshot resetSnapshot = g_local_player_cache->get();

            const bool cleanReset =
                resetSnapshot.pawn == 0 &&
                resetSnapshot.controller == 0 &&
                resetSnapshot.observer_pawn == 0 &&
                resetSnapshot.observer_controller == 0;

            if (cleanReset)
            {
                if (!s_lifecycleOkLogged)
                {
                    FileLog::Log("[P3B] LIFECYCLE OK - CACHE RESET CLEAN");
                    s_lifecycleOkLogged = true;
                }
            }
            else
            {
                FileLog::Log("[P3B] LIFECYCLE FAILED - CACHE NOT CLEAN AFTER RESET");
            }

            s_lastPawn = 0;
        }

        s_wasInGame = inGame;
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    Gui::MaybeRebuildFont();
    ImGui::NewFrame();

    Esp::Draw();
    Esp::DrawOverlay();
    // UpdateAim + UpdateTrigger moved to CUserCmd (LegitCmd::OnCreateMove in CreateMove hook)
    Esp::UpdateMisc();
    Esp::UpdateSkins();
    nerv_bridge::tick(g_showMenu, (void*)g_ctx->local_pawn, (void*)g_ctx->local_controller);

    // --- Rage Pipeline Tick ---
    if (foundationInit && RageDryRun::Live::g_enabled)
    {
        auto& rs = RageDryRun::g_state;
        const auto& rage = Esp::g_rage;

        // Rage activation check (key + mode)
        bool rageActive = false;
        if (rage.masterEnable)
        {
            if (rage.aimType == 2) rageActive = true; // Always
            else if (rage.aimKey != 0)
            {
                const bool down = (GetAsyncKeyState(rage.aimKey) & 0x8000) != 0;
                if (rage.aimType == 1) // Toggle
                {
                    static bool prevDown = false, toggled = false;
                    if (down && !prevDown) toggled = !toggled;
                    prevDown = down;
                    rageActive = toggled;
                }
                else rageActive = down; // Hold
            }
            else rageActive = true; // no key set = always
        }

        // Detect active weapon group from local player's weapon name
        int activeGroup = 2; // default rifle
        {
            HMODULE cl = GetModuleHandleA("client.dll");
            if (cl)
            {
                uintptr_t cb = reinterpret_cast<uintptr_t>(cl);
                uintptr_t lp = *reinterpret_cast<uintptr_t*>(cb + 0x23C6268);
                if (lp >= 0x10000 && lp < 0x0000FFFFFFFFFFFFull)
                {
                    char wpnName[48] = {};
                    // Read weapon name via ESP's proven path
                    uintptr_t ws = *reinterpret_cast<uintptr_t*>(lp + 0x1208);
                    if (ws >= 0x10000 && ws < 0x0000FFFFFFFFFFFFull)
                    {
                        uint32_t hAct = *reinterpret_cast<uint32_t*>(ws + 0x60);
                        uintptr_t wpn = Esp::LookupEntity(static_cast<int>(hAct & 0x7FFF));
                        if (wpn)
                        {
                            uintptr_t vd = *reinterpret_cast<uintptr_t*>(wpn + 0x380 + 0x8);
                            if (vd >= 0x10000 && vd < 0x0000FFFFFFFFFFFFull)
                            {
                                const char* n = *reinterpret_cast<const char* const*>(vd + 0x720);
                                if (n && reinterpret_cast<uintptr_t>(n) >= 0x10000)
                                {
                                    if (std::strncmp(n, "weapon_", 7) == 0) n += 7;
                                    std::snprintf(wpnName, sizeof(wpnName), "%s", n);
                                }
                            }
                        }
                    }
                    if (wpnName[0])
                    {
                        auto has = [&](const char* s) { return std::strstr(wpnName, s) != nullptr; };
                        if (has("awp") || has("ssg08") || has("scar20") || has("g3sg1"))
                            activeGroup = 4; // sniper
                        else if (has("nova") || has("xm1014") || has("mag7") || has("sawedoff"))
                            activeGroup = 3; // shotgun
                        else if (has("mp9") || has("mac10") || has("mp7") || has("mp5") || has("ump45") || has("p90") || has("bizon"))
                            activeGroup = 1; // smg
                        else if (has("ak47") || has("m4a1") || has("aug") || has("sg556") || has("sg553") || has("galil") || has("famas"))
                            activeGroup = 2; // rifle
                        else if (has("negev") || has("m249"))
                            activeGroup = 5; // lmg
                        else if (has("knife") || has("bayonet") || has("grenade") || has("flashbang") || has("molotov") ||
                                 has("smoke") || has("decoy") || has("incgrenade") || has("c4") || has("taser") || has("healthshot"))
                            activeGroup = -1; // utility, don't rage
                        else
                            activeGroup = 0; // pistol
                    }
                }
            }
        }

        // Get active group config
        const auto& grp = (activeGroup >= 0 && activeGroup < 6) ? rage.groups[activeGroup] : rage.groups[0];
        bool groupEnabled = rageActive && (activeGroup >= 0) && grp.enable;

        // Sync GUI rage config -> pipeline config
        rs.config.enabled = groupEnabled;
        rs.config.auto_fire_plan = true;
        rs.config.selection = rage.selection;
        rs.config.max_fov = grp.maxFov;
        rs.config.hitchance = grp.hitChance;
        rs.config.minimum_damage = grp.minDamage;
        rs.config.point_scale = grp.pointScale;
        rs.config.silent_plan = grp.silent;
        rs.config.no_spread_plan = grp.noSpread;
        rs.config.doubletap_plan = grp.doubletap;
        rs.config.prefer_body = grp.forceBAim;
        rs.config.require_visibility = false;
        rs.config.primary_hitbox = grp.hbHead ? 0 : (grp.hbChest ? 2 : (grp.hbStomach ? 3 : 0));
        rs.config.hitboxes[0] = grp.hbHead;
        rs.config.hitboxes[2] = grp.hbChest;
        rs.config.hitboxes[3] = grp.hbStomach;
        rs.config.hitboxes[5] = grp.hbArms;
        rs.config.hitboxes[6] = grp.hbArms;
        rs.config.hitboxes[7] = grp.hbLegs;
        rs.config.hitboxes[8] = grp.hbLegs;

        // Force shot air/ground (bind key: 0 = always when toggled, >0 = hold)
        {
            bool fsAirActive = grp.forceShotAir &&
                (grp.forceShotAirKey == 0 || (GetAsyncKeyState(grp.forceShotAirKey) & 0x8000));
            bool fsGroundActive = grp.forceShotGround &&
                (grp.forceShotGroundKey == 0 || (GetAsyncKeyState(grp.forceShotGroundKey) & 0x8000));
            rs.config.force_shot_air = fsAirActive;
            rs.config.force_shot_ground = fsGroundActive;
        }

        // Overrides: when GUI value > 0 AND bind held (or bind==0), activate
        {
            bool hcBindHeld = grp.hitChanceOverrideKey == 0 ||
                (GetAsyncKeyState(grp.hitChanceOverrideKey) & 0x8000);
            rs.config.hitchance_override = (grp.hitChanceOverride > 0 && hcBindHeld)
                ? grp.hitChanceOverride : 0;

            bool dmgBindHeld = grp.minDmgOverrideKey == 0 ||
                (GetAsyncKeyState(grp.minDmgOverrideKey) & 0x8000);
            if (grp.minDmgOverride > 0 && dmgBindHeld)
            {
                rs.config.damage_override = true;
                rs.config.override_damage = grp.minDmgOverride;
            }
            else
            {
                rs.config.damage_override = false;
            }
        }

        // Dynamic point scale + debug multipoints
        rs.config.dynamic_point_scale = grp.dynamicPointScale;
        rs.config.debug_multipoints = grp.debugMultipoints;

        // Publish provider snapshots into g_state
        RageDryRun::publish_bound_snapshots();
        rs.source = RageDryRun::SourceMode::Live;
        rs.live_entity_count = static_cast<int>(rs.candidates.size());

        // Evaluate: select target, run all evaluators, build action plan
        rs.evaluate();
        rs.action.execution_enabled = groupEnabled;

        // Log readiness transitions
        RageDryRun::Live::log_live_transitions();

        // Log live state periodically
        {
            static DWORD s_lastLiveLog = 0;
            DWORD now = GetTickCount();
            if (now - s_lastLiveLog > 3000)
            {
                s_lastLiveLog = now;
                const auto& r = rs.readiness;
                const auto& a = rs.action;
                char buf[512];
                std::snprintf(buf, sizeof(buf),
                    "[P6LIVE] READY frame=%d entities=%d bones=%d hitboxes=%d "
                    "weapon=%d prediction=%d trace=%d pen=%d | "
                    "target=%d hc=%.0f dmg=%.0f fire=%d exec=%d force=%d",
                    r.combat_frame == RageDryRun::Readiness::Ready ? 1 : 0,
                    rs.live_entity_count,
                    r.bones == RageDryRun::Readiness::Ready ? 1 : 0,
                    r.hitboxes == RageDryRun::Readiness::Ready ? 1 : 0,
                    r.weapon == RageDryRun::Readiness::Ready ? 1 : 0,
                    r.prediction == RageDryRun::Readiness::Ready ? 1 : 0,
                    r.trace == RageDryRun::Readiness::Ready ? 1 : 0,
                    r.penetration == RageDryRun::Readiness::Ready ? 1 : 0,
                    a.target_found ? a.target_id : -1,
                    a.hitchance, a.predicted_damage,
                    a.would_fire ? 1 : 0,
                    a.execution_enabled ? 1 : 0,
                    a.force_shot_active ? 1 : 0);
                FileLog::Log(buf);
            }
        }

        // Execution moved to CreateMove hook (rage_cmd_execution.h).
        // The present hook only evaluates; CreateMove applies the plan
        // via CUserCmd mutation for proper silent aim and attack timing.
    }

    static float g_menuAnim = 0.f;
    {
        float dt = ImGui::GetIO().DeltaTime;
        if (dt <= 0.f) dt = 1.f / 60.f;
        const float tgt = g_showMenu ? 1.f : 0.f;
        const float step = 14.f * dt;
        g_menuAnim += (tgt - g_menuAnim) * (step < 1.f ? step : 1.f);
    }
    if (g_menuAnim > 0.01f)
    {
        NexusRedesign::Render(g_menuAnim);

        // Registered features get a menu-frame callback without TempleWare
        // knowing or implementing any feature-specific controls.
        VelocityRageCompat::FeatureIntegration::on_menu();
    }

    ImGui::Render();
    g_pContext->OMSetRenderTargets(1, &mainRenderTargetView, NULL);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    return oPresent(pSwapChain, SyncInterval, Flags);
}

DWORD WINAPI MainThread(LPVOID lpReserved)
{
    FileLog::Log("MainThread started");

    bool init_hook = false;
    do
    {
        if (!init_hook)
        {
            if (kiero::init(kiero::RenderType::D3D11) == kiero::Status::Success)
            {
                kiero::bind(8, (void**)&oPresent, hkPresent);
                init_hook = true;
                FileLog::Log("kiero bind(8) done");
            }
        }
        Sleep(100);
    } while (!GetAsyncKeyState(VK_END));

    VelocityRageCompat::OwnerBindings::uninstall();
    VelocityRageCompat::FeatureIntegration::shutdown();
    VelocityRageCompat::g_port_context.reset_volatile();
    VelocityRageCompat::shutdown_runtime();
    FileLog::Log("[P4COMPAT] RUNTIME SHUTDOWN CLEAN");
    kiero::shutdown();
    FreeLibraryAndExitThread(g_hModule, EXIT_SUCCESS);
    return TRUE;
}

BOOL WINAPI DllMain(HMODULE hMod, DWORD dwReason, LPVOID lpReserved)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        g_hModule = hMod;
        DisableThreadLibraryCalls(hMod);
        FileLog::Initialize();
        FileLog::Log("DLL_PROCESS_ATTACH");
        CreateThread(nullptr, 0, MainThread, hMod, 0, nullptr);
    }
    return TRUE;
}
