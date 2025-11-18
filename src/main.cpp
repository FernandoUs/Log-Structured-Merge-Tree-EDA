#include "cli/CLI.h"
#include "workload/Workload.h"
#include <iostream>
#include <iomanip>

using namespace std;

int main(int argc, char* argv[]) {
    try {
        cli::CLI<int> cli;

        if (argc > 1) {
            string mode(argv[1]);

            if (mode == "benchmark") {
                cout << "Running benchmark mode...\n";

                workload::DatasetGenerator generator;

                auto randomDataset = generator.generateRandomDataset<int>(10000);
                cout << "Generated " << randomDataset.size() << " random points\n";

                auto clusteredDataset = generator.generateClusteredDataset<int>(10000, 20);
                cout << "Generated " << clusteredDataset.size() << " clustered points\n";

                vector<spatial::MBR> highSelectivityQueries;
                vector<spatial::MBR> lowSelectivityQueries;

                for (int i = 0; i < 10; ++i) {
                    highSelectivityQueries.push_back(generator.generateQueryBox(1e-3));
                    lowSelectivityQueries.push_back(generator.generateQueryBox(1e-5));
                }

                cout << "Generated query sets\n";

                vector<workload::BenchmarkRunner<int>::BenchmarkConfig> configs = {
                    {"Binomial k=4 / Simple", "Binomial", "Simple", "Size", 4},
                    {"Binomial k=10 / Simple", "Binomial", "Simple", "Size", 10},
                    {"Binomial k=4 / Hilbert", "Binomial", "Hilbert", "Size", 4},
                    {"Tiered B=4 / Simple", "Tiered", "Simple", "Size", 4},
                    {"Tiered B=10 / Simple", "Tiered", "Simple", "Size", 10},
                    {"Leveled / STR / Simple", "Leveled", "Simple", "STR", 10},
                    {"Leveled / STR / Hilbert", "Leveled", "Hilbert", "STR", 10},
                    {"Leveled / RStarGrove / Simple", "Leveled", "Simple", "RStarGrove", 10},
                    {"Concurrent / Simple", "Concurrent", "Simple", "Size", 2}
                };

                workload::BenchmarkRunner<int> runner;

                cout << "\n=== Testing with Random Dataset ===\n";
                auto randomResults = runner.runComparison(configs, randomDataset, highSelectivityQueries);
                workload::BenchmarkRunner<int>::printResults(randomResults);

                cout << "\n=== Testing with Clustered Dataset ===\n";
                auto clusteredResults = runner.runComparison(configs, clusteredDataset, lowSelectivityQueries);
                workload::BenchmarkRunner<int>::printResults(clusteredResults);

            } else if (mode == "demo") {
                cout << "Running demo mode...\n\n";

                cli.executeCommand("CREATE TABLE cities (id INT, location POINT, population DOUBLE)");

                cout << "\nInserting sample data...\n";
                cli.executeCommand("INSERT INTO cities VALUES (0.1, 0.1, 1000000)");
                cli.executeCommand("INSERT INTO cities VALUES (0.5, 0.5, 500000)");
                cli.executeCommand("INSERT INTO cities VALUES (0.9, 0.9, 2000000)");
                cli.executeCommand("INSERT INTO cities VALUES (0.3, 0.7, 750000)");
                cli.executeCommand("INSERT INTO cities VALUES (0.8, 0.2, 300000)");

                cout << "\nExecuting spatial queries...\n";
                cout << cli.executeCommand("SELECT COUNT(*) FROM cities WHERE spatial_intersect(location, 0, 0, 0.5, 0.5)") << "\n";
                cout << cli.executeCommand("SELECT COUNT(*) FROM cities WHERE spatial_intersect(location, 0, 0, 1, 1)") << "\n";

                cout << "\nMetrics:\n";
                auto& trees = cli.getLSMTrees();
                if (trees.find("cities") != trees.end()) {
                    const auto& metrics = trees["cities"]->getMetrics();
                    cout << "  Total writes: " << metrics.totalWrites << "\n";
                    cout << "  Total reads: " << metrics.totalReads << "\n";
                    cout << "  Avg latency: " << metrics.avgQueryLatency << " ms\n";
                }

                cout << "\nDemo complete. Starting interactive mode...\n";
                cli.start();

            } else {
                cout << "Unknown mode: " << mode << "\n";
                cout << "Usage: " << argv[0] << " [benchmark|demo]\n";
                cout << "  benchmark - Run full performance evaluation\n";
                cout << "  demo      - Run interactive demo\n";
                cout << "  (no args) - Start interactive CLI\n";
                return 1;
            }
        } else {
            cli.start();
        }

    } catch (const exception& e) {
        cerr << "Fatal error: " << e.what() << endl;
        return 1;
    }

    return 0;
}
