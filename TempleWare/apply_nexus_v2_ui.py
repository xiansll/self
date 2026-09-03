#!/usr/bin/env python3
from pathlib import Path
import re, shutil
from datetime import datetime

NEW_HUMAN = '\n    void DrawHumanHitbox(float ox, float oy, float bw, float bh, int* sel)\n    {\n        ImDrawList* d = DL();\n\n        // Nexus V2 anatomical mannequin. Fully draw-list based; no external SVG.\n        const float baseW = 230.f;\n        const float baseH = 505.f;\n        const float k = (std::min)(bw / S(baseW), bh / S(baseH));\n        const float dw = S(baseW) * k;\n        const float dh = S(baseH) * k;\n        const ImVec2 o(ox + (bw - dw) * 0.5f, oy + (bh - dh) * 0.5f);\n\n        auto P = [&](float x, float y) -> ImVec2 {\n            return ImVec2(o.x + S(x) * k, o.y + S(y) * k);\n        };\n        auto Poly = [&](const ImVec2* pts, int n, ImU32 fill, ImU32 line, float th = 1.15f) {\n            d->AddConvexPolyFilled(pts, n, fill);\n            d->AddPolyline(pts, n, line, true, S(th) * k);\n        };\n        auto Ellipse = [&](ImVec2 c, float rx, float ry, ImU32 fill, ImU32 line, float th = 1.15f) {\n            ImVec2 pts[36];\n            for (int i = 0; i < 36; ++i) {\n                const float a = (6.28318530718f * (float)i) / 36.f;\n                pts[i] = ImVec2(c.x + std::cos(a) * S(rx) * k, c.y + std::sin(a) * S(ry) * k);\n            }\n            d->AddConvexPolyFilled(pts, 36, fill);\n            d->AddPolyline(pts, 36, line, true, S(th) * k);\n        };\n\n        const ImU32 body = U32(RGBA(27, 29, 34, 255));\n        const ImU32 mesh = U32(RGBA(112, 116, 124, 155));\n        const ImU32 edge = U32(WithA(g_accent, 0.90f));\n        const ImU32 soft = U32(WithA(g_accent, 0.035f));\n\n        d->AddCircleFilled(P(115, 258), S(104.f) * k, soft, 64);\n\n        // Head\n        Ellipse(P(115, 52), 31, 39, body, edge, 1.25f);\n        d->AddLine(P(115, 14), P(115, 90), mesh, S(0.65f) * k);\n        d->AddLine(P(85, 53), P(145, 53), mesh, S(0.65f) * k);\n        d->AddBezierCurve(P(92, 75), P(101, 82), P(129, 82), P(138, 75), mesh, S(0.55f) * k);\n\n        // Neck\n        ImVec2 neck[] = { P(103,88), P(127,88), P(130,108), P(115,115), P(100,108) };\n        Poly(neck, 5, body, mesh);\n\n        // Torso\n        ImVec2 torso[] = {\n            P(75,110), P(95,102), P(115,111), P(135,102), P(155,110),\n            P(165,143), P(158,171), P(146,197), P(146,250), P(137,290),\n            P(115,309), P(93,290), P(84,250), P(84,197), P(72,171), P(65,143)\n        };\n        Poly(torso, 16, body, edge, 1.25f);\n        d->AddLine(P(115,111), P(115,309), mesh, S(0.70f) * k);\n        d->AddBezierCurve(P(75,112), P(82,145), P(96,158), P(115,163), mesh, S(0.75f) * k);\n        d->AddBezierCurve(P(155,112), P(148,145), P(134,158), P(115,163), mesh, S(0.75f) * k);\n        d->AddLine(P(72,167), P(158,167), mesh, S(0.62f) * k);\n        d->AddLine(P(85,197), P(145,197), mesh, S(0.62f) * k);\n        d->AddLine(P(84,224), P(146,224), mesh, S(0.62f) * k);\n        d->AddLine(P(84,251), P(146,251), mesh, S(0.62f) * k);\n        d->AddLine(P(92,277), P(138,277), mesh, S(0.62f) * k);\n        d->AddBezierCurve(P(84,252), P(96,267), P(105,287), P(115,306), mesh, S(0.55f) * k);\n        d->AddBezierCurve(P(146,252), P(134,267), P(125,287), P(115,306), mesh, S(0.55f) * k);\n\n        // Arms\n        ImVec2 la[] = { P(68,116), P(50,126), P(39,160), P(34,209), P(38,229), P(49,229), P(58,181), P(77,145) };\n        ImVec2 ra[] = { P(162,116), P(180,126), P(191,160), P(196,209), P(192,229), P(181,229), P(172,181), P(153,145) };\n        Poly(la, 8, body, edge);\n        Poly(ra, 8, body, edge);\n        d->AddCircle(P(43,228), S(10.f) * k, mesh, 22, S(0.8f) * k);\n        d->AddCircle(P(187,228), S(10.f) * k, mesh, 22, S(0.8f) * k);\n        d->AddLine(P(50,131), P(38,219), mesh, S(0.55f) * k);\n        d->AddLine(P(180,131), P(192,219), mesh, S(0.55f) * k);\n\n        // Hands / fingers\n        for (int side = 0; side < 2; ++side) {\n            const float sx = side ? 188.f : 42.f;\n            const float dir = side ? 1.f : -1.f;\n            for (int f = 0; f < 4; ++f) {\n                const float xx = sx + dir * ((float)f - 1.5f) * 3.2f;\n                d->AddLine(P(xx,238), P(xx + dir * 2.f, 258.f + f * 2.2f), mesh, S(0.65f) * k);\n            }\n        }\n\n        // Legs\n        ImVec2 ll[] = { P(93,290), P(114,308), P(109,356), P(108,438), P(102,482), P(78,482), P(77,438), P(75,327) };\n        ImVec2 rl[] = { P(116,308), P(137,290), P(155,327), P(153,438), P(152,482), P(128,482), P(122,438), P(121,356) };\n        Poly(ll, 8, body, edge);\n        Poly(rl, 8, body, edge);\n        Ellipse(P(92,360), 15, 16, body, mesh, 0.8f);\n        Ellipse(P(138,360), 15, 16, body, mesh, 0.8f);\n        d->AddLine(P(78,407), P(108,407), mesh, S(0.62f) * k);\n        d->AddLine(P(122,407), P(152,407), mesh, S(0.62f) * k);\n\n        // Feet\n        ImVec2 lf[] = { P(78,477), P(102,477), P(108,494), P(72,500), P(65,494), P(68,486) };\n        ImVec2 rf[] = { P(128,477), P(152,477), P(162,486), P(165,494), P(158,500), P(122,494) };\n        Poly(lf, 6, body, edge);\n        Poly(rf, 6, body, edge);\n\n        auto Overlay = [&](int part, ImU32 fill, ImU32 line) {\n            switch (part) {\n            case 0:\n                Ellipse(P(115,52), 31, 39, fill, line, 1.5f);\n                break;\n            case 1: {\n                ImVec2 q[] = { P(103,88),P(127,88),P(130,108),P(115,115),P(100,108) };\n                Poly(q,5,fill,line,1.5f); break;\n            }\n            case 2: {\n                ImVec2 q[] = { P(75,110),P(95,102),P(115,111),P(135,102),P(155,110),P(165,143),P(158,171),P(146,197),P(84,197),P(72,171),P(65,143) };\n                Poly(q,11,fill,line,1.5f); break;\n            }\n            case 3: {\n                ImVec2 q[] = { P(84,197),P(146,197),P(146,251),P(84,251) };\n                Poly(q,4,fill,line,1.5f); break;\n            }\n            case 4: {\n                ImVec2 q[] = { P(84,251),P(146,251),P(137,290),P(115,309),P(93,290) };\n                Poly(q,5,fill,line,1.5f); break;\n            }\n            case 5: Poly(la,8,fill,line,1.5f); break;\n            case 6: Poly(ra,8,fill,line,1.5f); break;\n            case 7: Poly(ll,8,fill,line,1.5f); break;\n            case 8: Poly(rl,8,fill,line,1.5f); break;\n            }\n        };\n\n        struct H { int id; float x1,y1,x2,y2; };\n        static const H hs[] = {\n            {0,82,12,148,93}, {1,98,84,132,116}, {2,62,103,168,199},\n            {3,80,194,150,254}, {4,80,248,150,313},\n            {5,28,110,80,269}, {6,150,110,202,269},\n            {7,68,286,116,503}, {8,114,286,162,503}\n        };\n\n        const ImVec2 mp = ImGui::GetIO().MousePos;\n        int hovered = -1;\n        for (const auto& h : hs) {\n            const ImVec2 a = P(h.x1,h.y1), b = P(h.x2,h.y2);\n            if (mp.x >= a.x && mp.x <= b.x && mp.y >= a.y && mp.y <= b.y) {\n                hovered = h.id;\n                break;\n            }\n        }\n\n        ImGui::SetCursorScreenPos(ImVec2(ox,oy));\n        ImGui::InvisibleButton("##humanhb_v2", ImVec2(bw,bh));\n        if (ImGui::IsItemClicked() && hovered >= 0)\n            *sel = hovered;\n\n        if (hovered >= 0 && hovered != *sel)\n            Overlay(hovered, U32(WithA(g_accent,0.10f)), U32(WithA(g_accent,0.42f)));\n        if (*sel >= 0 && *sel < 9)\n            Overlay(*sel, U32(WithA(g_accent,0.28f)), U32(g_accent));\n\n        const char* names[] = { "HEAD","NECK","CHEST","STOMACH","PELVIS","L. ARM","R. ARM","L. LEG","R. LEG" };\n        if (*sel >= 0 && *sel < 9) {\n            const char* nm = names[*sel];\n            Text(ImVec2(ox + (bw - TW(nm))*0.5f, oy + bh + S(2.f)), U32(g_accent), nm);\n        }\n    }\n'
NEW_AIMBOT = '\n    void PageAimbot(float x, float y, float w, float h)\n    {\n        auto& a = Esp::g_aimbot;\n        auto& t = Esp::g_trigger;\n\n        PageTitle(x, y, "AIMBOT");\n        Text(ImVec2(x + S(30.f), y + S(22.f)), U32(DIM), "Configure aim assistance and targeting preferences.");\n\n        const float bodyY = y + S(58.f);\n        const float gap = S(14.f);\n        const float availableH = h - S(58.f);\n\n        const float leftW = (std::max)(S(390.f), w * 0.41f);\n        const float rightX = x + leftW + gap;\n        const float rightW = w - leftW - gap;\n\n        BeginCard("HITBOX", x, bodyY, leftW, 3);\n        {\n            const float ix = x + S(16.f);\n            Text(ImVec2(ix, CardBodyY()), U32(g_accent), "SELECTED");\n            const char* names[] = { "Head","Neck","Chest","Stomach","Pelvis","Left arm","Right arm","Left leg","Right leg" };\n            const char* nm = (a.hitbox >= 0 && a.hitbox < 9) ? names[a.hitbox] : "None";\n            Text(ImVec2(ix, CardBodyY() + S(20.f)), U32(TEXT), nm);\n            Text(ImVec2(ix, CardBodyY() + S(42.f)), U32(DIM), "Click a body region");\n\n            const float figureTop = bodyY + S(86.f);\n            const float figureBottom = bodyY + availableH - S(58.f);\n            const float figureH = figureBottom - figureTop;\n            const float figureW = leftW - S(116.f);\n            DrawHumanHitbox(x + S(94.f), figureTop, figureW, figureH, &a.hitbox);\n\n            const float qy = bodyY + availableH - S(48.f);\n            const float qx = x + S(16.f);\n            const float qgap = S(5.f);\n            const float qw = (leftW - S(32.f) - qgap * 4.f) / 5.f;\n            const char* qn[] = { "Head","Neck","Chest","Stomach","Pelvis" };\n            for (int i=0;i<5;++i) {\n                char id[24]; std::snprintf(id,sizeof(id),"##hbq%d",i);\n                if (Button(id, qx + i*(qw+qgap), qy, qw, S(30.f), qn[i], a.hitbox == i))\n                    a.hitbox = i;\n            }\n        }\n        EndCard(bodyY + availableH - S(14.f));\n\n        const float colGap = S(14.f);\n        const float colW = (rightW - colGap) * 0.5f;\n        const float topH = S(286.f);\n        const float row2 = bodyY + topH + gap;\n        const float bottomH = availableH - topH - gap;\n\n        BeginCard("TARGET SELECTION", rightX, bodyY, colW, 1);\n        {\n            float ry = CardBodyY();\n            const float ix = rightX + S(16.f), iw = colW - S(32.f);\n            const char* selection[] = { "FOV","Distance","Health" };\n            ry = RowCombo("Selection", ix, ry, iw, &a.selection, selection, 3);\n            const char* aimTypes[] = { "Hold","Toggle","Always" };\n            ry = RowCombo("Aim Type", ix, ry, iw, &a.aimType, aimTypes, 3);\n            ry = RowKey("Aim Key", ix, ry, iw, &a.aimKey);\n            ry = RowToggle("Enabled##aim", ix, ry, iw, &a.enable);\n            ry = RowToggle("Silent", ix, ry, iw, &a.silent);\n            ry = RowToggle("Prefer Body", ix, ry, iw, &a.preferBody);\n        }\n        EndCard(bodyY + topH - S(14.f));\n\n        BeginCard("ACCURACY", rightX + colW + colGap, bodyY, colW, 2);\n        {\n            float ry = CardBodyY();\n            const float ix = rightX + colW + colGap + S(16.f), iw = colW - S(32.f);\n            ry = RowSlider("FOV", ix, ry, iw, &a.fov, 0.f, 30.f, "%.1f");\n            ry = RowSlider("Smooth", ix, ry, iw, &a.smooth, 0.f, 1.f, "%.2f");\n            ry = RowSliderI("Hit Chance", ix, ry, iw, &a.hitChance, 0, 100, "%d%%");\n            ry = RowSliderI("Minimum Damage", ix, ry, iw, &a.minDamage, 0, 100, "%d");\n            ry = RowToggle("Multipoint", ix, ry, iw, &a.multipoint);\n        }\n        EndCard(bodyY + topH - S(14.f));\n\n        BeginCard("AIM / RCS", rightX, row2, colW, 1);\n        {\n            float ry = CardBodyY();\n            const float ix = rightX + S(16.f), iw = colW - S(32.f);\n            ry = RowToggle("Draw FOV", ix, ry, iw, &a.drawFov);\n            ry = RowToggle("Recoil Control", ix, ry, iw, &a.rcs);\n            ry = RowSliderI("RCS X", ix, ry, iw, &a.rcsX, 0, 100, "%d%%");\n            ry = RowSliderI("RCS Y", ix, ry, iw, &a.rcsY, 0, 100, "%d%%");\n            ry = RowToggle("Auto Stop", ix, ry, iw, &a.autoStop);\n            ry = RowToggle("Safe Points", ix, ry, iw, &a.safePoints);\n        }\n        EndCard(row2 + bottomH - S(14.f));\n\n        BeginCard("TRIGGERBOT", rightX + colW + colGap, row2, colW, 1);\n        {\n            float ry = CardBodyY();\n            const float ix = rightX + colW + colGap + S(16.f), iw = colW - S(32.f);\n            ry = RowToggle("Enabled##trig", ix, ry, iw, &t.enable);\n            ry = RowToggle("Team Check", ix, ry, iw, &t.teamCheck);\n            ry = RowKey("Trigger Key", ix, ry, iw, &t.key);\n            ry = RowSliderI("Delay (ms)", ix, ry, iw, &t.delayMs, 0, 250, "%d");\n            ry = RowSlider("Hitbox Scale", ix, ry, iw, &a.hitboxScale, 0.25f, 1.0f, "%.2f");\n        }\n        EndCard(row2 + bottomH - S(14.f));\n    }\n'
NEW_SIDEBAR = '\n// ============ SIDEBAR ============\n        {\n            const float sx = wp.x, sy = wp.y;\n            RectF(ImVec2(sx, sy), ImVec2(sx + sideW, wp.y + ws.y), U32(BG1), S(10.f));\n            RectF(ImVec2(sx + sideW - S(1.f), sy), ImVec2(sx + sideW, wp.y + ws.y), U32(BORDER));\n\n            Text(ImVec2(sx + S(24.f), sy + S(22.f)), U32(TEXT), "neXus");\n            Text(ImVec2(sx + S(24.f), sy + S(45.f)), U32(g_accent), "CS2");\n            Text(ImVec2(sx + S(49.f), sy + S(45.f)), U32(DIM), "INTERNAL");\n\n            const char* nav[] = { "Dashboard","Aimbot","Visuals","World","Movement","Inventory","Misc","Configs","Lua Scripts","Settings" };\n            float ny = sy + S(88.f);\n            for (int i = 0; i < 10; ++i)\n            {\n                char id[24]; std::snprintf(id, sizeof(id), "##nav%d", i);\n                const float ih = S(42.f);\n                const ImVec2 ip(sx + S(12.f), ny), isz(sideW - S(24.f), ih);\n                bool hov = false;\n                if (Hit(id, ip, isz, hov)) { g_nav = i; g_sub = 0; }\n                const bool act = (g_nav == i);\n                if (act || hov)\n                    RectF(ip, ImVec2(ip.x + isz.x, ip.y + ih),\n                          U32(act ? WithA(g_accent, 0.13f) : WithA(TEXT, 0.035f)), S(7.f));\n                if (act)\n                    RectF(ip, ImVec2(ip.x + S(3.f), ip.y + ih), U32(g_accent), S(2.f));\n\n                const ImVec4 tc = act ? g_accent : (hov ? TEXT : DIM);\n                NavIcon(i, ip.x + S(22.f), ny + ih * 0.5f, U32(tc));\n                Text(ImVec2(ip.x + S(43.f), ny + (ih - TH()) * 0.5f), U32(tc), nav[i]);\n                ny += S(48.f);\n            }\n\n            const float uy = wp.y + ws.y - S(74.f);\n            RectF(ImVec2(sx + S(12.f), uy), ImVec2(sx + sideW - S(12.f), uy + S(58.f)), U32(CARD), S(7.f));\n            Rect(ImVec2(sx + S(12.f), uy), ImVec2(sx + sideW - S(12.f), uy + S(58.f)), U32(BORDER), S(7.f));\n            DL()->AddCircleFilled(ImVec2(sx + S(34.f), uy + S(29.f)), S(14.f), U32(WithA(g_accent, 0.16f)));\n            Text(ImVec2(sx + S(57.f), uy + S(13.f)), U32(TEXT), "neXus User");\n            Text(ImVec2(sx + S(57.f), uy + S(34.f)), U32(g_accent), "Premium");\n        }\n'
NEW_TOPBAR = '\n// ============ TOPBAR ============\n        const float cx = wp.x + sideW;\n        const float contentX = cx + pad;\n        const float contentW = ws.x - sideW - pad * 2.f;\n        const float topH = S(66.f);\n        {\n            ImGui::SetCursorScreenPos(ImVec2(cx, wp.y));\n            ImGui::InvisibleButton("##drag", ImVec2(ws.x - sideW, topH));\n            if (ImGui::IsItemActive()) {\n                const ImVec2 delta = ImGui::GetIO().MouseDelta;\n                ImGui::SetWindowPos(ImVec2(wp.x + delta.x, wp.y + delta.y));\n            }\n\n            Text(ImVec2(contentX, wp.y + S(24.f)), U32(DIM), "NEXUS / CS2");\n\n            const float sw = S(118.f), sh = S(30.f);\n            const float sx = wp.x + ws.x - pad - sw;\n            const float sy = wp.y + S(18.f);\n            RectF(ImVec2(sx,sy), ImVec2(sx+sw,sy+sh), U32(CARD), S(7.f));\n            Rect(ImVec2(sx,sy), ImVec2(sx+sw,sy+sh), U32(BORDER), S(7.f));\n            DL()->AddCircleFilled(ImVec2(sx+S(15.f), sy+sh*0.5f), S(4.f), U32(g_accent));\n            Text(ImVec2(sx+S(27.f),sy+S(7.f)), U32(DIM), "Injected");\n            Text(ImVec2(sx+sw-S(30.f),sy+S(7.f)), U32(g_accent), "CS2");\n\n            RectF(ImVec2(cx, wp.y + topH - S(1.f)), ImVec2(wp.x + ws.x, wp.y + topH), U32(BORDER));\n        }\n'

