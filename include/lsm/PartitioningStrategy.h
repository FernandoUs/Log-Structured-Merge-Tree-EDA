#pragma once

#include "../spatial/SpatialComparators.h"
#include "../spatial/MBR.h"
#include "LSMComponent.h"
#include <vector>
#include <memory>
#include <algorithm>
#include <cmath>

using namespace std;

namespace lsm {

namespace sp = spatial;

template<typename T>
class PartitioningStrategy {
public:
    virtual ~PartitioningStrategy() = default;
    
    virtual vector<LSMComponent<T>*> partition(
        const vector<sp::SpatialRecord<T>>& records,
        size_t targetLevel,
        size_t dimensions,
        size_t maxComponentSize) const = 0;
};

template<typename T>
class SizePartitioning : public PartitioningStrategy<T> {
private:
    enum ComparatorType { SIMPLE, HILBERT };
    ComparatorType comparatorType;

public:
    explicit SizePartitioning(bool useHilbert = false)
        : comparatorType(useHilbert ? HILBERT : SIMPLE) {}

    vector<LSMComponent<T>*> partition(
        const vector<sp::SpatialRecord<T>>& records,
        size_t targetLevel,
        size_t dimensions,
        size_t maxComponentSize) const override {
        return {};
    }
};

template<typename T>
class STRPartitioning : public PartitioningStrategy<T> {
public:
    vector<LSMComponent<T>*> partition(
        const vector<sp::SpatialRecord<T>>& records,
        size_t targetLevel,
        size_t dimensions,
        size_t maxComponentSize) const override {
        return {};
    }

private:
    vector<LSMComponent<T>*> strPartitionRecursive(
        const vector<sp::SpatialRecord<T>>& records,
        size_t targetLevel,
        size_t dimensions,
        size_t maxComponentSize,
        size_t currentDim) const {
        return {};
    }
};

template<typename T>
class RStarGrovePartitioning : public PartitioningStrategy<T> {
private:
    double sampleRatio;

public:
    explicit RStarGrovePartitioning(double sampling = 0.1)
        : sampleRatio(sampling) {}

    vector<LSMComponent<T>*> partition(
        const vector<sp::SpatialRecord<T>>& records,
        size_t targetLevel,
        size_t dimensions,
        size_t maxComponentSize) const override {
        return {};
    }

private:
    vector<sp::SpatialRecord<T>> selectSample(
        const vector<sp::SpatialRecord<T>>& records) const {
        return {};
    }

    vector<sp::MBR> computeBoundaries(
        const vector<sp::SpatialRecord<T>>& sample,
        size_t dimensions,
        size_t maxComponentSize) const {
        return {};
    }

    vector<LSMComponent<T>*> assignToComponents(
        const vector<sp::SpatialRecord<T>>& records,
        const vector<sp::MBR>& boundaries,
        size_t targetLevel,
        size_t dimensions) const {
        return {};
    }
};

}