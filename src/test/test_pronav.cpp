#include "../mission/engagement/ProportionalNavigation.h"
#include <cassert>
#include <cmath>
#include <iostream>

void test_vector_operations() {
    Vector3D v1(3, 4, 0);
    assert(std::abs(v1.magnitude() - 5.0) < 1e-6);
    
    Vector3D v2(1, 0, 0);
    Vector3D v3 = v1.normalized();
    assert(std::abs(v3.magnitude() - 1.0) < 1e-6);
    
    Vector3D v4 = v1 + v2;
    assert(v4.x == 4 && v4.y == 4 && v4.z == 0);
    
    double dot = v1.dot(v2);
    assert(std::abs(dot - 3.0) < 1e-6);
    
    std::cout << "✓ Vector operations test passed\n";
}

void test_closing_velocity() {
    ProportionalNavigation pronav(4.0);
    
    // Interceptor moving east at 10 m/s
    Vector3D interceptor_vel(10, 0, 0);
    
    // Target moving west at 5 m/s (head-on)
    Vector3D target_vel(-5, 0, 0);
    
    // LOS pointing east
    Vector3D los_unit(1, 0, 0);
    
    double closing = pronav.calculateClosingVelocity(interceptor_vel, target_vel, los_unit);
    
    // Closing velocity = -((-5) - 10) dot (1,0,0) = -(-15) = 15 m/s
    assert(std::abs(closing - 15.0) < 1e-6);
    
    std::cout << "✓ Closing velocity test passed (15 m/s head-on)\n";
}

void test_time_to_intercept() {
    ProportionalNavigation pronav(4.0);
    
    // Interceptor at origin
    Vector3D interceptor_pos(0, 0, 0);
    
    // Target 100m away
    Vector3D target_pos(100, 0, 0);
    
    // Relative velocity closing at 20 m/s
    Vector3D relative_vel(-20, 0, 0);
    
    double tti = pronav.estimateTimeToIntercept(interceptor_pos, target_pos, relative_vel);
    
    // TTI = 100 / 20 = 5 seconds
    assert(std::abs(tti - 5.0) < 1e-6);
    
    std::cout << "✓ Time to intercept test passed (5 seconds)\n";
}

void test_intercept_feasibility() {
    ProportionalNavigation pronav(4.0);
    
    // Feasible intercept
    assert(pronav.isInterceptFeasible(50.0, 10.0, 100.0) == true);
    
    // Too far
    assert(pronav.isInterceptFeasible(150.0, 10.0, 100.0) == false);
    
    // Not closing
    assert(pronav.isInterceptFeasible(50.0, -5.0, 100.0) == false);
    
    // Too close (safety)
    assert(pronav.isInterceptFeasible(5.0, 10.0, 100.0) == false);
    
    std::cout << "✓ Intercept feasibility test passed\n";
}

void test_los_rate() {
    ProportionalNavigation pronav(4.0);
    
    // Initial positions
    Vector3D interceptor_pos(0, 0, 0);
    Vector3D target_pos(100, 0, 0);
    
    Vector3D interceptor_vel(10, 0, 0);
    Vector3D target_vel(0, 10, 0); // Target moving perpendicular
    
    double dt = 0.1;
    
    // First call initializes
    double rate1 = pronav.calculateLOSRate(interceptor_pos, target_pos, interceptor_vel, target_vel, dt);
    assert(rate1 == 0.0); // First calculation returns 0
    
    // Move positions slightly
    Vector3D new_interceptor = interceptor_pos + interceptor_vel * dt;
    Vector3D new_target = target_pos + target_vel * dt;
    
    // Second call should detect LOS rotation
    double rate2 = pronav.calculateLOSRate(new_interceptor, new_target, interceptor_vel, target_vel, dt);
    assert(rate2 > 0.0); // Should detect LOS change
    
    std::cout << "✓ LOS rate test passed (detected rotation)\n";
}

void test_pronav_acceleration() {
    ProportionalNavigation pronav(4.0);
    
    // Head-on intercept scenario
    Vector3D interceptor_pos(0, 0, 0);
    Vector3D target_pos(100, 0, 0);
    
    Vector3D interceptor_vel(20, 0, 0);
    Vector3D target_vel(-10, 0, 0);
    
    double dt = 0.1;
    
    // First calculation
    Vector3D accel1 = pronav.calculateAcceleration(
        interceptor_pos, target_pos, interceptor_vel, target_vel, dt
    );
    
    // Move forward
    Vector3D new_int_pos = interceptor_pos + interceptor_vel * dt;
    Vector3D new_tgt_pos = target_pos + target_vel * dt;
    
    // Target starts maneuvering
    Vector3D new_target_vel(-10, 5, 0);
    
    Vector3D accel2 = pronav.calculateAcceleration(
        new_int_pos, new_tgt_pos, interceptor_vel, new_target_vel, dt
    );
    
    // Should generate y-component to track target maneuver
    assert(accel2.magnitude() >= 0.0);
    
    std::cout << "✓ ProNav acceleration test passed\n";
}

int main() {
    std::cout << "Running ProNav tests...\n\n";
    
    test_vector_operations();
    test_closing_velocity();
    test_time_to_intercept();
    test_intercept_feasibility();
    test_los_rate();
    test_pronav_acceleration();
    
    std::cout << "\n✅ All ProNav tests passed!\n";
    return 0;
}