def find_gui():
    candidates = [
        Path.cwd() / "TempleWare-CS2" / "source" / "gui" / "gui.cpp",
        Path.cwd() / "source" / "gui" / "gui.cpp",
        Path(r"C:\CS\TempleWare\TempleWare-CS2\source\gui\gui.cpp"),
    ]
    for p in candidates:
        if p.is_file():
            return p
    raise FileNotFoundError("gui.cpp bulunamadi. Scripti C:\\CS\\TempleWare icinden calistir.")

def replace_function(src, signature, replacement):
    start = src.find(signature)
    if start < 0:
        raise RuntimeError("Function bulunamadi: " + signature)
    brace = src.find("{", start)
    if brace < 0:
        raise RuntimeError("Function body bulunamadi: " + signature)

    depth = 0
    i = brace
    in_str = in_char = esc = line_comment = block_comment = False
    while i < len(src):
        c = src[i]
        n = src[i+1] if i+1 < len(src) else ""
        if line_comment:
            if c == "\n": line_comment = False
            i += 1; continue
        if block_comment:
            if c == "*" and n == "/": block_comment = False; i += 2
            else: i += 1
            continue
        if in_str:
            if esc: esc = False
            elif c == "\\": esc = True
            elif c == '"': in_str = False
            i += 1; continue
        if in_char:
            if esc: esc = False
            elif c == "\\": esc = True
            elif c == "'": in_char = False
            i += 1; continue
        if c == "/" and n == "/": line_comment = True; i += 2; continue
        if c == "/" and n == "*": block_comment = True; i += 2; continue
        if c == '"': in_str = True; i += 1; continue
        if c == "'": in_char = True; i += 1; continue
        if c == "{": depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return src[:start] + replacement.rstrip() + src[i+1:]
        i += 1
    raise RuntimeError("Function sonu bulunamadi: " + signature)

