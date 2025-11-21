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

    MBR(size_t dimensions)
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
        if (point.dimensions() != dimensions()) return false;
        for (size_t i = 0; i < dimensions(); ++i) {
            double v = point[i];
            if (v < lower[i] || v > upper[i]) return false;
        }
        return true;
    }
    
    bool intersects(const MBR& other) const {
        if (other.dimensions() != dimensions()) return false;
        for (size_t i = 0; i < dimensions(); ++i) {
            if (upper[i] < other.lower[i] || lower[i] > other.upper[i]) return false;
        }
        return true;
    }
    
    void expand(const Point& point) {
        if (point.dimensions() != dimensions()) return;
        for (size_t i = 0; i < dimensions(); ++i) {
            lower[i] = min(lower[i], point[i]);
            upper[i] = max(upper[i], point[i]);
        }
    }
    void expand(const MBR& other) {
        if (other.dimensions() != dimensions()) return;
        for (size_t i = 0; i < dimensions(); ++i) {
            lower[i] = min(lower[i], other.lower[i]);
            upper[i] = max(upper[i], other.upper[i]);
        }
    }
    double area() const {
        if (!isValid()) return 0.0;
        double a = 1.0;
        for (size_t i = 0; i < dimensions(); ++i) {
            double d = upper[i] - lower[i];
            if (d <= 0) return 0.0;
            a *= d;
        }
        return a;
    }
    
    double perimeter() const {
        if (!isValid()) return 0.0;
        double sum = 0.0;
        for (size_t i = 0; i < dimensions(); ++i) {
            double d = upper[i] - lower[i];
            if (d < 0) d = 0;
            sum += d;
        }
        return 2.0 * sum;
    }

    Point center() const {
        if (!isValid()) return Point();
        Point c(dimensions());
        for (size_t i = 0; i < dimensions(); ++i) {
            c[i] = (lower[i] + upper[i]) / 2.0;
        }
        return c;
    }
    
    bool isValid() const {
        if (dimensions() == 0) return false;
        for (size_t i = 0; i < dimensions(); ++i) {
            if (lower[i] > upper[i]) return false;
            if (lower[i] == numeric_limits<double>::max() && upper[i] == numeric_limits<double>::lowest()) return false;
        }
        return true;
    }
};

}
