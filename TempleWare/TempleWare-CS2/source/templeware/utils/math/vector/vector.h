#pragma once
#include "../../module/module.h"

#include <limits>
#include <cmath>
#include <numbers>
#include "../../../../../external/imgui/imgui.h"

constexpr auto _PI = 3.14159265358979323846;

constexpr auto _PI2 = _PI * 2;

constexpr auto _RAD_PI = 180 / _PI;

constexpr auto _DEG_PI = _PI / 180;

#define deg2rad(degrees) (degrees * _DEG_PI)
#define rad2deg(radians) (radians * _RAD_PI)

struct QAngle_t;
struct Matrix3x4_t;

struct Vector2D_t {
	constexpr Vector2D_t(const float x = 0.0f, const float y = 0.0f) :
		x(x), y(y) {
	}

	[[nodiscard]] bool IsZero() const {
		return (this->x == 0.0f && this->y == 0.0f);
	}

	float x = 0.0f, y = 0.0f;
};

struct Vector_t {
public:
	constexpr Vector_t(const float x = 0.0f, const float y = 0.0f, const float z = 0.0f) :
		x(x), y(y), z(z) {
	}

	constexpr Vector_t(const float* arrVector) :
		x(arrVector[0]), y(arrVector[1]), z(arrVector[2]) {
	}

	constexpr Vector_t(const Vector2D_t& vecBase2D) :
		x(vecBase2D.x), y(vecBase2D.y) {
	}

	float Dot(const Vector_t& other) const {
		return (x * other.x + y * other.y + z * other.z);
	}

	float DotProduct(const Vector_t& vecDot, const bool bAdditional = false) const {
		if (bAdditional)
			return this->x * vecDot.y + this->y * vecDot.x + this->z * vecDot.z;

		return (this->x * vecDot.x + this->y * vecDot.y + this->z * vecDot.z);
	}

	float LengthSqr() const {
		return x * x + y * y + z * z;
	}

	float Length2D() const {
		return sqrtf(x * x + y * y);
	}

	float Length2DSqr() const {
		return x * x + y * y;
	}

	float DistTo(const Vector_t& other) const {
		float dx = x - other.x;
		float dy = y - other.y;
		float dz = z - other.z;
		return sqrtf(dx * dx + dy * dy + dz * dz);
	}

	float dist(Vector_t vec) {
		return (*this - vec).Length();
	}

	Vector_t RoundFloat() const {
		return Vector_t(
			std::floor(x + 0.5f),
			std::floor(y + 0.5f),
			std::floor(z + 0.5f)
		);
	}

	ImVec2 im()
	{
		return ImVec2(static_cast<int>(x), static_cast<int>(y));
	}

	[[nodiscard]] float& operator[](const int nIndex) {
		return reinterpret_cast<float*>(this)[nIndex];
	}

	[[nodiscard]] const float& operator[](const int nIndex) const {
		return reinterpret_cast<const float*>(this)[nIndex];
	}

	[[nodiscard]] bool IsZero() const {
		return (this->x == 0.0f && this->y == 0.0f && this->z == 0.0f);
	}

	constexpr Vector_t& operator=(const Vector_t& vecBase) {
		this->x = vecBase.x;
		this->y = vecBase.y;
		this->z = vecBase.z;
		return *this;
	}

	constexpr Vector_t& operator=(const Vector2D_t& vecBase2D) {
		this->x = vecBase2D.x;
		this->y = vecBase2D.y;
		this->z = 0.0f;
		return *this;
	}

	constexpr Vector_t& operator+=(const Vector_t& vecBase) {
		this->x += vecBase.x;
		this->y += vecBase.y;
		this->z += vecBase.z;
		return *this;
	}

	constexpr Vector_t& operator-=(const Vector_t& vecBase) {
		this->x -= vecBase.x;
		this->y -= vecBase.y;
		this->z -= vecBase.z;
		return *this;
	}

	constexpr Vector_t& operator*=(const Vector_t& vecBase) {
		this->x *= vecBase.x;
		this->y *= vecBase.y;
		this->z *= vecBase.z;
		return *this;
	}

