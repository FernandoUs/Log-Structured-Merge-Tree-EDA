#pragma once

#include <vector>
#include <cmath>
#include <stdexcept>

using namespace std;

namespace spatial {

class Point {
private:
    vector<double> coords;

public:
    Point() : coords() {}
    Point(size_t dimensions) : coords(dimensions, 0.0) {}
    Point(const vector<double>& coordinates) : coords(coordinates) {}
    Point(initializer_list<double> coordinates) : coords(coordinates) {}

    size_t dimensions() const { return coords.size(); }
    double operator[](size_t index) const {
        if (index >= coords.size()) throw out_of_range("Point index out of range");
        return coords[index];
    }
    double& operator[](size_t index) {
        if (index >= coords.size()) throw out_of_range("Point index out of range");
        return coords[index];
    }
    const vector<double>& getCoords() const { return coords; }

    double distanceTo(const Point& other) const {
        if (dimensions() != other.dimensions()) throw invalid_argument("Points must have same dimensions");
        double sum = 0.0;
        for (size_t i = 0; i < dimensions(); ++i) {
            double diff = coords[i] - other.coords[i];
            sum += diff * diff;
        }
        return sqrt(sum);
    }

    bool operator==(const Point& other) const {
        if (dimensions() != other.dimensions()) return false;
        for (size_t i = 0; i < dimensions(); ++i) {
            if (abs(coords[i] - other.coords[i]) > 1e-9) return false;
        }
        return true;
    }

    bool operator!=(const Point& other) const { return !(*this == other); }
};

}
