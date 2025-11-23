#pragma once

#include "../cli/CLI.h"
#include "../spatial/Point.h"
#include "../spatial/MBR.h"
#include <string>
#include <vector>
#include <memory>
#include <chrono>

// Forward declarations
struct GLFWwindow;
struct ImVec2;
typedef unsigned int ImU32;

namespace gui {

template<typename T = int>
class SpatialGUI {
private:
    GLFWwindow* window;
    cli::CLI<T>* cli;
    bool running;

    // Estado de la UI
    char tableNameBuffer[128];
    char sqlCommandBuffer[512];
    char logBuffer[4096];

    // Estado de visualización espacial
    float viewOffsetX;
    float viewOffsetY;
    float viewScale;

    // Consulta espacial interactiva
    bool isDefiningQueryBox;
    sp::Point queryBoxStart;
    sp::Point queryBoxEnd;
    std::vector<sp::SpatialRecord<T>> queryResults;

    // Estado de inserción
    float insertX;
    float insertY;
    int insertValue;

    // Benchmarking
    bool isBenchmarkRunning;
    std::string benchmarkStatus;
    int benchmarkSize;

    // Tablas
    std::string selectedTable;
    std::vector<sp::SpatialRecord<T>> currentTableData;

    // Métricas en tiempo real
    std::chrono::steady_clock::time_point lastUpdate;

    void initImGui();
    void shutdownImGui();
    void renderMainWindow();
    void renderVisualizationWindow();
    void renderMetricsWindow();
    void renderControlPanel();
    void renderQueryWindow();
    void renderLogWindow();

    void drawPoint(float x, float y, ImU32 color, float size = 5.0f);
    void drawRectangle(float x1, float y1, float x2, float y2, ImU32 color, float thickness = 2.0f);
    void drawFilledRectangle(float x1, float y1, float x2, float y2, ImU32 color);

    void worldToScreen(float wx, float wy, float& sx, float& sy, const ImVec2& canvasPos, const ImVec2& canvasSize);
    void screenToWorld(float sx, float sy, float& wx, float& wy, const ImVec2& canvasPos, const ImVec2& canvasSize);

    void loadTableData();
    void autoFitView();
    void executeSQL(const std::string& sql);
    void addLog(const std::string& message);
    void handleBenchmark();
    void handleCSVImport(const std::string& filepath, const std::string& tableName);
    void handleImageImport(const std::string& filepath, const std::string& tableName);

public:
    SpatialGUI(cli::CLI<T>* cliInstance);
    ~SpatialGUI();

    bool initialize(int width = 1600, int height = 900);
    void run();
    void shutdown();
};

} // namespace gui
