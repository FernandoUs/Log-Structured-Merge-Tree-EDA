#pragma once

#include "../spatial/Point.h"
#include "../lsm/LSMTree.h"
#include "../spatial/MBR.h"
#include <vector>
#include <random>
#include <cmath>
#include <iostream>
#include <iomanip>

using namespace std;
namespace workload {

namespace sp = spatial;


class DatasetGenerator {
private:
    mt19937 rng;
    
public:
    DatasetGenerator(unsigned seed = 42) : rng(seed) {}
    
    template<typename T>
    vector<sp::SpatialRecord<T>> generateRandomDataset(
        size_t count,
        double minX = 0.0, double maxX = 1.0,
        double minY = 0.0, double maxY = 1.0) {
        
        uniform_real_distribution<double> distX(minX, maxX);
        uniform_real_distribution<double> distY(minY, maxY);
        
    vector<sp::SpatialRecord<T>> records;
        records.reserve(count);
        
        for (size_t i = 0; i < count; ++i) {
            sp::Point p({distX(rng), distY(rng)});
            T data = static_cast<T>(i);
            records.emplace_back(p, data, false);
        }
        
        return records;
    }
    
    template<typename T>
    vector<sp::SpatialRecord<T>> generateClusteredDataset(
        size_t count,
        size_t numClusters = 10,
        double clusterRadius = 0.05) {
        
        uniform_real_distribution<double> dist(0.0, 1.0);
        normal_distribution<double> clusterDist(0.0, clusterRadius);
        
    vector<sp::Point> clusterCenters;
        for (size_t i = 0; i < numClusters; ++i) {
            sp::Point center({dist(rng), dist(rng)});
            clusterCenters.push_back(center);
        }
        
    vector<sp::SpatialRecord<T>> records;
        records.reserve(count);
        
        uniform_int_distribution<size_t> clusterChoice(0, numClusters - 1);
        
        for (size_t i = 0; i < count; ++i) {
            size_t clusterIdx = clusterChoice(rng);
            const sp::Point& center = clusterCenters[clusterIdx];
            
            double x = center[0] + clusterDist(rng);
            double y = center[1] + clusterDist(rng);
            
            x = max(0.0, min(1.0, x));
            y = max(0.0, min(1.0, y));
            
            sp::Point p({x, y});
            T data = static_cast<T>(i);
            records.emplace_back(p, data, false);
        }
        
        return records;
    }
    
    sp::MBR generateQueryBox(double selectivity, 
                         double minX = 0.0, double maxX = 1.0,
                         double minY = 0.0, double maxY = 1.0) {
        
        uniform_real_distribution<double> distX(minX, maxX);
        uniform_real_distribution<double> distY(minY, maxY);
        
        double totalArea = (maxX - minX) * (maxY - minY);
        double queryArea = totalArea * selectivity;
        
        double side = sqrt(queryArea);
        
        double x1 = distX(rng);
        double y1 = distY(rng);
        
        double x2 = min(x1 + side, maxX);
        double y2 = min(y1 + side, maxY);
        
        sp::Point lower({x1, y1});
        sp::Point upper({x2, y2});
        
        return sp::MBR(lower, upper);
    }
};

    template<typename T>
    class WorkloadExecutor {
private:
    lsm::LSMTree<T>& lsmTree;
    DatasetGenerator generator;
    
public:
    explicit WorkloadExecutor(lsm::LSMTree<T>& tree) : lsmTree(tree) {}
    
    void loadPhase(const vector<sp::SpatialRecord<T>>& records) {
        for (const auto& rec : records) {
            lsmTree.insert(rec.point, rec.data);
        }
        
        lsmTree.flush();
    }
    
    void insertPhase(const vector<sp::SpatialRecord<T>>& records) {
        for (const auto& rec : records) {
            lsmTree.insert(rec.point, rec.data);
        }
    }
    
