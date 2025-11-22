#pragma once

#include "../spatial/RTree.h"
#include "../spatial/SpatialComparators.h"
#include "MergePolicy.h"
#include "LSMComponent.h"
#include "PartitioningStrategy.h"
#include <map>
#include <mutex>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <string>
#include "../json.hpp"

using namespace std;
using json = nlohmann::json;
namespace sp = spatial;
namespace fs = std::filesystem;
namespace lsm
{

    class ILSMTree
    {
    public:
        virtual void flush() = 0;
        virtual ~ILSMTree() = default;
    };

    class GlobalBudget
    {
    private:
        size_t maxBytes;
        size_t currentBytes;
        vector<ILSMTree *> subscribers;

    public:
        GlobalBudget(size_t limit) : maxBytes(limit), currentBytes(0) {}

        void subscribe(ILSMTree *tree)
        {
            subscribers.push_back(tree);
        }

        bool requestSpace(size_t bytesNeeded)
        {
            if (currentBytes + bytesNeeded > maxBytes)
            {
                flushAll();
                if (currentBytes + bytesNeeded > maxBytes)
                {
                    return false;
                }
            }
            currentBytes += bytesNeeded;
            return true;
        }

        void releaseSpace(size_t bytesFreed)
        {
            if (bytesFreed > currentBytes)
                currentBytes = 0;
            else
                currentBytes -= bytesFreed;
        }

        size_t getUsage() const
        {
            return currentBytes;
        }

    private:
        void flushAll()
        {
            for (ILSMTree *tree : subscribers)
            {
                tree->flush();
            }
        }
    };

    template <typename T>
    class MemTable
    {
    private:
        map<sp::Point, sp::SpatialRecord<T>, sp::SimpleComparator> data;
        size_t currentSize;
        mutable mutex mtx;

    public:
        MemTable() : currentSize(0) {}
        void insert(const sp::SpatialRecord<T> &record, size_t customSize)
        {
            lock_guard<mutex> lock(mtx);
            auto it = data.find(record.point);
            if (it != data.end())
            {
                it->second = record;
            }
            else
            {
                data[record.point] = record;
                currentSize += customSize;
            }
        }

        bool remove(const sp::Point &point, size_t customSize)
        {
            // * No hace nada en HD*
        }

        vector<sp::SpatialRecord<T>> rangeSearch(const sp::MBR &queryBox) const
        {
            lock_guard<mutex> lock(mtx);
            vector<sp::SpatialRecord<T>> results;
            for (const auto &pair : data)
            {
                const auto &record = pair.second;
                cout << record.point[0] << ", " << record.point[1] << endl;
                if (queryBox.contains(record.point))
                {
                    results.push_back(record);
                }
            }
            return results;
        }

        vector<sp::SpatialRecord<T>> getAllRecords() const
        {
            lock_guard<mutex> lock(mtx);
            vector<sp::SpatialRecord<T>> records;
            records.reserve(data.size());
            for (const auto &pair : data)
            {
                records.push_back(pair.second);
            }
            return records;
        }

        void clear()
        {
            lock_guard<mutex> lock(mtx);
            data.clear();
            currentSize = 0;
        }

        size_t sizeInBytes() const
        {
            lock_guard<mutex> lock(mtx);
            return currentSize;
        }

        size_t size() const
        {
            lock_guard<mutex> lock(mtx);
            return data.size();
        }

        bool isEmpty() const
        {
            lock_guard<mutex> lock(mtx);
            return data.empty();
        }
    };

    struct LSMMetrics
    {
        uint64_t totalWrites = 0;

        uint64_t totalUserBytesWritten = 0;
        uint64_t totalDiskBytesWritten = 0;
        double totalWriteTimeSeconds = 0.0;
        double writeAmplification = 1.0;

        uint64_t readAmplification = 0;
        uint64_t totalReads = 0;
        uint64_t totalMerges = 0;
        double avgQueryLatency = 0.0;

        void reset()
        {
            *this = LSMMetrics();
        }

        void updateWA()
        {
            if (totalUserBytesWritten > 0)
            {
                writeAmplification = (double)totalDiskBytesWritten / (double)totalUserBytesWritten;
            }
            else
            {
                writeAmplification = 1.0;
            }
        }
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(LSMMetrics,
                                       totalWrites, totalUserBytesWritten, totalDiskBytesWritten, writeAmplification,
                                       readAmplification, totalReads, totalMerges, avgQueryLatency)
    };

    template <typename T>
    class LSMTree : public ILSMTree
    {
    private:
        MemTable<T> memTable;
        vector<LSMComponent<T> *> diskComponents;
        size_t dimensions;
        mutable mutex treeMutex;
        LSMMetrics metrics;
        size_t maxComponentsBeforeMerge;
        GlobalBudget *globalBudget;
        size_t recordWeight;
        MergePolicy<T> *mergePolicy;
        string tableName;
        sp::ISpatialComparator<T> *comparator;
        bool persistenceEnabled;
        PartitioningStrategy<T> *partitioningStrategy;
        size_t maxComponentSize;

