#pragma once

#include <vector>

struct GeoPoint {
    double lat;
    double lon;
    double alt;
};

class SearchPattern {
public:
    virtual ~SearchPattern() = default;

    // Generate next set of points to search
    virtual std::vector<GeoPoint> next() = 0;

    // Has the pattern exhausted?
    virtual bool completed() const = 0;

    // Reset pattern (for retry)
    virtual void reset() = 0;
};