	constexpr Vector_t& operator/=(const Vector_t& vecBase) {
		this->x /= vecBase.x;
		this->y /= vecBase.y;
		this->z /= vecBase.z;
		return *this;
	}

	constexpr Vector_t& operator+=(const float flAdd) {
		this->x += flAdd;
		this->y += flAdd;
		this->z += flAdd;
		return *this;
	}

	constexpr Vector_t& operator-=(const float flSubtract) {
		this->x -= flSubtract;
		this->y -= flSubtract;
		this->z -= flSubtract;
		return *this;
	}

	constexpr Vector_t& operator*=(const float flMultiply) {
		this->x *= flMultiply;
		this->y *= flMultiply;
		this->z *= flMultiply;
		return *this;
	}

	constexpr Vector_t& operator/=(const float flDivide) {
		this->x /= flDivide;
		this->y /= flDivide;
		this->z /= flDivide;
		return *this;
	}

	constexpr Vector_t& operator-() {
		this->x = -this->x;
		this->y = -this->y;
		this->z = -this->z;
		return *this;
	}

	constexpr Vector_t operator-() const {
		return { -this->x, -this->y, -this->z };
	}

	Vector_t operator+(const Vector_t& vecAdd) const {
		return { this->x + vecAdd.x, this->y + vecAdd.y, this->z + vecAdd.z };
	}

	Vector_t operator-(const Vector_t& vecSubtract) const {
		return { this->x - vecSubtract.x, this->y - vecSubtract.y, this->z - vecSubtract.z };
	}

	Vector_t operator*(const Vector_t& vecMultiply) const {
		return { this->x * vecMultiply.x, this->y * vecMultiply.y, this->z * vecMultiply.z };
	}

	Vector_t operator/(const Vector_t& vecDivide) const {
		return { this->x / vecDivide.x, this->y / vecDivide.y, this->z / vecDivide.z };
	}

	Vector_t operator+(const float flAdd) const {
		return { this->x + flAdd, this->y + flAdd, this->z + flAdd };
	}

	Vector_t operator-(const float flSubtract) const {
		return { this->x - flSubtract, this->y - flSubtract, this->z - flSubtract };
	}

	Vector_t operator*(const float flMultiply) const {
		return { this->x * flMultiply, this->y * flMultiply, this->z * flMultiply };
	}

	Vector_t operator/(const float flDivide) const {
		return { this->x / flDivide, this->y / flDivide, this->z / flDivide };
	}
	Vector_t Normalize() const {
		float len = Length();
		if (len != 0)
			return { x / len, y / len, z / len };
		return { 0, 0, 0 };
	}

	void ToDirections(Vector_t* forward, Vector_t* right, Vector_t* up) const noexcept
	{
		constexpr auto deg_to_rad = std::numbers::pi_v<float> / 180.0f;

		const auto sp = std::sinf(x * deg_to_rad);
		const auto cp = std::cosf(x * deg_to_rad);
		const auto sy = std::sinf(y * deg_to_rad);
		const auto cy = std::cosf(y * deg_to_rad);
		const auto sr = std::sinf(z * deg_to_rad);
		const auto cr = std::cosf(z * deg_to_rad);

		if (forward)
		{
			forward->x = cp * cy;
			forward->y = cp * sy;
			forward->z = -sp;
		}

		if (right)
		{
			right->x = -1.0f * sr * sp * cy + -1.0f * cr * -sy;
			right->y = -1.0f * sr * sp * sy + -1.0f * cr * cy;
			right->z = -1.0f * sr * cp;
		}

		if (up)
		{
			up->x = cr * sp * cy + -sr * -sy;
			up->y = cr * sp * sy + -sr * cy;
			up->z = cr * cp;
		}
	}

	void NormalizeInPlace() {
		float length = sqrtf(x * x + y * y + z * z);
		if (length > 0.0f) {
			float invLength = 1.0f / length;
			x *= invLength;
			y *= invLength;
			z *= invLength;
		}
	}

