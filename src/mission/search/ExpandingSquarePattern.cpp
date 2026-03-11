#include "ExpandingSquarePattern.h"
#include <cmath>

using namespace std;

// Approx conversions: meters -> degrees
// Latitude: ~111.32km per degree everywhere
static double metersToDegreesLat(double meters) {
    return meters / 111320.0;
}

// Longitude: scales with cos(latitude)
static double metersToDegreesLon(double meters, double at_lat_deg) {
    const double lat_rad = at_lat_deg * M_PI / 180.0;
    const double meters_per_degree = 111320.0 * std::cos(lat_rad);
    if (std::fabs(meters_per_degree) < 1e-6) return 0.0;
    return meters / meters_per_degree;
}

ExpandingSquarePattern::ExpandingSquarePattern(
    GeoPoint center,
    double initial_leg_m,
    double step_m,
    int max_legs)
    : center_(center),
      initial_leg_(initial_leg_m),
      step_(step_m),
      max_legs_(max_legs),
      current_pos_(center) {}

vector<GeoPoint> ExpandingSquarePattern::next() {

    if (completed())
        return {};

    vector<GeoPoint> waypoints;

    // Emit up to two legs per invocation so we grow the square every turn
    for (int i = 0; i < 2 && !completed(); ++i) {

        // Expand after every two legs to build the spiral correctly
        double leg_length = initial_leg_ + (legs_generated_ / 2) * step_;
        double delta_lat = metersToDegreesLat(leg_length);
        double delta_lon = metersToDegreesLon(leg_length, current_pos_.lat);

        GeoPoint next = current_pos_;
        switch (direction_) {
        case 0: next.lat += delta_lat; break; // North
        case 1: next.lon += delta_lon; break; // East
        case 2: next.lat -= delta_lat; break; // South
        case 3: next.lon -= delta_lon; break; // West
        }

        // Preserve altitude from the current position
        next.alt = current_pos_.alt;

        waypoints.push_back(next);

        // Advance state for the next leg
        current_pos_ = next;
        direction_ = (direction_ + 1) % 4;
        legs_generated_++;
    }

    return waypoints;
}

bool ExpandingSquarePattern::completed() const {
    return legs_generated_ >= max_legs_;
}

void ExpandingSquarePattern::reset() {
    legs_generated_ = 0;
    direction_ = 0;
    current_pos_ = center_;
}
