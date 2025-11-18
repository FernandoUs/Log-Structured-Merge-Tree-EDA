#pragma once

#include "../spatial/RTree.h"
#include "../spatial/SpatialComparators.h"
#include "LSMComponent.h"
#include <map>
#include <mutex>
#include <vector>
#include <algorithm>
using namespace std;
namespace sp = spatial;

namespace lsm {

template<typename T>
class MemTable {
private:
    map<sp::Point, sp::SpatialRecord<T>, sp::SimpleComparator> data;
    size_t maxSize;
    size_t currentSize;
    mutable mutex mtx;

public:
    explicit MemTable(size_t maxSizeBytes = 64 * 1024 * 1024)
        : maxSize(maxSizeBytes), currentSize(0) {}

    bool insert(const sp::SpatialRecord<T>& record) {
        return false;
    }

    bool remove(const sp::Point& point) {
        return false;
    }

    vector<sp::SpatialRecord<T>> rangeSearch(const sp::MBR& queryBox) const {
        return {};
    }

    vector<sp::SpatialRecord<T>> getAllRecords() const {
        return {};
    }

    void clear() {
    }

    bool isFull() const {
        lock_guard<mutex> lock(mtx);
        return currentSize >= maxSize;
    }

    size_t size() const {
        lock_guard<mutex> lock(mtx);
        return data.size();
    }

    bool isEmpty() const {
        lock_guard<mutex> lock(mtx);
        return data.empty();
    }
};

struct LSMMetrics {
    uint64_t writeAmplification;
    uint64_t readAmplification;
    uint64_t totalWrites;
    uint64_t totalReads;
    uint64_t totalMerges;
    double avgQueryLatency;

    LSMMetrics() : writeAmplification(0), readAmplification(0),
                   totalWrites(0), totalReads(0), totalMerges(0),
                   avgQueryLatency(0.0) {}

    void reset() {
        writeAmplification = 0;
        readAmplification = 0;
        totalWrites = 0;
        totalReads = 0;
        totalMerges = 0;
        avgQueryLatency = 0.0;
    }
};

template<typename T>
class LSMTree {
private:
    MemTable<T> memTable;
    vector<LSMComponent<T>*> diskComponents; 
    size_t dimensions;
    mutable mutex treeMutex;
    LSMMetrics metrics;
    size_t maxComponentsBeforeMerge;

public:
    explicit LSMTree(size_t dims = 2, size_t maxComponents = 10)
        : memTable(), dimensions(dims), maxComponentsBeforeMerge(maxComponents) {}

    ~LSMTree() {
        for (LSMComponent<T>* comp : diskComponents) {
            delete comp;
        }
    }

    bool insert(const sp::Point& point, const T& data) {
        return false;
    }

    bool remove(const sp::Point& point) {
        return false;
    }

    void flush() {
    }

    vector<sp::SpatialRecord<T>> spatialRangeQuery(const sp::MBR& queryBox) {
        return {};
    }

    vector<sp::SpatialRecord<T>> pointQuery(const sp::Point& point) {
        return {};
    }

    void removeDuplicatesAndTombstones(vector<sp::SpatialRecord<T>>& results) const {
    }

    const LSMMetrics& getMetrics() const { return metrics; }
    void resetMetrics() { metrics.reset(); }

    size_t getComponentCount() const {
        lock_guard<mutex> lock(treeMutex);
        return diskComponents.size();
    }

    size_t getTotalRecords() const {
        lock_guard<mutex> lock(treeMutex);
        size_t total = memTable.size();
        for (const auto& comp : diskComponents) {
            total += comp->size();
        }
        return total;
    }
};

}