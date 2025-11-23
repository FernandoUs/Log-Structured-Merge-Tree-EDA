#pragma once
#include <fstream>
#include <vector>
#include <iomanip>
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
        struct ExperimentConfig {
                string policy;
                int param;
                string comparator;
                string partition;
                string alias;
            };

            void runFullExperiment() {
                cout << "========================================\n";
                cout << "   INICIANDO EXPERIMENTO AUTOMATIZADO   \n";
                cout << "========================================\n";

                vector<ExperimentConfig> configs = {

                    {"Tiered", 4, "Simple", "None", "Tiered-B4-Simple"},
                    {"Tiered", 4, "Hilbert", "None", "Tiered-B4-Hilbert"},
                    {"Tiered", 10, "Simple", "None", "Tiered-B10-Simple"},
                    {"Tiered", 10, "Hilbert", "None", "Tiered-B10-Hilbert"},

                    {"Binomial", 4, "Simple", "None", "Binomial-K4-Simple"},
                    {"Binomial", 4, "Hilbert", "None", "Binomial-K4-Hilbert"},
                    {"Binomial", 10, "Simple", "None", "Binomial-K10-Simple"},
                    {"Binomial", 10, "Hilbert", "None", "Binomial-K10-Hilbert"},

                    {"Concurrent", 2, "Simple", "None", "Concurrent-Simple"},
                    {"Concurrent", 2, "Hilbert", "None", "Concurrent-Simple"}, // Corregido nombre

                    {"Leveled", 4, "Simple", "Size", "Leveled-Simple-Size"},
                    {"Leveled", 4, "Hilbert", "Size", "Leveled-Hilbert-Size"},

                    {"Leveled", 4, "Simple", "STR", "Leveled-STR"},
                    {"Leveled", 4, "Hilbert", "STR", "Leveled-Hilbert-STR"}, // Corregido nombre

                    {"Leveled", 4, "Simple", "RStarGrove", "Leveled-RStarGrove"},
                    {"Leveled", 4, "Hilbert", "RStarGrove", "Leveled-Hilbert-RStarGrove"} // Corregido nombre
                };

                string csvPath = "resultados_completo.csv";
                ofstream csv(csvPath);
                csv << "Alias,Policy,Param,Comparator,Partition,TotalWrites,WriteTime(s),Throughput,WA,Lat_Q1_Small(ms),Lat_Q2_Large(ms),RA_Avg\n";

                string tableName = "test_lab";
                size_t TOTAL_RECORDS = 15000000;

                workload::DatasetGenerator gen;
                sp::MBR querySmall = gen.generateQueryBox(0.0001);
                sp::MBR queryLarge = gen.generateQueryBox(0.01);

                for (const auto& cfg : configs) {
                    cout << "\n>>> TEST: " << cfg.alias << " <<<\n";

                    executor.executeClean(tableName);

                    stringstream ss;
                    ss << "CREATE TABLE " << tableName << " (id INT, loc POINT) WITH POLICY "
                        << cfg.policy << " " << cfg.param
                        << " COMPARATOR " << cfg.comparator;

                    if (cfg.partition != "None") {
                        ss << " PARTITION " << cfg.partition;
                    }

                    executor.execute(ss.str());

                    auto& indices = tableIndices;
                    if (indices.find(tableName) == indices.end()) {
                        cout << "Error: Tabla no creada.\n"; continue;
                    }
                    lsm::LSMTree<T>* tree = indices[tableName].secondary;
                    if (!tree) { cout << "Error: Árbol nulo.\n"; continue; }

                    workload::WorkloadExecutor<T> loader(*tree);
                    auto startW = chrono::high_resolution_clock::now();

                    loader.executeIngestion(TOTAL_RECORDS, false, {}, 0.0, 0);
                    tree->flush();

                    auto endW = chrono::high_resolution_clock::now();

                    double writeSec = chrono::duration<double>(endW - startW).count();
                    if (writeSec <= 0) writeSec = 0.001;

                    auto mWrite = tree->getMetrics();

                    double latSmallTotal = 0.0;
                    double latLargeTotal = 0.0;
                    int runs = 5;
                    tree->resetMetrics();

                    for(int i=0; i<runs; ++i) {
                        auto t1 = chrono::high_resolution_clock::now();
                        tree->spatialRangeQuery(querySmall);
                        auto t2 = chrono::high_resolution_clock::now();
                        latSmallTotal += chrono::duration<double>(t2 - t1).count() * 1000.0;
                    }

                    for(int i=0; i<runs; ++i) {
                        auto t1 = chrono::high_resolution_clock::now();
                        tree->spatialRangeQuery(queryLarge);
                        auto t2 = chrono::high_resolution_clock::now();
                        latLargeTotal += chrono::duration<double>(t2 - t1).count() * 1000.0;
                    }

                    auto mRead = tree->getMetrics();

                    double avgLatSmall = latSmallTotal / (double)runs;
                    double avgLatLarge = latLargeTotal / (double)runs;
                    double avgRA = (double)mRead.readAmplification / (double)(runs * 2);

                    csv << cfg.alias << "," << cfg.policy << "," << cfg.param << ","
                        << cfg.comparator << "," << cfg.partition << ","
                        << mWrite.totalWrites << ","
                        << fixed << setprecision(4) << writeSec << ","
                        << (TOTAL_RECORDS / writeSec) << ","
                        << mWrite.writeAmplification << ","
                        << avgLatSmall << ","
                        << avgLatLarge << ","
                        << avgRA << "\n";

                    csv.flush();
                    cout << "   [DONE] WA: " << mWrite.writeAmplification
                            << " | Lat(S): " << avgLatSmall << "ms\n";
                }

                executor.executeClean(tableName);
                csv.close();
                cout << "\n*** CSV GENERADO: " << csvPath << " ***\n";
            }
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

                if (input == "test") {
                    runFullExperiment();
                    continue;
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
                // Manejar comandos especiales primero
                string trimmedCmd = sql;
                trimmedCmd.erase(0, trimmedCmd.find_first_not_of(" \t\n\r"));

                if (trimmedCmd.rfind("clean", 0) == 0)
                {
                    return executor.executeClean(trimmedCmd.substr(5));
                }
                else if (trimmedCmd.rfind("benchmark", 0) == 0)
                {
                    stringstream ss(trimmedCmd);
                    string cmd, tableName;
                    int numRecords = 150000; // Default
                    ss >> cmd >> tableName;

                    // Intentar leer cantidad opcional
                    if (ss >> numRecords) {
                        // Se proporcionó cantidad
                    } else {
                        // Usar default
                        numRecords = 150000;
                    }

                    if (tableName.empty())
                    {
                        return "Error: Usage: benchmark <table_name> [num_records]";
                    }

                    // Normalizar nombre
                    transform(tableName.begin(), tableName.end(), tableName.begin(),
                              [](unsigned char c) { return std::tolower(c); });

                    if (!catalog.tableExists(tableName))
                    {
                        return "Error: Table '" + tableName + "' does not exist.";
                    }

                    lsm::LSMTree<T> *tree = tableIndices[tableName].secondary;
                    workload::WorkloadExecutor<T> loader(*tree);

                    auto start = chrono::high_resolution_clock::now();
                    loader.executeIngestion(numRecords, false, {}, 0.0, 0);
                    tree->flush();
                    auto end = chrono::high_resolution_clock::now();
                    auto duration = chrono::duration_cast<chrono::seconds>(end - start).count();

                    stringstream result;
                    result << "Benchmark completed in " << duration << " seconds. ";
                    result << "Throughput: " << (numRecords / (duration > 0 ? duration : 1)) << " ops/sec";
                    return result.str();
                }
                else
                {
                    // Comandos SQL normales
                    return executor.execute(sql);
                }
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

            size_t TOTAL_RECORDS = 150000;

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
