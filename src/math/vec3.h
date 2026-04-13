#ifndef VEC3_H
#define VEC3_H

#include <cmath>
#include <iostream>

class Vec3 {
public:
    float x, y, z;

    // Constructors
    Vec3() : x(0), y(0), z(0) {}
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}

    // Access operators
    float operator[](int i) const {
        if (i == 0) return x;
        if (i == 1) return y;
        return z;
    }

    float& operator[](int i) {
        if (i == 0) return x;
        if (i == 1) return y;
        return z;
    }

    // Vector operations
    Vec3 operator-() const { return Vec3(-x, -y, -z); }

    Vec3 operator+(const Vec3& v) const {
        return Vec3(x + v.x, y + v.y, z + v.z);
    }

    Vec3 operator-(const Vec3& v) const {
        return Vec3(x - v.x, y - v.y, z - v.z);
    }

    Vec3 operator*(const Vec3& v) const {
        return Vec3(x * v.x, y * v.y, z * v.z);
    }

    Vec3 operator*(float t) const {
        return Vec3(x * t, y * t, z * t);
    }

    Vec3 operator/(float t) const {
        return Vec3(x / t, y / t, z / t);
    }

    Vec3& operator+=(const Vec3& v) {
        x += v.x;
        y += v.y;
        z += v.z;
        return *this;
    }

    Vec3& operator*=(float t) {
        x *= t;
        y *= t;
        z *= t;
        return *this;
    }

    // Dot product
    float dot(const Vec3& v) const {
        return x * v.x + y * v.y + z * v.z;
    }

    // Cross product
    Vec3 cross(const Vec3& v) const {
        return Vec3(
            y * v.z - z * v.y,
            z * v.x - x * v.z,
            x * v.y - y * v.x
        );
    }

    // Length and normalization
    float length() const {
        return std::sqrt(length_squared());
    }

    float length_squared() const {
        return x * x + y * y + z * z;
    }

    Vec3 normalized() const {
        float len = length();
        if (len > 0) {
            return *this / len;
        }
        return Vec3(0, 0, 0);
    }

    bool near_zero() const {
        // Return true if the vector is very close to zero in all dimensions
        const auto s = 1e-8f;
        return (std::fabs(x) < s) && (std::fabs(y) < s) && (std::fabs(z) < s);
    }

    // Static utility functions
    static Vec3 random() {
        return Vec3(
            static_cast<float>(rand()) / static_cast<float>(RAND_MAX),
            static_cast<float>(rand()) / static_cast<float>(RAND_MAX),
            static_cast<float>(rand()) / static_cast<float>(RAND_MAX)
        );
    }

    static Vec3 random(float min, float max) {
        return Vec3(
            min + (max - min) * (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)),
            min + (max - min) * (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)),
            min + (max - min) * (static_cast<float>(rand()) / static_cast<float>(RAND_MAX))
        );
    }
};

// Vector utility functions
inline std::ostream& operator<<(std::ostream& out, const Vec3& v) {
    return out << v.x << ' ' << v.y << ' ' << v.z;
}

inline Vec3 operator*(float t, const Vec3& v) {
    return v * t;
}

inline float dot(const Vec3& u, const Vec3& v) {
    return u.dot(v);
}

inline Vec3 cross(const Vec3& u, const Vec3& v) {
    return u.cross(v);
}

inline Vec3 unit_vector(Vec3 v) {
    return v.normalized();
}

// Reflect function for reflections
inline Vec3 reflect(const Vec3& v, const Vec3& n) {
    return v - 2 * dot(v, n) * n;
}

// Type aliases for Vec3
using Point3 = Vec3;   // 3D point
using Color = Vec3;    // RGB color

#endif // VEC3_H
