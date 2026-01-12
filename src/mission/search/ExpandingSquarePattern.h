#pragma once

#include "SearchPattern.h"

class ExpandingSquarePattern : public SearchPattern {
public:
    ExpandingSquarePattern(
        GeoPoint center,
        double initial_leg_m,
        double step_m,
        int max_legs);

    std::vector<GeoPoint> next() override;
    bool completed() const override;
    void reset() override;

private:
    GeoPoint center_;
    GeoPoint current_pos_;

    double initial_leg_;
    double step_;
    int max_legs_;

    int current_leg_ = 0;
    int direction_ = 0; // 0:N, 1:E, 2:S, 3:W
};
