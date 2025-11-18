#pragma once

#include "Point.h"
#include <algorithm>
#include <limits>

namespace spatial {

class MBR {
private:
    Point lower;
    Point upper;
    
public:
    MBR() : lower(), upper() {}

    explicit MBR(size_t dimensions)
        : lower(dimensions), upper(dimensions) {
        for (size_t i = 0; i < dimensions; ++i) {
            lower[i] = numeric_limits<double>::max();
            upper[i] = numeric_limits<double>::lowest();
        }
    }

    MBR(const Point& lowerBound, const Point& upperBound)
        : lower(lowerBound), upper(upperBound) {
        if (lower.dimensions() != upper.dimensions()) {
            throw invalid_argument("MBR bounds must have same dimensions");
        }
    }

    const Point& getLower() const { return lower; }

    const Point& getUpper() const { return upper; }

    size_t dimensions() const { return lower.dimensions(); }

    void setLower(const Point& p) { lower = p; }

    void setUpper(const Point& p) { upper = p; }

    bool contains(const Point& point) const {
        return false;
    }
    
    bool intersects(const MBR& other) const {
        return false;
    }
    
    void expand(const Point& point) {
    }
    
    void expand(const MBR& other) {
    }
    
    double area() const {
        return 0.0;
    }
    
    double perimeter() const {
        return 0.0;
    }

    Point center() const {
        return Point();
    }
    
    bool isValid() const {
        return true;
    }
};

}
