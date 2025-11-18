#pragma once

#include "LSMComponent.h"
#include "../spatial/SpatialComparators.h"
#include <vector>
#include <memory>
#include <queue>
#include <algorithm>

using namespace std;

namespace lsm {
    
namespace sp = spatial;

template<typename T>
class MergePolicy {
public:
    virtual ~MergePolicy() = default;
    virtual bool shouldMerge(const vector<LSMComponent<T>*>& components) const = 0;
    virtual vector<LSMComponent<T>*> selectComponentsToMerge(
        const vector<LSMComponent<T>*>& components) const = 0;

    LSMComponent<T>* mergeComponents(
        const vector<LSMComponent<T>*>& components,
        size_t targetLevel,
        size_t dimensions) const {
        return nullptr;
    }
};

template<typename T>
class BinomialMergePolicy : public MergePolicy<T> {
private:
    size_t k;

public:
    explicit BinomialMergePolicy(size_t ratio = 4) : k(ratio) {}

    bool shouldMerge(const vector<LSMComponent<T>*>& components) const override {
        return false;
    }

    vector<LSMComponent<T>*> selectComponentsToMerge(
        const vector<LSMComponent<T>*>& components) const override {
        return {};
    }
};

template<typename T>
class TieredMergePolicy : public MergePolicy<T> {
private:
    size_t B;

public:
    explicit TieredMergePolicy(size_t branchingFactor = 4) : B(branchingFactor) {}

    bool shouldMerge(const vector<LSMComponent<T>*>& components) const override {
        return false;
    }

    vector<LSMComponent<T>*> selectComponentsToMerge(
        const vector<LSMComponent<T>*>& components) const override {
        return {};
    }
};

template<typename T>
class ConcurrentMergePolicy : public MergePolicy<T> {
private:
    size_t minComponents;

public:
    explicit ConcurrentMergePolicy(size_t minComps = 2) : minComponents(minComps) {}

    bool shouldMerge(const vector<LSMComponent<T>*>& components) const override {
        return false;
    }

    vector<LSMComponent<T>*> selectComponentsToMerge(
        const vector<LSMComponent<T>*>& components) const override {
        return {};
    }
};

template<typename T>
class LeveledMergePolicy : public MergePolicy<T> {
private:
    size_t sizeRatio;
    size_t baseSize;

public:
    explicit LeveledMergePolicy(size_t ratio = 10, size_t base = 1000)
        : sizeRatio(ratio), baseSize(base) {}

    size_t getMaxSizeForLevel(size_t level) const {
        return 0;
    }

    bool shouldMerge(const vector<LSMComponent<T>*>& components) const override {
        return false;
    }

    vector<LSMComponent<T>*> selectComponentsToMerge(
        const vector<LSMComponent<T>*>& components) const override {
        return {};
    }
};

}