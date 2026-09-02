#pragma once

// neXus redesign renderer.
// Presentation-only: binds to the same existing config objects and does not add
// gameplay logic, hooks, command mutation, signatures, offsets, or resolver work.

#include "../esp/esp.h"
#include "../trace/trace.h"
#include "../nerv/nerv_bridge.h"
#include "../templeware/config/gui_config.h"
#include "../../external/imgui/imgui.h"

#include <windows.h>
#include <array>
#include <vector>
#include <string>
#include <cstdio>
#include <cstring>
#include <cmath>

namespace NexusRedesign
{
    inline ImVec4 accent      = ImVec4(1.00f, 0.47f, 0.04f, 1.00f);
    inline ImVec4 bg          = ImVec4(0.030f, 0.033f, 0.040f, 1.00f);
    inline ImVec4 sidebar     = ImVec4(0.041f, 0.044f, 0.052f, 1.00f);
    inline ImVec4 panel       = ImVec4(0.060f, 0.064f, 0.075f, 1.00f);
    inline ImVec4 panel2      = ImVec4(0.071f, 0.075f, 0.087f, 1.00f);
    inline ImVec4 border      = ImVec4(0.125f, 0.130f, 0.150f, 1.00f);
    inline ImVec4 text        = ImVec4(0.92f, 0.93f, 0.96f, 1.00f);
    inline ImVec4 muted       = ImVec4(0.47f, 0.49f, 0.55f, 1.00f);
    inline ImVec4 success     = ImVec4(0.30f, 0.82f, 0.49f, 1.00f);

    inline int  nav = 1;
    inline bool centerNext = true;
    inline int  listeningKey = 0;
    inline bool keyArmed = false;

    inline ImU32 C(const ImVec4& v) { return ImGui::ColorConvertFloat4ToU32(v); }
    inline ImVec4 Alpha(ImVec4 v, float a) { v.w = a; return v; }
    inline ImVec4 Mix(const ImVec4& a, const ImVec4& b, float t)
    {
        return ImVec4(a.x + (b.x-a.x)*t, a.y + (b.y-a.y)*t,
                      a.z + (b.z-a.z)*t, a.w + (b.w-a.w)*t);
    }

    inline void ApplyStyle()
    {
        ImGuiStyle& s = ImGui::GetStyle();
        s.WindowPadding = ImVec2(0, 0);
        s.WindowRounding = 12.f;
        s.WindowBorderSize = 0.f;
        s.ChildRounding = 9.f;
        s.ChildBorderSize = 1.f;
        s.FrameRounding = 6.f;
        s.PopupRounding = 7.f;
        s.ScrollbarRounding = 10.f;
        s.GrabRounding = 10.f;
        s.FramePadding = ImVec2(10.f, 7.f);
        s.ItemSpacing = ImVec2(9.f, 9.f);
        s.ItemInnerSpacing = ImVec2(7.f, 6.f);
        s.ScrollbarSize = 8.f;

        s.Colors[ImGuiCol_WindowBg] = bg;
        s.Colors[ImGuiCol_ChildBg] = panel;
        s.Colors[ImGuiCol_PopupBg] = panel;
        s.Colors[ImGuiCol_Border] = border;
        s.Colors[ImGuiCol_Text] = text;
        s.Colors[ImGuiCol_TextDisabled] = muted;
        s.Colors[ImGuiCol_FrameBg] = panel2;
        s.Colors[ImGuiCol_FrameBgHovered] = Mix(panel2, accent, 0.12f);
        s.Colors[ImGuiCol_FrameBgActive] = Mix(panel2, accent, 0.20f);
        s.Colors[ImGuiCol_Button] = panel2;
        s.Colors[ImGuiCol_ButtonHovered] = Mix(panel2, accent, 0.18f);
        s.Colors[ImGuiCol_ButtonActive] = Mix(panel2, accent, 0.28f);
        s.Colors[ImGuiCol_Header] = Mix(panel, accent, 0.12f);
        s.Colors[ImGuiCol_HeaderHovered] = Mix(panel, accent, 0.20f);
        s.Colors[ImGuiCol_HeaderActive] = Mix(panel, accent, 0.26f);
        s.Colors[ImGuiCol_SliderGrab] = accent;
        s.Colors[ImGuiCol_SliderGrabActive] = accent;
        s.Colors[ImGuiCol_CheckMark] = accent;
        s.Colors[ImGuiCol_Separator] = border;
        s.Colors[ImGuiCol_ResizeGrip] = Alpha(accent, 0.18f);
        s.Colors[ImGuiCol_ResizeGripHovered] = Alpha(accent, 0.45f);
        s.Colors[ImGuiCol_ResizeGripActive] = Alpha(accent, 0.70f);
    }

    inline void TextMuted(const char* s) { ImGui::TextColored(muted, "%s", s); }

    inline void SectionIcon(ImDrawList* d, ImVec2 c, int type, ImU32 col)
    {
        const float r = 8.f;
        if (type == 0) // crosshair
        {
            d->AddCircle(c, 6.f, col, 24, 1.6f);
            d->AddLine(ImVec2(c.x, c.y-r), ImVec2(c.x, c.y-3.f), col, 1.6f);
            d->AddLine(ImVec2(c.x, c.y+3.f), ImVec2(c.x, c.y+r), col, 1.6f);
            d->AddLine(ImVec2(c.x-r, c.y), ImVec2(c.x-3.f, c.y), col, 1.6f);
            d->AddLine(ImVec2(c.x+3.f, c.y), ImVec2(c.x+r, c.y), col, 1.6f);
        }
        else if (type == 1) // target square
        {
            d->AddRect(ImVec2(c.x-7,c.y-7), ImVec2(c.x+7,c.y+7), col, 2.f, 0, 1.5f);
            d->AddCircleFilled(c, 1.8f, col);
        }
        else if (type == 2) // gauge
        {
            d->AddCircle(c, 7.f, col, 24, 1.5f);
            d->AddLine(c, ImVec2(c.x+4.f,c.y-4.f), col, 1.5f);
        }
        else
        {
            d->AddCircle(c, 6.f, col, 24, 1.5f);
            d->AddCircleFilled(c, 2.f, col);
        }
    }

