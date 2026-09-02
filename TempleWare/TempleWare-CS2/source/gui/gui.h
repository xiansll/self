#pragma once

// TempleWare menu — a velocity-inspired design recreated in Dear ImGui:
// accent-tinted translucent cards, an icon/label rail, accent-fill toggles,
// pill sliders, live pastel themes and open/close animation.
namespace Gui
{
    // Draw the whole menu. `alpha` (0..1) drives the open/close fade.
    void Render(float alpha);

    // Rebuild the font atlas at the current UI scale for crisp text.
    // MUST be called each frame BEFORE ImGui::NewFrame(); it is a no-op unless
    // the target pixel size changed.
    void MaybeRebuildFont();

    // Recenter the window on screen next frame (recover a lost/moved window).
    void CenterWindow();
}
