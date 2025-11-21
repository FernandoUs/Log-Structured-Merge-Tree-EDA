#pragma once

#include "../spatial/Point.h"
#include "../lsm/LSMTree.h"
#include "../spatial/MBR.h"
#include "../lsm/MergePolicy.h"
#include <vector>
#include <random>
#include <cmath>
#include <iostream>
#include <iomanip>

using namespace std;
namespace workload {

namespace sp = spatial;

struct DataConfig {
    size_t totalRecords;
    bool isClustered;
    size_t numClusters;
    double clusterRadius;
    
    DataConfig() : totalRecords(0), isClustered(false), numClusters(10), clusterRadius(0.05) {}
};

class DatasetGenerator {
private:
    mt19937 rng;
    
public:
    DatasetGenerator(unsigned seed = 42) : rng(seed) {}
    
    template<typename T>
    vector<sp::SpatialRecord<T>> generateRandomBatch(
        size_t count, size_t startId) {
        uniform_real_distribution<double> distX(-180.0, 180.0); 
        uniform_real_distribution<double> distY(-90.0, 90.0);   
        
        vector<sp::SpatialRecord<T>> records;
        records.reserve(count);
        
        for (size_t i = 0; i < count; ++i) {
            sp::Point p({distX(rng), distY(rng)});
            T data = static_cast<T>(startId + i);
            records.emplace_back(p, data, false);
        }
        return records;
    }
    
    template<typename T>
    vector<sp::SpatialRecord<T>> generateClusteredBatch(
        size_t count, size_t startId, 
        const vector<sp::Point>& clusterCenters, double clusterRadius) {
        
        normal_distribution<double> clusterDist(0.0, clusterRadius);
        uniform_int_distribution<size_t> clusterChoice(0, clusterCenters.size() - 1);
        
        vector<sp::SpatialRecord<T>> records;
        records.reserve(count);
        
        for (size_t i = 0; i < count; ++i) {
            size_t clusterIdx = clusterChoice(rng);
            const sp::Point& center = clusterCenters[clusterIdx];
            
            double x = center[0] + clusterDist(rng);
            double y = center[1] + clusterDist(rng);
            
            x = max(-180.0, min(180.0, x));
            y = max(-90.0, min(90.0, y));
            
            sp::Point p({x, y});
            T data = static_cast<T>(startId + i);
            records.emplace_back(p, data, false);
        }
        return records;
    }
    
    vector<sp::Point> generateClusterCenters(size_t numClusters) {
        uniform_real_distribution<double> distLon(-180.0, 180.0);
        uniform_real_distribution<double> distLat(-90.0, 90.0);
        
        vector<sp::Point> centers;
        for (size_t i = 0; i < numClusters; ++i) {
            centers.emplace_back(vector<double>{distLon(rng), distLat(rng)});
        }
        return centers;
    }
    
    sp::MBR generateQueryBox(double selectivity) {
        uniform_real_distribution<double> distLon(-180.0, 180.0);
        uniform_real_distribution<double> distLat(-90.0, 90.0);
        
        double width = 360.0 * selectivity;
        double height = 180.0 * selectivity;
        double x1 = distLon(rng);
        double y1 = distLat(rng);
        double x2 = min(x1 + width, 180.0);
        double y2 = min(y1 + height, 90.0);
        
        sp::Point lower({x1, y1});
        sp::Point upper({x2, y2});
        
        return sp::MBR(lower, upper);
    }
    
    vector<sp::MBR> generateQueries(size_t count, double selectivity) {
        vector<sp::MBR> queries;
        queries.reserve(count);
        for(size_t i=0; i<count; ++i) queries.push_back(generateQueryBox(selectivity));
        return queries;
    }
};

template<typename T>
class WorkloadExecutor {
private:
    lsm::LSMTree<T>& lsmTree;
    DatasetGenerator generator;
    const size_t BATCH_SIZE = 10000;

public:
    WorkloadExecutor(lsm::LSMTree<T>& tree) : lsmTree(tree) {}
    
    void executeIngestion(size_t totalRecords, bool isClustered, 
                         const vector<sp::Point>& centers, double radius, size_t startId = 0) {
        
        size_t remaining = totalRecords;
        size_t currentId = startId;
        
        while (remaining > 0) {
            size_t currentBatch = min(BATCH_SIZE, remaining);
            vector<sp::SpatialRecord<T>> batch;
            
            if (isClustered) {
                batch = generator.generateClusteredBatch<T>(currentBatch, currentId, centers, radius);
            } else {
                batch = generator.generateRandomBatch<T>(currentBatch, currentId);
            }
            
            for (const auto& rec : batch) {
                lsmTree.insert(rec.point, rec.data);
            }
            
            remaining -= currentBatch;
            currentId += currentBatch;
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
    
    void runWorkload(const DataConfig& config, const vector<sp::MBR>& queries) {
        cout << "=== Workload Execution ===\n";
        
        vector<sp::Point> centers;
        if (config.isClustered) {
            centers = generator.generateClusterCenters(config.numClusters);
        }

        size_t loadCount = static_cast<size_t>(config.totalRecords * 0.8);
        cout << "Loading " << loadCount << " records (Batched)...\n";
        executeIngestion(loadCount, config.isClustered, centers, config.clusterRadius, 0);
        
        lsmTree.flush();
        cout << "Load complete.\n";
        
        size_t insertCount = config.totalRecords - loadCount;
        cout << "Inserting " << insertCount << " records (Batched)...\n";
        executeIngestion(insertCount, config.isClustered, centers, config.clusterRadius, loadCount);
        cout << "Inserts complete.\n";
        
        cout << "Executing " << queries.size() << " range queries...\n";
        auto results = readPhase(queries);
        cout << "Read phase complete.\n";
        
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
        lsm::GlobalBudget* budget; 
    };
    
    struct BenchmarkResult {
        string configName;
        uint64_t writeAmplification;
        uint64_t readAmplification;
        double avgQueryLatency;
        size_t componentCount;
    };
    
    vector<BenchmarkResult> runComparison(
        const vector<BenchmarkConfig>& configs,
        const DataConfig& dataConfig,
        const vector<sp::MBR>& queries) {
        
        vector<BenchmarkResult> results;
        
        for (const auto& config : configs) {
            cout << "\n=== Testing Configuration: " << config.name << " ===\n";

            auto* policy = new lsm::TieredMergePolicy<T>(4);

            lsm::LSMTree<T> tree(config.budget, policy, 24, 2);
            
            WorkloadExecutor<T> executor(tree);
            
            executor.runWorkload(dataConfig, queries);
            
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