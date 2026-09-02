#include "vector.h"
#include <algorithm>

Vector_t& Vector_t::Clamp()
{
	x = std::clamp(x, -89.f, 89.f);
	y = std::clamp(std::remainder(y, 360.0f), -180.f, 180.f);
	z = 0.f;

	return *this;
}
