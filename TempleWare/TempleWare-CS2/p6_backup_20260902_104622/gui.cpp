#include "gui.h"
#include "nexus_body_embedded.h"
#include "../esp/esp.h"
#include "../trace/trace.h"
#include "../icons/icons.h"
#include "../templeware/config/gui_config.h"
#include "../templeware/config/config.h"
#include "../templeware/features/skinchanger/skinchanger.h"

#include "../../external/imgui/imgui.h"
#include "../nerv/nerv_bridge.h"
#include "../../external/imgui/imgui_impl_dx11.h"

#include <windows.h>
#include <wincodec.h>
#include <unordered_map>
#include <vector>
#include <string>
#include <cmath>
#include <cstdio>
#include <cstring>

#pragma comment(lib, "windowscodecs.lib")
extern ID3D11Device* g_pDevice;

// =====================================================================
//  NEXUS â€” fully custom-drawn UI (ImGui used only as the vertex batcher)
// =====================================================================
namespace
{
    // ---- palette ----
    ImVec4 RGBA(int r, int g, int b, int a = 255) { return ImVec4(r / 255.f, g / 255.f, b / 255.f, a / 255.f); }
    ImU32  U32(const ImVec4& c) { return ImGui::ColorConvertFloat4ToU32(c); }
    ImVec4 Lerp4(const ImVec4& a, const ImVec4& b, float t) { return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t); }
    ImVec4 WithA(ImVec4 c, float a) { c.w = a; return c; }

    ImVec4 g_accent = RGBA(255, 126, 8);
    const ImVec4 BG0   = RGBA(10, 12, 16);
    const ImVec4 BG1   = RGBA(14, 16, 21);
    const ImVec4 CARD  = RGBA(19, 22, 28);
    const ImVec4 FRAME = RGBA(28, 31, 39);
    const ImVec4 BORDER= RGBA(39, 43, 53);
    const ImVec4 TEXT  = RGBA(239, 241, 245);
    const ImVec4 DIM   = RGBA(132, 139, 151);
    const ImVec4 GREEN = RGBA(91, 205, 130);

    float g_uiScale = 1.0f;
    int   g_builtFontPx = -1;
    bool  g_needCenter = true;
    int   g_nav = 1;       // start on Aimbot to match the reference
    int   g_sub = 0;

    float S(float x) { return x * g_uiScale; }

    // ---- animation ----
    std::unordered_map<ImGuiID, float> g_anim;
    float Anim(ImGuiID id, float target, float speed = 12.f)
    {
        float dt = ImGui::GetIO().DeltaTime; if (dt <= 0.f) dt = 1.f / 60.f;
        float& v = g_anim[id];
        v += (target - v) * (speed * dt < 1.f ? speed * dt : 1.f);
        return v;
    }

    ImDrawList* DL() { return ImGui::GetWindowDrawList(); }

    // ---- primitives ----
    void RectF(ImVec2 a, ImVec2 b, ImU32 col, float r = 0.f) { DL()->AddRectFilled(a, b, col, r); }
    void Rect(ImVec2 a, ImVec2 b, ImU32 col, float r = 0.f, float th = 1.f) { DL()->AddRect(a, b, col, r, 0, th); }
    void Text(ImVec2 p, ImU32 col, const char* t)
    {
        const char* h = std::strstr(t, "##");   // hide id-disambiguation suffix
        if (h) { char b[128]; int n = (int)(h - t); if (n > 127) n = 127; std::memcpy(b, t, n); b[n] = 0; DL()->AddText(p, col, b); }
        else DL()->AddText(p, col, t);
    }
    void TextR(float rightX, float y, ImU32 col, const char* t) { const ImVec2 s = ImGui::CalcTextSize(t); DL()->AddText(ImVec2(rightX - s.x, y), col, t); }
    float TW(const char* t) { return ImGui::CalcTextSize(t).x; }
    float TH() { return ImGui::GetFontSize(); }

    // ---------------------------------------------------------------
    //  Widgets â€” each takes an absolute rect and advances nothing; the
    //  page code positions them. Interaction via InvisibleButton.
    // ---------------------------------------------------------------
    bool Hit(const char* id, ImVec2 pos, ImVec2 size, bool& hovered)
    {
        ImGui::SetCursorScreenPos(pos);
        ImGui::InvisibleButton(id, size);
        hovered = ImGui::IsItemHovered();
        return ImGui::IsItemClicked();
    }

    // Minimal line icons for the sidebar (hand-drawn, index = nav item).
    void NavIcon(int idx, float cx, float cy, ImU32 col)
    {
        ImDrawList* d = DL();
        const float u = S(8.f);        // half-size
        const float th = S(1.6f);
        auto L = [&](float x1, float y1, float x2, float y2) { d->AddLine(ImVec2(cx + x1, cy + y1), ImVec2(cx + x2, cy + y2), col, th); };
        auto R = [&](float x1, float y1, float x2, float y2) { d->AddRect(ImVec2(cx + x1, cy + y1), ImVec2(cx + x2, cy + y2), col, S(1.5f), 0, th); };
        auto C = [&](float x, float y, float r) { d->AddCircle(ImVec2(cx + x, cy + y), r, col, 16, th); };
        switch (idx)
        {
        case 0: // Dashboard - grid
            R(-u, -u, -1, -1); R(1, -u, u, -1); R(-u, 1, -1, u); R(1, 1, u, u); break;
        case 1: // Aimbot - crosshair
            C(0, 0, u * 0.7f); L(0, -u, 0, -u * 0.4f); L(0, u * 0.4f, 0, u); L(-u, 0, -u * 0.4f, 0); L(u * 0.4f, 0, u, 0); break;
        case 2: // Visuals - eye
            d->AddBezierCurve(ImVec2(cx - u, cy), ImVec2(cx - u * 0.3f, cy - u), ImVec2(cx + u * 0.3f, cy - u), ImVec2(cx + u, cy), col, th);
            d->AddBezierCurve(ImVec2(cx - u, cy), ImVec2(cx - u * 0.3f, cy + u), ImVec2(cx + u * 0.3f, cy + u), ImVec2(cx + u, cy), col, th);
            C(0, 0, u * 0.35f); break;
        case 3: // World - cube
            R(-u, -u * 0.6f, u * 0.4f, u); L(-u, -u * 0.6f, -u * 0.4f, -u); L(u * 0.4f, -u * 0.6f, u, -u); L(u * 0.4f, u, u, u * 0.4f); L(-u * 0.4f, -u, u, -u); break;
        case 4: // Movement - run (chevrons)
            L(-u, -u, 0, 0); L(0, 0, -u, u); L(0, -u, u, 0); L(u, 0, 0, u); break;
        case 5: // Inventory - pistol (simple)
            L(-u, -u * 0.4f, u, -u * 0.4f); L(-u, -u * 0.4f, -u, u * 0.4f); L(-u * 0.3f, -u * 0.4f, -u * 0.3f, u); break;
        case 6: // Misc - sliders
            L(-u, -u * 0.5f, u, -u * 0.5f); L(-u, u * 0.5f, u, u * 0.5f); d->AddCircleFilled(ImVec2(cx + u * 0.2f, cy - u * 0.5f), S(2.f), col); d->AddCircleFilled(ImVec2(cx - u * 0.3f, cy + u * 0.5f), S(2.f), col); break;
        case 7: // Configs - folder
            L(-u, -u * 0.4f, -u * 0.2f, -u * 0.4f); L(-u * 0.2f, -u * 0.4f, 0, -u * 0.7f); L(0, -u * 0.7f, u, -u * 0.7f); R(-u, -u * 0.4f, u, u * 0.7f); break;
        case 8: // Lua - code brackets
            L(-u * 0.3f, -u, -u, 0); L(-u, 0, -u * 0.3f, u); L(u * 0.3f, -u, u, 0); L(u, 0, u * 0.3f, u); break;
        default: // Settings - gear
            C(0, 0, u * 0.55f); for (int k = 0; k < 8; ++k) { float a = k * 0.785f; L(cosf(a) * u * 0.55f, sinf(a) * u * 0.55f, cosf(a) * u, sinf(a) * u); } break;
        }
    }

    // Orange pill toggle at (x,y). Returns clicked.
    bool Toggle(const char* id, float x, float y, bool* v)
    {
        const float w = S(38.f), h = S(20.f);
        bool hov = false;
        const bool clk = Hit(id, ImVec2(x, y), ImVec2(w, h), hov);
        if (clk) *v = !*v;
        const float on = Anim(ImGui::GetID(id), *v ? 1.f : 0.f, 16.f);
        const ImU32 track = U32(Lerp4(FRAME, g_accent, on));
        RectF(ImVec2(x, y), ImVec2(x + w, y + h), track, h * 0.5f);
        const float kr = h * 0.5f - S(2.5f);
        const float kx = x + S(2.5f) + kr + (w - S(5.f) - 2 * kr) * on;
        DL()->AddCircleFilled(ImVec2(kx, y + h * 0.5f), kr, U32(RGBA(255, 255, 255)));
        return clk;
    }

    // Slider row: label at left of `x`, track from x..x+tw, value at right.
    void Slider(const char* id, float x, float y, float tw, float* val, float mn, float mx, const char* fmt, bool isInt = false)
    {
        const float h = S(4.f);
        const float cy = y + TH() * 0.5f;
        bool hov = false;
        ImGui::SetCursorScreenPos(ImVec2(x, y - S(6.f)));
        ImGui::InvisibleButton(id, ImVec2(tw, TH() + S(6.f)));
        const bool active = ImGui::IsItemActive();
        hov = ImGui::IsItemHovered();
        if (active)
        {
            const float t = (ImGui::GetIO().MousePos.x - x) / tw;
            float nt = t < 0 ? 0 : (t > 1 ? 1 : t);
            *val = mn + (mx - mn) * nt;
            if (isInt) *val = std::floor(*val + 0.5f);
        }
        const float frac = (mx > mn) ? ((*val - mn) / (mx - mn)) : 0.f;
        RectF(ImVec2(x, cy - h * 0.5f), ImVec2(x + tw, cy + h * 0.5f), U32(FRAME), h * 0.5f);
        RectF(ImVec2(x, cy - h * 0.5f), ImVec2(x + tw * frac, cy + h * 0.5f), U32(g_accent), h * 0.5f);
        DL()->AddCircleFilled(ImVec2(x + tw * frac, cy), S(5.f), U32(RGBA(255, 255, 255)));
        char buf[32];
        if (isInt) std::snprintf(buf, sizeof(buf), fmt, (int)(*val + 0.5f));
        else       std::snprintf(buf, sizeof(buf), fmt, *val);
        TextR(x + tw + S(46.f), y, U32(TEXT), buf);
    }
    void SliderI(const char* id, float x, float y, float tw, int* val, int mn, int mx, const char* fmt)
    {
        float f = (float)*val; Slider(id, x, y, tw, &f, (float)mn, (float)mx, fmt, true); *val = (int)(f + 0.5f);
    }

    // Custom dropdown. Draws closed box at rect; opens a styled popup.
    bool Combo(const char* id, float x, float y, float w, int* val, const char* const* items, int count)
    {
        const float h = S(26.f);
        bool hov = false;
        const bool clk = Hit(id, ImVec2(x, y), ImVec2(w, h), hov);
        RectF(ImVec2(x, y), ImVec2(x + w, y + h), U32(FRAME), S(5.f));
        if (hov) Rect(ImVec2(x, y), ImVec2(x + w, y + h), U32(WithA(g_accent, 0.5f)), S(5.f));
        const char* cur = (*val >= 0 && *val < count) ? items[*val] : "";
        Text(ImVec2(x + S(10.f), y + (h - TH()) * 0.5f), U32(TEXT), cur);
        // arrow
        const float ax = x + w - S(16.f), ay = y + h * 0.5f;
        DL()->AddTriangleFilled(ImVec2(ax - S(4.f), ay - S(2.f)), ImVec2(ax + S(4.f), ay - S(2.f)), ImVec2(ax, ay + S(3.f)), U32(DIM));
        if (clk) ImGui::OpenPopup(id);

        bool changed = false;
        ImGui::PushStyleColor(ImGuiCol_PopupBg, U32(BG1));
        ImGui::PushStyleColor(ImGuiCol_Border, U32(BORDER));
        ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, S(6.f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(S(4.f), S(4.f)));
        ImGui::SetNextWindowPos(ImVec2(x, y + h + S(3.f)));
        ImGui::SetNextWindowSize(ImVec2(w, 0));
        if (ImGui::BeginPopup(id))
        {
            for (int i = 0; i < count; ++i)
            {
                const bool sel = (i == *val);
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, U32(WithA(g_accent, 0.25f)));
                ImGui::PushStyleColor(ImGuiCol_Text, sel ? U32(g_accent) : U32(TEXT));
                if (ImGui::Selectable(items[i], sel)) { *val = i; changed = true; }
                ImGui::PopStyleColor(2);
            }
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
        return changed;
    }

    // Keybind button: click to listen, next key/mouse becomes the bind.
    const char* KeyName(int k)
    {
        static char buf[32];
        switch (k)
        {
        case 0: return "NONE";
        case 1: return "MOUSE 1"; case 2: return "MOUSE 2"; case 4: return "MOUSE 3";
        case 5: return "MOUSE 4"; case 6: return "MOUSE 5";
        case VK_SPACE: return "SPACE"; case VK_SHIFT: return "SHIFT"; case VK_MENU: return "ALT";
        case VK_CONTROL: return "CTRL"; case VK_TAB: return "TAB"; case VK_INSERT: return "INSERT";
        }
        UINT sc = MapVirtualKeyA(k, MAPVK_VK_TO_VSC);
        if (sc && GetKeyNameTextA((LONG)(sc << 16), buf, sizeof(buf)) > 0) return buf;
        std::snprintf(buf, sizeof(buf), "0x%02X", k);
        return buf;
    }
    static int  g_listening = 0;    // ImGuiID of the widget currently listening
    static bool g_kbArmed = false;  // becomes true once the opening click is released
    void Keybind(const char* id, float x, float y, float w, int* key)
    {
        const float h = S(26.f);
        const ImGuiID wid = ImGui::GetID(id);
        bool hov = false;
        const bool clk = Hit(id, ImVec2(x, y), ImVec2(w, h), hov);
        if (clk) { g_listening = (int)wid; g_kbArmed = false; }   // start listening; wait for click release
        const bool listening = (g_listening == (int)wid);

        RectF(ImVec2(x, y), ImVec2(x + w, y + h), U32(FRAME), S(5.f));
        if (listening) Rect(ImVec2(x, y), ImVec2(x + w, y + h), U32(g_accent), S(5.f));
        const char* label = listening ? "Press a key" : KeyName(*key);
        Text(ImVec2(x + (w - TW(label)) * 0.5f, y + (h - TH()) * 0.5f), U32(listening ? g_accent : TEXT), label);

        if (listening)
        {
            // arm only after the mouse buttons that opened the picker are released,
            // so the opening click isn't captured as the bind.
            if (!g_kbArmed)
            {
                if (!(GetAsyncKeyState(VK_LBUTTON) & 0x8000) && !(GetAsyncKeyState(VK_RBUTTON) & 0x8000))
                    g_kbArmed = true;
                return;
            }
            for (int k = 1; k < 256; ++k)
            {
                if (k == VK_CAPITAL || k == VK_NUMLOCK || k == VK_SCROLL) continue; // ignore lock toggles
                if (GetAsyncKeyState(k) & 0x8000)
                {
                    if (k == VK_ESCAPE) { *key = 0; g_listening = 0; break; } // ESC clears
                    *key = k; g_listening = 0; break;
                }
            }
        }
    }

    // Small orange section icons for card headers.
    void SecIcon(int idx, float cx, float cy, ImU32 col)
    {
        ImDrawList* d = DL(); const float u = S(7.f), th = S(1.6f);
        auto L = [&](float x1, float y1, float x2, float y2) { d->AddLine(ImVec2(cx + x1, cy + y1), ImVec2(cx + x2, cy + y2), col, th); };
        switch (idx)
        {
        case 0: // lightning
            L(1, -u, -3, 1); L(-3, 1, 1, 1); L(1, 1, -1, u); break;
        case 1: // crosshair
            d->AddCircle(ImVec2(cx, cy), u * 0.7f, col, 16, th); L(0, -u, 0, -u * 0.4f); L(0, u * 0.4f, 0, u); L(-u, 0, -u * 0.4f, 0); L(u * 0.4f, 0, u, 0); break;
        case 2: // gauge
            d->AddCircle(ImVec2(cx, cy + u * 0.2f), u, col, 16, th); L(0, u * 0.2f, u * 0.5f, -u * 0.4f); break;
        case 3: // hitbox (rounded square + dot)
            d->AddRect(ImVec2(cx - u, cy - u), ImVec2(cx + u, cy + u), col, S(2.f), 0, th); d->AddCircleFilled(ImVec2(cx, cy), S(1.8f), col); break;
        case 4: // anti-aim (two arrows)
            L(-u, 0, u, 0); L(-u, 0, -u * 0.4f, -u * 0.4f); L(-u, 0, -u * 0.4f, u * 0.4f); L(u, 0, u * 0.4f, -u * 0.4f); L(u, 0, u * 0.4f, u * 0.4f); break;
        default: // keyboard
            d->AddRect(ImVec2(cx - u, cy - u * 0.6f), ImVec2(cx + u, cy + u * 0.6f), col, S(2.f), 0, th);
            d->AddCircleFilled(ImVec2(cx - u * 0.4f, cy), S(1.4f), col); d->AddCircleFilled(ImVec2(cx + u * 0.4f, cy), S(1.4f), col); break;
        }
    }

    // Card with a title header. Height is supplied explicitly to EndCard()
    // (bg drawn behind content via a draw-list channel split).
    struct CardCtx { ImVec2 start; float w; };
    CardCtx g_card;
    void BeginCard(const char* title, float x, float y, float w, int icon = -1)
    {
        g_card.start = ImVec2(x, y); g_card.w = w;
        DL()->ChannelsSplit(2); DL()->ChannelsSetCurrent(1);
        if (title && title[0])
        {
            float tx = x + S(16.f);
            if (icon >= 0) { SecIcon(icon, x + S(22.f), y + S(20.f), U32(g_accent)); tx = x + S(38.f); }
            Text(ImVec2(tx, y + S(14.f)), U32(TEXT), title);
        }
    }
    float CardBodyY() { return g_card.start.y + S(44.f); }
    void EndCard(float bottomY)
    {
        DL()->ChannelsSetCurrent(0);
        const ImVec2 a = g_card.start, b(g_card.start.x + g_card.w, bottomY + S(14.f));
        RectF(a, b, U32(CARD), S(8.f));
        Rect(a, b, U32(BORDER), S(8.f));
        DL()->ChannelsMerge();
    }

    // A labelled control row helper: draws label at (x,y), returns the x for the control on the right side of `rowW`.
    void Label(float x, float y, const char* t, ImU32 col) { Text(ImVec2(x, y), col, t); }

    // Filled button. `accent` fills with the theme accent; otherwise a frame tint
    // that lifts toward accent on hover. Returns true on click.
    bool Button(const char* id, float x, float y, float w, float h, const char* label, bool accent = false)
    {
        bool hov = false;
        const bool clk = Hit(id, ImVec2(x, y), ImVec2(w, h), hov);
        ImVec4 col = accent ? (hov ? g_accent : WithA(g_accent, 0.85f))
                            : (hov ? WithA(g_accent, 0.35f) : FRAME);
        RectF(ImVec2(x, y), ImVec2(x + w, y + h), U32(col), S(6.f));
        Text(ImVec2(x + (w - TW(label)) * 0.5f, y + (h - TH()) * 0.5f), U32(TEXT), label);
        return clk;
    }

    // ---------------- style + font ----------------
    void ApplyStyle()
    {
        ImGuiStyle& s = ImGui::GetStyle();
        s.WindowRounding = S(10.f); s.WindowBorderSize = 0.f; s.WindowPadding = ImVec2(0, 0);
        s.Colors[ImGuiCol_WindowBg] = BG0;
        s.Colors[ImGuiCol_Text] = TEXT;
    }
}

