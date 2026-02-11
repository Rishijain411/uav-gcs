#pragma once

#include <cmath>

// --------------------------------------------------
// Vector3D for 3D navigation calculations
// --------------------------------------------------
struct Vector3D {
    double x;
    double y;
    double z;

    Vector3D() : x(0), y(0), z(0) {}
    Vector3D(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}

    double magnitude() const {
        return std::sqrt(x*x + y*y + z*z);
    }

    Vector3D normalized() const {
        double mag = magnitude();
        if (mag < 1e-9) return Vector3D(0, 0, 0);
        return Vector3D(x / mag, y / mag, z / mag);
    }

    Vector3D operator+(const Vector3D& other) const {
        return Vector3D(x + other.x, y + other.y, z + other.z);
    }

    Vector3D operator-(const Vector3D& other) const {
        return Vector3D(x - other.x, y - other.y, z - other.z);
    }

    Vector3D operator*(double scalar) const {
        return Vector3D(x * scalar, y * scalar, z * scalar);
    }

    double dot(const Vector3D& other) const {
        return x * other.x + y * other.y + z * other.z;
    }

    Vector3D cross(const Vector3D& other) const {
        return Vector3D(
            y * other.z - z * other.y,
            z * other.x - x * other.z,
            x * other.y - y * other.x
        );
    }
};

// --------------------------------------------------
// Proportional Navigation (PN) Guidance
// --------------------------------------------------
class ProportionalNavigation {
public:
    explicit ProportionalNavigation(double nav_constant = 3.0);

    // -------------------------------
    // Existing API (DO NOT CHANGE)
    // -------------------------------
    double calculateLOSRate(
        const Vector3D& interceptor_pos,
        const Vector3D& target_pos,
        double dt);

    // -------------------------------
    // Extended kinematic API (tests)
    // -------------------------------
    double calculateLOSRate(
        const Vector3D& interceptor_pos,
        const Vector3D& target_pos,
        const Vector3D& interceptor_vel,
        const Vector3D& target_vel,
        double dt);

    double estimateTimeToIntercept(
        const Vector3D& interceptor_pos,
        const Vector3D& target_pos,
        const Vector3D& relative_vel);

    double calculateClosingVelocity(
        const Vector3D& interceptor_pos,
        const Vector3D& target_pos,
        const Vector3D& relative_vel);

    bool isInterceptFeasible(
        double closing_velocity,
        double range,
        double max_time);

    Vector3D calculateAcceleration(
        const Vector3D& interceptor_pos,
        const Vector3D& target_pos,
        const Vector3D& interceptor_vel,
        const Vector3D& target_vel,
        double dt);

    void reset();

private:
    // ⚠️ MUST match .cpp exactly
    double nav_constant_;
};
