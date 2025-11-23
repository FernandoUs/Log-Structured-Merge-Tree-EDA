#pragma once

#include "../spatial/RTree.h"
#include "../spatial/MBR.h"
#include "../spatial/Point.h"
#include "../spatial/SpatialComparators.h"
#include <vector>
#include <memory>
#include <string>
#include <fstream>
#include <iostream>
#include <chrono>
#include <atomic>
using namespace std;
namespace sp = spatial;

namespace lsm {

template<typename T>
class LSMComponent {
private:
    sp::RTree<T>* rtree;
    sp::MBR totalMBR;
    size_t level;
    uint64_t timestamp;
    string filename;
    size_t recordCount;
    size_t weight;  // Peso para política Binomial (número de flushes fusionados)
    static std::atomic<uint64_t> globalIdCounter;
public:
    LSMComponent(size_t lvl = 0, size_t dims = 2, size_t w = 1)
        : rtree(new sp::RTree<T>(dims)), totalMBR(dims), level(lvl),
          timestamp(0), recordCount(0), weight(w) {
        auto now = chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        timestamp = chrono::duration_cast<chrono::milliseconds>(duration).count();
        uint64_t uniqueID = globalIdCounter.fetch_add(1);
        filename = "component_L" + to_string(level) + "_" + to_string(timestamp) + to_string(uniqueID) + ".dat";
    }

    LSMComponent(string fname, size_t dims = 2) 
        : totalMBR(dims), level(0), timestamp(0), filename(fname), recordCount(0), weight(1) {
        rtree = new sp::RTree<T>(dims);
    }

    ~LSMComponent() {
        delete rtree;
    }

    void build(const vector<sp::SpatialRecord<T>>& records) {
        recordCount = records.size();
        if (records.empty()) return;
        totalMBR = sp::MBR(records[0].point.dimensions());
        for (const auto& rec : records) {
            if (!totalMBR.isValid()) totalMBR = sp::MBR(rec.point.dimensions());
            totalMBR.expand(rec.point);
        }
        vector<sp::SpatialRecord<T>> recordsCopy = records;
        rtree->build(recordsCopy);
    }

    vector<sp::SpatialRecord<T>> rangeSearch(const sp::MBR& queryBox) const {
        if (!totalMBR.intersects(queryBox)) {
            cout << "No intersection with total MBR\n";
            return {};
        }
        return rtree->rangeSearch(queryBox);
    }

    bool saveToDisk(const string& directory = "data") const {
        string fullPath = directory + "/" + filename;
        ofstream file(fullPath, ios::binary);
        if (!file.is_open()) {
            cerr << "Error: No se pudo crear archivo " << fullPath << endl;
            return false;
        }
        file.write(reinterpret_cast<const char*>(&level), sizeof(level));
        file.write(reinterpret_cast<const char*>(&timestamp), sizeof(timestamp));
        file.write(reinterpret_cast<const char*>(&recordCount), sizeof(recordCount));
        file.write(reinterpret_cast<const char*>(&weight), sizeof(weight));
        
        size_t dims = totalMBR.getLower().dimensions();
        for(size_t i=0; i<dims; ++i) {
            double minVal = totalMBR.getLower()[i];
            double maxVal = totalMBR.getUpper()[i];
            file.write(reinterpret_cast<const char*>(&minVal), sizeof(double));
            file.write(reinterpret_cast<const char*>(&maxVal), sizeof(double));
        }

        if (rtree) {
            vector<sp::SpatialRecord<T>> records = rtree->getAllRecords();
            for (const auto& rec : records) {
                double x = rec.point[0];
                double y = rec.point[1];
                file.write(reinterpret_cast<const char*>(&x), sizeof(double));
                file.write(reinterpret_cast<const char*>(&y), sizeof(double));
                file.write(reinterpret_cast<const char*>(&rec.data), sizeof(T));
                file.write(reinterpret_cast<const char*>(&rec.isTombstone), sizeof(bool));
            }
        }

        file.close();
        return true;
    }

    bool loadFromDisk(const string& directory = "data") {
        string fullPath = directory + "/" + filename;
        ifstream file(fullPath, ios::binary);
        if (!file.is_open()) return false;
        file.read(reinterpret_cast<char*>(&level), sizeof(level));
        file.read(reinterpret_cast<char*>(&timestamp), sizeof(timestamp));
        file.read(reinterpret_cast<char*>(&recordCount), sizeof(recordCount));
        file.read(reinterpret_cast<char*>(&weight), sizeof(weight));

        size_t dims = totalMBR.getLower().dimensions();
        vector<double> minCoords(dims), maxCoords(dims);

        for(size_t i=0; i<dims; ++i) {
            file.read(reinterpret_cast<char*>(&minCoords[i]), sizeof(double));
            file.read(reinterpret_cast<char*>(&maxCoords[i]), sizeof(double));
        }
        totalMBR = sp::MBR(sp::Point(minCoords), sp::Point(maxCoords));

        vector<sp::SpatialRecord<T>> records;
        records.reserve(recordCount);
        for (size_t i = 0; i < recordCount; ++i) {
            double x, y;
            T data;
            bool isTombstone;
            file.read(reinterpret_cast<char*>(&x), sizeof(double));
            file.read(reinterpret_cast<char*>(&y), sizeof(double));
            file.read(reinterpret_cast<char*>(&data), sizeof(T));
            file.read(reinterpret_cast<char*>(&isTombstone), sizeof(bool));
            sp::Point p({x, y});
            records.emplace_back(p, data, isTombstone);
        }
        file.close();
        this->build(records);
        return true;
    }

    bool loadMetadata(const string& directory = "data") {
        string fullPath = directory + "/" + filename;
        cout << "Loading metadata from " << fullPath << endl;
        ifstream file(fullPath, ios::binary);
        if (!file.is_open()) return false;
        file.read((char*)&level, sizeof(level));
        file.read((char*)&timestamp, sizeof(timestamp));
        file.read((char*)&recordCount, sizeof(recordCount));
        file.read((char*)&weight, sizeof(weight));

        size_t dims = totalMBR.getLower().dimensions();
        vector<double> minCoords(dims), maxCoords(dims);

        for(size_t i=0; i<dims; ++i) {
            file.read(reinterpret_cast<char*>(&minCoords[i]), sizeof(double));
            file.read(reinterpret_cast<char*>(&maxCoords[i]), sizeof(double));
        }
        totalMBR = sp::MBR(sp::Point(minCoords), sp::Point(maxCoords));

        file.close();
        return true;
    }

    void clearMemoryData() {
        if (rtree) {
            delete rtree;
            rtree = new sp::RTree<T>(totalMBR.getLower().dimensions());
        }
    }
    const sp::MBR& getMBR() const { return totalMBR; }

    size_t getLevel() const { return level; }

    uint64_t getTimestamp() const { return timestamp; }

    size_t getRecordCount() const { return this->recordCount; }
    
    size_t size() const { return recordCount; }

    const string& getFilename() const { return filename; }
    
    size_t getWeight() const { return weight; }
    
    void setWeight(size_t w) { weight = w; }

};
    template<typename T>
    std::atomic<uint64_t> LSMComponent<T>::globalIdCounter(0);
}
