#pragma once

#include <cmath>

namespace BeamCast {

// 2D Vector for positions, directions, velocities
struct Vec2 {
    double x, y;

    Vec2() : x(0.0), y(0.0) {}
    Vec2(double x_, double y_) : x(x_), y(y_) {}

    // Vector operations
    Vec2 operator+(const Vec2& other) const { return Vec2(x + other.x, y + other.y); }
    Vec2 operator-(const Vec2& other) const { return Vec2(x - other.x, y - other.y); }
    Vec2 operator*(double scalar) const { return Vec2(x * scalar, y * scalar); }
    Vec2 operator/(double scalar) const { return Vec2(x / scalar, y / scalar); }

    Vec2& operator+=(const Vec2& other) { x += other.x; y += other.y; return *this; }
    Vec2& operator-=(const Vec2& other) { x -= other.x; y -= other.y; return *this; }
    Vec2& operator*=(double scalar) { x *= scalar; y *= scalar; return *this; }

    // Dot product
    double dot(const Vec2& other) const { return x * other.x + y * other.y; }

    // Length and normalization
    double length() const { return std::sqrt(x * x + y * y); }
    double lengthSquared() const { return x * x + y * y; }

    Vec2 normalized() const {
        double len = length();
        if (len < 1e-10) return Vec2(0, 0);
        return *this / len;
    }

    // Rotation (for angles, wedges, etc.)
    Vec2 rotated(double radians) const {
        double c = std::cos(radians);
        double s = std::sin(radians);
        return Vec2(x * c - y * s, x * s + y * c);
    }

    // Perpendicular vector (90° counter-clockwise)
    Vec2 perpendicular() const { return Vec2(-y, x); }

    // Angle from this vector to another (in radians)
    double angleTo(const Vec2& other) const {
        return std::atan2(other.y - y, other.x - x);
    }

    // Distance to another point
    double distanceTo(const Vec2& other) const {
        return (*this - other).length();
    }
};

// Scalar multiplication from left
inline Vec2 operator*(double scalar, const Vec2& vec) {
    return vec * scalar;
}

// Useful constants
namespace Math {
    constexpr double PI = 3.14159265358979323846;
    constexpr double TWO_PI = 2.0 * PI;
    constexpr double HALF_PI = PI / 2.0;

    // Convert degrees to radians
    inline double toRadians(double degrees) { return degrees * PI / 180.0; }

    // Convert radians to degrees
    inline double toDegrees(double radians) { return radians * 180.0 / PI; }

    // Clamp value between min and max
    inline double clamp(double value, double min, double max) {
        if (value < min) return min;
        if (value > max) return max;
        return value;
    }
}

} // namespace BeamCast
