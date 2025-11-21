#pragma once

#include "Point.h"
#include "MBR.h"
#include <functional>
#include <cstdint>
#include <algorithm>
#include <cmath>

using namespace std;

namespace spatial {

int normalize(double value, double min, double max, int order) {
    if (max <= min) return 0;
    double span = max - min;
    double t = (value - min) / span;
    if (t < 0) t = 0; 
    if (t > 1) t = 1;
    int levels = 1 << order;
    return static_cast<int>(t * (levels - 1));
}

template<typename T>
struct SpatialRecord {
    Point point;
    T data;
    bool isTombstone;
    
    SpatialRecord() : point(), data(), isTombstone(false) {}
    SpatialRecord(const Point& p, const T& d, bool tombstone = false) 
        : point(p), data(d), isTombstone(tombstone) {}
};

template<typename T>
class ISpatialComparator {
public:
    virtual ~ISpatialComparator() = default;
    virtual bool compare(const SpatialRecord<T>& a, const SpatialRecord<T>& b) const = 0;
};

class SimpleComparator {
public:
    template<typename T>
    bool operator()(const SpatialRecord<T>& a, const SpatialRecord<T>& b) const {
        const Point &p1 = a.point;
        const Point &p2 = b.point;
        size_t d = min(p1.dimensions(), p2.dimensions());
        for (size_t i = 0; i < d; ++i) {
            if (p1[i] < p2[i]) return true;
            if (p1[i] > p2[i]) return false;
        }
        return p1.dimensions() < p2.dimensions();
    }
    
    bool operator()(const Point& p1, const Point& p2) const {
        size_t d = min(p1.dimensions(), p2.dimensions());
        for (size_t i = 0; i < d; ++i) {
            if (p1[i] < p2[i]) return true;
            if (p1[i] > p2[i]) return false;
        }
        return p1.dimensions() < p2.dimensions();
    }
};

template<typename T>
class SimpleComparatorAdapter : public ISpatialComparator<T> {
private:
    SimpleComparator comp;
public:
    bool compare(const SpatialRecord<T>& a, const SpatialRecord<T>& b) const override {
        return comp(a, b);
    }
};

class HilbertCurveComparator {
private:
    const int MAX_ITERATIONS = 16;

    uint64_t hilbertIndex2D(int x, int y, int order) const {
        uint64_t index = 0;
        for (int i = order - 1; i >= 0; --i) {
            uint64_t rx = (x >> i) & 1;
            uint64_t ry = (y >> i) & 1;
            index = (index << 2) | (rx * 3 ^ ry);
        }
        return index;
    }
    
public:
    uint64_t computeHilbertIndex(const Point& p, const MBR& bounds) const {
        int order = min(MAX_ITERATIONS, 15);
        
        if (p.dimensions() < 2) return 0;

        double minx = bounds.getLower()[0];
        double miny = bounds.getLower()[1];
        double maxx = bounds.getUpper()[0];
        double maxy = bounds.getUpper()[1];
        
        int nx = normalize(p[0], minx, maxx, order);
        int ny = normalize(p[1], miny, maxy, order);
        
        return hilbertIndex2D(nx, ny, order);
    }
    
    template<typename T>
    bool operator()(const SpatialRecord<T>& a, const SpatialRecord<T>& b, const MBR& bounds) const {
        return computeHilbertIndex(a.point, bounds) < computeHilbertIndex(b.point, bounds);
    }
    
    bool operator()(const Point& p1, const Point& p2, const MBR& bounds) const {
        return computeHilbertIndex(p1, bounds) < computeHilbertIndex(p2, bounds);
    }
};

template<typename T>
class HilbertComparatorAdapter : public ISpatialComparator<T> {
private:
    HilbertCurveComparator comp;
    MBR globalBounds;
public:
    HilbertComparatorAdapter(const MBR& bounds) : globalBounds(bounds) {}

    bool compare(const SpatialRecord<T>& a, const SpatialRecord<T>& b) const override {
        return comp(a, b, globalBounds);
    }
};

}