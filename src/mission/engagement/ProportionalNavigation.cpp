#include "ProportionalNavigation.h"
#include <algorithm>

ProportionalNavigation::ProportionalNavigation(double nav_constant)
    : nav_constant_(nav_constant) {}

double ProportionalNavigation::calculateLOSRate(
    const Vector3D& interceptor_pos,
    const Vector3D& target_pos,
    const Vector3D& interceptor_vel,
    const Vector3D& target_vel,
    double dt)
{
    // Calculate Line-of-Sight (LOS) vector
    Vector3D los = target_pos - interceptor_pos;
    double range = los.magnitude();
    
    if (range < 1e-6) {
        return 0.0; // Already at target
    }
    
    Vector3D los_unit = los.normalized();
    
    // Calculate LOS rate (angular rate of LOS rotation)
    if (first_calculation_) {
        last_los_unit_ = los_unit;
        first_calculation_ = false;
        return 0.0;
    }
    
    // LOS rate = change in LOS direction / dt
    Vector3D los_change = los_unit - last_los_unit_;
    double los_rate = los_change.magnitude() / dt;
    
    last_los_unit_ = los_unit;
    
    return los_rate;
}

Vector3D ProportionalNavigation::calculateAcceleration(
    const Vector3D& interceptor_pos,
    const Vector3D& target_pos,
    const Vector3D& interceptor_vel,
    const Vector3D& target_vel,
    double dt)
{
    // Calculate relative position and velocity
    Vector3D los = target_pos - interceptor_pos;
    double range = los.magnitude();
    
    if (range < 1e-6) {
        return Vector3D(0, 0, 0);
    }
    
    Vector3D los_unit = los.normalized();
    Vector3D relative_vel = target_vel - interceptor_vel;
    
    // Closing velocity (component along LOS)
    double closing_vel = -relative_vel.dot(los_unit); // Negative = closing
    
    if (closing_vel <= 0) {
        // Target is receding, can't intercept
        return Vector3D(0, 0, 0);
    }
    
    // Calculate LOS rate vector
    Vector3D los_rate_vec(0, 0, 0);
    if (!first_calculation_ && dt > 1e-6) {
        los_rate_vec = (los_unit - last_los_unit_) * (1.0 / dt);
    }
    
    // Proportional Navigation: a = N * Vc * λ_dot
    // where N = nav constant, Vc = closing velocity, λ_dot = LOS rate vector
    Vector3D acceleration = los_rate_vec * nav_constant_ * closing_vel;
    
    last_los_unit_ = los_unit;
    first_calculation_ = false;
    
    return acceleration;
}

double ProportionalNavigation::calculateClosingVelocity(
    const Vector3D& interceptor_vel,
    const Vector3D& target_vel,
    const Vector3D& los_unit)
{
    Vector3D relative_vel = target_vel - interceptor_vel;
    
    // Closing velocity = -relative velocity component along LOS
    // Negative value means closing, positive means opening
    return -relative_vel.dot(los_unit);
}

double ProportionalNavigation::estimateTimeToIntercept(
    const Vector3D& interceptor_pos,
    const Vector3D& target_pos,
    const Vector3D& relative_vel)
{
    Vector3D los = target_pos - interceptor_pos;
    double range = los.magnitude();
    
    if (range < 1e-6) {
        return 0.0;
    }
    
    Vector3D los_unit = los.normalized();
    
    // Closing velocity
    double closing_vel = -relative_vel.dot(los_unit);
    
    if (closing_vel <= 0) {
        return -1.0; // Not closing, infinite time
    }
    
    // Simple estimate: time = range / closing_velocity
    return range / closing_vel;
}

bool ProportionalNavigation::isInterceptFeasible(
    double range,
    double closing_speed,
    double max_intercept_range)
{
    // Check if target is within intercept range
    if (range > max_intercept_range) {
        return false;
    }
    
    // Check if closing (positive closing speed)
    if (closing_speed <= 0) {
        return false;
    }
    
    // Check minimum engagement range (safety)
    const double min_safe_range = 10.0; // meters
    if (range < min_safe_range) {
        return false;
    }
    
    return true;
}