        void saveMetrics()
        {
            string metricsFile = "data/" + tableName + "/metrics.json";
            string tableDir = "data/" + tableName;
            if (!fs::exists(tableDir))
                fs::create_directories(tableDir);
            json j = metrics;
            ofstream file(metricsFile);
            if (file.is_open())
            {
                file << j.dump(4);
                file.close();
            }
        }

        void loadMetrics()
        {
            string metricsFile = "data/" + tableName + "/metrics.json";
            if (!fs::exists(metricsFile))
                return;
            ifstream file(metricsFile);
            if (file.is_open())
            {
                try
                {
                    json j;
                    file >> j;
                    metrics = j.get<LSMMetrics>();
                }
                catch (...)
                {
                    metrics.reset();
                }
            }
        }

        void checkAndMerge()
        {
            if (!mergePolicy)
                return;
            string tableDir = "data/" + tableName;
            if (!fs::exists(tableDir))
                fs::create_directories(tableDir);

            while (mergePolicy->shouldMerge(diskComponents))
            {
                auto victims = mergePolicy->selectComponentsToMerge(diskComponents);
                if (victims.empty())
                    break;
                cout << "[MERGE] Fusionando " << victims.size() << " componentes..." << endl;
                size_t newLevel = victims[0]->getLevel() + 1;

                vector<LSMComponent<T> *> newComponents;

                if (partitioningStrategy)
                {
                    newComponents = mergePolicy->mergeComponentsPartitioned(
                        victims,
                        newLevel,
                        dimensions,
                        tableDir,
                        comparator,
                        partitioningStrategy,
                        maxComponentSize);
                }
                else
                {
                    LSMComponent<T> *mergedComponent = mergePolicy->mergeComponents(
                        victims,
                        newLevel,
                        dimensions,
                        tableDir,
                        comparator);
                    if (mergedComponent)
                    {
                        newComponents.push_back(mergedComponent);
                    }
                }

                if (newComponents.empty())
                {
                    cerr << "[ERROR] Falló la creación del componente fusionado." << endl;
                    break;
                }

                for (auto *comp : newComponents)
                {
                    comp->saveToDisk(tableDir);
                    try
                    {
                        string fullPath = tableDir + "/" + comp->getFilename();
                        size_t bytesWritten = fs::file_size(fullPath);
                        metrics.totalDiskBytesWritten += bytesWritten;
                    }
                    catch (...)
                    {
                    }
                }
                metrics.updateWA();

                vector<LSMComponent<T> *> nextDiskComponents;
                bool mergedInserted = false;
                for (auto *existing : diskComponents)
                {
                    bool isVictim = false;
                    for (auto *v : victims)
                    {
                        if (existing == v)
                        {
                            isVictim = true;
                            break;
                        }
                    }

                    if (isVictim)
                    {
                        if (!mergedInserted)
                        {
                            for (auto *newComp : newComponents)
                            {
                                nextDiskComponents.push_back(newComp);
                            }
                            mergedInserted = true;
                        }
                        string filename = existing->getFilename();
                        string fullPath = tableDir + "/" + filename;

                        if (fs::exists(fullPath))
                        {
                            fs::remove(fullPath);
                        }
                        delete existing;
                    }
                    else
                    {
                        nextDiskComponents.push_back(existing);
                    }
                }
                diskComponents = nextDiskComponents;
                metrics.totalMerges++;

                cout << "[MERGE] Éxito. Nuevos componentes: " << newComponents.size()
                     << " (total registros: ";
                size_t totalRecords = 0;
                for (auto *comp : newComponents)
                {
                    totalRecords += comp->size();
                }
                cout << totalRecords << ")" << endl;
            }
        }

    public:
        LSMTree(string name, GlobalBudget *budget, MergePolicy<T> *policy, size_t bytesPerRecord, sp::ISpatialComparator<T> *comp, bool persist = true, size_t dims = 2, size_t maxComponents = 10, PartitioningStrategy<T> *partStrategy = nullptr, size_t maxCompSize = 10000)
            : tableName(name), memTable(), dimensions(dims), maxComponentsBeforeMerge(maxComponents),
              globalBudget(budget), recordWeight(bytesPerRecord), mergePolicy(policy), comparator(comp),
              persistenceEnabled(persist), partitioningStrategy(partStrategy), maxComponentSize(maxCompSize)
        {
            if (globalBudget)
            {
                globalBudget->subscribe(this);
            }
            if (persistenceEnabled)
            {
                loadExistingComponents();
            }
            loadMetrics();
        }

        ~LSMTree() override
        {
            flush();
            saveMetrics();
            for (LSMComponent<T> *comp : diskComponents)
            {
                delete comp;
            }
            if (mergePolicy)
                delete mergePolicy;
            if (comparator)
                delete comparator;
            if (partitioningStrategy)
                delete partitioningStrategy;
        }

