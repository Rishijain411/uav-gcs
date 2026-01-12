#include "ExpandingSquarePattern.h"
#include <cmath>

using namespace std;

// Approx conversion: meters → degrees (latitude)
static double metersToDegrees(double meters) {
    return meters / 111320.0;
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

    double leg_length = initial_leg_ + (current_leg_ * step_);
    double delta = metersToDegrees(leg_length);

    GeoPoint next = current_pos_;

    switch (direction_) {
    case 0: next.lat += delta; break; // North
    case 1: next.lon += delta; break; // East
    case 2: next.lat -= delta; break; // South
    case 3: next.lon -= delta; break; // West
    }

    direction_ = (direction_ + 1) % 4;

    if (direction_ == 0)
        current_leg_++;

    current_pos_ = next;
    return { next };
}

bool ExpandingSquarePattern::completed() const {
    return current_leg_ >= max_legs_;
}

void ExpandingSquarePattern::reset() {
    current_leg_ = 0;
    direction_ = 0;
    current_pos_ = center_;
}
