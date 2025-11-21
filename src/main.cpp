#include <iostream>
#include <exception>
#include "../include/cli/CLI.h"

using namespace std;

int main(int argc, char* argv[]) {
    try {
        cli::CLI<int> app;
        app.start();
    } catch (const exception& e) {
        cerr << "\n[CRITICAL ERROR]: " << e.what() << endl;
        return 1;
    }
    return 0;
}