def replace_between(src, start_marker, end_marker, replacement):
    a = src.find(start_marker)
    if a < 0: raise RuntimeError("Marker bulunamadi: " + start_marker)
    b = src.find(end_marker, a)
    if b < 0: raise RuntimeError("Marker bulunamadi: " + end_marker)
    return src[:a] + replacement.rstrip() + "\n\n        " + src[b:]

def main():
    path = find_gui()
    text = path.read_text(encoding="utf-8-sig")
    original = text

    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    backup = path.with_name("gui.cpp.bak_nexus_v2_" + stamp)
    shutil.copy2(path, backup)

    palette_pattern = (
        r'ImVec4 g_accent = RGBA\([^)]+\);\s*'
        r'const ImVec4 BG0\s*= RGBA\([^)]+\);\s*'
        r'const ImVec4 BG1\s*= RGBA\([^)]+\);\s*'
        r'const ImVec4 CARD\s*= RGBA\([^)]+\);\s*'
        r'const ImVec4 FRAME\s*= RGBA\([^)]+\);\s*'
        r'const ImVec4 BORDER\s*= RGBA\([^)]+\);\s*'
        r'const ImVec4 TEXT\s*= RGBA\([^)]+\);\s*'
        r'const ImVec4 DIM\s*= RGBA\([^)]+\);\s*'
        r'const ImVec4 GREEN\s*= RGBA\([^)]+\);'
    )
    palette = """ImVec4 g_accent = RGBA(255, 126, 8);
    const ImVec4 BG0   = RGBA(10, 12, 16);
    const ImVec4 BG1   = RGBA(14, 16, 21);
    const ImVec4 CARD  = RGBA(19, 22, 28);
    const ImVec4 FRAME = RGBA(28, 31, 39);
    const ImVec4 BORDER= RGBA(39, 43, 53);
    const ImVec4 TEXT  = RGBA(239, 241, 245);
    const ImVec4 DIM   = RGBA(132, 139, 151);
    const ImVec4 GREEN = RGBA(91, 205, 130);"""
    text, n = re.subn(palette_pattern, palette, text, count=1)
    if n != 1: raise RuntimeError("Palette blogu bulunamadi.")

    text = text.replace("const ImVec2 defSize(S(1180.f), S(760.f));", "const ImVec2 defSize(S(1380.f), S(820.f));")
    text = text.replace("ImGui::SetNextWindowSizeConstraints(ImVec2(S(900.f), S(600.f)), ImVec2(6000, 4000));",
                        "ImGui::SetNextWindowSizeConstraints(ImVec2(S(1080.f), S(680.f)), ImVec2(6000, 4000));")
    text = text.replace("const float sideW = S(210.f);", "const float sideW = S(190.f);")
    text = text.replace("s.WindowRounding = S(12.f);", "s.WindowRounding = S(10.f);")
    text = text.replace("ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, S(12.f));",
                        "ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, S(10.f));")

    text = replace_between(text, "// ============ SIDEBAR ============", "// ============ TOPBAR ============", NEW_SIDEBAR)
    text = replace_between(text, "// ============ TOPBAR ============", "// ============ CONTENT ============", NEW_TOPBAR)
    text = replace_function(text, "void DrawHumanHitbox(float ox, float oy, float bw, float bh, int* sel)", NEW_HUMAN)
    text = replace_function(text, "void PageAimbot(float x, float y, float w, float h)", NEW_AIMBOT)

    if text == original:
        raise RuntimeError("Degisiklik uygulanmadi.")

    path.write_text(text, encoding="utf-8-sig")
    print("[OK] Nexus V2 UI uygulandi:", path)
    print("[OK] Backup:", backup)
    print()
    print("Simdi build:")
    print(r"cd C:\CS\TempleWare\TempleWare-CS2")
    print(r"MSBuild.exe TempleWare-CS2.vcxproj /t:Rebuild /p:Configuration=Release /p:Platform=x64")

if __name__ == "__main__":
    main()
