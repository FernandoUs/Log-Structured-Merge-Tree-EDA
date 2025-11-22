#pragma once

#include "../sql/QueryExecutor.h"
#include "../workload/Workload.h"
#include <iostream>
#include <string>
#include <memory>

using namespace std;

namespace cli
{

    using namespace sql;

    template <typename T = int>
    class CLI
    {
    private:
        CatalogManager catalog;
        map<string, TableIndex<T>> tableIndices;
        lsm::GlobalBudget *globalBudget;
        QueryExecutor<T> executor;
        bool running;

    public:
        CLI()
            : catalog(),
              tableIndices(),
              globalBudget(new lsm::GlobalBudget(64 * 1024 * 1024)),
              executor(catalog, tableIndices, globalBudget),
              running(false)
        {
            vector<string> existingTables = catalog.getAllTableNames();
            for (const string &tableName : existingTables)
            {
                TableSchema savedSchema = catalog.getTable(tableName);
                auto *pPolicy = lsm::PolicyFactory<T>::create(savedSchema.mergePolicy, savedSchema.policyParam);
                auto *sPolicy = lsm::PolicyFactory<T>::create(savedSchema.mergePolicy, savedSchema.policyParam);
                sp::ISpatialComparator<T> *pComp = new sp::SimpleComparatorAdapter<T>();
                sp::ISpatialComparator<T> *sComp;

                if (savedSchema.spatialComparator == "Hilbert")
                {
                    sp::Point minP({-180.0, -90.0});
                    sp::Point maxP({180.0, 90.0});
                    sp::MBR world(minP, maxP);
                    sComp = new sp::HilbertComparatorAdapter<T>(world);
                }
                else
                {
                    sComp = new sp::SimpleComparatorAdapter<T>();
                }

                lsm::PartitioningStrategy<T> *partStrategy = nullptr;
                string partName = savedSchema.partitioningStrategy;
                transform(partName.begin(), partName.end(), partName.begin(),
                          [](unsigned char c)
                          { return std::tolower(c); });

                if (partName == "str")
                {
                    partStrategy = new lsm::STRPartitioning<T>();
                }
                else if (partName == "rstargrove" || partName == "rstar" || partName == "r*groove")
                {
                    partStrategy = new lsm::RStarGrovePartitioning<T>(0.1);
                }
                else if (partName == "size")
                {
                    bool useHilbert = (savedSchema.spatialComparator == "Hilbert");
                    partStrategy = new lsm::SizePartitioning<T>(useHilbert);
                }

                TableIndex<T> indices;
                indices.primary = new lsm::LSMTree<T>(tableName, globalBudget, pPolicy, 1024, pComp, false, 2, 10, nullptr, 3000000);
                indices.secondary = new lsm::LSMTree<T>(tableName, globalBudget, sPolicy, 24, sComp, true, 2, 10, partStrategy, 3000000);

                tableIndices[tableName] = indices;
                cout << "System: Restored table '" << tableName << "'" << endl;
            }
        }

        ~CLI()
        {
            for (auto &pair : tableIndices)
            {
                if (pair.second.primary)
                {
                    delete pair.second.primary;
                    pair.second.primary = nullptr;
                }
                if (pair.second.secondary)
                {
                    delete pair.second.secondary;
                    pair.second.secondary = nullptr;
                }
            }
            delete globalBudget;
        }

        void start()
        {
            running = true;

            printBanner();
            printHelp();

            string input;

            while (running)
            {
                cout << "\nspatial-db> ";
                getline(cin, input);

                if (input.empty())
                    continue;

                if (input == "exit" || input == "quit")
                {
                    running = false;
                    cout << "Goodbye!\n";
                    break;
                }

                if (input == "help")
                {
                    printHelp();
                    continue;
                }

                if (input == "metrics")
                {
                    printMetrics();
                    continue;
                }

                if (input == "tables")
                {
                    printTables();
                    continue;
                }

                if (input == "clear")
                {
                    clearMetrics();
                    continue;
                }
                if (input.rfind("clean", 0) == 0)
                {
                    string tableName = input.substr(5);
                    string result = executor.executeClean(tableName);
                    cout << result << "\n";
                    continue;
                }
                if (input.rfind("benchmark", 0) == 0)
                {
                    handleBenchmark(input);
                    continue;
                }
                try
                {
                    string result = executor.execute(input);
                    cout << result << "\n";
                }
                catch (const exception &e)
                {
                    cout << "Error: " << e.what() << "\n";
                }
            }
        }

        string executeCommand(const string &sql)
        {
            try
            {
                return executor.execute(sql);
            }
            catch (const exception &e)
            {
                return "Error: " + string(e.what());
            }
        }

