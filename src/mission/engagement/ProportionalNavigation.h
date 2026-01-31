#pragma once

#include <cmath>

// Vector3D for 3D navigation calculations
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
        return Vector3D(x/mag, y/mag, z/mag);
    }
    
    Vector3D operator-(const Vector3D& other) const {
        return Vector3D(x - other.x, y - other.y, z - other.z);
    }
    
    Vector3D operator+(const Vector3D& other) const {
        return Vector3D(x + other.x, y + other.y, z + other.z);
    }
    
    Vector3D operator*(double scalar) const {
        return Vector3D(x * scalar, y * scalar, z * scalar);
    }
    
    double dot(const Vector3D& other) const {
        return x*other.x + y*other.y + z*other.z;
    }
    
    Vector3D cross(const Vector3D& other) const {
        return Vector3D(
            y*other.z - z*other.y,
            z*other.x - x*other.z,
            x*other.y - y*other.x
        );
    }
};

// Proportional Navigation guidance
class ProportionalNavigation {
public:
    // Navigation constant (typically 3-5)
    explicit ProportionalNavigation(double nav_constant = 4.0);
    
    // Calculate Line-of-Sight (LOS) rate
    // interceptor_pos: current interceptor position
    // target_pos: current target position
    // interceptor_vel: current interceptor velocity
    // target_vel: current target velocity
    // dt: time delta since last calculation
    double calculateLOSRate(
        const Vector3D& interceptor_pos,
        const Vector3D& target_pos,
        const Vector3D& interceptor_vel,
        const Vector3D& target_vel,
        double dt
    );
    
    // Calculate required acceleration for intercept
    Vector3D calculateAcceleration(
        const Vector3D& interceptor_pos,
        const Vector3D& target_pos,
        const Vector3D& interceptor_vel,
        const Vector3D& target_vel,
        double dt
    );
    
    // Calculate closing velocity (negative = closing)
    double calculateClosingVelocity(
        const Vector3D& interceptor_vel,
        const Vector3D& target_vel,
        const Vector3D& los_unit
    );
    
    // Estimate time to intercept
    double estimateTimeToIntercept(
        const Vector3D& interceptor_pos,
        const Vector3D& target_pos,
        const Vector3D& relative_vel
    );
    
    // Check if intercept is feasible
    bool isInterceptFeasible(
        double range,
        double closing_speed,
        double max_intercept_range
    );

private:
    double nav_constant_;
    Vector3D last_los_unit_;
    bool first_calculation_ = true;
};