    vector<size_t> readPhase(const vector<sp::MBR>& queryBoxes) {
        vector<size_t> resultCounts;
        
        for (const auto& box : queryBoxes) {
            auto results = lsmTree.spatialRangeQuery(box);
            resultCounts.push_back(results.size());
        }
        
        return resultCounts;
    }
    
    void runWorkload(
        const vector<sp::SpatialRecord<T>>& loadData,
        const vector<sp::SpatialRecord<T>>& insertData,
        const vector<sp::MBR>& queries) {
        
        cout << "=== Workload Execution ===\n";
        
        cout << "Loading " << loadData.size() << " records...\n";
        loadPhase(loadData);
        cout << "Load complete.\n";
        
        cout << "Inserting " << insertData.size() << " additional records...\n";
        insertPhase(insertData);
        cout << "Inserts complete.\n";
        
        cout << "Executing " << queries.size() << " range queries...\n";
        auto results = readPhase(queries);
        
        cout << "Read phase complete.\n";
        cout << "Query results:\n";
        for (size_t i = 0; i < results.size(); ++i) {
            cout << "  Query " << i << ": " << results[i] << " results\n";
        }
        
    const auto& metrics = lsmTree.getMetrics();
        cout << "\n=== Performance Metrics ===\n";
        cout << "Write Amplification: " << metrics.writeAmplification << "\n";
        cout << "Read Amplification: " << metrics.readAmplification << "\n";
        cout << "Avg Query Latency: " << metrics.avgQueryLatency << " ms\n";
    }
};

    template<typename T>
    class BenchmarkRunner {
public:
    struct BenchmarkConfig {
        string name;
        string mergePolicy;
        string comparator;
        string partitioning;
        int policyParameter;
    };
    
    struct BenchmarkResult {
        string configName;
        double writeAmplification;
        double readAmplification;
        double avgQueryLatency;
        size_t componentCount;
    };
    
    vector<BenchmarkResult> runComparison(
    const vector<BenchmarkConfig>& configs,
    const vector<sp::SpatialRecord<T>>& dataset,
    const vector<sp::MBR>& queries) {
        
        vector<BenchmarkResult> results;
        
        for (const auto& config : configs) {
            cout << "\n=== Testing Configuration: " << config.name << " ===\n";
            
            lsm::LSMTree<T> tree(2);
            
            WorkloadExecutor<T> executor(tree);
            
            size_t loadSize = static_cast<size_t>(dataset.size() * 0.8);
            vector<sp::SpatialRecord<T>> loadData(dataset.begin(), dataset.begin() + loadSize);
            vector<sp::SpatialRecord<T>> insertData(dataset.begin() + loadSize, dataset.end());
            
            executor.runWorkload(loadData, insertData, queries);
            
            const auto& metrics = tree.getMetrics();
            
            BenchmarkResult result;
            result.configName = config.name;
            result.writeAmplification = metrics.writeAmplification;
            result.readAmplification = metrics.readAmplification;
            result.avgQueryLatency = metrics.avgQueryLatency;
            result.componentCount = tree.getComponentCount();
            
            results.push_back(result);
        }
        
        return results;
    }
    
    static void printResults(const vector<BenchmarkResult>& results) {
        cout << "\n=== Benchmark Results ===\n";
        cout << string(100, '=') << "\n";
        cout << left << setw(30) << "Configuration"
                  << setw(15) << "WA"
                  << setw(15) << "RA"
                  << setw(20) << "Latency (ms)"
                  << setw(15) << "Components" << "\n";
        cout << string(100, '-') << "\n";
        
        for (const auto& result : results) {
            cout << left << setw(30) << result.configName
                      << setw(15) << result.writeAmplification
                      << setw(15) << result.readAmplification
                      << setw(20) << result.avgQueryLatency
                      << setw(15) << result.componentCount << "\n";
        }
        
        cout << string(100, '=') << "\n";
    }
};

}