    inline void CardBegin(const char* id, const char* title, ImVec2 size, int icon = 3)
    {
        ImGui::PushID(id);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, panel);
        ImGui::PushStyleColor(ImGuiCol_Border, border);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 9.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.f, 14.f));
        ImGui::BeginChild("##card", size, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        ImDrawList* d = ImGui::GetWindowDrawList();
        const ImVec2 p = ImGui::GetCursorScreenPos();
        SectionIcon(d, ImVec2(p.x + 8.f, p.y + 8.f), icon, C(accent));
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 25.f);
        ImGui::TextColored(text, "%s", title);
        ImGui::Dummy(ImVec2(0, 4.f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 2.f));
    }

    inline void CardEnd()
    {
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
        ImGui::PopID();
    }

    inline bool ToggleSwitch(const char* id, bool* v)
    {
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 size(42.f, 22.f);
        ImGui::InvisibleButton(id, size);
        if (ImGui::IsItemClicked()) *v = !*v;

        ImDrawList* d = ImGui::GetWindowDrawList();
        const ImVec4 track = *v ? Alpha(accent, 0.90f) : ImVec4(0.16f,0.17f,0.20f,1.f);
        d->AddRectFilled(pos, ImVec2(pos.x+size.x,pos.y+size.y), C(track), 11.f);
        const float cx = *v ? pos.x + size.x - 11.f : pos.x + 11.f;
        d->AddCircleFilled(ImVec2(cx,pos.y+11.f), 7.5f, IM_COL32(235,236,241,255));
        if (ImGui::IsItemHovered())
            d->AddRect(pos, ImVec2(pos.x+size.x,pos.y+size.y), C(Alpha(accent,0.45f)), 11.f, 0, 1.f);
        return ImGui::IsItemClicked();
    }

    inline void RowToggle(const char* label, bool* v)
    {
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(text, "%s", label);
        ImGui::SameLine();
        const float x = ImGui::GetWindowContentRegionMax().x - 42.f;
        ImGui::SetCursorPosX(x);
        char id[96]; std::snprintf(id, sizeof(id), "##tgl_%s", label);
        ToggleSwitch(id, v);
    }

    inline void RowSliderF(const char* label, float* v, float mn, float mx, const char* fmt)
    {
        ImGui::TextColored(muted, "%s", label);
        ImGui::SameLine();
        char val[32]; std::snprintf(val, sizeof(val), fmt, *v);
        ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - ImGui::CalcTextSize(val).x);
        ImGui::TextColored(text, "%s", val);
        char id[96]; std::snprintf(id, sizeof(id), "##s_%s", label);
        ImGui::SetNextItemWidth(-1.f);
        ImGui::SliderFloat(id, v, mn, mx, "", ImGuiSliderFlags_NoInput);
    }

    inline void RowSliderI(const char* label, int* v, int mn, int mx, const char* suffix = "")
    {
        ImGui::TextColored(muted, "%s", label);
        ImGui::SameLine();
        char val[32]; std::snprintf(val, sizeof(val), "%d%s", *v, suffix);
        ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - ImGui::CalcTextSize(val).x);
        ImGui::TextColored(text, "%s", val);
        char id[96]; std::snprintf(id, sizeof(id), "##si_%s", label);
        ImGui::SetNextItemWidth(-1.f);
        ImGui::SliderInt(id, v, mn, mx, "", ImGuiSliderFlags_NoInput);
    }

    inline void RowCombo(const char* label, int* v, const char* const* items, int count)
    {
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(muted, "%s", label);
        ImGui::SameLine();
        const float cw = 155.f;
        ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - cw);
        char id[96]; std::snprintf(id, sizeof(id), "##cmb_%s", label);
        ImGui::SetNextItemWidth(cw);
        ImGui::Combo(id, v, items, count);
    }

    inline const char* KeyName(int key)
    {
        static char b[32];
        if (!key) return "None";
        if (key == VK_LBUTTON) return "Mouse 1";
        if (key == VK_RBUTTON) return "Mouse 2";
        if (key == VK_MBUTTON) return "Mouse 3";
        if (key == VK_XBUTTON1) return "Mouse 4";
        if (key == VK_XBUTTON2) return "Mouse 5";
        UINT sc = MapVirtualKeyA((UINT)key, MAPVK_VK_TO_VSC);
        if (sc && GetKeyNameTextA((LONG)(sc << 16), b, sizeof(b)) > 0) return b;
        std::snprintf(b, sizeof(b), "0x%02X", key);
        return b;
    }

    inline void RowKey(const char* label, int* key)
    {
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(text, "%s", label);
        ImGui::SameLine();
        const float bw = 100.f;
        ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - bw);
        char id[96]; std::snprintf(id, sizeof(id), "##key_%s", label);
        const ImGuiID wid = ImGui::GetID(id);
        const bool listening = listeningKey == (int)wid;
        if (ImGui::Button(listening ? "Press key" : KeyName(*key), ImVec2(bw, 0)))
        {
            listeningKey = (int)wid;
            keyArmed = false;
        }
        if (listening)
        {
            if (!keyArmed)
            {
                if (!(GetAsyncKeyState(VK_LBUTTON)&0x8000) && !(GetAsyncKeyState(VK_RBUTTON)&0x8000)) keyArmed = true;
            }
            else
            {
                for (int k=1; k<256; ++k)
                {
                    if (GetAsyncKeyState(k)&0x8000)
                    {
                        *key = (k == VK_ESCAPE) ? 0 : k;
                        listeningKey = 0;
                        break;
                    }
                }
            }
        }
    }

    inline void ColorRow(const char* label, float col[4])
    {
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(muted, "%s", label);
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - 74.f);
        char id[96]; std::snprintf(id, sizeof(id), "##col_%s", label);
        ImGui::ColorEdit4(id, col, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoLabel);
    }

    template <size_t N>
    inline void Poly(ImDrawList* d, const std::array<ImVec2,N>& p, ImU32 fill, ImU32 stroke, float th=1.2f)
    {
        d->AddConvexPolyFilled(p.data(), (int)N, fill);
        d->AddPolyline(p.data(), (int)N, stroke, true, th);
    }

    inline void Ellipse(ImDrawList* d, ImVec2 c, float rx, float ry, ImU32 fill, ImU32 stroke, float th=1.2f)
    {
        std::array<ImVec2, 40> pts{};
        for (int i=0; i<(int)pts.size(); ++i)
        {
            const float a = 6.2831853f * (float)i / (float)pts.size();
            pts[i] = ImVec2(c.x + std::cos(a)*rx, c.y + std::sin(a)*ry);
        }
        d->AddConvexPolyFilled(pts.data(), (int)pts.size(), fill);
        d->AddPolyline(pts.data(), (int)pts.size(), stroke, true, th);
    }

    inline bool InRect(const ImVec2& m, float x1, float y1, float x2, float y2)
    {
        return m.x >= x1 && m.x <= x2 && m.y >= y1 && m.y <= y2;
    }

    inline void DrawAnatomy(ImVec2 pos, ImVec2 size, int* selected)
    {
        ImDrawList* d = ImGui::GetWindowDrawList();
        const float ar = 0.46f;
        const float fw = size.y * ar;
        const float ox = pos.x + (size.x - fw) * 0.5f;
        const float oy = pos.y;
        const float sx = fw;
        const float sy = size.y;
        auto P = [&](float x, float y) { return ImVec2(ox + x*sx, oy + y*sy); };

        const ImU32 outline = C(Alpha(accent, 0.92f));
        const ImU32 wire = IM_COL32(105,108,116,215);
        const ImU32 fill = IM_COL32(30,32,37,245);
        const ImU32 selFill = C(Alpha(accent, 0.45f));
        auto RF = [&](int region) { return (*selected == region) ? selFill : fill; };

        // very soft halo, matching the reference without requiring shadow APIs
        for (int i=5; i>=1; --i)
        {
            const float e = (float)i * 2.6f;
            d->AddRect(ImVec2(ox-e,oy+sy*0.015f-e), ImVec2(ox+sx+e,oy+sy*0.985f+e),
                       C(Alpha(accent,0.012f*(6-i))), 24.f, 0, 2.f);
        }

        // head + face grid
        Ellipse(d, P(.50f,.085f), sx*.115f, sy*.072f, RF(0), outline, 1.4f);
        d->AddLine(P(.50f,.016f), P(.50f,.153f), wire, 0.8f);
        d->AddLine(P(.39f,.085f), P(.61f,.085f), wire, 0.8f);

        std::array<ImVec2,4> neck{P(.455f,.145f),P(.545f,.145f),P(.565f,.205f),P(.435f,.205f)};
        Poly(d, neck, RF(1), outline);
        d->AddLine(P(.50f,.15f),P(.50f,.205f),wire,0.8f);

        // chest / shoulders
        std::array<ImVec2,8> chest{
            P(.435f,.195f),P(.325f,.215f),P(.265f,.265f),P(.345f,.39f),
            P(.50f,.415f),P(.655f,.39f),P(.735f,.265f),P(.675f,.215f)};
        Poly(d, chest, RF(2), outline, 1.5f);
        d->AddLine(P(.50f,.205f),P(.50f,.415f),wire,0.85f);
        d->AddLine(P(.34f,.30f),P(.66f,.30f),wire,0.75f);
        d->AddLine(P(.36f,.37f),P(.64f,.37f),wire,0.75f);

        std::array<ImVec2,6> stomach{P(.345f,.39f),P(.50f,.415f),P(.655f,.39f),P(.625f,.56f),P(.50f,.59f),P(.375f,.56f)};
        Poly(d, stomach, RF(3), outline);
        d->AddLine(P(.50f,.415f),P(.50f,.59f),wire,0.8f);
        d->AddLine(P(.37f,.47f),P(.63f,.47f),wire,0.7f);
        d->AddLine(P(.38f,.525f),P(.62f,.525f),wire,0.7f);

        std::array<ImVec2,6> pelvis{P(.375f,.56f),P(.50f,.59f),P(.625f,.56f),P(.60f,.665f),P(.50f,.69f),P(.40f,.665f)};
        Poly(d, pelvis, RF(4), outline);
        d->AddLine(P(.50f,.59f),P(.50f,.69f),wire,0.8f);

        // arms: upper/lower pieces + joints + small hands
        std::array<ImVec2,4> lua{P(.325f,.22f),P(.265f,.265f),P(.19f,.435f),P(.265f,.455f)};
        std::array<ImVec2,4> lfa{P(.19f,.435f),P(.265f,.455f),P(.18f,.625f),P(.105f,.595f)};
        std::array<ImVec2,4> rua{P(.675f,.22f),P(.735f,.265f),P(.81f,.435f),P(.735f,.455f)};
        std::array<ImVec2,4> rfa{P(.81f,.435f),P(.735f,.455f),P(.82f,.625f),P(.895f,.595f)};
        Poly(d,lua,RF(5),outline); Poly(d,lfa,RF(5),outline);
        Poly(d,rua,RF(6),outline); Poly(d,rfa,RF(6),outline);
        Ellipse(d,P(.225f,.445f),sx*.045f,sy*.025f,RF(5),wire,1.f);
        Ellipse(d,P(.775f,.445f),sx*.045f,sy*.025f,RF(6),wire,1.f);
        Ellipse(d,P(.145f,.615f),sx*.042f,sy*.020f,RF(5),wire,1.f);
        Ellipse(d,P(.855f,.615f),sx*.042f,sy*.020f,RF(6),wire,1.f);
        std::array<ImVec2,4> lh{P(.105f,.595f),P(.18f,.625f),P(.155f,.685f),P(.085f,.655f)};
        std::array<ImVec2,4> rh{P(.895f,.595f),P(.82f,.625f),P(.845f,.685f),P(.915f,.655f)};
        Poly(d,lh,RF(5),outline); Poly(d,rh,RF(6),outline);

        // legs
        std::array<ImVec2,4> lt{P(.40f,.665f),P(.50f,.69f),P(.485f,.82f),P(.37f,.82f)};
        std::array<ImVec2,4> rt{P(.50f,.69f),P(.60f,.665f),P(.63f,.82f),P(.515f,.82f)};
        std::array<ImVec2,4> ls{P(.37f,.82f),P(.485f,.82f),P(.47f,.955f),P(.365f,.955f)};
        std::array<ImVec2,4> rs{P(.515f,.82f),P(.63f,.82f),P(.635f,.955f),P(.53f,.955f)};
        Poly(d,lt,RF(7),outline); Poly(d,ls,RF(7),outline);
        Poly(d,rt,RF(8),outline); Poly(d,rs,RF(8),outline);
        Ellipse(d,P(.428f,.82f),sx*.055f,sy*.028f,RF(7),wire,1.f);
        Ellipse(d,P(.572f,.82f),sx*.055f,sy*.028f,RF(8),wire,1.f);
        std::array<ImVec2,4> lf{P(.365f,.952f),P(.47f,.952f),P(.485f,.985f),P(.33f,.985f)};
        std::array<ImVec2,4> rf{P(.53f,.952f),P(.635f,.952f),P(.67f,.985f),P(.515f,.985f)};
        Poly(d,lf,RF(7),outline); Poly(d,rf,RF(8),outline);

        // joints / panel details
        d->AddCircle(P(.50f,.415f), 2.2f, wire, 16, 1.f);
        d->AddCircle(P(.50f,.59f), 2.2f, wire, 16, 1.f);
        d->AddLine(P(.41f,.72f),P(.485f,.72f),wire,0.7f);
        d->AddLine(P(.515f,.72f),P(.59f,.72f),wire,0.7f);
        d->AddLine(P(.38f,.89f),P(.47f,.89f),wire,0.7f);
        d->AddLine(P(.53f,.89f),P(.62f,.89f),wire,0.7f);

        // one large hit target over the entire figure; region resolution stays presentation-only.
        ImGui::SetCursorScreenPos(pos);
        ImGui::InvisibleButton("##anatomy", size);
        if (ImGui::IsItemHovered())
        {
            const ImVec2 m = ImGui::GetIO().MousePos;
            int h = -1;
            if (InRect(m, ox+sx*.37f,oy+sy*.01f, ox+sx*.63f,oy+sy*.16f)) h=0;
            else if (InRect(m,ox+sx*.43f,oy+sy*.145f,ox+sx*.57f,oy+sy*.21f)) h=1;
            else if (InRect(m,ox+sx*.27f,oy+sy*.20f,ox+sx*.73f,oy+sy*.41f)) h=2;
            else if (InRect(m,ox+sx*.34f,oy+sy*.40f,ox+sx*.66f,oy+sy*.57f)) h=3;
            else if (InRect(m,ox+sx*.36f,oy+sy*.56f,ox+sx*.64f,oy+sy*.69f)) h=4;
            else if (InRect(m,ox+sx*.07f,oy+sy*.20f,ox+sx*.34f,oy+sy*.69f)) h=5;
            else if (InRect(m,ox+sx*.66f,oy+sy*.20f,ox+sx*.93f,oy+sy*.69f)) h=6;
            else if (InRect(m,ox+sx*.32f,oy+sy*.66f,ox+sx*.50f,oy+sy*.99f)) h=7;
            else if (InRect(m,ox+sx*.50f,oy+sy*.66f,ox+sx*.68f,oy+sy*.99f)) h=8;
            if (h >= 0)
            {
                ImGui::SetTooltip("%s", (const char*[]){"Head","Neck","Chest","Stomach","Pelvis","Left arm","Right arm","Left leg","Right leg"}[h]);
                if (ImGui::IsItemClicked()) *selected = h;
            }
        }
    }

    inline bool Chip(const char* id, const char* label, bool activeChip, ImVec2 size)
    {
        ImGui::PushID(id);
        ImGui::PushStyleColor(ImGuiCol_Button, activeChip ? Alpha(accent,.20f) : panel2);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Alpha(accent,.26f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, Alpha(accent,.34f));
        ImGui::PushStyleColor(ImGuiCol_Text, activeChip ? accent : muted);
        const bool r = ImGui::Button(label, size);
        ImGui::PopStyleColor(4);
        ImGui::PopID();
        return r;
    }

    inline void HitboxPanel(ImVec2 size)
    {
        auto& a = Esp::g_aimbot;
        CardBegin("hitbox", "HITBOX", size, 1);
        const float footer = 74.f;
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const ImVec2 p = ImGui::GetCursorScreenPos();
        DrawAnatomy(p, ImVec2(avail.x, avail.y-footer), &a.hitbox);
        ImGui::Dummy(ImVec2(0, avail.y-footer+4.f));

        const char* names[] = {"HEAD","NECK","CHEST","STOMACH","PELVIS","L. ARM","R. ARM","L. LEG","R. LEG"};
        if (a.hitbox >= 0 && a.hitbox < 9)
        {
            ImGui::TextColored(accent, "%s", names[a.hitbox]);
            ImGui::SameLine();
            ImGui::TextColored(muted, "selected");
        }
        ImGui::Dummy(ImVec2(0,2.f));
        const float gap = 5.f;
        const float cw = (ImGui::GetContentRegionAvail().x - gap*5.f) / 6.f;
        if (Chip("h0","Head",a.hitbox==0,ImVec2(cw,27))) a.hitbox=0; ImGui::SameLine(0,gap);
        if (Chip("h1","Neck",a.hitbox==1,ImVec2(cw,27))) a.hitbox=1; ImGui::SameLine(0,gap);
        if (Chip("h2","Chest",a.hitbox==2,ImVec2(cw,27))) a.hitbox=2; ImGui::SameLine(0,gap);
        if (Chip("h3","Body",a.hitbox==3||a.hitbox==4,ImVec2(cw,27))) a.hitbox=3; ImGui::SameLine(0,gap);
        if (Chip("h4","Arms",a.hitbox==5||a.hitbox==6,ImVec2(cw,27))) a.hitbox=5; ImGui::SameLine(0,gap);
        if (Chip("h5","Legs",a.hitbox==7||a.hitbox==8,ImVec2(cw,27))) a.hitbox=7;
        CardEnd();
    }

    inline void PageAimbot()
    {
        auto& a = Esp::g_aimbot;
        auto& t = Esp::g_trigger;
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const float gap = 14.f;
        const float leftW = avail.x * 0.39f;
        const float rightW = avail.x - leftW - gap;

        ImGui::BeginGroup();
        HitboxPanel(ImVec2(leftW, avail.y));
        ImGui::EndGroup();
        ImGui::SameLine(0, gap);

        ImGui::BeginGroup();
        const float topH = 202.f;
        const float half = (rightW-gap)/2.f;

        CardBegin("activation","ACTIVATION",ImVec2(half,topH),0);
        RowToggle("Enable", &a.enable);
        RowKey("Aim key", &a.aimKey);
        const char* aimTypes[] = {"Hold","Toggle","Always"};
        RowCombo("Aim type", &a.aimType, aimTypes, 3);
        CardEnd();
        ImGui::SameLine(0,gap);
        CardBegin("targeting","TARGETING",ImVec2(half,topH),0);
        RowSliderF("FOV", &a.fov, 0.f, 30.f, "%.1f");
        RowSliderF("Smooth", &a.smooth, 0.f, 1.f, "%.2f");
        RowToggle("Draw FOV", &a.drawFov);
        CardEnd();

        ImGui::Dummy(ImVec2(0,gap));
        const float midH = 216.f;
        CardBegin("rcs","ACCURACY / RCS",ImVec2(half,midH),2);
        RowToggle("Recoil control", &a.rcs);
        RowSliderI("RCS X", &a.rcsX, 0, 100, "%");
        RowSliderI("RCS Y", &a.rcsY, 0, 100, "%");
        CardEnd();
        ImGui::SameLine(0,gap);
        CardBegin("trigger","TRIGGERBOT",ImVec2(half,midH),0);
        RowToggle("Enable", &t.enable);
        RowToggle("Team check", &t.teamCheck);
        RowSliderI("Delay", &t.delayMs, 0, 250, " ms");
        RowKey("Trigger key", &t.key);
        CardEnd();

        ImGui::Dummy(ImVec2(0,gap));
        CardBegin("master","AIMBOT MASTER",ImVec2(rightW,76.f),0);
        ImGui::TextColored(muted,"Current profile uses the existing TempleWare aim configuration.");
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - 42.f);
        ToggleSwitch("##master", &a.enable);
        CardEnd();

        ImGui::Dummy(ImVec2(0,gap));
        CardBegin("keys","KEYBINDS",ImVec2(rightW,avail.y-topH-midH-76.f-gap*3.f),3);
        const float cell = (ImGui::GetContentRegionAvail().x-20.f)/3.f;
        ImGui::BeginGroup(); ImGui::TextColored(muted,"Aimbot"); ImGui::TextColored(text,"%s",KeyName(a.aimKey)); ImGui::EndGroup();
        ImGui::SameLine(cell);
        ImGui::BeginGroup(); ImGui::TextColored(muted,"Triggerbot"); ImGui::TextColored(text,"%s",KeyName(t.key)); ImGui::EndGroup();
        ImGui::SameLine(cell*2.f);
        ImGui::BeginGroup(); ImGui::TextColored(muted,"Menu"); ImGui::TextColored(accent,"INSERT"); ImGui::EndGroup();
        CardEnd();
        ImGui::EndGroup();
    }

    inline void PageVisuals()
    {
        auto& c = Esp::g_config;
        const ImVec2 av = ImGui::GetContentRegionAvail();
        const float gap=14.f, w=(av.x-gap)/2.f, h=(av.y-gap)/2.f;
        CardBegin("player","PLAYER ESP",ImVec2(w,h),1);
        RowToggle("ESP enabled",&c.enabled); RowToggle("Box",&c.box); RowToggle("Skeleton",&c.skeleton);
        RowToggle("Health bar",&c.healthBar); RowToggle("Ammo bar",&c.ammoBar); RowToggle("Name",&c.name); RowToggle("Weapon",&c.weapon);
        CardEnd(); ImGui::SameLine(0,gap);
        CardBegin("visinfo","PLAYER INFO",ImVec2(w,h),3);
        RowToggle("Flags",&c.flags); RowToggle("Ping",&c.showPing); RowToggle("Distance",&c.distance); RowToggle("Armor",&c.showArmor); RowToggle("Team ESP",&c.teamEsp);
        ColorRow("Box color",c.boxColor); ColorRow("Skeleton",c.skeletonColor);
        CardEnd();
        ImGui::Dummy(ImVec2(0,gap));
        CardBegin("chams","CHAMS / GLOW",ImVec2(w,h),2);
        RowToggle("Enemy chams",&c.chams); RowToggle("Enemy XQZ",&c.chamsXqz); RowToggle("Enemy glow",&c.glow); RowToggle("Team glow",&c.glowTeam); RowToggle("Local glow",&c.glowLocal);
        CardEnd(); ImGui::SameLine(0,gap);
        CardBegin("extras","EXTRAS",ImVec2(w,h),3);
        RowToggle("Head circle",&c.headCircle); RowToggle("Snapline",&c.snapline); RowToggle("Off-screen arrows",&c.offArrows); RowToggle("Sound ESP",&c.soundEsp); RowToggle("Real LOS",&c.realVis);
        CardEnd();
    }

    inline void PageWorld()
    {
        auto& c = Esp::g_config;
        const ImVec2 av = ImGui::GetContentRegionAvail();
        const float gap=14.f, w=(av.x-gap)/2.f;
        CardBegin("world1","BOMB & GRENADES",ImVec2(w,av.y),3);
        RowToggle("Bomb ESP",&c.bombEsp); RowToggle("Grenade ESP",&c.nadeEsp); RowToggle("Grenade trajectory",&c.nadeTrajectory);
        RowToggle("Throw predictor",&c.nadeThrow); RowToggle("Inferno fill",&c.infernoFill); RowToggle("Spectator list",&c.specList); RowToggle("Ragdoll ESP",&c.ragdollEsp);
        ColorRow("Bomb color",c.bombColor); ColorRow("Grenade color",c.nadeColor);
        CardEnd(); ImGui::SameLine(0,gap);
        CardBegin("world2","ITEM ESP",ImVec2(w,av.y),1);
        RowToggle("Item names",&c.itemEsp); RowToggle("Item icons",&c.itemIcon); RowToggle("Item glow",&c.itemGlow); RowToggle("Item chams",&c.itemChams); RowToggle("Item distance",&c.itemDistance);
        ColorRow("Item color",c.itemColor); ColorRow("Glow color",c.itemGlowColor);
        CardEnd();
    }

    inline void PageMovement()
    {
        auto& m = Esp::g_movement;
        const ImVec2 av=ImGui::GetContentRegionAvail(); const float gap=14.f,w=(av.x-gap)/2.f;
        CardBegin("move1","MOVEMENT",ImVec2(w,av.y),3);
        RowToggle("Bunny hop",&m.bhop); RowToggle("Auto strafe",&m.autoStrafe); RowToggle("Fake duck",&m.fakeDuck); RowToggle("Slow walk",&m.slowWalk);
        CardEnd(); ImGui::SameLine(0,gap);
        CardBegin("move2","KEYBINDS",ImVec2(w,av.y),0);
        RowKey("Bhop key",&m.bhopKey); RowKey("Fake duck",&m.fakeDuckKey); RowKey("Slow walk",&m.slowWalkKey);
        CardEnd();
    }

    inline void PageMisc()
    {
        auto& c=Esp::g_config;
        const ImVec2 av=ImGui::GetContentRegionAvail(); const float gap=14.f,w=(av.x-gap*2.f)/3.f;
        CardBegin("misc1","VISUALS",ImVec2(w,av.y),3);
        RowToggle("Anti-flash",&c.antiFlash); RowToggle("FOV changer",&c.fovChanger); if(c.fovChanger) RowSliderI("FOV",&c.fovValue,60,160);
        RowToggle("Local opacity",&c.localOpacity); if(c.localOpacity) RowSliderF("Opacity",&c.localOpacityVal,0.f,1.f,"%.2f");
        CardEnd(); ImGui::SameLine(0,gap);
        CardBegin("misc2","VELOCITY",ImVec2(w,av.y),2);
        RowToggle("Velocity bar",&c.velBar); RowToggle("Gradient",&c.velGradient); RowToggle("Glow",&c.velGlow); RowToggle("Show speed",&c.velText); RowToggle("Speed graph",&c.velGraph); RowSliderI("Max speed",&c.velMax,100,1000);
        CardEnd(); ImGui::SameLine(0,gap);
        CardBegin("misc3","HUD",ImVec2(w,av.y),0);
        RowToggle("Watermark",&c.watermark); RowToggle("Hit marker",&c.hitMarker); RowToggle("Damage numbers",&c.damageNumbers); RowToggle("Bullet tracer",&c.bulletTracer); RowToggle("Crosshair",&c.crosshair);
        if(c.crosshair){RowSliderF("Size",&c.crosshairSize,1.f,20.f,"%.0f"); RowSliderF("Gap",&c.crosshairGap,0.f,12.f,"%.0f");}
        CardEnd();
    }

    inline void PageInventory()
    {
        const ImVec2 av=ImGui::GetContentRegionAvail();
        CardBegin("inventory","INVENTORY",av,3);
        ImGui::PushItemWidth(280.f);
        nerv_bridge::draw_skins_ui();
        ImGui::PopItemWidth();
        CardEnd();
    }

    inline void PageConfigs()
    {
        static char name[64]="";
        static std::vector<std::string> configs = gui_config::List();
        static int selected=-1;
        const ImVec2 av=ImGui::GetContentRegionAvail(); const float gap=14.f,w=(av.x-gap)/2.f;
        CardBegin("cfgmanage","MANAGE CONFIG",ImVec2(w,av.y),3);
        ImGui::TextColored(muted,"Config name");
        ImGui::SetNextItemWidth(-1.f); ImGui::InputTextWithHint("##cfgname","e.g. legit",name,sizeof(name));
        if(ImGui::Button("Save",ImVec2((ImGui::GetContentRegionAvail().x-8.f)/2.f,34.f)) && name[0]){gui_config::Save(name); configs=gui_config::List();}
        ImGui::SameLine(0,8.f);
        if(ImGui::Button("Load",ImVec2(-1.f,34.f)) && name[0]) gui_config::Load(name);
        if(ImGui::Button("Delete",ImVec2(-1.f,34.f)) && name[0]){gui_config::Remove(name); configs=gui_config::List(); selected=-1;}
        CardEnd(); ImGui::SameLine(0,gap);
        CardBegin("cfglist","SAVED CONFIGS",ImVec2(w,av.y),3);
        for(int i=0;i<(int)configs.size();++i)
        {
            const bool s=selected==i;
            if(ImGui::Selectable(configs[i].c_str(),s))
            {
                selected=i; std::snprintf(name,sizeof(name),"%s",configs[i].c_str());
            }
        }
        CardEnd();
    }

    inline void PageDashboard()
    {
        const auto& st = Esp::GetStats();
        const ImVec2 av=ImGui::GetContentRegionAvail(); const float gap=14.f,w=(av.x-gap*2.f)/3.f;
        CardBegin("dash1","RUNTIME",ImVec2(w,150),3);
        TextMuted("Entity system"); ImGui::SameLine(); ImGui::TextColored(st.entitySystemReady?success:muted,st.entitySystemReady?"Ready":"Waiting");
        TextMuted("View matrix"); ImGui::SameLine(); ImGui::TextColored(st.viewMatrixReady?success:muted,st.viewMatrixReady?"Ready":"Waiting");
        CardEnd(); ImGui::SameLine(0,gap);
        CardBegin("dash2","PLAYERS",ImVec2(w,150),1);
        ImGui::TextColored(muted,"Players found"); ImGui::TextColored(text,"%d",st.playersFound);
        ImGui::TextColored(muted,"Local team"); ImGui::TextColored(text,"%d",st.localTeam);
        CardEnd(); ImGui::SameLine(0,gap);
        CardBegin("dash3","TRACE",ImVec2(w,150),2);
        ImGui::TextColored(muted,"Backend"); ImGui::TextColored(Trace::Ready()?success:muted,Trace::Ready()?"Ready":"Not ready");
        CardEnd();
        ImGui::Dummy(ImVec2(0,gap));
        CardBegin("dash4","WELCOME TO neXus",ImVec2(av.x,av.y-164.f),0);
        ImGui::TextColored(text,"Dark, compact and focused on fast navigation.");
        ImGui::Dummy(ImVec2(0,6.f));
        ImGui::TextWrapped("The redesign keeps the existing TempleWare settings bindings while replacing the old three-column layout and the placeholder stick-figure hitbox with a larger anatomical control surface.");
        CardEnd();
    }

    inline void PageSettings()
    {
        const ImVec2 av=ImGui::GetContentRegionAvail(); const float gap=14.f,w=(av.x-gap)/2.f;
        CardBegin("theme","APPEARANCE",ImVec2(w,av.y),3);
        ImGui::TextColored(muted,"Accent color");
        ImGui::ColorEdit4("##accent",&accent.x,ImGuiColorEditFlags_NoInputs|ImGuiColorEditFlags_AlphaBar);
        ImGui::Dummy(ImVec2(0,8.f));
        ImGui::TextWrapped("The rest of the palette stays deliberately neutral so the accent remains the only strong color.");
        CardEnd(); ImGui::SameLine(0,gap);
        CardBegin("about","ABOUT",ImVec2(w,av.y),3);
        ImGui::TextColored(text,"neXus / CS2 INTERNAL");
        ImGui::TextColored(muted,"UI redesign baseline");
        ImGui::Dummy(ImVec2(0,10.f));
        ImGui::TextWrapped("Insert toggles the menu. The redesign is presentation-only and reuses the existing configuration state.");
        CardEnd();
    }

    inline void PagePlaceholder(const char* title)
    {
        const ImVec2 av=ImGui::GetContentRegionAvail();
        CardBegin("placeholder",title,av,3);
        ImGui::TextColored(muted,"This section is ready for its existing controls to be moved into the new visual system.");
        CardEnd();
    }

    inline const char* NavName(int i)
    {
        static const char* names[] = {"Dashboard","Aimbot","Visuals","World","Movement","Inventory","Misc","Configs","Lua Scripts","Settings"};
        return names[(i>=0&&i<10)?i:0];
    }

    inline const char* NavSub(int i)
    {
        static const char* subs[] = {
            "Runtime overview and status.","Configure aim assistance and hitbox selection.","Player ESP, chams and overlays.",
            "World entities and utility overlays.","Movement helpers and keybinds.","Skins and inventory controls.",
            "HUD and quality-of-life settings.","Save and load menu profiles.","Script workspace.","Theme and menu preferences."};
        return subs[(i>=0&&i<10)?i:0];
    }

    inline void NavIcon(ImDrawList* d, int idx, ImVec2 c, ImU32 col)
    {
        const float r=7.f;
        if(idx==1){SectionIcon(d,c,0,col);return;}
        if(idx==2){d->AddCircle(c,r,col,24,1.4f); d->AddCircleFilled(c,2.f,col);return;}
        if(idx==0){for(int y=-1;y<=1;y+=2)for(int x=-1;x<=1;x+=2)d->AddRect(ImVec2(c.x+x*5.f-2.5f,c.y+y*5.f-2.5f),ImVec2(c.x+x*5.f+2.5f,c.y+y*5.f+2.5f),col,1.f,0,1.2f);return;}
        if(idx==4){d->AddLine(ImVec2(c.x-r,c.y-r),c,col,1.5f);d->AddLine(c,ImVec2(c.x-r,c.y+r),col,1.5f);d->AddLine(c,ImVec2(c.x+r,c.y),col,1.5f);return;}
        if(idx==7){d->AddRect(ImVec2(c.x-r,c.y-4),ImVec2(c.x+r,c.y+6),col,1.f,0,1.3f);d->AddLine(ImVec2(c.x-r,c.y-4),ImVec2(c.x-2,c.y-8),col,1.3f);d->AddLine(ImVec2(c.x-2,c.y-8),ImVec2(c.x+r,c.y-8),col,1.3f);return;}
        d->AddRect(ImVec2(c.x-r,c.y-r),ImVec2(c.x+r,c.y+r),col,2.f,0,1.3f);
    }

    inline void Render(float alpha)
    {
        ApplyStyle();
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
        ImGui::SetNextWindowSize(ImVec2(1380.f, 840.f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(ImVec2(1120.f,700.f),ImVec2(2200.f,1400.f));
        if(centerNext)
        {
            const ImVec2 ds=ImGui::GetIO().DisplaySize;
            ImGui::SetNextWindowPos(ImVec2((ds.x-1380.f)*.5f,(ds.y-840.f)*.5f),ImGuiCond_Always);
            centerNext=false;
        }

        ImGui::Begin("neXus##redesign",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
        const ImVec2 wp=ImGui::GetWindowPos(); const ImVec2 ws=ImGui::GetWindowSize();
        ImDrawList* d=ImGui::GetWindowDrawList();
        const float sideW=188.f, topH=72.f;

        // root + sidebar
        d->AddRectFilled(wp,ImVec2(wp.x+ws.x,wp.y+ws.y),C(bg),12.f);
        d->AddRectFilled(wp,ImVec2(wp.x+sideW,wp.y+ws.y),C(sidebar),12.f,ImDrawFlags_RoundCornersLeft);
        d->AddLine(ImVec2(wp.x+sideW,wp.y),ImVec2(wp.x+sideW,wp.y+ws.y),C(border),1.f);

        // logo
        d->AddText(ImVec2(wp.x+24.f,wp.y+25.f),C(text),"neXus");
        d->AddText(ImVec2(wp.x+24.f,wp.y+48.f),C(accent),"CS2");
        d->AddText(ImVec2(wp.x+52.f,wp.y+48.f),C(muted),"INTERNAL");

        // sidebar navigation
        const char* names[]={"Dashboard","Aimbot","Visuals","World","Movement","Inventory","Misc","Configs","Lua Scripts","Settings"};
        float ny=wp.y+92.f;
        for(int i=0;i<10;++i)
        {
            const ImVec2 a(wp.x+12.f,ny), b(wp.x+sideW-12.f,ny+42.f);
            ImGui::SetCursorScreenPos(a); char id[24]; std::snprintf(id,sizeof(id),"##nav%d",i);
            ImGui::InvisibleButton(id,ImVec2(b.x-a.x,b.y-a.y));
            if(ImGui::IsItemClicked()) nav=i;
            const bool active=nav==i, hover=ImGui::IsItemHovered();
            if(active) d->AddRectFilled(a,b,C(Alpha(accent,.14f)),8.f);
            else if(hover) d->AddRectFilled(a,b,C(ImVec4(.09f,.095f,.11f,1.f)),8.f);
            if(active) d->AddRectFilled(a,ImVec2(a.x+3.f,b.y),C(accent),2.f);
            const ImU32 tc=C(active?accent:(hover?text:muted));
            NavIcon(d,i,ImVec2(a.x+22.f,a.y+21.f),tc);
            d->AddText(ImVec2(a.x+43.f,a.y+13.f),tc,names[i]);
            ny+=46.f;
        }

        // user card
        const ImVec2 ua(wp.x+12.f,wp.y+ws.y-78.f), ub(wp.x+sideW-12.f,wp.y+ws.y-14.f);
        d->AddRectFilled(ua,ub,C(panel),8.f); d->AddRect(ua,ub,C(border),8.f);
        d->AddCircleFilled(ImVec2(ua.x+24.f,ua.y+24.f),14.f,C(Alpha(accent,.16f)));
        SectionIcon(d,ImVec2(ua.x+24.f,ua.y+24.f),1,C(accent));
        d->AddText(ImVec2(ua.x+47.f,ua.y+14.f),C(text),"neXus User");
        d->AddText(ImVec2(ua.x+47.f,ua.y+34.f),C(accent),"Premium");

        // top bar + drag strip
        const float cx=wp.x+sideW;
        ImGui::SetCursorScreenPos(ImVec2(cx,wp.y)); ImGui::InvisibleButton("##drag",ImVec2(ws.x-sideW,topH));
        if(ImGui::IsItemActive()){const ImVec2 md=ImGui::GetIO().MouseDelta;ImGui::SetWindowPos(ImVec2(wp.x+md.x,wp.y+md.y));}
        d->AddLine(ImVec2(cx,wp.y+topH),ImVec2(wp.x+ws.x,wp.y+topH),C(border),1.f);
        SectionIcon(d,ImVec2(cx+31.f,wp.y+31.f),nav==1?0:3,C(accent));
        d->AddText(ImVec2(cx+52.f,wp.y+20.f),C(text),NavName(nav));
        d->AddText(ImVec2(cx+52.f,wp.y+42.f),C(muted),NavSub(nav));

        // status pill
        const float pillW=136.f; const ImVec2 pa(wp.x+ws.x-pillW-26.f,wp.y+20.f),pb(wp.x+ws.x-26.f,wp.y+52.f);
        d->AddRectFilled(pa,pb,C(panel),7.f); d->AddRect(pa,pb,C(border),7.f);
        d->AddCircleFilled(ImVec2(pa.x+14.f,pa.y+16.f),4.f,C(accent));
        d->AddText(ImVec2(pa.x+26.f,pa.y+9.f),C(muted),"Injected");
        d->AddText(ImVec2(pb.x-31.f,pb.y-23.f),C(accent),"CS2");

        // page area
        ImGui::SetCursorScreenPos(ImVec2(cx+18.f,wp.y+topH+18.f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg,ImVec4(0,0,0,0));
        ImGui::BeginChild("##page",ImVec2(ws.x-sideW-36.f,ws.y-topH-36.f),false,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
        switch(nav)
        {
        case 0: PageDashboard(); break;
        case 1: PageAimbot(); break;
        case 2: PageVisuals(); break;
        case 3: PageWorld(); break;
        case 4: PageMovement(); break;
        case 5: PageInventory(); break;
        case 6: PageMisc(); break;
        case 7: PageConfigs(); break;
        case 8: PagePlaceholder("LUA SCRIPTS"); break;
        case 9: PageSettings(); break;
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::End();
        ImGui::PopStyleVar();
    }
}
