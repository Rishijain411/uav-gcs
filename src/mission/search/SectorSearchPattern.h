#pragma once
#include "SearchPattern.h"
#include <vector>

class SectorSearchPattern : public SearchPattern {
public:
    // radius_m: distance from center to each vertex
    SectorSearchPattern(const GeoPoint& center, double radius_m);

    std::vector<GeoPoint> next() override;
    bool completed() const override;
    void reset() override;

private:
    GeoPoint center_;
    double radius_;
    int current_step_ = 0;
    bool completed_ = false;

    // Helper to calculate GPS coordinates from center + offset
    GeoPoint calculateOffset(const GeoPoint& start, double dn, double de);
};