// forward page fns
namespace { void PageAimbot(float, float, float, float); void PageVisuals(float, float, float, float);
    void PageWorld(float, float, float, float); void PageSettings(float, float, float, float);
    void PageMisc(float, float, float, float);
    void PageInventory(float, float, float, float);
    void PageConfigs(float, float, float, float);
    void PageSimple(const char*, float, float, float, float); }

namespace Gui
{
    void MaybeRebuildFont()
    {
        int px = (int)(15.f * g_uiScale + 0.5f); if (px < 8) px = 8;
        if (px == g_builtFontPx) return;
        ImGuiIO& io = ImGui::GetIO();
        io.Fonts->Clear();
        ImFont* f = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", (float)px);
        if (!f) io.Fonts->AddFontDefault();
        io.Fonts->Build();
        ImGui_ImplDX11_InvalidateDeviceObjects();
        ImGui_ImplDX11_CreateDeviceObjects();
        g_builtFontPx = px; io.FontGlobalScale = 1.f;
    }
    void CenterWindow() { g_needCenter = true; }

    void Render(float alpha)
    {
        ApplyStyle();
        if (g_uiScale < 0.7f) g_uiScale = 0.7f; if (g_uiScale > 3.0f) g_uiScale = 3.0f;

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, S(10.f));
        const ImVec2 defSize(S(1380.f), S(820.f));
        ImGui::SetNextWindowSize(defSize, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(ImVec2(S(1080.f), S(680.f)), ImVec2(6000, 4000));
        if (g_needCenter)
        {
            const ImVec2 d = ImGui::GetIO().DisplaySize;
            ImGui::SetNextWindowPos(ImVec2((d.x - defSize.x) * 0.5f, (d.y - defSize.y) * 0.5f), ImGuiCond_Always);
            ImGui::SetNextWindowSize(defSize, ImGuiCond_Always);
            g_needCenter = false;
        }
        ImGui::Begin("NEXUS##root", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        const ImVec2 wp = ImGui::GetWindowPos();
        const ImVec2 ws = ImGui::GetWindowSize();
        RectF(wp, ImVec2(wp.x + ws.x, wp.y + ws.y), U32(BG0), S(12.f));

        const float sideW = S(190.f);
        const float pad = S(16.f);

        
// ============ SIDEBAR ============
        {
            const float sx = wp.x, sy = wp.y;
            RectF(ImVec2(sx, sy), ImVec2(sx + sideW, wp.y + ws.y), U32(BG1), S(10.f));
            RectF(ImVec2(sx + sideW - S(1.f), sy), ImVec2(sx + sideW, wp.y + ws.y), U32(BORDER));

            Text(ImVec2(sx + S(24.f), sy + S(22.f)), U32(TEXT), "neXus");
            Text(ImVec2(sx + S(24.f), sy + S(45.f)), U32(g_accent), "CS2");
            Text(ImVec2(sx + S(49.f), sy + S(45.f)), U32(DIM), "INTERNAL");

            const char* nav[] = { "Dashboard","Aimbot","Visuals","World","Movement","Inventory","Misc","Configs","Lua Scripts","Settings" };
            float ny = sy + S(88.f);
            for (int i = 0; i < 10; ++i)
            {
                char id[24]; std::snprintf(id, sizeof(id), "##nav%d", i);
                const float ih = S(42.f);
                const ImVec2 ip(sx + S(12.f), ny), isz(sideW - S(24.f), ih);
                bool hov = false;
                if (Hit(id, ip, isz, hov)) { g_nav = i; g_sub = 0; }
                const bool act = (g_nav == i);
                if (act || hov)
                    RectF(ip, ImVec2(ip.x + isz.x, ip.y + ih),
                          U32(act ? WithA(g_accent, 0.13f) : WithA(TEXT, 0.035f)), S(7.f));
                if (act)
                    RectF(ip, ImVec2(ip.x + S(3.f), ip.y + ih), U32(g_accent), S(2.f));

                const ImVec4 tc = act ? g_accent : (hov ? TEXT : DIM);
                NavIcon(i, ip.x + S(22.f), ny + ih * 0.5f, U32(tc));
                Text(ImVec2(ip.x + S(43.f), ny + (ih - TH()) * 0.5f), U32(tc), nav[i]);
                ny += S(48.f);
            }

            const float uy = wp.y + ws.y - S(74.f);
            RectF(ImVec2(sx + S(12.f), uy), ImVec2(sx + sideW - S(12.f), uy + S(58.f)), U32(CARD), S(7.f));
            Rect(ImVec2(sx + S(12.f), uy), ImVec2(sx + sideW - S(12.f), uy + S(58.f)), U32(BORDER), S(7.f));
            DL()->AddCircleFilled(ImVec2(sx + S(34.f), uy + S(29.f)), S(14.f), U32(WithA(g_accent, 0.16f)));
            Text(ImVec2(sx + S(57.f), uy + S(13.f)), U32(TEXT), "neXus User");
            Text(ImVec2(sx + S(57.f), uy + S(34.f)), U32(g_accent), "Premium");
        }

        
// ============ TOPBAR ============
        const float cx = wp.x + sideW;
        const float contentX = cx + pad;
        const float contentW = ws.x - sideW - pad * 2.f;
        const float topH = S(66.f);
        {
            ImGui::SetCursorScreenPos(ImVec2(cx, wp.y));
            ImGui::InvisibleButton("##drag", ImVec2(ws.x - sideW, topH));
            if (ImGui::IsItemActive()) {
                const ImVec2 delta = ImGui::GetIO().MouseDelta;
                ImGui::SetWindowPos(ImVec2(wp.x + delta.x, wp.y + delta.y));
            }

            Text(ImVec2(contentX, wp.y + S(24.f)), U32(DIM), "NEXUS / CS2");

            const float sw = S(118.f), sh = S(30.f);
            const float sx = wp.x + ws.x - pad - sw;
            const float sy = wp.y + S(18.f);
            RectF(ImVec2(sx,sy), ImVec2(sx+sw,sy+sh), U32(CARD), S(7.f));
            Rect(ImVec2(sx,sy), ImVec2(sx+sw,sy+sh), U32(BORDER), S(7.f));
            DL()->AddCircleFilled(ImVec2(sx+S(15.f), sy+sh*0.5f), S(4.f), U32(g_accent));
            Text(ImVec2(sx+S(27.f),sy+S(7.f)), U32(DIM), "Injected");
            Text(ImVec2(sx+sw-S(30.f),sy+S(7.f)), U32(g_accent), "CS2");

            RectF(ImVec2(cx, wp.y + topH - S(1.f)), ImVec2(wp.x + ws.x, wp.y + topH), U32(BORDER));
        }

        // ============ CONTENT ============
        const float cyTop = wp.y + topH + pad;
        const float cH = ws.y - topH - pad * 2.f;
        switch (g_nav)
        {
        case 1: PageAimbot(contentX, cyTop, contentW, cH); break;
        case 2: PageVisuals(contentX, cyTop, contentW, cH); break;
        case 3: PageWorld(contentX, cyTop, contentW, cH); break;
        case 5: PageInventory(contentX, cyTop, contentW, cH); break;
        case 6: PageMisc(contentX, cyTop, contentW, cH); break;
        case 7: PageConfigs(contentX, cyTop, contentW, cH); break;
        case 9: PageSettings(contentX, cyTop, contentW, cH); break;
        case 0: PageSimple("DASHBOARD", contentX, cyTop, contentW, cH); break;
        default: PageSimple(nullptr, contentX, cyTop, contentW, cH); break;
        }

        ImGui::End();
        ImGui::PopStyleVar(2);
    }
}

// =====================================================================
//  Pages
// =====================================================================
namespace
{
    // sub-tab column; returns nothing, sets g_sub
    void SubTabs(float x, float& y, const char* const* tabs, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            char id[24]; std::snprintf(id, sizeof(id), "##sub%d", i);
            const float h = S(34.f), w = S(120.f);
            bool hov = false;
            if (Hit(id, ImVec2(x, y), ImVec2(w, h), hov)) g_sub = i;
            const bool act = (g_sub == i);
            const float aa = Anim(ImGui::GetID(id), act ? 1.f : 0.f, 12.f);
            if (aa > 0.01f) RectF(ImVec2(x, y), ImVec2(x + w, y + h), U32(WithA(g_accent, 0.12f)), S(6.f));
            const ImVec4 tc = Lerp4(hov ? TEXT : DIM, g_accent, aa);
            Text(ImVec2(x + S(12.f), y + (h - TH()) * 0.5f), U32(tc), tabs[i]);
            y += h + S(2.f);
        }
    }

