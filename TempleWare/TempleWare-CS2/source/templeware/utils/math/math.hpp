#pragma once

#include <cmath>
#include <cstdint>
#include <numbers>
#include <algorithm>

namespace math {

constexpr float PI = 3.14159265358979323846f;
constexpr float PI_2 = PI * 2.0f;
constexpr float RAD_TO_DEG = 180.0f / PI;
constexpr float DEG_TO_RAD = PI / 180.0f;

inline constexpr float deg2rad(float deg) noexcept { return deg * DEG_TO_RAD; }
inline constexpr float rad2deg(float rad) noexcept { return rad * RAD_TO_DEG; }

struct Vector2D {
    float x = 0.0f, y = 0.0f;

    constexpr Vector2D() noexcept = default;
    constexpr Vector2D(float x_, float y_) noexcept : x(x_), y(y_) {}

    [[nodiscard]] constexpr bool is_zero() const noexcept { return x == 0.0f && y == 0.0f; }

    [[nodiscard]] constexpr float length_sqr() const noexcept { return x * x + y * y; }
    [[nodiscard]] constexpr float length() const noexcept { return std::sqrt(length_sqr()); }

    constexpr Vector2D& operator+=(const Vector2D& v) noexcept { x += v.x; y += v.y; return *this; }
    constexpr Vector2D& operator-=(const Vector2D& v) noexcept { x -= v.x; y -= v.y; return *this; }
    constexpr Vector2D& operator*=(float s) noexcept { x *= s; y *= s; return *this; }
    constexpr Vector2D& operator/=(float s) noexcept { x /= s; y /= s; return *this; }

    [[nodiscard]] constexpr Vector2D operator+(const Vector2D& v) const noexcept { return { x + v.x, y + v.y }; }
    [[nodiscard]] constexpr Vector2D operator-(const Vector2D& v) const noexcept { return { x - v.x, y - v.y }; }
    [[nodiscard]] constexpr Vector2D operator*(float s) const noexcept { return { x * s, y * s }; }
    [[nodiscard]] constexpr Vector2D operator/(float s) const noexcept { return { x / s, y / s }; }
    [[nodiscard]] constexpr Vector2D operator-() const noexcept { return { -x, -y }; }

    [[nodiscard]] constexpr bool operator==(const Vector2D& v) const noexcept { return x == v.x && y == v.y; }
    [[nodiscard]] constexpr bool operator!=(const Vector2D& v) const noexcept { return !(*this == v); }

    [[nodiscard]] constexpr float dot(const Vector2D& v) const noexcept { return x * v.x + y * v.y; }
};

struct Vector3D {
    float x = 0.0f, y = 0.0f, z = 0.0f;

    constexpr Vector3D() noexcept = default;
    constexpr Vector3D(float x_, float y_, float z_) noexcept : x(x_), y(y_), z(z_) {}
    constexpr Vector3D(const Vector2D& v) noexcept : x(v.x), y(v.y), z(0.0f) {}

    [[nodiscard]] constexpr bool is_zero() const noexcept { return x == 0.0f && y == 0.0f && z == 0.0f; }
    [[nodiscard]] constexpr bool is_valid() const noexcept { return std::isfinite(x) && std::isfinite(y) && std::isfinite(z); }

    [[nodiscard]] constexpr float length_sqr() const noexcept { return x * x + y * y + z * z; }
    [[nodiscard]] constexpr float length_2d_sqr() const noexcept { return x * x + y * y; }
    [[nodiscard]] float length() const noexcept {
        const float ls = length_sqr();
        return ls > 0.0f ? std::sqrt(ls) : 0.0f;
    }
    [[nodiscard]] float length_2d() const noexcept {
        const float ls = length_2d_sqr();
        return ls > 0.0f ? std::sqrt(ls) : 0.0f;
    }

    constexpr Vector3D& operator+=(const Vector3D& v) noexcept { x += v.x; y += v.y; z += v.z; return *this; }
    constexpr Vector3D& operator-=(const Vector3D& v) noexcept { x -= v.x; y -= v.y; z -= v.z; return *this; }
    constexpr Vector3D& operator*=(float s) noexcept { x *= s; y *= s; z *= s; return *this; }
    constexpr Vector3D& operator/=(float s) noexcept { x /= s; y /= s; z /= s; return *this; }

    [[nodiscard]] constexpr Vector3D operator+(const Vector3D& v) const noexcept { return { x + v.x, y + v.y, z + v.z }; }
    [[nodiscard]] constexpr Vector3D operator-(const Vector3D& v) const noexcept { return { x - v.x, y - v.y, z - v.z }; }
    [[nodiscard]] constexpr Vector3D operator*(float s) const noexcept { return { x * s, y * s, z * s }; }
    [[nodiscard]] constexpr Vector3D operator/(float s) const noexcept { return { x / s, y / s, z / s }; }
    [[nodiscard]] constexpr Vector3D operator-() const noexcept { return { -x, -y, -z }; }

    [[nodiscard]] constexpr bool operator==(const Vector3D& v) const noexcept { return x == v.x && y == v.y && z == v.z; }
    [[nodiscard]] constexpr bool operator!=(const Vector3D& v) const noexcept { return !(*this == v); }

