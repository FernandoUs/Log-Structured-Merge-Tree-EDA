#pragma once

#include "../sql/QueryExecutor.h"
#include <iostream>
#include <string>
#include <memory>

using namespace std;

namespace cli {

using namespace sql;

template<typename T = int>
class CLI {
private:
    CatalogManager catalog;
    map<string, lsm::LSMTree<T>*> lsmTrees;
    QueryExecutor<T> executor;
    bool running;
    
public:
    CLI() : executor(catalog, lsmTrees), running(false) {
        vector<string> existingTables = catalog.getAllTableNames();
        for (const string& tableName : existingTables) {
            lsmTrees[tableName] = new lsm::LSMTree<T>(2);
            cout << "System: Restored table '" << tableName << "'" << endl;
        }
    }
    
    ~CLI() {
        for (auto const& [name, tree] : lsmTrees) {
            delete tree;
        }
    }

    void start() {
        running = true;
        
        printBanner();
        printHelp();
        
        string input;
        
        while (running) {
            cout << "\nspatial-db> ";
            getline(cin, input);
            
            if (input.empty()) continue;
            
            if (input == "exit" || input == "quit") {
                running = false;
                cout << "Goodbye!\n";
                break;
            }
            
            if (input == "help") {
                printHelp();
                continue;
            }
            
            if (input == "metrics") {
                printMetrics();
                continue;
            }
            
            if (input == "tables") {
                printTables();
                continue;
            }
            
            if (input == "clear") {
                clearMetrics();
                continue;
            }
            
            try {
                string result = executor.execute(input);
                cout << result << "\n";
            } catch (const exception& e) {
                cout << "Error: " << e.what() << "\n";
            }
        }
    }
    
    string executeCommand(const string& sql) {
        try {
            return executor.execute(sql);
        } catch (const exception& e) {
            return "Error: " + string(e.what());
        }
    }
    
    CatalogManager& getCatalog() { return catalog; }
    
    map<string, lsm::LSMTree<T>*>& getLSMTrees() {
        return lsmTrees;
    }
    
private:
    void printBanner() {
        cout << R"(
╔═══════════════════════════════════════════════════════════╗
║       LSM-Tree Spatial Database System                    ║
║       Implementation based on research paper              ║
║       "Comparison of LSM indexing techniques"             ║
╚═══════════════════════════════════════════════════════════╝
)" << "\n";
    }
    
    void printHelp() {
        cout << R"(
Available Commands:
  SQL Statements:
    CREATE TABLE name (col1 type1, col2 type2, ...)
    INSERT INTO table VALUES (x, y, data)
    SELECT COUNT(*) FROM table WHERE spatial_intersect(col, x1, y1, x2, y2)
    SELECT * FROM table WHERE spatial_intersect(col, x1, y1, x2, y2)
  
  Special Commands:
    help       - Show this help message
    metrics    - Display performance metrics
    tables     - List all tables
    clear      - Clear metrics
    exit/quit  - Exit the system
  
  Example Usage:
    CREATE TABLE points (id INT, location POINT, value DOUBLE)
    INSERT INTO points VALUES (0.5, 0.5, 100)
    SELECT COUNT(*) FROM points WHERE spatial_intersect(location, 0, 0, 1, 1)
)" << "\n";
    }
    
    void printMetrics() {
        cout << "\n=== Performance Metrics ===\n";
        
        for (const auto& [tableName, tree] : lsmTrees) {
            const auto& metrics = tree->getMetrics();
            
            cout << "\nTable: " << tableName << "\n";
            cout << "  Write Amplification: " << metrics.writeAmplification << "\n";
            cout << "  Read Amplification: " << metrics.readAmplification << "\n";
            cout << "  Total Writes: " << metrics.totalWrites << "\n";
            cout << "  Total Reads: " << metrics.totalReads << "\n";
            cout << "  Total Merges: " << metrics.totalMerges << "\n";
            cout << "  Avg Query Latency: " << metrics.avgQueryLatency << " ms\n";
            cout << "  Component Count: " << tree->getComponentCount() << "\n";
            cout << "  Total Records: " << tree->getTotalRecords() << "\n";
        }
    }
    
    void printTables() {
        cout << "\n=== Tables ===\n";
        
        for (const auto& [tableName, tree] : lsmTrees) {
            cout << "  - " << tableName << " (" << tree->getTotalRecords() << " records)\n";
        }
        
        if (lsmTrees.empty()) {
            cout << "  No tables created yet.\n";
        }
    }
    
    void clearMetrics() {
        for (auto& [tableName, tree] : lsmTrees) {
            tree->resetMetrics();
        }
        cout << "Metrics cleared.\n";
    }
};

}