    void PageTitle(float x, float y, const char* t)
    {
        ImGui::PushFont(ImGui::GetFont());
        Text(ImVec2(x, y), U32(g_accent), t);
        ImGui::PopFont();
    }

    // A control row inside a card: label left, control positioned on the right half.
    // Returns the y for the next row.
    float RowToggle(const char* lbl, float x, float y, float w, bool* v)
    {
        Label(x, y + S(3.f), lbl, U32(TEXT));
        char id[64]; std::snprintf(id, sizeof(id), "##t_%s", lbl);
        Toggle(id, x + w - S(38.f), y, v);
        return y + S(30.f);
    }
    float RowSlider(const char* lbl, float x, float y, float w, float* v, float mn, float mx, const char* fmt, bool isInt = false)
    {
        Label(x, y, lbl, U32(DIM));
        char id[64]; std::snprintf(id, sizeof(id), "##s_%s", lbl);
        const float tw = w * 0.42f;
        Slider(id, x + w - tw - S(50.f), y, tw, v, mn, mx, fmt, isInt);
        return y + S(28.f);
    }
    float RowSliderI(const char* lbl, float x, float y, float w, int* v, int mn, int mx, const char* fmt)
    {
        float f = (float)*v; float ny = RowSlider(lbl, x, y, w, &f, (float)mn, (float)mx, fmt, true); *v = (int)(f + 0.5f); return ny;
    }
    float RowCombo(const char* lbl, float x, float y, float w, int* v, const char* const* items, int n)
    {
        Label(x, y + S(4.f), lbl, U32(DIM));
        char id[64]; std::snprintf(id, sizeof(id), "##c_%s", lbl);
        const float cw = w * 0.5f;
        Combo(id, x + w - cw, y, cw, v, items, n);
        return y + S(34.f);
    }
    float RowKey(const char* lbl, float x, float y, float w, int* v)
    {
        Label(x, y + S(4.f), lbl, U32(TEXT));
        char id[64]; std::snprintf(id, sizeof(id), "##k_%s", lbl);
        Keybind(id, x + w - S(90.f), y, S(90.f), v);
        return y + S(34.f);
    }

