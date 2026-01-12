#include <iostream>

using namespace std;

#include "mission/search/ExpandingSquarePattern.h"

int main() {
    GeoPoint center {12.0, 77.0, 100.0};

    ExpandingSquarePattern pattern(
        center,
        100.0,
        50.0,
        4
    );

    while (!pattern.completed()) {
        auto pts = pattern.next();
        for (auto& p : pts) {
            cout << "Lat: " << p.lat
                 << " Lon: " << p.lon << endl;
        }
    }

    cout << "[TEST] Search pattern OK\n";
    return 0;
}