        CatalogManager &getCatalog() { return catalog; }

        map<string, TableIndex<T>> &getTableIndices()
        {
            return tableIndices;
        }

    private:
        void handleBenchmark(const string &input)
        {
            stringstream ss(input);
            string cmd, tableName;
            ss >> cmd >> tableName;

            if (tableName.empty())
            {
                cout << "Usage: benchmark <table_name>\n";
                return;
            }

            string normalizedName = tableName;
            transform(normalizedName.begin(), normalizedName.end(), normalizedName.begin(),
                      [](unsigned char c)
                      { return std::tolower(c); });

            tableName = normalizedName;

            if (!catalog.tableExists(tableName))
            {
                cout << "Error: Table '" << tableName << "' does not exist.\n";
                return;
            }

            size_t TOTAL_RECORDS = 15000000;

            cout << "[BENCHMARK] Starting ingestion of " << TOTAL_RECORDS << " records into '" << tableName << "'...\n";
            lsm::LSMTree<T> *tree = tableIndices[tableName].secondary;
            workload::WorkloadExecutor<T> loader(*tree);

            auto start = chrono::high_resolution_clock::now();
            loader.executeIngestion(TOTAL_RECORDS, false, {}, 0.0, 0);
            tree->flush();

            auto end = chrono::high_resolution_clock::now();
            auto duration = chrono::duration_cast<chrono::seconds>(end - start).count();

            cout << "[BENCHMARK] Completed in " << duration << " seconds.\n";
            cout << "Throughput: " << (TOTAL_RECORDS / (duration > 0 ? duration : 1)) << " ops/sec\n";
        }

        void printBanner()
        {
            cout << R"(
╔═══════════════════════════════════════════════════════════╗
║       LSM-Tree Spatial Database System                    ║
║       Implementation based on research paper              ║
║       "Comparison of LSM indexing techniques"             ║
╚═══════════════════════════════════════════════════════════╝
)" << "\n";
        }

        void printHelp()
        {
            cout << R"(
Available Commands:
  SQL Statements:
    CREATE TABLE name (col1 type1, col2 type2, ...)
    INSERT INTO table VALUES (x, y, data)
    SELECT COUNT(*) FROM table WHERE spatial_intersect(col, x1, y1, x2, y2)
  
  Special Commands:
    help       - Show this help message
    metrics    - Display performance metrics
    tables     - List all tables
    clear      - Clear metrics
    exit/quit  - Exit the system
    clean <table_name> - Reset data and metrics for the specified table
    benchmark <table_name> - Run ingestion benchmark on the specified table

  Example Usage:
    CREATE TABLE points (id INT, location POINT, value DOUBLE) [with policy <policy_name> <param> COMPARATOR <comparator_name>]
    INSERT INTO points VALUES (0.5, 0.5, 100)
    SELECT COUNT(*) FROM points WHERE spatial_intersect(location, 0, 0, 1, 1)
)" << "\n";
        }

        void printMetrics()
        {
            cout << "\n=== Performance Metrics ===\n";

            for (const auto &[tableName, indices] : tableIndices)
            {
                const auto &metrics = indices.secondary->getMetrics();
                cout << "\nTable: " << tableName << "\n";
                cout << "  Total User Bytes Written: " << metrics.totalUserBytesWritten << " bytes\n";
                cout << "  Total Disk Bytes Written: " << metrics.totalDiskBytesWritten << " bytes\n";
                cout << "  Write Amplification: " << metrics.writeAmplification << "\n";
                cout << "  Read Amplification: " << metrics.readAmplification << "\n";
                cout << "  Total Writes: " << metrics.totalWrites << "\n";
                cout << "  Total Reads: " << metrics.totalReads << "\n";
                cout << "  Total Merges: " << metrics.totalMerges << "\n";
                cout << "  Avg Query Latency: " << metrics.avgQueryLatency << " ms\n";
                cout << "  Component Count: " << indices.secondary->getComponentCount() << "\n";
                cout << "  Total Records: " << indices.secondary->getTotalRecords() << "\n";
            }
        }

        void printTables()
        {
            cout << "\n=== Tables ===\n";

            for (const auto &[tableName, indices] : tableIndices)
            {
                cout << "  - " << tableName << " (" << indices.secondary->getTotalRecords() << " records)\n";
            }

            if (tableIndices.empty())
            {
                cout << "  No tables created yet.\n";
            }
        }

        void clearMetrics()
        {
            for (auto &[tableName, indices] : tableIndices)
            {
                if (indices.primary)
                    indices.primary->resetMetrics();
                if (indices.secondary)
                    indices.secondary->resetMetrics();
            }
            cout << "Metrics cleared.\n";
        }
    };

}