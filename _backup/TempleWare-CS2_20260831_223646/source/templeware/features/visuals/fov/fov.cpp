#include "../../../hooks/hooks.h"
#include "../../../config/config.h"

float H::hkGetRenderFov(void* rcx) {
	if (Config::visual_world.fovEnabled) {
		float flTargetFov = Config::visual_world.fov;
		g_flActiveFov = flTargetFov;
	}else
		g_flActiveFov = H::GetRenderFov.GetOriginal()(rcx);

	return g_flActiveFov;
}