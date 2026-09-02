#pragma once

#define INTERVAL_PER_TICK 0.015625f
#define TICK_INTERVAL (INTERVAL_PER_TICK)
#define TIME_TO_TICKS(TIME) (static_cast<int>(0.5f + static_cast<float>(TIME) / TICK_INTERVAL))
#define TICKS_TO_TIME(TICKS) (TICK_INTERVAL * static_cast<float>(TICKS))
#define ROUND_TO_TICKS(TIME) (TICK_INTERVAL * TIME_TO_TICKS(TIME))
#define TICK_NEVER_THINK (-1)

#include "../../cs2/entity/C_CSPlayerPawn/C_CSPlayerPawn.h"
#include "../../cs2/entity/CCSPlayerController/CCSPlayerController.h"
#include "../../cs2/entity/C_EnvSky/C_EnvSky.h"

#include "../../cs2/datatypes/cutlbuffer/cutlbuffer.h"
#include "../../cs2/datatypes/cutl/utlhash/utlhash.h"

#include "../interfaces/CCSGOInput/CCSGOInput.h"
#include "../interfaces/CUserCmd/CUserCmd.h"

#include "../utils/math/math.h"
#include "../utils/math/vector/vector.h"

#include "../utils/memory/memorycommon.h"
#include "../utils/memory/patternscan/patternscan.h"
#include "../utils/memory/vfunc/vfunc.h"
#include "../utils/schema/schema.h"

#include "../interfaces/interfaces.h"

#include "../config/config.h"

struct Globals_t {
	CUserCmd* user_cmd;
	C_CSPlayerPawn* local_pawn;
	CCSPlayerController* local_controller;
};

inline const auto g_ctx = std::make_unique<Globals_t>();