    // Small color swatch. `col` is float[4] RGBA (0..1). Clicking opens an
    // ImGui color-picker popup (with alpha). Draw-only; positions itself.
    void ColorSwatch(const char* id, float x, float y, float* col)
    {
        const float sw = S(26.f), sh = S(16.f);
        const float sYtop = y + S(2.f);
        // checker under the swatch so alpha is readable
        RectF(ImVec2(x, sYtop), ImVec2(x + sw, sYtop + sh), U32(RGBA(60, 60, 66)), S(4.f));
        RectF(ImVec2(x, sYtop), ImVec2(x + sw, sYtop + sh),
              ImGui::ColorConvertFloat4ToU32(ImVec4(col[0], col[1], col[2], col[3])), S(4.f));
        Rect(ImVec2(x, sYtop), ImVec2(x + sw, sYtop + sh), U32(BORDER), S(4.f));

        bool hov = false;
        if (Hit(id, ImVec2(x, y), ImVec2(sw, sh + S(4.f)), hov))
            ImGui::OpenPopup(id);

        ImGui::PushStyleColor(ImGuiCol_PopupBg, U32(BG1));
        ImGui::PushStyleColor(ImGuiCol_Border, U32(BORDER));
        ImGui::PushStyleColor(ImGuiCol_Text, U32(TEXT));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, U32(FRAME));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, U32(WithA(g_accent, 0.25f)));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, U32(WithA(g_accent, 0.35f)));
        if (ImGui::BeginPopup(id))
        {
            ImGui::ColorPicker4("##pick", col,
                ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoSidePreview |
                ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_DisplayHSV);
            ImGui::EndPopup();
        }
        ImGui::PopStyleColor(6);
    }

    // Standalone color row: label left, swatch at the right edge.
    float RowColor(const char* lbl, float x, float y, float w, float* col)
    {
        Label(x, y + S(3.f), lbl, U32(TEXT));
        char id[80]; std::snprintf(id, sizeof(id), "##csw_%s", lbl);
        ColorSwatch(id, x + w - S(26.f), y, col);
        return y + S(30.f);
    }

    // Toggle row with a color swatch just left of the toggle.
    float RowToggleC(const char* lbl, float x, float y, float w, bool* v, float* col)
    {
        Label(x, y + S(3.f), lbl, U32(TEXT));
        char cid[80]; std::snprintf(cid, sizeof(cid), "##csw_%s", lbl);
        ColorSwatch(cid, x + w - S(38.f) - S(34.f), y, col);
        char id[64]; std::snprintf(id, sizeof(id), "##t_%s", lbl);
        Toggle(id, x + w - S(38.f), y, v);
        return y + S(30.f);
    }

    // Styled single-line text box (ImGui InputText as the editor). Returns true
    // when the text changed this frame. Positions itself at (x,y).
    bool TextInput(const char* id, float x, float y, float w, char* buf, size_t bufsz, const char* hint)
    {
        ImGui::PushStyleColor(ImGuiCol_FrameBg, FRAME);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, WithA(g_accent, 0.15f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, WithA(g_accent, 0.20f));
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, S(5.f));
        ImGui::SetCursorScreenPos(ImVec2(x, y));
        ImGui::SetNextItemWidth(w);
        const bool changed = ImGui::InputTextWithHint(id, hint, buf, bufsz);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);
        return changed;
    }

    // Vertical scroll region: clips to (x,y,w,h) and applies a wheel-driven
    // offset (stored by the caller). Returns the y-origin to draw the first row
    // at (already shifted by -*scroll); the caller draws rows and must skip any
    // that fall outside [y, y+h] for correct clipping and hit-testing.
    float BeginScroll(float x, float y, float w, float h, float* scroll, float contentH)
    {
        const ImVec2 mn(x, y), mx(x + w, y + h);
        const float maxS = contentH > h ? contentH - h : 0.f;
        if (ImGui::IsMouseHoveringRect(mn, mx))
        {
            const float wheel = ImGui::GetIO().MouseWheel;
            if (wheel != 0.f) *scroll -= wheel * S(48.f);
        }
        if (*scroll < 0.f) *scroll = 0.f;
        if (*scroll > maxS) *scroll = maxS;
        ImGui::PushClipRect(mn, mx, true);
        // subtle scrollbar
        if (maxS > 0.f)
        {
            const float trackX = x + w - S(4.f);
            RectF(ImVec2(trackX, y), ImVec2(trackX + S(3.f), y + h), U32(WithA(FRAME, 0.6f)), S(2.f));
            const float thumbH = h * (h / contentH);
            const float thumbY = y + (h - thumbH) * (*scroll / maxS);
            RectF(ImVec2(trackX, thumbY), ImVec2(trackX + S(3.f), thumbY + thumbH), U32(WithA(g_accent, 0.7f)), S(2.f));
        }
        return y - *scroll;
    }
    void EndScroll() { ImGui::PopClipRect(); }

    // ---- Anatomical human hitbox: SVG silhouette texture + rect hit regions ----

    struct BodyTexture
    {
        ID3D11ShaderResourceView* srv = nullptr;
        int w = 0;
        int h = 0;
    };

    std::vector<unsigned char> DecodeBase64(const char* s)
    {
        static const signed char lut[256] = {
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
            52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-2,-1,-1,
            -1,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,
            15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
            -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
            41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
        };

        std::vector<unsigned char> out;
        if (!s) return out;
        unsigned int val = 0;
        int valb = -8;
        for (const unsigned char* p = reinterpret_cast<const unsigned char*>(s); *p; ++p)
        {
            const signed char c = lut[*p];
            if (c == -1) continue;
            if (c == -2) break;
            val = (val << 6) | static_cast<unsigned int>(c);
            valb += 6;
            if (valb >= 0)
            {
                out.push_back(static_cast<unsigned char>((val >> valb) & 0xFF));
                valb -= 8;
            }
        }
        return out;
    }

    bool LoadPngTextureFromBase64(const char* encoded, BodyTexture& out)
    {
        if (out.srv) return true;
        if (!g_pDevice || !encoded) return false;

        const std::vector<unsigned char> bytes = DecodeBase64(encoded);
        if (bytes.empty() || bytes.size() > 0xFFFFFFFFull) return false;

        HRESULT co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        (void)co;

        IWICImagingFactory* factory = nullptr;
        IWICStream* stream = nullptr;
        IWICBitmapDecoder* decoder = nullptr;
        IWICBitmapFrameDecode* frame = nullptr;
        IWICFormatConverter* converter = nullptr;
        ID3D11Texture2D* texture = nullptr;

        auto cleanup = [&]() {
            if (texture) texture->Release();
            if (converter) converter->Release();
            if (frame) frame->Release();
            if (decoder) decoder->Release();
            if (stream) stream->Release();
            if (factory) factory->Release();
        };

        HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                      IID_PPV_ARGS(&factory));
        if (FAILED(hr)) { cleanup(); return false; }

        hr = factory->CreateStream(&stream);
        if (FAILED(hr)) { cleanup(); return false; }

        hr = stream->InitializeFromMemory(
            const_cast<BYTE*>(reinterpret_cast<const BYTE*>(bytes.data())),
            static_cast<DWORD>(bytes.size()));
        if (FAILED(hr)) { cleanup(); return false; }

        hr = factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder);
        if (FAILED(hr)) { cleanup(); return false; }

        hr = decoder->GetFrame(0, &frame);
        if (FAILED(hr)) { cleanup(); return false; }

        UINT width = 0, height = 0;
        hr = frame->GetSize(&width, &height);
        if (FAILED(hr) || !width || !height) { cleanup(); return false; }

        hr = factory->CreateFormatConverter(&converter);
        if (FAILED(hr)) { cleanup(); return false; }

        hr = converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA,
                                   WICBitmapDitherTypeNone, nullptr, 0.0,
                                   WICBitmapPaletteTypeCustom);
        if (FAILED(hr)) { cleanup(); return false; }

        const UINT stride = width * 4;
        std::vector<unsigned char> pixels(static_cast<size_t>(stride) * height);
        hr = converter->CopyPixels(nullptr, stride, static_cast<UINT>(pixels.size()), pixels.data());
        if (FAILED(hr)) { cleanup(); return false; }

        D3D11_TEXTURE2D_DESC td{};
        td.Width = width;
        td.Height = height;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_IMMUTABLE;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA sd{};
        sd.pSysMem = pixels.data();
        sd.SysMemPitch = stride;

        hr = g_pDevice->CreateTexture2D(&td, &sd, &texture);
        if (FAILED(hr)) { cleanup(); return false; }

        D3D11_SHADER_RESOURCE_VIEW_DESC vd{};
        vd.Format = td.Format;
        vd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        vd.Texture2D.MipLevels = 1;

        hr = g_pDevice->CreateShaderResourceView(texture, &vd, &out.srv);
        if (FAILED(hr)) { cleanup(); return false; }

        out.w = static_cast<int>(width);
        out.h = static_cast<int>(height);
        cleanup();
        return true;
    }

    bool PointInBodyPoly(float px, float py, const ImVec2* poly, int count)
    {
        bool inside = false;
        for (int i = 0, j = count - 1; i < count; j = i++)
        {
            const ImVec2& a = poly[i];
            const ImVec2& b = poly[j];
            const bool cross = ((a.y > py) != (b.y > py)) &&
                (px < (b.x - a.x) * (py - a.y) / ((b.y - a.y) + 0.00001f) + a.x);
            if (cross) inside = !inside;
        }
        return inside;
    }

    int BodyRegionAt(float px, float py)
    {
        // Coordinates are in the original 1024x1535 image.
        const float hx = (px - 512.f) / 82.f;
        const float hy = (py - 125.f) / 107.f;
        if (hx * hx + hy * hy <= 1.f) return 0;

        static const ImVec2 neck[] = {
            {455,205},{570,205},{572,290},{635,310},{610,335},{512,320},{414,335},{389,310},{452,290}
        };
        static const ImVec2 chest[] = {
            {292,286},{390,282},{455,290},{512,306},{570,290},{635,282},{733,300},
            {716,392},{650,442},{585,452},{512,429},{439,452},{374,442},{308,392}
        };
        static const ImVec2 stomach[] = {
            {390,430},{438,451},{512,430},{586,451},{635,430},{626,655},{398,655}
        };
        static const ImVec2 pelvis[] = {
            {398,650},{626,650},{615,770},{575,802},{512,790},{449,802},{409,770}
        };
        static const ImVec2 larm[] = {
            {292,295},{370,315},{360,410},{326,478},{298,565},{273,656},{234,733},{224,797},
            {255,861},{225,900},{177,887},{168,814},{197,740},{237,640},{260,548},{273,450},{284,360}
        };
        static const ImVec2 rarm[] = {
            {732,295},{654,315},{664,410},{698,478},{726,565},{751,656},{790,733},{800,797},
            {769,861},{799,900},{847,887},{856,814},{827,740},{787,640},{764,548},{751,450},{740,360}
        };
        static const ImVec2 lleg[] = {
            {405,758},{512,785},{507,935},{488,1060},{455,1180},{432,1365},{414,1450},
            {350,1482},{294,1468},{344,1365},{350,1210},{350,1075},{349,945},{360,830}
        };
        static const ImVec2 rleg[] = {
            {512,785},{619,758},{664,830},{675,945},{674,1075},{674,1210},{680,1365},
            {730,1468},{674,1482},{610,1450},{592,1365},{569,1180},{536,1060},{517,935}
        };

        if (PointInBodyPoly(px, py, neck, IM_ARRAYSIZE(neck))) return 1;
        if (PointInBodyPoly(px, py, larm, IM_ARRAYSIZE(larm))) return 5;
        if (PointInBodyPoly(px, py, rarm, IM_ARRAYSIZE(rarm))) return 6;
        if (PointInBodyPoly(px, py, chest, IM_ARRAYSIZE(chest))) return 2;
        if (PointInBodyPoly(px, py, stomach, IM_ARRAYSIZE(stomach))) return 3;
        if (PointInBodyPoly(px, py, pelvis, IM_ARRAYSIZE(pelvis))) return 4;
        if (PointInBodyPoly(px, py, lleg, IM_ARRAYSIZE(lleg))) return 7;
        if (PointInBodyPoly(px, py, rleg, IM_ARRAYSIZE(rleg))) return 8;
        return -1;
    }

    void DrawHumanHitbox(float ox, float oy, float bw, float bh, int* sel)
    {
        static BodyTexture base;
        static BodyTexture region[9];
        static bool attempted = false;

        if (!attempted)
        {
            LoadPngTextureFromBase64(NexusBodyAsset::kBasePngB64, base);
            const char* overlays[9] = {
                NexusBodyAsset::kHeadPngB64,
                NexusBodyAsset::kNeckPngB64,
                NexusBodyAsset::kChestPngB64,
                NexusBodyAsset::kStomachPngB64,
                NexusBodyAsset::kPelvisPngB64,
                NexusBodyAsset::kLeftArmPngB64,
                NexusBodyAsset::kRightArmPngB64,
                NexusBodyAsset::kLeftLegPngB64,
                NexusBodyAsset::kRightLegPngB64
            };
            for (int i = 0; i < 9; ++i)
                LoadPngTextureFromBase64(overlays[i], region[i]);
            attempted = true;
        }

        const float aspect = static_cast<float>(NexusBodyAsset::kWidth) /
                             static_cast<float>(NexusBodyAsset::kHeight);
        float drawH = bh;
        float drawW = drawH * aspect;
        if (drawW > bw)
        {
            drawW = bw;
            drawH = drawW / aspect;
        }

        // UI fit: keep the exact body asset but render it 20% smaller.
        drawW *= 0.80f;
        drawH *= 0.80f;

        const ImVec2 a(ox + (bw - drawW) * 0.5f, oy + (bh - drawH) * 0.5f);
        const ImVec2 b(a.x + drawW, a.y + drawH);

        if (base.srv)
            DL()->AddImage((ImTextureID)base.srv, a, b);
        else
        {
            RectF(a, b, U32(RGBA(10, 10, 12)), S(6.f));
            Text(ImVec2(a.x + S(12.f), a.y + S(12.f)), U32(DIM), "Body asset failed to load");
        }

        const ImVec2 mp = ImGui::GetIO().MousePos;
        int hovered = -1;
        const bool inImage = mp.x >= a.x && mp.x <= b.x && mp.y >= a.y && mp.y <= b.y;
        if (inImage)
        {
            const float px = (mp.x - a.x) / drawW * NexusBodyAsset::kWidth;
            const float py = (mp.y - a.y) / drawH * NexusBodyAsset::kHeight;
            hovered = BodyRegionAt(px, py);
        }

        ImGui::SetCursorScreenPos(a);
        ImGui::InvisibleButton("##exact_body_hitbox", ImVec2(drawW, drawH));
        if (ImGui::IsItemClicked() && hovered >= 0)
            *sel = hovered;

        if (hovered >= 0 && hovered < 9 && hovered != *sel && region[hovered].srv)
            DL()->AddImage((ImTextureID)region[hovered].srv, a, b,
                           ImVec2(0,0), ImVec2(1,1), IM_COL32(255,255,255,115));

        if (*sel >= 0 && *sel < 9 && region[*sel].srv)
            DL()->AddImage((ImTextureID)region[*sel].srv, a, b);

        const char* names[] = {
            "HEAD","NECK","CHEST","STOMACH","PELVIS","L. ARM","R. ARM","L. LEG","R. LEG"
        };
        if (*sel >= 0 && *sel < 9)
        {
            const char* nm = names[*sel];
            Text(ImVec2(ox + (bw - TW(nm)) * 0.5f, oy + bh + S(2.f)), U32(g_accent), nm);
        }
    }

    // Fake-yaw radar (Anti-Aim card): a circle with a highlighted cone + a
    // secondary "real" indicator to the side.
    void FakeYawGraphic(float cx, float cy, float r, const ImVec4& col)
    {
        ImDrawList* d = DL();
        d->AddCircle(ImVec2(cx, cy), r, U32(WithA(col, 0.5f)), 40, S(1.4f));
        // cone (down-facing, ~60deg)
        const float a0 = 1.05f, a1 = 2.09f;
        d->PathLineTo(ImVec2(cx, cy));
        for (float a = a0; a <= a1; a += 0.1f) d->PathLineTo(ImVec2(cx + cosf(a) * r, cy + sinf(a) * r));
        d->PathFillConvex(U32(WithA(col, 0.30f)));
        d->AddLine(ImVec2(cx, cy), ImVec2(cx, cy - r), U32(col), S(2.f));
        d->AddCircleFilled(ImVec2(cx, cy), S(3.f), U32(col));
        // secondary indicator
        d->AddCircle(ImVec2(cx + r * 2.1f, cy), r * 0.75f, U32(WithA(DIM, 0.7f)), 32, S(1.2f));
        d->AddCircleFilled(ImVec2(cx + r * 2.1f, cy), S(2.5f), U32(DIM));
        d->AddLine(ImVec2(cx + r * 1.15f, cy), ImVec2(cx + r * 1.55f, cy), U32(DIM), S(1.2f));
    }

    // Bottom keybinds overview strip.
    void KeybindsStrip(float x, float y, float w)
    {
        BeginCard("KEYBINDS OVERVIEW", x, y, w, 5);
        struct KB { const char* name; int* key; };
        static int menuKey = 0x2D; // INSERT (display only)
        KB items[] = {
            { "Aimbot", &Esp::g_aimbot.aimKey }, { "Triggerbot", &Esp::g_trigger.key },
            { "Bhop", &Esp::g_movement.bhopKey }, { "Fake Duck", &Esp::g_movement.fakeDuckKey },
            { "Slow Walk", &Esp::g_movement.slowWalkKey }, { "Menu", &menuKey },
        };
        const float rowY = y + S(50.f);
        const float cellW = (w - S(32.f)) / 6.f;
        for (int i = 0; i < 6; ++i)
        {
            const float cx = x + S(16.f) + i * cellW;
            Label(cx, rowY, items[i].name, U32(DIM));
            char id[24]; std::snprintf(id, sizeof(id), "##kbo%d", i);
            Keybind(id, cx, rowY + S(20.f), cellW - S(16.f), items[i].key);
        }
        EndCard(rowY + S(50.f));
    }

    // ---------------- AIMBOT ----------------
    
    void PageAimbot(float x, float y, float w, float h)
    {
        auto& a = Esp::g_aimbot;
        auto& t = Esp::g_trigger;

        PageTitle(x, y, "AIMBOT");
        Text(ImVec2(x + S(30.f), y + S(22.f)), U32(DIM), "Configure aim assistance and targeting preferences.");

        const float bodyY = y + S(58.f);
        const float gap = S(14.f);
        const float availableH = h - S(58.f);

        const float leftW = (std::max)(S(390.f), w * 0.41f);
        const float rightX = x + leftW + gap;
        const float rightW = w - leftW - gap;

        BeginCard("HITBOX", x, bodyY, leftW, 3);
        {
            const float ix = x + S(16.f);
            Text(ImVec2(ix, CardBodyY()), U32(g_accent), "SELECTED");
            const char* names[] = { "Head","Neck","Chest","Stomach","Pelvis","Left arm","Right arm","Left leg","Right leg" };
            const char* nm = (a.hitbox >= 0 && a.hitbox < 9) ? names[a.hitbox] : "None";
            Text(ImVec2(ix, CardBodyY() + S(20.f)), U32(TEXT), nm);
            Text(ImVec2(ix, CardBodyY() + S(42.f)), U32(DIM), "Click a body region");

            const float figureTop = bodyY + S(86.f);
            const float figureBottom = bodyY + availableH - S(58.f);
            const float figureH = figureBottom - figureTop;
            const float figureW = leftW - S(116.f);
            DrawHumanHitbox(x + S(94.f), figureTop, figureW, figureH, &a.hitbox);

            const float qy = bodyY + availableH - S(48.f);
            const float qx = x + S(16.f);
            const float qgap = S(5.f);
            const float qw = (leftW - S(32.f) - qgap * 4.f) / 5.f;
            const char* qn[] = { "Head","Neck","Chest","Stomach","Pelvis" };
            for (int i=0;i<5;++i) {
                char id[24]; std::snprintf(id,sizeof(id),"##hbq%d",i);
                if (Button(id, qx + i*(qw+qgap), qy, qw, S(30.f), qn[i], a.hitbox == i))
                    a.hitbox = i;
            }
        }
        EndCard(bodyY + availableH - S(14.f));

        const float colGap = S(14.f);
        const float colW = (rightW - colGap) * 0.5f;
        const float topH = S(286.f);
        const float row2 = bodyY + topH + gap;
        const float bottomH = availableH - topH - gap;

        BeginCard("TARGET SELECTION", rightX, bodyY, colW, 1);
        {
            float ry = CardBodyY();
            const float ix = rightX + S(16.f), iw = colW - S(32.f);
            const char* selection[] = { "FOV","Distance","Health" };
            ry = RowCombo("Selection", ix, ry, iw, &a.selection, selection, 3);
            const char* aimTypes[] = { "Hold","Toggle","Always" };
            ry = RowCombo("Aim Type", ix, ry, iw, &a.aimType, aimTypes, 3);
            ry = RowKey("Aim Key", ix, ry, iw, &a.aimKey);
            ry = RowToggle("Enabled##aim", ix, ry, iw, &a.enable);
            ry = RowToggle("Silent", ix, ry, iw, &a.silent);
            ry = RowToggle("Prefer Body", ix, ry, iw, &a.preferBody);
        }
        EndCard(bodyY + topH - S(14.f));

        BeginCard("ACCURACY", rightX + colW + colGap, bodyY, colW, 2);
        {
            float ry = CardBodyY();
            const float ix = rightX + colW + colGap + S(16.f), iw = colW - S(32.f);
            ry = RowSlider("FOV", ix, ry, iw, &a.fov, 0.f, 30.f, "%.1f");
            ry = RowSlider("Smooth", ix, ry, iw, &a.smooth, 0.f, 1.f, "%.2f");
            ry = RowSliderI("Hit Chance", ix, ry, iw, &a.hitChance, 0, 100, "%d%%");
            ry = RowSliderI("Minimum Damage", ix, ry, iw, &a.minDamage, 0, 100, "%d");
            ry = RowToggle("Multipoint", ix, ry, iw, &a.multipoint);
        }
        EndCard(bodyY + topH - S(14.f));

        BeginCard("AIM / RCS", rightX, row2, colW, 1);
        {
            float ry = CardBodyY();
            const float ix = rightX + S(16.f), iw = colW - S(32.f);
            ry = RowToggle("Draw FOV", ix, ry, iw, &a.drawFov);
            ry = RowToggle("Recoil Control", ix, ry, iw, &a.rcs);
            ry = RowSliderI("RCS X", ix, ry, iw, &a.rcsX, 0, 100, "%d%%");
            ry = RowSliderI("RCS Y", ix, ry, iw, &a.rcsY, 0, 100, "%d%%");
            ry = RowToggle("Auto Stop", ix, ry, iw, &a.autoStop);
            ry = RowToggle("Safe Points", ix, ry, iw, &a.safePoints);
        }
        EndCard(row2 + bottomH - S(14.f));

        BeginCard("TRIGGERBOT", rightX + colW + colGap, row2, colW, 1);
        {
            float ry = CardBodyY();
            const float ix = rightX + colW + colGap + S(16.f), iw = colW - S(32.f);
            ry = RowToggle("Enabled##trig", ix, ry, iw, &t.enable);
            ry = RowToggle("Team Check", ix, ry, iw, &t.teamCheck);
            ry = RowKey("Trigger Key", ix, ry, iw, &t.key);
            ry = RowSliderI("Delay (ms)", ix, ry, iw, &t.delayMs, 0, 250, "%d");
            ry = RowSlider("Hitbox Scale", ix, ry, iw, &a.hitboxScale, 0.25f, 1.0f, "%.2f");
        }
        EndCard(row2 + bottomH - S(14.f));
    }

    // ---------------- VISUALS (our real ESP/chams/glow) ----------------
    void PageVisuals(float x, float y, float w, float h)
    {
        auto& c = Esp::g_config;
        PageTitle(x, y, "VISUALS");
        const float bodyY = y + S(30.f);
        const char* subs[] = { "Player","Chams","World","Misc" };
        float sy = bodyY + S(6.f);
        SubTabs(x, sy, subs, 4);
        const float gx = x + S(140.f), gw = w - S(140.f), gap = S(16.f);
        const float colW = (gw - gap) * 0.5f, c1 = gx, c2 = gx + colW + gap;

        if (g_sub == 0) // Player
        {
            BeginCard("Box & Skeleton", c1, bodyY, colW);
            float ry = CardBodyY(); const float ix = c1 + S(16.f), iw = colW - S(32.f);
            ry = RowToggleC("Box", ix, ry, iw, &c.box, c.boxColor);
            { const char* t[] = { "Full","Corner","3D" }; ry = RowCombo("Box type", ix, ry, iw, &c.boxType, t, 3); }
            ry = RowSlider("Thickness", ix, ry, iw, &c.boxThickness, 0.5f, 4.f, "%.1f");
            ry = RowToggle("Visibility color", ix, ry, iw, &c.visColor);
            if (c.visColor)
            {
                ry = RowColor("  Visible", ix, ry, iw, c.visibleColor);
                ry = RowColor("  Occluded", ix, ry, iw, c.occludedColor);
                ry = RowToggle("Real LOS", ix, ry, iw, &c.realVis);
            }
            ry = RowToggle("Filled", ix, ry, iw, &c.boxFill);
            ry = RowToggleC("Skeleton", ix, ry, iw, &c.skeleton, c.skeletonColor);
            EndCard(ry);

            BeginCard("Info", c2, bodyY, colW);
            float ry2 = CardBodyY(); const float ix2 = c2 + S(16.f), iw2 = colW - S(32.f);
            ry2 = RowToggle("Name", ix2, ry2, iw2, &c.name);
            ry2 = RowToggle("Health bar", ix2, ry2, iw2, &c.healthBar);
            ry2 = RowToggle("Armor bar", ix2, ry2, iw2, &c.showArmor);
            ry2 = RowToggleC("Ammo bar", ix2, ry2, iw2, &c.ammoBar, c.ammoColor);
            ry2 = RowToggle("Weapon", ix2, ry2, iw2, &c.weapon);
            { const char* t[] = { "Text","Icon","Both" }; ry2 = RowCombo("W.display", ix2, ry2, iw2, &c.weaponDisplay, t, 3); }
            ry2 = RowToggle("Flags", ix2, ry2, iw2, &c.flags);
            ry2 = RowToggle("Ping", ix2, ry2, iw2, &c.showPing);
            ry2 = RowToggle("Distance", ix2, ry2, iw2, &c.distance);
            EndCard(ry2);
        }
        else if (g_sub == 1) // Chams
        {
            BeginCard("Glow", c1, bodyY, colW);
            float ry = CardBodyY(); const float ix = c1 + S(16.f), iw = colW - S(32.f);
            ry = RowToggleC("Enemy glow", ix, ry, iw, &c.glow, c.glowColor);
            ry = RowToggleC("Team glow", ix, ry, iw, &c.glowTeam, c.glowTeamColor);
            ry = RowToggleC("Local glow", ix, ry, iw, &c.glowLocal, c.glowLocalColor);
            EndCard(ry);

            BeginCard("Chams", c2, bodyY, colW);
            float ry2 = CardBodyY(); const float ix2 = c2 + S(16.f), iw2 = colW - S(32.f);
            { const char* t[] = { "Flat","Illuminate","Glow","Matte","Outline","Hologram","Metallic","Liquid","Bloom","Distortion","Pearl" }; ry2 = RowCombo("Material", ix2, ry2, iw2, &c.chamsType, t, 11); }
            ry2 = RowToggleC("Enemy visible", ix2, ry2, iw2, &c.chams, c.chamsColor);
            ry2 = RowToggleC("Enemy XQZ", ix2, ry2, iw2, &c.chamsXqz, c.chamsXqzColor);
            ry2 = RowToggleC("Team", ix2, ry2, iw2, &c.chamsTeam, c.chamsTeamColor);
            ry2 = RowToggleC("Local", ix2, ry2, iw2, &c.chamsLocal, c.chamsLocalColor);
            ry2 = RowToggleC("Ragdoll", ix2, ry2, iw2, &c.ragdollChams, c.ragdollChamsColor);
            EndCard(ry2);
        }
        else if (g_sub == 2) // World
        {
            BeginCard("Extras", c1, bodyY, colW);
            float ry = CardBodyY(); const float ix = c1 + S(16.f), iw = colW - S(32.f);
            ry = RowToggleC("Head circle", ix, ry, iw, &c.headCircle, c.headCircleColor);
            ry = RowToggleC("Snapline", ix, ry, iw, &c.snapline, c.snaplineColor);
            ry = RowToggleC("Off-screen arrows", ix, ry, iw, &c.offArrows, c.offArrowColor);
            ry = RowToggleC("Sound ESP", ix, ry, iw, &c.soundEsp, c.soundColor);
            ry = RowToggle("Team ESP", ix, ry, iw, &c.teamEsp);
            EndCard(ry);
        }
        else // Misc
        {
            BeginCard("Master", c1, bodyY, colW);
            float ry = CardBodyY(); const float ix = c1 + S(16.f), iw = colW - S(32.f);
            ry = RowToggle("ESP enabled", ix, ry, iw, &c.enabled);
            EndCard(ry);
        }
    }

    // ---------------- WORLD ----------------
    void PageWorld(float x, float y, float w, float h)
    {
        auto& c = Esp::g_config;
        PageTitle(x, y, "WORLD");
        const float bodyY = y + S(30.f);
        const float gx = x + S(20.f), gw = w - S(20.f), gap = S(16.f);
        const float colW = (gw - gap) * 0.5f, c1 = gx, c2 = gx + colW + gap;

        BeginCard("Bomb & Grenades", c1, bodyY, colW);
        float ry = CardBodyY(); const float ix = c1 + S(16.f), iw = colW - S(32.f);
        ry = RowToggleC("Bomb ESP", ix, ry, iw, &c.bombEsp, c.bombColor);
        ry = RowToggleC("Grenade ESP", ix, ry, iw, &c.nadeEsp, c.nadeColor);
        ry = RowToggle("Grenade trajectory", ix, ry, iw, &c.nadeTrajectory);
        ry = RowToggleC("Throw predictor", ix, ry, iw, &c.nadeThrow, c.nadeThrowColor);
        ry = RowToggleC("Inferno fill", ix, ry, iw, &c.infernoFill, c.infernoColor);
        ry = RowToggleC("Spectator list", ix, ry, iw, &c.specList, c.specColor);
        ry = RowToggleC("Ragdoll ESP", ix, ry, iw, &c.ragdollEsp, c.ragdollColor);
        EndCard(ry);

        BeginCard("Item ESP", c2, bodyY, colW);
        float ry2 = CardBodyY(); const float ix2 = c2 + S(16.f), iw2 = colW - S(32.f);
        ry2 = RowToggleC("Item names", ix2, ry2, iw2, &c.itemEsp, c.itemColor);
        ry2 = RowToggle("Item icons", ix2, ry2, iw2, &c.itemIcon);
        ry2 = RowToggleC("Item glow", ix2, ry2, iw2, &c.itemGlow, c.itemGlowColor);
        ry2 = RowToggleC("Item chams", ix2, ry2, iw2, &c.itemChams, c.itemChamsColor);
        EndCard(ry2);
    }

    // ---------------- MISC ----------------
    void PageMisc(float x, float y, float w, float h)
    {
        auto& c = Esp::g_config;
        PageTitle(x, y, "MISC");
        Text(ImVec2(x + S(30.f), y + S(22.f)), U32(DIM), "Quality-of-life tweaks.");
        const float bodyY = y + S(58.f);
        const float gap = S(16.f);
        const float colW = (w - gap * 2.f) / 3.f;
        const float c1 = x;
        const float c2 = x + colW + gap;
        const float c3 = x + (colW + gap) * 2.f;

        BeginCard("VISUALS", c1, bodyY, colW, 3);
        float ry = CardBodyY(); const float ix = c1 + S(16.f), iw = colW - S(32.f);
        ry = RowToggle("Anti-flash", ix, ry, iw, &c.antiFlash);
        ry = RowToggle("FOV changer", ix, ry, iw, &c.fovChanger);
        if (c.fovChanger) ry = RowSliderI("FOV", ix, ry, iw, &c.fovValue, 60, 160, "%d");
        ry = RowToggle("Local opacity", ix, ry, iw, &c.localOpacity);
        if (c.localOpacity)
        {
            ry = RowSlider("Opacity", ix, ry, iw, &c.localOpacityVal, 0.f, 1.f, "%.2f");
            ry = RowToggle("Only scoped", ix, ry, iw, &c.localOnlyScoped);
        }
        EndCard(ry);

        BeginCard("VELOCITY BAR", c2, bodyY, colW, 2);
        float ry2 = CardBodyY(); const float ix2 = c2 + S(16.f), iw2 = colW - S(32.f);
        ry2 = RowToggle("Enable##vel", ix2, ry2, iw2, &c.velBar);
        if (c.velBar)
        {
            { const char* t[] = { "Bottom","Top" }; ry2 = RowCombo("Position", ix2, ry2, iw2, &c.velBarPos, t, 2); }
            ry2 = RowToggleC("Gradient", ix2, ry2, iw2, &c.velGradient, c.velColor);
            if (c.velGradient) ry2 = RowColor("  High-speed color", ix2, ry2, iw2, c.velColor2);
            else               ry2 = RowColor("  Bar color", ix2, ry2, iw2, c.velColor);
            ry2 = RowToggle("Glow", ix2, ry2, iw2, &c.velGlow);
            ry2 = RowToggle("Show speed", ix2, ry2, iw2, &c.velText);
            ry2 = RowSliderI("Max speed", ix2, ry2, iw2, &c.velMax, 100, 1000, "%d");
        }
        ry2 = RowToggle("Speed graph", ix2, ry2, iw2, &c.velGraph);
        EndCard(ry2);

        BeginCard("HUD", c3, bodyY, colW, 5);
        float ry3 = CardBodyY(); const float ix3 = c3 + S(16.f), iw3 = colW - S(32.f);
        ry3 = RowToggleC("Watermark", ix3, ry3, iw3, &c.watermark, c.watermarkColor);
        ry3 = RowToggleC("Hit marker", ix3, ry3, iw3, &c.hitMarker, c.hitMarkerColor);
        ry3 = RowToggleC("Damage numbers", ix3, ry3, iw3, &c.damageNumbers, c.damageColor);
        ry3 = RowToggleC("Bullet tracer", ix3, ry3, iw3, &c.bulletTracer, c.tracerColor);
        ry3 = RowToggleC("Crosshair", ix3, ry3, iw3, &c.crosshair, c.crosshairColor);
        if (c.crosshair)
        {
            ry3 = RowToggle("  Center dot", ix3, ry3, iw3, &c.crosshairDot);
            ry3 = RowSlider("  Size", ix3, ry3, iw3, &c.crosshairSize, 1.f, 20.f, "%.0f");
            ry3 = RowSlider("  Gap", ix3, ry3, iw3, &c.crosshairGap, 0.f, 12.f, "%.0f");
            ry3 = RowSlider("  Thickness", ix3, ry3, iw3, &c.crosshairThickness, 1.f, 4.f, "%.1f");
        }
        EndCard(ry3);
    }

    // ---------------- INVENTORY (SKINS) ----------------
    void PageInventory(float x, float y, float w, float h)
    {
        PageTitle(x, y, "INVENTORY");
        Text(ImVec2(x + S(30.f), y + S(22.f)), U32(DIM), "Knife / glove / weapon skin changer (nerv engine).");

        const float bodyY = y + S(48.f);
        ImGui::SetCursorScreenPos(ImVec2(x, bodyY));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, U32(CARD));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, U32(FRAME));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, U32(WithA(g_accent, 0.25f)));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, U32(WithA(g_accent, 0.35f)));
        ImGui::PushStyleColor(ImGuiCol_CheckMark, U32(g_accent));
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, U32(g_accent));
        ImGui::PushStyleColor(ImGuiCol_Button, U32(FRAME));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, U32(WithA(g_accent, 0.35f)));
        ImGui::PushStyleColor(ImGuiCol_Header, U32(WithA(g_accent, 0.25f)));
        ImGui::PushStyleColor(ImGuiCol_Text, U32(TEXT));
        ImGui::BeginChild("##nerv_skins", ImVec2(w, h - S(48.f)), true);
        ImGui::PushItemWidth(S(260.f));
        nerv_bridge::draw_skins_ui();
        ImGui::PopItemWidth();
        ImGui::EndChild();
        ImGui::PopStyleColor(10);
    }

    // ---------------- CONFIGS ----------------
    void PageConfigs(float x, float y, float w, float h)
    {
        PageTitle(x, y, "CONFIGS");
        Text(ImVec2(x + S(30.f), y + S(22.f)), U32(DIM), "Save and load your settings.");

        static char  nameBuf[64] = "";
        static std::vector<std::string> list = gui_config::List();
        static int   sel = -1;
        static char  msg[96] = "";
        static float msgT = 0.f;
        static ImVec4 msgCol = GREEN;
        auto notify = [&](const char* m, ImVec4 c) { std::snprintf(msg, sizeof(msg), "%s", m); msgCol = c; msgT = 2.5f; };
        const ImVec4 BAD = RGBA(230, 120, 120);

        const float bodyY = y + S(58.f);
        const float gap = S(16.f);
        const float leftW = w * 0.52f - gap * 0.5f;
        const float rightW = w - leftW - gap;

        // ---- left: manage ----
        BeginCard("MANAGE", x, bodyY, leftW);
        float ry = CardBodyY();
        const float ix = x + S(16.f), iw = leftW - S(32.f);

        Label(ix, ry, "Config name", U32(DIM));
        ry += S(22.f);
        {
            ImGui::PushStyleColor(ImGuiCol_FrameBg, FRAME);
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, FRAME);
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, FRAME);
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, S(5.f));
            ImGui::SetCursorScreenPos(ImVec2(ix, ry));
            ImGui::SetNextItemWidth(iw);
            ImGui::InputTextWithHint("##cfgname", "e.g. legit", nameBuf, sizeof(nameBuf));
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(4);
        }
        ry += S(40.f);

        const float bw = (iw - gap) * 0.5f, bh = S(30.f);
        if (Button("##cfgsave", ix, ry, bw, bh, "Save", true))
        {
            if (nameBuf[0] && gui_config::Save(nameBuf)) { list = gui_config::List(); notify("Config saved.", GREEN); }
            else notify(nameBuf[0] ? "Save failed." : "Enter a name first.", BAD);
        }
        if (Button("##cfgload", ix + bw + gap, ry, bw, bh, "Load", false))
        {
            if (nameBuf[0] && gui_config::Load(nameBuf)) notify("Config loaded.", GREEN);
            else notify(nameBuf[0] ? "Config not found." : "Enter a name first.", BAD);
        }
        ry += bh + S(10.f);
        if (Button("##cfgdel", ix, ry, bw, bh, "Delete", false))
        {
            if (nameBuf[0] && gui_config::Remove(nameBuf)) { list = gui_config::List(); sel = -1; nameBuf[0] = 0; notify("Config deleted.", GREEN); }
            else notify(nameBuf[0] ? "Config not found." : "Enter a name first.", BAD);
        }
        if (Button("##cfgrefresh", ix + bw + gap, ry, bw, bh, "Refresh", false)) { list = gui_config::List(); notify("List refreshed.", DIM); }
        ry += bh + S(14.f);

        if (msgT > 0.f)
        {
            msgT -= ImGui::GetIO().DeltaTime;
            const float a = msgT > 0.5f ? 1.f : (msgT > 0.f ? msgT / 0.5f : 0.f);
            Text(ImVec2(ix, ry), U32(WithA(msgCol, a)), msg);
        }
        ry += S(22.f);
        EndCard(ry);

        // ---- right: saved list ----
        const float rx = x + leftW + gap;
        BeginCard("SAVED CONFIGS", rx, bodyY, rightW);
        float ly = CardBodyY();
        const float lix = rx + S(16.f), liw = rightW - S(32.f);
        if (list.empty())
        {
            Text(ImVec2(lix, ly), U32(DIM), "No saved configs yet.");
            ly += S(24.f);
        }
        else
        {
            for (int i = 0; i < static_cast<int>(list.size()); ++i)
            {
                const float rh = S(26.f);
                char id[32]; std::snprintf(id, sizeof(id), "##cfgrow%d", i);
                bool hov = false;
                const bool clk = Hit(id, ImVec2(lix, ly), ImVec2(liw, rh), hov);
                const bool seld = (sel == i);
                if (seld || hov) RectF(ImVec2(lix, ly), ImVec2(lix + liw, ly + rh), U32(seld ? WithA(g_accent, 0.18f) : FRAME), S(5.f));
                Text(ImVec2(lix + S(8.f), ly + (rh - TH()) * 0.5f), U32(seld ? g_accent : TEXT), list[i].c_str());
                if (clk) { sel = i; std::snprintf(nameBuf, sizeof(nameBuf), "%s", list[i].c_str()); }
                ly += rh + S(4.f);
            }
        }
        EndCard(ly);
    }

    // ---------------- SETTINGS ----------------
    void PageSettings(float x, float y, float w, float h)
    {
        PageTitle(x, y, "SETTINGS");
        const float bodyY = y + S(30.f);
        const float colW = S(360.f), ix = x + S(36.f), iw = colW - S(32.f);
        BeginCard("Interface", x + S(20.f), bodyY, colW);
        float ry = CardBodyY();
        Label(ix, ry, "UI scale", U32(DIM));
        float sc = g_uiScale; Slider("##uisc", ix + colW - S(32.f) - S(160.f) - S(50.f), ry, S(160.f), &sc, 0.7f, 3.0f, "%.2fx"); g_uiScale = sc;
        ry += S(34.f);
        Label(ix, ry + S(4.f), "Center window", U32(TEXT));
        // simple button
        {
            const float bx = ix + colW - S(120.f) - S(16.f), bw = S(120.f), bh = S(26.f);
            bool bh2 = false;
            if (Hit("##centerbtn", ImVec2(bx, ry), ImVec2(bw, bh), bh2)) g_needCenter = true;
            RectF(ImVec2(bx, ry), ImVec2(bx + bw, ry + bh), U32(bh2 ? WithA(g_accent, 0.4f) : FRAME), S(5.f));
            Text(ImVec2(bx + (bw - TW("Recenter")) * 0.5f, ry + (bh - TH()) * 0.5f), U32(TEXT), "Recenter");
        }
        ry += S(40.f);
        EndCard(ry);
    }

    void PageSimple(const char* title, float x, float y, float w, float h)
    {
        PageTitle(x, y, title ? title : "COMING SOON");
        Text(ImVec2(x + S(20.f), y + S(50.f)), U32(DIM), "This section is part of the system being built.");
    }
}
