#pragma once
#include <algorithm>
#include "vector/vector.h"
#include "viewmatrix/viewmatrix.h"
#include <memory>

class C_BoneData;
struct Matrix3x4_t;

class CMath {
public:
    ViewMatrix viewMatrix;

    void vector_angles(Vector_t vec_forward, Vector_t& vec_angles);
    void angle_vectors(Vector_t angles, Vector_t& forward, Vector_t& right, Vector_t& up);
    Vector_t aim_direction(Vector_t src, Vector_t dst);
    void VectorTransform(Vector_t& in, Matrix3x4_t& matrix, Vector_t& out);
    bool WorldToScreen(const Vector_t& in, ImVec2& out);
};

extern std::unique_ptr<CMath> c_math;

namespace Math {
    template <typename T>
    constexpr T clamp(T value, T min, T max) {
        return value < min ? min : (value > max ? max : value);
    }

    template<typename T>
    inline T Min(T a, T b) {
        return a < b ? a : b;
    }

    template<typename T>
    inline T Max(T a, T b) {
        return a > b ? a : b;
    }

    template<typename T>
    inline T Clamp(T value, T min, T max) {
        if (value < min) return min;
        if (value > max) return max;
        return value;
    }
}

inline float RadToDeg(float radians) {
    return radians * (180.0f / 3.141592654f);
}

inline float DegToRad(float degrees) {
    return degrees * (3.141592654f / 180.0f);
}

__forceinline float NormalizeAngle(float angle)
{
    angle = fmodf(angle, 360.0f);
    if (angle > 180.0f)
        angle -= 360.0f;
    else if (angle < -180.0f)
        angle += 360.0f;
    return angle;
}

inline float VectorLength2D(const Vector_t& vec) {
    return sqrtf(vec.x * vec.x + vec.y * vec.y);
}

inline Vector_t AngleToVector(const QAngle_t& angles)
{
    float sp, sy, cp, cy;

    sy = sin(angles.y * (3.141592654f / 180.0f));
    cy = cos(angles.y * (3.141592654f / 180.0f));
    sp = sin(angles.x * (3.141592654f / 180.0f));
    cp = cos(angles.x * (3.141592654f / 180.0f));

    return Vector_t(cp * cy, cp * sy, -sp);
}

inline Vector_t AngleToDirection(const QAngle_t& angles)
{
    Vector_t direction;

    // Transform degrees into radians
    float pitch = angles.x * (_PI / 180.0f);
    float yaw = angles.y * (_PI / 180.0f);

    float cosPitch = cosf(pitch);
    direction.x = cosf(yaw) * cosPitch;
    direction.y = sinf(yaw) * cosPitch;
    direction.z = -sinf(pitch);  // Negative becos pitch in inverted in source/source2 games

    return direction;
}

inline QAngle_t VectorToAngle(const Vector_t& direction)
{
    QAngle_t angles;

    if (direction.y == 0.0f && direction.x == 0.0f) {
        angles.x = (direction.z > 0.0f) ? -90.0f : 90.0f;
        angles.y = 0.0f;
    }
    else {
        angles.x = -atan2(direction.z, direction.Length2DSqr()) * (180.0f / 3.141592654f);
        angles.y = atan2(direction.y, direction.x) * (180.0f / 3.141592654f);
    }

    angles.z = 0.0f;
    angles.Normalize();

    return angles;
}

inline void AngleVectors(const Vector_t& angles, Vector_t* forward, Vector_t* right = nullptr, Vector_t* up = nullptr) {
    float sp, sy, cp, cy;

    sy = sinf(angles.y * (3.141592654f / 180.0f));
    cy = cosf(angles.y * (3.141592654f / 180.0f));

    sp = sinf(angles.x * (3.141592654f / 180.0f));
    cp = cosf(angles.x * (3.141592654f / 180.0f));

    if (forward) {
        forward->x = cp * cy;
        forward->y = cp * sy;
        forward->z = -sp;
    }

    if (right || up) {
        float sr, cr;

        sr = sinf(angles.z * (3.141592654f / 180.0f));
        cr = cosf(angles.z * (3.141592654f / 180.0f));

        if (right) {
            right->x = -1.0f * sr * sp * cy + -1.0f * cr * -sy;
            right->y = -1.0f * sr * sp * sy + -1.0f * cr * cy;
            right->z = -1.0f * sr * cp;
        }

        if (up) {
            up->x = cr * sp * cy + -sr * -sy;
            up->y = cr * sp * sy + -sr * cy;
            up->z = cr * cp;
        }
    }
}