#pragma once

#include "Point.h"
#include "MBR.h"
#include <functional>
#include <cstdint>
#include <algorithm>

using namespace std;

namespace spatial {

template<typename T>
struct SpatialRecord {
    Point point;
    T data;
    bool isTombstone;
    
    SpatialRecord() : point(), data(), isTombstone(false) {}
    SpatialRecord(const Point& p, const T& d, bool tombstone = false) 
        : point(p), data(d), isTombstone(tombstone) {}
};

class SimpleComparator {
public:
    template<typename T>
    bool operator()(const SpatialRecord<T>& a, const SpatialRecord<T>& b) const {
        return false;
    }
    
    bool operator()(const Point& p1, const Point& p2) const {
        return false;
    }
};

class HilbertCurveComparator {
private:
    const int MAX_ITERATIONS = 16;

    uint64_t hilbertIndex2D(int x, int y, int order) {
        return 0;
    }
    
    int normalize(double value, double min, double max, int order) {
        return 0;
    }
    
public:
    uint64_t computeHilbertIndex(const Point& p, const MBR& bounds) {
        return 0;
    }
    
    template<typename T>
    bool operator()(const SpatialRecord<T>& a, const SpatialRecord<T>& b, const MBR& bounds) const {
        return false;
    }
    
    bool operator()(const Point& p1, const Point& p2, const MBR& bounds) const {
        return false;
    }
};

class ZOrderComparator {
private:
    uint64_t interleaveBits(uint32_t x, uint32_t y) {
        return 0;
    }
    
public:
    uint64_t computeZOrder(const Point& p, const MBR& bounds) {
        return 0;
    }
    
    template<typename T>
    bool operator()(const SpatialRecord<T>& a, const SpatialRecord<T>& b, const MBR& bounds) const {
        return false;
    }
};

}