	Vector_t Lerp(const Vector_t other, float t) const {
		return Vector_t(x + t * (other.x - x),
			y + t * (other.y - y),
			z + t * (other.z - z));
	}
	float Length() const {
		return std::sqrt(x * x + y * y + z * z);
	}
	float Distance(const Vector_t& other) const {
		return std::sqrt((x - other.x) * (x - other.x) +
			(y - other.y) * (y - other.y) +
			(z - other.z) * (z - other.z));
	}

	Vector_t Normalize_Angle() {
		while (x < -180.0f)
			x += 360.0f;
		while (x > 180.0f)
			x -= 360.0f;

		while (y < -180.0f)
			y += 360.0f;
		while (y > 180.0f)
			y -= 360.0f;

		while (z < -180.0f)
			z += 360.0f;
		while (z > 180.0f)
			z -= 360.0f;

		return *this;
	}

	void Clamp(float minVal, float maxVal)
	{
		x = (x < minVal) ? minVal : (x > maxVal) ? maxVal : x;
		y = (y < minVal) ? minVal : (y > maxVal) ? maxVal : y;
		z = (z < minVal) ? minVal : (z > maxVal) ? maxVal : z;
	}

	Vector_t& Clamp();

	float x = 0.0f, y = 0.0f, z = 0.0f;
};

struct Vector4D_t {
	constexpr Vector4D_t(const float x = 0.0f, const float y = 0.0f, const float z = 0.0f, const float w = 0.0f) :
		x(x), y(y), z(z), w(w) {
	}

	float x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f;
};

struct alignas(16) VectorAligned_t : Vector_t {
	VectorAligned_t() = default;

	explicit VectorAligned_t(const Vector_t& vecBase) {
		this->x = vecBase.x;
		this->y = vecBase.y;
		this->z = vecBase.z;
		this->w = 0.0f;
	}

	constexpr VectorAligned_t& operator=(const Vector_t& vecBase) {
		this->x = vecBase.x;
		this->y = vecBase.y;
		this->z = vecBase.z;
		this->w = 0.0f;
		return *this;
	}

	float w = 0.0f;
};

struct Matrix3x4_t;

struct QAngle_t {
	constexpr QAngle_t(float x = 0.f, float y = 0.f, float z = 0.f) :
		x(x), y(y), z(z) {
	}

	constexpr QAngle_t(const float* arrAngles) :
		x(arrAngles[0]), y(arrAngles[1]), z(arrAngles[2]) {
	}

#pragma region qangle_array_operators

	[[nodiscard]] float& operator[](const int nIndex) {
		return reinterpret_cast<float*>(this)[nIndex];
	}

	[[nodiscard]] const float& operator[](const int nIndex) const {
		return reinterpret_cast<const float*>(this)[nIndex];
	}

#pragma endregion

#pragma region qangle_relational_operators

	bool operator==(const QAngle_t& angBase) const {
		return this->IsEqual(angBase);
	}

	bool operator!=(const QAngle_t& angBase) const {
		return !this->IsEqual(angBase);
	}

#pragma endregion

#pragma region qangle_assignment_operators

	constexpr QAngle_t& operator=(const QAngle_t& angBase) {
		this->x = angBase.x;
		this->y = angBase.y;
		this->z = angBase.z;
		return *this;
	}

#pragma endregion

#pragma region qangle_arithmetic_assignment_operators

	constexpr QAngle_t& operator+=(const QAngle_t& angBase) {
		this->x += angBase.x;
		this->y += angBase.y;
		this->z += angBase.z;
		return *this;
	}

	constexpr QAngle_t& operator-=(const QAngle_t& angBase) {
		this->x -= angBase.x;
		this->y -= angBase.y;
		this->z -= angBase.z;
		return *this;
	}

	constexpr QAngle_t& operator*=(const QAngle_t& angBase) {
		this->x *= angBase.x;
		this->y *= angBase.y;
		this->z *= angBase.z;
		return *this;
	}

