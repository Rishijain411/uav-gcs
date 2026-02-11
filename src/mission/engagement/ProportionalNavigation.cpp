#include "ProportionalNavigation.h"
#include <iostream>
#include <cmath>
#include <limits>

// --------------------------------------------------
// Constructor
// --------------------------------------------------
ProportionalNavigation::ProportionalNavigation(double nav_constant)
    : nav_constant_(nav_constant) {}


// --------------------------------------------------
// LOS angular rate (full kinematic form)
// λ̇ = |r × ṙ| / |r|²
// --------------------------------------------------
double ProportionalNavigation::calculateLOSRate(
    const Vector3D& interceptor_pos,
    const Vector3D& target_pos,
    const Vector3D& interceptor_vel,
    const Vector3D& target_vel,
    double /*dt*/)
{
    Vector3D rel_pos = target_pos - interceptor_pos;
    Vector3D rel_vel = target_vel - interceptor_vel;

    double r2 = rel_pos.dot(rel_pos);
    if (r2 < 1e-6)
        return 0.0;

    Vector3D cross = rel_pos.cross(rel_vel);
    return cross.magnitude() / r2;
}


// --------------------------------------------------
// Legacy LOS rate (position-only, backward compatible)
// --------------------------------------------------
double ProportionalNavigation::calculateLOSRate(
    const Vector3D& interceptor_pos,
    const Vector3D& target_pos,
    double dt)
{
    static Vector3D last_rel_pos;
    Vector3D rel_pos = target_pos - interceptor_pos;

    if (dt <= 0.0)
        return 0.0;

    Vector3D rel_vel = (rel_pos - last_rel_pos) * (1.0 / dt);
    last_rel_pos = rel_pos;

    double r2 = rel_pos.dot(rel_pos);
    if (r2 < 1e-6)
        return 0.0;

    Vector3D cross = rel_pos.cross(rel_vel);
    return cross.magnitude() / r2;
}


// --------------------------------------------------
// Proportional Navigation acceleration
// a = N · Vc · λ̇ · n̂
// --------------------------------------------------
Vector3D ProportionalNavigation::calculateAcceleration(
    const Vector3D& interceptor_pos,
    const Vector3D& target_pos,
    const Vector3D& interceptor_vel,
    const Vector3D& target_vel,
    double dt)
{
    Vector3D rel_pos = target_pos - interceptor_pos;
    double range = rel_pos.magnitude();
    if (range < 1e-6)
        return Vector3D();

    Vector3D los_unit = rel_pos.normalized();
    Vector3D rel_vel = target_vel - interceptor_vel;

    double closing_vel = -rel_vel.dot(los_unit);
    if (closing_vel <= 0.0)
        return Vector3D();

    double los_rate =
        calculateLOSRate(
            interceptor_pos,
            target_pos,
            interceptor_vel,
            target_vel,
            dt);

    // Perpendicular to LOS (2D assumption)
    Vector3D los_normal(-los_unit.y, los_unit.x, 0.0);
    Vector3D accel = los_normal * (nav_constant_ * closing_vel * los_rate);

    // --- Debug trace (allowed by PRD) ---
    std::cout
        << "[PRONAV] LOS_rate=" << los_rate
        << " rad/s, ClosingVel=" << closing_vel
        << " m/s, CmdAccel=" << accel.magnitude()
        << " m/s²\n";

    return accel;
}


// --------------------------------------------------
// Closing velocity (header-consistent)
// --------------------------------------------------
double ProportionalNavigation::calculateClosingVelocity(
    const Vector3D& interceptor_pos,
    const Vector3D& target_pos,
    const Vector3D& relative_vel)
{
    Vector3D los_unit = (target_pos - interceptor_pos).normalized();
    return -relative_vel.dot(los_unit);
}


// --------------------------------------------------
// Time-to-intercept estimate
// --------------------------------------------------
double ProportionalNavigation::estimateTimeToIntercept(
    const Vector3D& interceptor_pos,
    const Vector3D& target_pos,
    const Vector3D& relative_vel)
{
    Vector3D rel_pos = target_pos - interceptor_pos;

    double closing_speed = -relative_vel.dot(rel_pos.normalized());
    if (closing_speed <= 0.0)
        return std::numeric_limits<double>::infinity();

    return rel_pos.magnitude() / closing_speed;
}


// --------------------------------------------------
// Intercept feasibility
// --------------------------------------------------
bool ProportionalNavigation::isInterceptFeasible(
    double closing_velocity,
    double range,
    double max_range)
{
    const double min_safe_range = 10.0;

    if (range > max_range) return false;
    if (range < min_safe_range) return false;
    if (closing_velocity <= 0.0) return false;

    return true;
}


// --------------------------------------------------
// Reset (no internal state yet)
// --------------------------------------------------
void ProportionalNavigation::reset() {
    // Intentionally empty — reserved for future state
}
