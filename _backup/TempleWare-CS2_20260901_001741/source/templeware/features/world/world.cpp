#include <algorithm>
#include <iostream>
#include "../../hooks/hooks.h"
#include "../../players/players.h"
#include "../../config/config.h"
#include "../../../../external/imgui/imgui.h"
#include "../../interfaces/interfaces.h"
#include <unordered_map>
#include "../../../cs2/entity/C_EnvSky/C_EnvSky.h"


void* __fastcall H::hkUpdateAggregateSceneObject(void* a1, void* a2, c_aggregate_object_array* a3)
{
	static auto original = H::UpdateAggregateSceneObject.GetOriginal();

	void* result = original(a1, a2, a3);

	if (Config::visual_world.Night && a3 && a3->data && I::SceneSystem && I::SceneSystem->light_data_queue && I::SceneSystem->light_data_queue->light_data)
	{
		c_color floatColor = { (float)Config::visual_world.NightColor.x, (float)Config::visual_world.NightColor.y, (float)Config::visual_world.NightColor.z };

		C_ByteColor Color = (floatColor * 255).to_byte();

		for (int i = 0; i < a3->data->count; i++)
		{
			int index = (a3->data->index + i) << 5;
			*reinterpret_cast<C_ByteColor*>(reinterpret_cast<uintptr_t>(I::SceneSystem->light_data_queue->light_data) + index) = Color;
		}
	}

	return result;
}

void __fastcall H::hkUpdateLightObject(__int64 a1, __int64 a2, __int64 a3)
{
	static auto original = UpdateLightObject.GetOriginal();

	static std::unordered_map<uintptr_t, float[3]> originalColors;

	if (Config::visual_world.Night && a2)
	{
		float* r = (float*)(a2 + 228);
		float* g = (float*)(a2 + 232);
		float* b = (float*)(a2 + 236);

		if (originalColors.find(a2) == originalColors.end())
		{
			originalColors[a2][0] = *r;
			originalColors[a2][1] = *g;
			originalColors[a2][2] = *b;
		}

		*r = originalColors[a2][0] * Config::visual_world.NightColor.x;
		*g = originalColors[a2][1] * Config::visual_world.NightColor.y;
		*b = originalColors[a2][2] * Config::visual_world.NightColor.z;
	}
	else
	{
		auto it = originalColors.find(a2);
		if (it != originalColors.end())
		{
			float* r = (float*)(a2 + 228);
			float* g = (float*)(a2 + 232);
			float* b = (float*)(a2 + 236);
			*r = it->second[0];
			*g = it->second[1];
			*b = it->second[2];
		}
	}

	return original(a1, a2, a3);
}
