#pragma once

#include "../spatial/RTree.h"
#include "../spatial/MBR.h"
#include "../spatial/Point.h"
#include "../spatial/SpatialComparators.h"
#include <vector>
#include <memory>
#include <string>
#include <fstream>
#include <chrono>
using namespace std;
namespace sp = spatial;

namespace lsm {

template<typename T>
class LSMComponent {
private:
    sp::RTree<T> rtree;
    sp::MBR totalMBR;
    size_t level;
    uint64_t timestamp;
    string filename;
    size_t recordCount;

public:
    LSMComponent(size_t lvl = 0, size_t dims = 2)
        : rtree(dims), totalMBR(dims), level(lvl),
          timestamp(0), recordCount(0) {
        auto now = chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        timestamp = chrono::duration_cast<chrono::milliseconds>(duration).count();
        filename = "component_L" + to_string(level) + "_" + to_string(timestamp) + ".dat";
    }

    void build(vector<sp::SpatialRecord<T>> records) {
        recordCount = 0;
    }

    vector<sp::SpatialRecord<T>> rangeSearch(const sp::MBR& queryBox) const {
        return {};
    }

    const sp::MBR& getMBR() const { return totalMBR; }

    size_t getLevel() const { return level; }

    uint64_t getTimestamp() const { return timestamp; }

    size_t size() const { return recordCount; }

    const string& getFilename() const { return filename; }

    bool saveToDisk(const string& directory = "./data") const {
        return false;
    }

    bool loadFromDisk(const string& filepath) {
        return false;
    }
};

}
