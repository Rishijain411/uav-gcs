#include "SectorSearchPattern.h"
#include <cmath>

SectorSearchPattern::SectorSearchPattern(const GeoPoint& center, double radius_m)
    : center_(center), radius_(radius_m) {}

void SectorSearchPattern::reset() {
    current_step_ = 0;
    completed_ = false;
}

bool SectorSearchPattern::completed() const {
    return completed_;
}

std::vector<GeoPoint> SectorSearchPattern::next() {
    if (completed_) return {};

    std::vector<GeoPoint> points;
    
    // Each sector has 3 legs: Center -> Vertex A -> Vertex B -> Center
    // 3 sectors total = 9 legs.
    double angle_deg = (current_step_ / 3) * 120.0;
    int sub_step = current_step_ % 3;

    if (sub_step == 0) {
        // Leg 1: Move from Center to the vertex
        double rad = angle_deg * M_PI / 180.0;
        points.push_back(calculateOffset(center_, radius_ * cos(rad), radius_ * sin(rad)));
    } 
    else if (sub_step == 1) {
        // Leg 2: Move to the next vertex (120 deg chord)
        double rad = (angle_deg + 120.0) * M_PI / 180.0;
        points.push_back(calculateOffset(center_, radius_ * cos(rad), radius_ * sin(rad)));
    } 
    else {
        // Leg 3: Return to Center
        points.push_back(center_);
    }

    current_step_++;
    if (current_step_ >= 9) completed_ = true;

    return points;
}

GeoPoint SectorSearchPattern::calculateOffset(const GeoPoint& start, double dn, double de) {
    // Simple flat-earth approximation for search patterns (5m accuracy)
    const double lat_deg_per_m = 1.0 / 111111.0;
    const double lon_deg_per_m = 1.0 / (111111.0 * cos(start.lat * M_PI / 180.0));
    
    return {
        start.lat + (dn * lat_deg_per_m),
        start.lon + (de * lon_deg_per_m),
        start.alt
    };
}