        void loadExistingComponents()
        {
            string tableDir = "data/" + tableName;
            if (!fs::exists(tableDir))
            {
                fs::create_directories(tableDir);
                return;
            }
            vector<string> componentFiles;
            for (const auto &entry : fs::directory_iterator(tableDir))
            {
                string fname = entry.path().filename().string();
                if (fname.find("component_") == 0 && fname.find(".dat") != string::npos)
                {
                    componentFiles.push_back(fname);
                }
            }
            sort(componentFiles.rbegin(), componentFiles.rend());
            for (const string &fname : componentFiles)
            {
                LSMComponent<T> *comp = new LSMComponent<T>(fname, dimensions);
                if (comp->loadMetadata(tableDir))
                {
                    diskComponents.push_back(comp);
                }
                else
                {
                    delete comp;
                }
            }
        }

        bool insert(const sp::Point &point, const T &data)
        {
            auto start = chrono::high_resolution_clock::now();

            if (!globalBudget->requestSpace(recordWeight))
            {
                return false;
            }
            sp::SpatialRecord<T> record(point, data);
            memTable.insert(record, recordWeight);

            auto end = chrono::high_resolution_clock::now();
            chrono::duration<double> elapsed = end - start;
            metrics.totalWrites++;
            metrics.totalUserBytesWritten += recordWeight;
            metrics.totalWriteTimeSeconds += elapsed.count();
            metrics.updateWA();
            return true;
        }

        bool remove(const sp::Point &point)
        {
            // * No hace nada en HD*
        }

        void flush() override
        {
            lock_guard<mutex> lock(treeMutex);
            if (memTable.isEmpty())
                return;

            auto records = memTable.getAllRecords();
            size_t bytesFreed = memTable.sizeInBytes();

            LSMComponent<T> *newComponent = new LSMComponent<T>(0, dimensions);
            newComponent->build(records);

            string tableDir = "data/" + tableName;
            if (!fs::exists(tableDir))
                fs::create_directories(tableDir);
            newComponent->saveToDisk(tableDir);

            try
            {
                string fullPath = tableDir + "/" + newComponent->getFilename();
                size_t bytesWritten = fs::file_size(fullPath);
                metrics.totalDiskBytesWritten += bytesWritten;
                metrics.updateWA();
            }
            catch (const fs::filesystem_error &e)
            {
                cerr << "[METRICS ERROR] No se pudo leer el tamaño del archivo: " << e.what() << endl;
            }

            diskComponents.insert(diskComponents.begin(), newComponent);
            memTable.clear();
            globalBudget->releaseSpace(bytesFreed);
            checkAndMerge();
        }

        vector<sp::SpatialRecord<T>> spatialRangeQuery(const sp::MBR &queryBox)
        {
            lock_guard<mutex> lock(treeMutex);
            metrics.totalReads++;

            vector<sp::SpatialRecord<T>> results = memTable.rangeSearch(queryBox);
            string tableDir = "data/" + tableName;
            int componentsChecked = 0;

            auto start_time = std::chrono::high_resolution_clock::now();
            for (auto *comp : diskComponents)
            {
                if (comp->getMBR().intersects(queryBox))
                {
                    bool loaded = comp->loadFromDisk(tableDir);
                    if (loaded)
                    {
                        auto compResults = comp->rangeSearch(queryBox);
                        results.insert(results.end(), compResults.begin(), compResults.end());
                        componentsChecked++;
                        comp->clearMemoryData();
                    }
                }
            }
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                   end_time - start_time)
                                   .count();
            double current_latency_ms = (double)duration_us / 1000.0;
            if (metrics.totalReads == 1)
            {
                metrics.avgQueryLatency = current_latency_ms;
            }
            else
            {
                metrics.avgQueryLatency += (current_latency_ms - metrics.avgQueryLatency) / metrics.totalReads;
            }
            metrics.readAmplification += componentsChecked;
            return results;
        }

        void removeDuplicatesAndTombstones(vector<sp::SpatialRecord<T>> &results) const
        {
            if (results.empty())
                return;

            map<sp::Point, sp::SpatialRecord<T>, sp::SimpleComparator> uniqueMap;
            for (const auto &rec : results)
            {
                uniqueMap.insert({rec.point, rec});
            }
            results.clear();
            for (const auto &pair : uniqueMap)
            {
                if (!pair.second.isTombstone)
                {
                    results.push_back(pair.second);
                }
            }
        }

        const LSMMetrics &getMetrics() const { return metrics; }
        void resetMetrics() { metrics.reset(); }

        size_t getComponentCount() const
        {
            lock_guard<mutex> lock(treeMutex);
            return diskComponents.size();
        }

        size_t getTotalRecords() const
        {
            lock_guard<mutex> lock(treeMutex);
            size_t total = memTable.size();
            for (const auto &comp : diskComponents)
            {
                total += comp->size();
            }
            return total;
        }
    };

}