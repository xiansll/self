#pragma once
#include "econ_item_system.h"
#include "features.h"

namespace features::skinchanger {

inline void Initialize() {
    g_econ_item_system.Initialize();
}

inline void OnFrameStageNotify() {
    if (!Config::skin_changer.enabled) return;

    if (Config::skin_changer.guns) g_guns.OnFrameStageNotify();
    if (Config::skin_changer.knives) g_knives.OnFrameStageNotify();
    if (Config::skin_changer.gloves) g_gloves.OnFrameStageNotify();
    if (Config::skin_changer.agents) g_agents.OnFrameStageNotify();
}

inline void Shutdown() {
    g_econ_item_system.FlushSkinImages();
}

} // namespace features::skinchanger