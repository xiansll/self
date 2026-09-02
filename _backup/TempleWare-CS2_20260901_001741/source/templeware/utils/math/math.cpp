#include "math.h"
#include "../memory/gaa/gaa.h"
#include "../memory/patternscan/patternscan.h"
#include "../../../../external/imgui/imgui_internal.h"

std::unique_ptr<CMath> c_math = std::make_unique<CMath>();

void CMath::vector_angles(Vector_t vec_forward, Vector_t& vec_angles)
{
	if (vec_forward.x == 0.f && vec_forward.y == 0.f)
	{
		vec_angles.y = 0.f;
		vec_angles.x = vec_forward.z > 0.f ? 270.f : 90.f;
	}
	else
	{
		vec_angles.y = rad2deg(std::atan2(vec_forward.y, vec_forward.x));
		if (vec_angles.y < 0.f)
			vec_angles.y += 360.f;

		vec_angles.x = rad2deg(std::atan2(-vec_forward.z, vec_forward.Length2D()));
		if (vec_angles.x < 0.f)
			vec_angles.x += 360.f;
	}

	vec_angles.x = std::remainder(vec_angles.x, 360.f);
	vec_angles.y = std::remainder(vec_angles.y, 360.f);
	vec_angles.z = std::remainder(vec_angles.z, 360.f);
}

void CMath::angle_vectors(Vector_t angles, Vector_t& forward, Vector_t& right, Vector_t& up)
{
	float			angle;
	static float	sr, sp, sy, cr, cp, cy;

	angle = angles.y * (_PI2 / 360);
	sy = sin(angle);
	cy = cos(angle);

	angle = angles.x * (_PI2 / 360);
	sp = sin(angle);
	cp = cos(angle);

	angle = angles.z * (_PI2 / 360);
	sr = sin(angle);
	cr = cos(angle);

	forward.x = cp * cy;
	forward.y = cp * sy;
	forward.z = -sp;

	right.x = (-1 * sr * sp * cy + -1 * cr * -sy);
	right.y = (-1 * sr * sp * sy + -1 * cr * cy);
	right.z = -1 * sr * cp;

	up.x = (cr * sp * cy + -sr * -sy);
	up.y = (cr * sp * sy + -sr * cy);
	up.z = cr * cp;
}

Vector_t CMath::aim_direction(Vector_t src, Vector_t dst)
{
	Vector_t result;

	Vector_t delta = dst - src;

	this->vector_angles(delta, result);

	return result;
}

bool CMath::WorldToScreen(const Vector_t& in, ImVec2& out)
{
	if (!ImGui::GetCurrentContext()) return false;

	if (!viewMatrix.viewMatrix)
		viewMatrix.viewMatrix = (viewmatrix_t*)M::getAbsoluteAddress(M::patternScan("client", "48 8D 0D ? ? ? ? 48 C1 E0 06"), 3, 0);

	auto z = viewMatrix.viewMatrix->matrix[3][0] * in.x + viewMatrix.viewMatrix->matrix[3][1] * in.y +
		viewMatrix.viewMatrix->matrix[3][2] * in.z + viewMatrix.viewMatrix->matrix[3][3];

	if (z < 0.001f)
		return false;

	out = { (ImGui::GetIO().DisplaySize.x * 0.5f), (ImGui::GetIO().DisplaySize.y * 0.5f) };
	out.x *= 1.0f + (viewMatrix.viewMatrix->matrix[0][0] * in.x + viewMatrix.viewMatrix->matrix[0][1] * in.y +
		viewMatrix.viewMatrix->matrix[0][2] * in.z + viewMatrix.viewMatrix->matrix[0][3]) /
		z;
	out.y *= 1.0f - (viewMatrix.viewMatrix->matrix[1][0] * in.x + viewMatrix.viewMatrix->matrix[1][1] * in.y +
		viewMatrix.viewMatrix->matrix[1][2] * in.z + viewMatrix.viewMatrix->matrix[1][3]) /
		z;

	out = ImFloor(out);
	return true;
}