    [[nodiscard]] constexpr float operator[](int i) const noexcept { return (&x)[i]; }
    [[nodiscard]] constexpr float& operator[](int i) noexcept { return (&x)[i]; }

    [[nodiscard]] constexpr float dot(const Vector3D& v) const noexcept { return x * v.x + y * v.y + z * v.z; }

    [[nodiscard]] constexpr Vector3D cross(const Vector3D& v) const noexcept {
        return { y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x };
    }

    [[nodiscard]] float distance(const Vector3D& v) const noexcept { return (*this - v).length(); }
    [[nodiscard]] float distance_sqr(const Vector3D& v) const noexcept { return (*this - v).length_sqr(); }

    Vector3D& normalize() noexcept {
        const float len = length();
        if (len > 0.0f) {
            const float inv = 1.0f / len;
            x *= inv; y *= inv; z *= inv;
        }
        return *this;
    }

    [[nodiscard]] Vector3D normalized() const noexcept {
        Vector3D r = *this;
        r.normalize();
        return r;
    }

    void to_directions(Vector3D* forward, Vector3D* right = nullptr, Vector3D* up = nullptr) const noexcept {
        const float sp = std::sinf(x * DEG_TO_RAD);
        const float cp = std::cosf(x * DEG_TO_RAD);
        const float sy = std::sinf(y * DEG_TO_RAD);
        const float cy = std::cosf(y * DEG_TO_RAD);

        if (forward) {
            forward->x = cp * cy;
            forward->y = cp * sy;
            forward->z = -sp;
        }

        if (right || up) {
            const float sr = std::sinf(z * DEG_TO_RAD);
            const float cr = std::cosf(z * DEG_TO_RAD);

            if (right) {
                right->x = -sr * sp * cy - cr * sy;
                right->y = -sr * sp * sy + cr * cy;
                right->z = -sr * cp;
            }

            if (up) {
                up->x = cr * sp * cy - sr * sy;
                up->y = cr * sp * sy + sr * cy;
                up->z = cr * cp;
            }
        }
    }
};

struct Vector4D {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f;

    constexpr Vector4D() noexcept = default;
    constexpr Vector4D(float x_, float y_, float z_, float w_) noexcept : x(x_), y(y_), z(z_), w(w_) {}
};

struct QAngle {
    float x = 0.0f, y = 0.0f, z = 0.0f;

    constexpr QAngle() noexcept = default;
    constexpr QAngle(float x_, float y_, float z_) noexcept : x(x_), y(y_), z(z_) {}

    [[nodiscard]] constexpr bool is_zero() const noexcept { return x == 0.0f && y == 0.0f && z == 0.0f; }
    [[nodiscard]] constexpr bool is_valid() const noexcept { return std::isfinite(x) && std::isfinite(y) && std::isfinite(z); }

    constexpr QAngle& operator+=(const QAngle& a) noexcept { x += a.x; y += a.y; z += a.z; return *this; }
    constexpr QAngle& operator-=(const QAngle& a) noexcept { x -= a.x; y -= a.y; z -= a.z; return *this; }
    constexpr QAngle& operator*=(float s) noexcept { x *= s; y *= s; z *= s; return *this; }
    constexpr QAngle& operator/=(float s) noexcept { x /= s; y /= s; z /= s; return *this; }

    [[nodiscard]] constexpr QAngle operator+(const QAngle& a) const noexcept { return { x + a.x, y + a.y, z + a.z }; }
    [[nodiscard]] constexpr QAngle operator-(const QAngle& a) const noexcept { return { x - a.x, y - a.y, z - a.z }; }
    [[nodiscard]] constexpr QAngle operator*(float s) const noexcept { return { x * s, y * s, z * s }; }
    [[nodiscard]] constexpr QAngle operator/(float s) const noexcept { return { x / s, y / s, z / s }; }
    [[nodiscard]] constexpr QAngle operator-() const noexcept { return { -x, -y, -z }; }

    [[nodiscard]] constexpr bool operator==(const QAngle& a) const noexcept { return x == a.x && y == a.y && z == a.z; }
    [[nodiscard]] constexpr bool operator!=(const QAngle& a) const noexcept { return !(*this == a); }

    [[nodiscard]] constexpr float operator[](int i) const noexcept { return (&x)[i]; }
    [[nodiscard]] constexpr float& operator[](int i) noexcept { return (&x)[i]; }

    void normalize() noexcept {
        x = std::remainderf(x, 360.0f);
        y = std::remainderf(y, 360.0f);
        z = std::remainderf(z, 360.0f);
    }

    [[nodiscard]] QAngle normalized() const noexcept {
        QAngle r = *this;
        r.normalize();
        return r;
    }

    [[nodiscard]] float length_2d() const noexcept { return std::sqrtf(x * x + y * y); }

