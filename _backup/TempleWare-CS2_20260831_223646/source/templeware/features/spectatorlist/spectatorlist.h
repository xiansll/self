#pragma once

#include "../../globals/globals.h"

class SpectatorList {
public:
	void OnRender();
};

const auto g_spectatorlist= std::make_unique<SpectatorList>();