	constexpr QAngle_t& operator/=(const QAngle_t& angBase) {
		this->x /= angBase.x;
		this->y /= angBase.y;
		this->z /= angBase.z;
		return *this;
	}

	constexpr QAngle_t& operator+=(const float flAdd) {
		this->x += flAdd;
		this->y += flAdd;
		this->z += flAdd;
		return *this;
	}

	constexpr QAngle_t& operator-=(const float flSubtract) {
		this->x -= flSubtract;
		this->y -= flSubtract;
		this->z -= flSubtract;
		return *this;
	}

	constexpr QAngle_t& operator*=(const float flMultiply) {
		this->x *= flMultiply;
		this->y *= flMultiply;
		this->z *= flMultiply;
		return *this;
	}

	constexpr QAngle_t& operator/=(const float flDivide) {
		this->x /= flDivide;
		this->y /= flDivide;
		this->z /= flDivide;
		return *this;
	}

#pragma endregion

#pragma region qangle_arithmetic_unary_operators

	constexpr QAngle_t& operator-() {
		this->x = -this->x;
		this->y = -this->y;
		this->z = -this->z;
		return *this;
	}

	constexpr QAngle_t operator-() const {
		return { -this->x, -this->y, -this->z };
	}

#pragma endregion

#pragma region qangle_arithmetic_ternary_operators

	constexpr QAngle_t operator+(const QAngle_t& angAdd) const {
		return { this->x + angAdd.x, this->y + angAdd.y, this->z + angAdd.z };
	}

	constexpr QAngle_t operator-(const QAngle_t& angSubtract) const {
		return { this->x - angSubtract.x, this->y - angSubtract.y, this->z - angSubtract.z };
	}

	constexpr QAngle_t operator*(const QAngle_t& angMultiply) const {
		return { this->x * angMultiply.x, this->y * angMultiply.y, this->z * angMultiply.z };
	}

	constexpr QAngle_t operator/(const QAngle_t& angDivide) const {
		return { this->x / angDivide.x, this->y / angDivide.y, this->z / angDivide.z };
	}

	constexpr QAngle_t operator+(const float flAdd) const {
		return { this->x + flAdd, this->y + flAdd, this->z + flAdd };
	}

	constexpr QAngle_t operator-(const float flSubtract) const {
		return { this->x - flSubtract, this->y - flSubtract, this->z - flSubtract };
	}

	constexpr QAngle_t operator*(const float flMultiply) const {
		return { this->x * flMultiply, this->y * flMultiply, this->z * flMultiply };
	}

	constexpr QAngle_t operator/(const float flDivide) const {
		return { this->x / flDivide, this->y / flDivide, this->z / flDivide };
	}

#pragma endregion

	[[nodiscard]] bool IsValid() const {
		return (std::isfinite(this->x) && std::isfinite(this->y) && std::isfinite(this->z));
	}

	[[nodiscard]] bool IsEqual(const QAngle_t& angEqual, const float flErrorMargin = std::numeric_limits<float>::epsilon()) const {
		return (std::fabsf(this->x - angEqual.x) < flErrorMargin && std::fabsf(this->y - angEqual.y) < flErrorMargin && std::fabsf(this->z - angEqual.z) < flErrorMargin);
	}

	[[nodiscard]] bool IsZero() const {
		return (this->x == 0.0f && this->y == 0.0f && this->z == 0.0f);
	}

	/// @returns: length of hypotenuse
	[[nodiscard]] float Length2D() const {
		return std::sqrtf(x * x + y * y);
	}

	QAngle_t& Normalize() {
		this->x = std::remainderf(this->x, 360.f);
		this->y = std::remainderf(this->y, 360.f);
		this->z = std::remainderf(this->z, 360.f);
		return *this;
	}

	void ToDirections(Vector_t* pvecForward, Vector_t* pvecRight = nullptr, Vector_t* pvecUp = nullptr) const;

	[[nodiscard]] Matrix3x4_t ToMatrix(const Vector_t& vecOrigin = {}) const;

public:
	float x = 0.0f, y = 0.0f, z = 0.0f;
};