    void to_directions(Vector3D* forward, Vector3D* right = nullptr, Vector3D* up = nullptr) const noexcept {
        const float sp = std::sinf(x * DEG_TO_RAD);
        const float cp = std::cosf(x * DEG_TO_RAD);
        const float sy = std::sinf(y * DEG_TO_RAD);
        const float cy = std::cosf(y * DEG_TO_RAD);

        if (forward) {
            forward->x = cp * cy;
            forward->y = cp * sy;
            forward->z = -sp;
        }

        if (right || up) {
            const float sr = std::sinf(z * DEG_TO_RAD);
            const float cr = std::cosf(z * DEG_TO_RAD);

            if (right) {
                right->x = -sr * sp * cy - cr * sy;
                right->y = -sr * sp * sy + cr * cy;
                right->z = -sr * cp;
            }

            if (up) {
                up->x = cr * sp * cy - sr * sy;
                up->y = cr * sp * sy + sr * cy;
                up->z = cr * cp;
            }
        }
    }
};

struct Matrix3x4 {
    float m[3][4] = {};

    [[nodiscard]] constexpr float* operator[](int i) noexcept { return m[i]; }
    [[nodiscard]] constexpr const float* operator[](int i) const noexcept { return m[i]; }

    [[nodiscard]] Vector3D transform(const Vector3D& v) const noexcept {
        return {
            m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z + m[0][3],
            m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z + m[1][3],
            m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z + m[2][3]
        };
    }
};

struct alignas(16) VectorAligned : Vector3D {
    float w = 0.0f;

    constexpr VectorAligned() noexcept = default;
    constexpr VectorAligned(const Vector3D& v) noexcept : Vector3D(v), w(0.0f) {}
    constexpr VectorAligned& operator=(const Vector3D& v) noexcept {
        x = v.x; y = v.y; z = v.z; w = 0.0f;
        return *this;
    }
};

namespace helpers {

inline void angle_vectors(const QAngle& angles, Vector3D* forward, Vector3D* right = nullptr, Vector3D* up = nullptr) noexcept {
    angles.to_directions(forward, right, up);
}

inline void angle_vectors_2d(float yaw, Vector3D& forward, Vector3D& right) noexcept {
    const float rad = yaw * DEG_TO_RAD;
    const float sy = std::sinf(rad);
    const float cy = std::cosf(rad);
    forward = { cy, sy, 0.0f };
    right = { -sy, cy, 0.0f };
}

inline void normalize_angles(QAngle& angles) noexcept {
    while (angles.y > 180.0f) angles.y -= 360.0f;
    while (angles.y < -180.0f) angles.y += 360.0f;
    while (angles.x > 89.0f) angles.x -= 180.0f;
    while (angles.x < -89.0f) angles.x += 180.0f;
    angles.z = 0.0f;
}

inline void normalize_angle(float& angle) noexcept {
    while (angle > 180.0f) angle -= 360.0f;
    while (angle < -180.0f) angle += 360.0f;
}

[[nodiscard]] inline QAngle vector_to_angle(const Vector3D& forward) noexcept {
    const float pitch = rad2deg(std::atan2(-forward.z, forward.length_2d()));
    const float yaw = rad2deg(std::atan2(forward.y, forward.x));
    return { pitch, yaw, 0.0f };
}

[[nodiscard]] inline QAngle calculate_angle(const Vector3D& src, const Vector3D& dst) noexcept {
    const Vector3D delta = dst - src;
    const float length = delta.length_2d();
    QAngle angles;
    angles.x = rad2deg(std::atan2f(-delta.z, length));
    angles.y = rad2deg(std::atan2f(delta.y, delta.x));
    angles.z = 0.0f;
    return angles;
}

[[nodiscard]] inline float angle_distance(const QAngle& from, const QAngle& to) noexcept {
    const float pitch = to.x - from.x;
    float yaw = to.y - from.y;
    while (yaw > 180.0f) yaw -= 360.0f;
    while (yaw < -180.0f) yaw += 360.0f;
    return std::sqrtf(pitch * pitch + yaw * yaw);
}

[[nodiscard]] inline constexpr float deg_to_rad(float degrees) noexcept { return degrees * DEG_TO_RAD; }
[[nodiscard]] inline constexpr float rad_to_deg(float radians) noexcept { return radians * RAD_TO_DEG; }

[[nodiscard]] inline float normalize_yaw(float yaw) noexcept {
    while (yaw > 180.0f) yaw -= 360.0f;
    while (yaw < -180.0f) yaw += 360.0f;
    return yaw;
}

} // namespace helpers

} // namespace math

using Vec2 = math::Vector2D;
using Vec3 = math::Vector3D;
using Vec4 = math::Vector4D;
using Ang = math::QAngle;
using Mat3x4 = math::Matrix3x4;

using Vector2D_t = math::Vector2D;
using Vector_t = math::Vector3D;
using Vector4D_t = math::Vector4D;
using QAngle_t = math::QAngle;
using Matrix3x4_t = math::Matrix3x4;
using VectorAligned_t = math::VectorAligned;

constexpr float _PI = math::PI;
constexpr float _PI2 = math::PI_2;
constexpr float _RAD_PI = math::RAD_TO_DEG;
constexpr float _DEG_PI = math::DEG_TO_RAD;
#define deg2rad(degrees) math::deg2rad(degrees)
#define rad2deg(radians) math::rad2deg(radians)