#if defined(__APPLE__)
#define GL_SILENCE_DEPRECATION
#endif

#include "../../include/gui/GUI.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <thread>
#include <cstdio>

#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

namespace gui {

template<typename T>
SpatialGUI<T>::SpatialGUI(cli::CLI<T>* cliInstance)
    : window(nullptr)
    , cli(cliInstance)
    , running(false)
    , viewOffsetX(0.0f)
    , viewOffsetY(0.0f)
    , viewScale(1.0f)
    , isDefiningQueryBox(false)
    , queryBoxStart({0.0, 0.0})
    , queryBoxEnd({0.0, 0.0})
    , insertX(0.0f)
    , insertY(0.0f)
    , insertValue(0)
    , isBenchmarkRunning(false)
    , benchmarkSize(150000)
{
    memset(tableNameBuffer, 0, sizeof(tableNameBuffer));
    memset(sqlCommandBuffer, 0, sizeof(sqlCommandBuffer));
    memset(logBuffer, 0, sizeof(logBuffer));
    lastUpdate = std::chrono::steady_clock::now();
}

template<typename T>
SpatialGUI<T>::~SpatialGUI() {
    shutdown();
}

template<typename T>
bool SpatialGUI<T>::initialize(int width, int height) {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    // Configuración de OpenGL
    #if defined(__APPLE__)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    #endif

    window = glfwCreateWindow(width, height, "LSM-Tree Spatial Database Visualizer", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // VSync

    initImGui();

    addLog("Sistema inicializado correctamente");
    addLog("Bienvenido al visualizador LSM-Tree con R-Tree");

    return true;
}

template<typename T>
void SpatialGUI<T>::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Estilo moderno
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;

    // Colores personalizados
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.13f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.16f, 0.16f, 0.21f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.20f, 0.25f, 0.30f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.56f, 0.90f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.20f, 0.25f, 0.30f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    #if defined(__APPLE__)
    ImGui_ImplOpenGL3_Init("#version 150");
    #else
    ImGui_ImplOpenGL3_Init("#version 130");
    #endif
}

template<typename T>
void SpatialGUI<T>::shutdownImGui() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

template<typename T>
void SpatialGUI<T>::run() {
    running = true;

    while (running && !glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        renderMainWindow();
        renderVisualizationWindow();
        renderMetricsWindow();
        renderControlPanel();
        renderQueryWindow();
        renderLogWindow();

        ImGui::Render();

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.08f, 0.08f, 0.11f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }
}

template<typename T>
void SpatialGUI<T>::shutdown() {
    if (window) {
        shutdownImGui();
        glfwDestroyWindow(window);
        glfwTerminate();
        window = nullptr;
    }
}

template<typename T>
void SpatialGUI<T>::renderMainWindow() {
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_FirstUseEver);

    ImGui::Begin("🗄️ Gestor de Tablas", nullptr, ImGuiWindowFlags_NoCollapse);

    ImGui::Text("Crear Nueva Tabla:");
    ImGui::Separator();

    ImGui::InputText("Nombre", tableNameBuffer, sizeof(tableNameBuffer));

    static const char* policies[] = { "Binomial", "Tiered", "Concurrent", "Leveled" };
    static int currentPolicy = 1;
    ImGui::Combo("Política de Merge", &currentPolicy, policies, IM_ARRAYSIZE(policies));

    static int policyParam = 4;
    ImGui::SliderInt("Parámetro", &policyParam, 2, 10);

    static const char* comparators[] = { "Simple", "Hilbert" };
    static int currentComparator = 0;
    ImGui::Combo("Comparador Espacial", &currentComparator, comparators, IM_ARRAYSIZE(comparators));

    if (ImGui::Button("Crear Tabla", ImVec2(-1, 0))) {
        if (strlen(tableNameBuffer) > 0) {
            std::stringstream ss;
            ss << "CREATE TABLE " << tableNameBuffer
               << " (id INT, location POINT, value DOUBLE) WITH POLICY "
               << policies[currentPolicy] << " " << policyParam
               << " COMPARATOR " << comparators[currentComparator];
            std::string result = cli->executeCommand(ss.str());
            addLog(result);
            memset(tableNameBuffer, 0, sizeof(tableNameBuffer));
            loadTableData();
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Tablas Existentes:");

    auto tables = cli->getCatalog().getAllTableNames();
    for (const auto& table : tables) {
        if (ImGui::Selectable(table.c_str(), selectedTable == table)) {
            selectedTable = table;
            loadTableData();
            addLog("Tabla seleccionada: " + table);
        }
    }

    ImGui::End();
}

template<typename T>
void SpatialGUI<T>::renderVisualizationWindow() {
    ImGui::SetNextWindowPos(ImVec2(420, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(760, 600), ImGuiCond_FirstUseEver);

    ImGui::Begin("🗺️ Visualización Espacial", nullptr, ImGuiWindowFlags_NoCollapse);

    if (selectedTable.empty()) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Selecciona una tabla para visualizar");
        ImGui::End();
        return;
    }

    ImGui::Text("Tabla: %s | Registros: %lu", selectedTable.c_str(), currentTableData.size());
    ImGui::SameLine();
    if (ImGui::Button("🔄 Recargar")) {
        loadTableData();
    }

    // Mostrar info de bounding box si hay datos
    if (!currentTableData.empty()) {
        double minX = currentTableData[0].point[0];
        double maxX = currentTableData[0].point[0];
        double minY = currentTableData[0].point[1];
        double maxY = currentTableData[0].point[1];

        for (const auto& record : currentTableData) {
            minX = std::min(minX, record.point[0]);
            maxX = std::max(maxX, record.point[0]);
            minY = std::min(minY, record.point[1]);
            maxY = std::max(maxY, record.point[1]);
        }

        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
            "| Rango: X[%.1f, %.1f] Y[%.1f, %.1f]", minX, maxX, minY, maxY);
    }

    ImGui::Separator();

    // Canvas para visualización
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size = ImGui::GetContentRegionAvail();
    canvas_size.y -= 60; // Espacio para controles

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(canvas_pos,
        ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
        IM_COL32(20, 20, 25, 255));
    draw_list->AddRect(canvas_pos,
        ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
        IM_COL32(80, 80, 100, 255));

    ImGui::InvisibleButton("canvas", canvas_size);

    // Interacción con el mouse
    if (ImGui::IsItemHovered()) {
        // Zoom con scroll
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0) {
            viewScale *= (1.0f + wheel * 0.1f);
            viewScale = std::max(0.1f, std::min(viewScale, 50.0f));
        }

        // Pan con click derecho
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
            ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
            viewOffsetX += delta.x / viewScale;
            viewOffsetY += delta.y / viewScale;
            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Right);
        }

        // Definir caja de consulta con click izquierdo
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            ImVec2 mouse_pos = ImGui::GetMousePos();
            float wx, wy;
            screenToWorld(mouse_pos.x, mouse_pos.y, wx, wy, canvas_pos, canvas_size);
            queryBoxStart = sp::Point({static_cast<double>(wx), static_cast<double>(wy)});
            isDefiningQueryBox = true;
        }

        if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && isDefiningQueryBox) {
            ImVec2 mouse_pos = ImGui::GetMousePos();
            float wx, wy;
            screenToWorld(mouse_pos.x, mouse_pos.y, wx, wy, canvas_pos, canvas_size);
            queryBoxEnd = sp::Point({static_cast<double>(wx), static_cast<double>(wy)});
        }

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && isDefiningQueryBox) {
            isDefiningQueryBox = false;
            // Ejecutar consulta espacial
            if (!selectedTable.empty() && cli->getTableIndices().count(selectedTable) > 0) {
                sp::MBR queryBox(queryBoxStart, queryBoxEnd);
                auto* tree = cli->getTableIndices()[selectedTable].secondary;
                queryResults = tree->spatialRangeQuery(queryBox);
                addLog("Consulta espacial ejecutada: " + std::to_string(queryResults.size()) + " resultados");
            }
        }
    }

    // Dibujar puntos
    for (const auto& record : currentTableData) {
        float sx, sy;
        worldToScreen(record.point[0], record.point[1], sx, sy, canvas_pos, canvas_size);
        drawPoint(sx, sy, IM_COL32(100, 150, 255, 200), 4.0f);
    }

    // Dibujar resultados de consulta
    for (const auto& record : queryResults) {
        float sx, sy;
        worldToScreen(record.point[0], record.point[1], sx, sy, canvas_pos, canvas_size);
        drawPoint(sx, sy, IM_COL32(255, 100, 100, 255), 6.0f);
    }

    // Dibujar caja de consulta
    if (isDefiningQueryBox || !queryResults.empty()) {
        float x1, y1, x2, y2;
        worldToScreen(queryBoxStart[0], queryBoxStart[1], x1, y1, canvas_pos, canvas_size);
        worldToScreen(queryBoxEnd[0], queryBoxEnd[1], x2, y2, canvas_pos, canvas_size);
        drawRectangle(x1, y1, x2, y2, IM_COL32(255, 255, 0, 180), 2.0f);
    }

    ImGui::SetCursorScreenPos(ImVec2(canvas_pos.x, canvas_pos.y + canvas_size.y + 10));
    ImGui::Text("Controles: Click Izq=Consulta | Click Der+Arrastrar=Pan | Scroll=Zoom");
    ImGui::Text("Zoom: %.2fx | Offset: (%.1f, %.1f) | Resultados: %lu",
        viewScale, viewOffsetX, viewOffsetY, queryResults.size());

    if (ImGui::Button("🔄 Auto-Ajustar")) {
        autoFitView();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Vista")) {
        viewOffsetX = viewOffsetY = 0.0f;
        viewScale = 1.0f;
        queryResults.clear();
    }

    ImGui::End();
}

template<typename T>
void SpatialGUI<T>::renderMetricsWindow() {
    ImGui::SetNextWindowPos(ImVec2(10, 220), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 390), ImGuiCond_FirstUseEver);

    ImGui::Begin("📊 Métricas de Rendimiento", nullptr, ImGuiWindowFlags_NoCollapse);

    if (selectedTable.empty()) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Selecciona una tabla para ver métricas");
        ImGui::End();
        return;
    }

    auto& tableIndices = cli->getTableIndices();
    if (tableIndices.count(selectedTable) == 0) {
        ImGui::End();
        return;
    }

    const auto& metrics = tableIndices[selectedTable].secondary->getMetrics();

    ImGui::Text("Tabla: %s", selectedTable.c_str());
    ImGui::Separator();

    // Escritura
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Escritura");
    ImGui::Text("  Bytes Usuario: %llu", metrics.totalUserBytesWritten);
    ImGui::Text("  Bytes Disco: %llu", metrics.totalDiskBytesWritten);
    ImGui::Text("  Amplificación: %.2f", metrics.writeAmplification);
    ImGui::Text("  Total Escrituras: %llu", metrics.totalWrites);

    ImGui::Spacing();

    // Lectura
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.8f, 1.0f), "Lectura");
    ImGui::Text("  Amplificación: %llu", metrics.readAmplification);
    ImGui::Text("  Total Lecturas: %llu", metrics.totalReads);
    ImGui::Text("  Latencia Promedio: %.2f ms", metrics.avgQueryLatency);

    ImGui::Spacing();

    // Estructura
    ImGui::TextColored(ImVec4(0.8f, 0.6f, 0.4f, 1.0f), "Estructura");
    ImGui::Text("  Total Merges: %llu", metrics.totalMerges);
    ImGui::Text("  Componentes: %lu", tableIndices[selectedTable].secondary->getComponentCount());
    ImGui::Text("  Registros Totales: %lu", tableIndices[selectedTable].secondary->getTotalRecords());

    ImGui::Spacing();
    ImGui::Separator();

    if (ImGui::Button("🔄 Limpiar Métricas", ImVec2(-1, 0))) {
        tableIndices[selectedTable].secondary->resetMetrics();
        addLog("Métricas limpiadas para " + selectedTable);
    }

    ImGui::End();
}

template<typename T>
void SpatialGUI<T>::renderControlPanel() {
    ImGui::SetNextWindowPos(ImVec2(1190, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 600), ImGuiCond_FirstUseEver);

    ImGui::Begin("⚙️ Panel de Control", nullptr, ImGuiWindowFlags_NoCollapse);

    if (ImGui::CollapsingHeader("Inserción de Datos", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (selectedTable.empty()) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Selecciona una tabla primero");
        } else {
            ImGui::InputFloat("X", &insertX);
            ImGui::InputFloat("Y", &insertY);
            ImGui::InputInt("Valor", &insertValue);

            if (ImGui::Button("➕ Insertar Punto", ImVec2(-1, 0))) {
                std::stringstream ss;
                ss << "INSERT INTO " << selectedTable << " VALUES ("
                   << insertX << ", " << insertY << ", " << insertValue << ")";
                std::string result = cli->executeCommand(ss.str());
                addLog(result);
                loadTableData();
            }
        }
    }

    if (ImGui::CollapsingHeader("Benchmark")) {
        if (selectedTable.empty()) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Selecciona una tabla primero");
        } else {
            ImGui::InputInt("Registros", &benchmarkSize);
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Cantidad de puntos aleatorios a insertar");

            if (isBenchmarkRunning) {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "⏳ Ejecutando...");
                ImGui::Text("%s", benchmarkStatus.c_str());
            } else {
                if (ImGui::Button("🚀 Ejecutar Benchmark", ImVec2(-1, 0))) {
                    isBenchmarkRunning = true;
                    benchmarkStatus = "Iniciando benchmark...";

                    // Ejecutar en hilo separado
                    std::thread([this]() {
                        handleBenchmark();
                        isBenchmarkRunning = false;
                    }).detach();
                }
            }
        }
    }

    if (ImGui::CollapsingHeader("Comandos SQL")) {
        ImGui::InputTextMultiline("##sql", sqlCommandBuffer, sizeof(sqlCommandBuffer),
            ImVec2(-1, 100));

        if (ImGui::Button("▶️ Ejecutar SQL", ImVec2(-1, 0))) {
            if (strlen(sqlCommandBuffer) > 0) {
                std::string cmd(sqlCommandBuffer);
                std::string result = cli->executeCommand(cmd);
                addLog("SQL: " + cmd);
                addLog("Resultado: " + result);
                loadTableData();
            }
        }
    }

    if (ImGui::CollapsingHeader("Acciones")) {
        if (selectedTable.empty()) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Selecciona una tabla primero");
        } else {
            if (ImGui::Button("🗑️ Limpiar Tabla", ImVec2(-1, 0))) {
                std::string result = cli->executeCommand("clean " + selectedTable);
                addLog(result);
                selectedTable.clear();
                currentTableData.clear();
                queryResults.clear();
            }
        }
    }

    if (ImGui::CollapsingHeader("Importar Datos")) {
        static char csvPath[256] = "";
        static char csvTableName[128] = "";

        ImGui::Text("Importar desde CSV (id, x, y):");
        ImGui::InputText("Ruta CSV", csvPath, sizeof(csvPath));
        ImGui::InputText("Nombre Tabla##csv", csvTableName, sizeof(csvTableName));

        if (ImGui::Button("📂 Cargar CSV", ImVec2(-1, 0))) {
            if (strlen(csvPath) > 0 && strlen(csvTableName) > 0) {
                handleCSVImport(std::string(csvPath), std::string(csvTableName));
            } else {
                addLog("Error: Especifica ruta y nombre de tabla");
            }
        }

        ImGui::Spacing();
        ImGui::Separator();

        static char imagePath[256] = "";
        static char imageTableName[128] = "";

        ImGui::Text("Convertir Imagen a Puntos:");
        ImGui::InputText("Ruta Imagen", imagePath, sizeof(imagePath));
        ImGui::InputText("Nombre Tabla##img", imageTableName, sizeof(imageTableName));

        if (ImGui::Button("🖼️ Cargar Imagen", ImVec2(-1, 0))) {
            if (strlen(imagePath) > 0 && strlen(imageTableName) > 0) {
                handleImageImport(std::string(imagePath), std::string(imageTableName));
            } else {
                addLog("Error: Especifica ruta y nombre de tabla");
            }
        }
    }

    ImGui::End();
}

template<typename T>
void SpatialGUI<T>::renderQueryWindow() {
    ImGui::SetNextWindowPos(ImVec2(420, 620), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(760, 270), ImGuiCond_FirstUseEver);

    ImGui::Begin("🔍 Resultados de Consulta", nullptr, ImGuiWindowFlags_NoCollapse);

    ImGui::Text("Resultados encontrados: %lu", queryResults.size());
    ImGui::Separator();

    if (ImGui::BeginTable("QueryResults", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("X");
        ImGui::TableSetupColumn("Y");
        ImGui::TableSetupColumn("Valor");
        ImGui::TableHeadersRow();

        int displayLimit = std::min((int)queryResults.size(), 50);
        for (int i = 0; i < displayLimit; ++i) {
            const auto& rec = queryResults[i];
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%.2f", rec.point[0]);
            ImGui::TableNextColumn();
            ImGui::Text("%.2f", rec.point[1]);
            ImGui::TableNextColumn();
            ImGui::Text("%f", (double)rec.data);
        }

        if (queryResults.size() > 50) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                "... y %lu más", queryResults.size() - 50);
        }

        ImGui::EndTable();
    }

    ImGui::End();
}

template<typename T>
void SpatialGUI<T>::renderLogWindow() {
    ImGui::SetNextWindowPos(ImVec2(10, 620), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 270), ImGuiCond_FirstUseEver);

    ImGui::Begin("📋 Log del Sistema", nullptr, ImGuiWindowFlags_NoCollapse);

    ImGui::TextWrapped("%s", logBuffer);

    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0f);

    ImGui::End();
}

template<typename T>
void SpatialGUI<T>::drawPoint(float x, float y, ImU32 color, float size) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddCircleFilled(ImVec2(x, y), size, color);
}

template<typename T>
void SpatialGUI<T>::drawRectangle(float x1, float y1, float x2, float y2, ImU32 color, float thickness) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRect(ImVec2(x1, y1), ImVec2(x2, y2), color, 0.0f, 0, thickness);
}

template<typename T>
void SpatialGUI<T>::drawFilledRectangle(float x1, float y1, float x2, float y2, ImU32 color) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(ImVec2(x1, y1), ImVec2(x2, y2), color);
}

template<typename T>
void SpatialGUI<T>::worldToScreen(float wx, float wy, float& sx, float& sy,
    const ImVec2& canvasPos, const ImVec2& canvasSize) {
    sx = canvasPos.x + canvasSize.x / 2 + (wx + viewOffsetX) * viewScale;
    sy = canvasPos.y + canvasSize.y / 2 - (wy + viewOffsetY) * viewScale;
}

template<typename T>
void SpatialGUI<T>::screenToWorld(float sx, float sy, float& wx, float& wy,
    const ImVec2& canvasPos, const ImVec2& canvasSize) {
    wx = (sx - canvasPos.x - canvasSize.x / 2) / viewScale - viewOffsetX;
    wy = -(sy - canvasPos.y - canvasSize.y / 2) / viewScale - viewOffsetY;
}

template<typename T>
void SpatialGUI<T>::loadTableData() {
    currentTableData.clear();

    if (selectedTable.empty() || cli->getTableIndices().count(selectedTable) == 0) {
        return;
    }

    auto* tree = cli->getTableIndices()[selectedTable].secondary;

    // Obtener todos los datos (consulta completa con rango muy amplio)
    sp::Point minP({-1e9, -1e9});
    sp::Point maxP({1e9, 1e9});
    sp::MBR fullBox(minP, maxP);

    currentTableData = tree->spatialRangeQuery(fullBox);

    addLog("Tabla cargada: " + std::to_string(currentTableData.size()) + " registros");

    // Auto-ajustar vista a los datos cargados
    if (!currentTableData.empty()) {
        autoFitView();
    }
}

template<typename T>
void SpatialGUI<T>::autoFitView() {
    if (currentTableData.empty()) {
        return;
    }

    // Calcular bounding box de todos los puntos
    double minX = currentTableData[0].point[0];
    double maxX = currentTableData[0].point[0];
    double minY = currentTableData[0].point[1];
    double maxY = currentTableData[0].point[1];

    for (const auto& record : currentTableData) {
        double x = record.point[0];
        double y = record.point[1];

        if (x < minX) minX = x;
        if (x > maxX) maxX = x;
        if (y < minY) minY = y;
        if (y > maxY) maxY = y;
    }

    // Calcular centro y escala
    double centerX = (minX + maxX) / 2.0;
    double centerY = (minY + maxY) / 2.0;
    double rangeX = maxX - minX;
    double rangeY = maxY - minY;

    // Evitar división por cero
    if (rangeX < 0.001) rangeX = 100.0;
    if (rangeY < 0.001) rangeY = 100.0;

    // Calcular escala para que quepa todo en la vista
    // Usamos el 80% del canvas para dejar margen
    float canvasWidth = 700.0f;  // Aproximado
    float canvasHeight = 500.0f;

    float scaleX = (canvasWidth * 0.8f) / rangeX;
    float scaleY = (canvasHeight * 0.8f) / rangeY;

    // Usar la escala menor para que todo quepa
    viewScale = std::min(scaleX, scaleY);

    // Limitar escala mínima y máxima
    if (viewScale < 0.0001f) viewScale = 0.0001f;
    if (viewScale > 100.0f) viewScale = 1.0f;

    // Centrar la vista en el centro de los datos
    viewOffsetX = -centerX;
    viewOffsetY = -centerY;

    std::stringstream ss;
    ss << "Vista auto-ajustada: Centro(" << centerX << ", " << centerY
       << ") Escala: " << viewScale;
    addLog(ss.str());
}

template<typename T>
void SpatialGUI<T>::executeSQL(const std::string& sql) {
    std::string result = cli->executeCommand(sql);
    addLog("SQL: " + sql);
    addLog("Resultado: " + result);
    loadTableData();
}

template<typename T>
void SpatialGUI<T>::addLog(const std::string& message) {
    std::string timestamped = "[" +
        std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - lastUpdate).count()) + "s] " + message + "\n";

    size_t currentLen = strlen(logBuffer);
    size_t messageLen = timestamped.length();

    if (currentLen + messageLen < sizeof(logBuffer) - 1) {
        strcat(logBuffer, timestamped.c_str());
    } else {
        // Limpiar buffer si está lleno
        memset(logBuffer, 0, sizeof(logBuffer));
        strcpy(logBuffer, timestamped.c_str());
    }
}

template<typename T>
void SpatialGUI<T>::handleBenchmark() {
    if (selectedTable.empty() || cli->getTableIndices().count(selectedTable) == 0) {
        benchmarkStatus = "Error: No hay tabla seleccionada";
        return;
    }

    benchmarkStatus = "Ejecutando benchmark...";
    addLog("Iniciando benchmark en tabla: " + selectedTable);

    std::stringstream cmd;
    cmd << "benchmark " << selectedTable << " " << benchmarkSize;
    std::string result = cli->executeCommand(cmd.str());

    benchmarkStatus = "Completado";
    addLog("Benchmark finalizado: " + result);
    loadTableData();
}

template<typename T>
void SpatialGUI<T>::handleCSVImport(const std::string& filepath, const std::string& tableName) {
    addLog("Iniciando importación CSV desde: " + filepath);

    std::ifstream file(filepath);
    if (!file.is_open()) {
        addLog("Error: No se pudo abrir el archivo CSV");
        return;
    }

    // Crear tabla si no existe
    std::stringstream createCmd;
    createCmd << "CREATE TABLE " << tableName << " (id INT, location POINT, value DOUBLE) WITH POLICY Tiered 4 COMPARATOR Simple";
    std::string result = cli->executeCommand(createCmd.str());
    addLog("Crear tabla: " + result);

    // Leer CSV
    std::string line;
    bool firstLine = true;
    int count = 0;
    int errors = 0;

    // Auto-detectar delimitador
    char delimiter = ',';

    while (std::getline(file, line)) {
        // Limpiar espacios en blanco al inicio y final
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (line.empty()) continue;

        // Saltar primera línea si es header
        if (firstLine) {
            firstLine = false;
            // Auto-detectar delimitador
            if (line.find('\t') != std::string::npos) {
                delimiter = '\t';
                addLog("Delimitador detectado: TAB");
            } else {
                delimiter = ',';
                addLog("Delimitador detectado: COMA");
            }
            // Verificar si es header (contiene letras)
            if (line.find("id") != std::string::npos || line.find("x") != std::string::npos) {
                continue;
            }
        }

        // Parsear línea: id, x, y
        std::stringstream ss(line);
        std::string id_str, x_str, y_str;

        if (!std::getline(ss, id_str, delimiter) ||
            !std::getline(ss, x_str, delimiter) ||
            !std::getline(ss, y_str, delimiter)) {
            errors++;
            if (errors <= 5) {
                addLog("Error parseando línea: " + line);
            }
            continue;
        }

        // Trim espacios de cada campo
        auto trim = [](std::string& s) {
            s.erase(0, s.find_first_not_of(" \t\r\n"));
            s.erase(s.find_last_not_of(" \t\r\n") + 1);
        };
        trim(id_str);
        trim(x_str);
        trim(y_str);

        try {
            double x = std::stod(x_str);
            double y = std::stod(y_str);
            long long id = std::stoll(id_str);

            std::stringstream insertCmd;
            // Usar precision completa para coordenadas grandes
            insertCmd << std::fixed << std::setprecision(10);
            insertCmd << "INSERT INTO " << tableName << " VALUES ("
                      << x << ", " << y << ", " << id << ")";
            cli->executeCommand(insertCmd.str());
            count++;

            if (count % 1000 == 0) {
                addLog("Procesados " + std::to_string(count) + " registros...");
            }
        } catch (const std::exception& e) {
            errors++;
            if (errors <= 5) {
                addLog("Error convirtiendo datos: " + std::string(e.what()));
            }
        }
    }

    file.close();

    selectedTable = tableName;
    loadTableData();

    std::stringstream summary;
    summary << "CSV importado: " << count << " puntos cargados en tabla '" << tableName << "'";
    if (errors > 0) {
        summary << " (" << errors << " errores ignorados)";
    }
    addLog(summary.str());
}

template<typename T>
void SpatialGUI<T>::handleImageImport(const std::string& filepath, const std::string& tableName) {
    addLog("Iniciando conversión de imagen: " + filepath);

    // Verificar extensión
    std::string ext = filepath.substr(filepath.find_last_of(".") + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext != "png" && ext != "jpg" && ext != "jpeg" && ext != "bmp") {
        addLog("Error: Formato no soportado. Use PNG, JPG o BMP");
        return;
    }

    // Crear script Python temporal para procesar la imagen (optimizado para siluetas)
    std::string pythonScript = R"(
import sys
from PIL import Image
import numpy as np

def image_to_points(image_path, output_csv):
    try:
        img = Image.open(image_path)

        # Convertir a escala de grises
        img_gray = img.convert('L')

        # Redimensionar manteniendo aspect ratio (máximo 800x800 para mejor detalle)
        max_size = 800
        if img_gray.width > max_size or img_gray.height > max_size:
            img_gray.thumbnail((max_size, max_size), Image.Resampling.LANCZOS)

        # Convertir a numpy array
        pixels = np.array(img_gray)

        # Obtener dimensiones
        height, width = pixels.shape

        # Generar puntos solo en píxeles negros (siluetas/contornos)
        points = []
        threshold = 128  # Píxeles más oscuros que este valor se consideran negros

        # Optimización: procesar solo píxeles negros
        black_pixels = np.where(pixels < threshold)

        # Crear puntos a partir de píxeles negros con muestreo
        scale_x = 100.0 / width
        scale_y = 100.0 / height

        # Reducir densidad: tomar cada N píxeles (ajusta sampling_rate para más/menos puntos)
        sampling_rate = 3  # Tomar 1 de cada 3 píxeles (reduce ~66% de puntos)

        for i in range(0, len(black_pixels[0]), sampling_rate):
            y = black_pixels[0][i]
            x = black_pixels[1][i]

            # Normalizar coordenadas a rango [0, 100]
            nx = x * scale_x
            ny = (height - y - 1) * scale_y  # Invertir Y para que coincida con coord cartesianas

            points.append(f"{len(points)}\t{nx:.6f}\t{ny:.6f}\n")

        # Guardar a CSV
        with open(output_csv, 'w') as f:
            f.write("id\tx\ty\n")
            f.writelines(points)

        print(f"SUCCESS: {len(points)} points generated from silhouette")
        return 0
    except Exception as e:
        print(f"ERROR: {str(e)}")
        return 1

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python script.py <image_path> <output_csv>")
        sys.exit(1)
    sys.exit(image_to_points(sys.argv[1], sys.argv[2]))
)";

    // Guardar script temporal
    std::string scriptPath = "/tmp/image_to_points.py";
    std::ofstream scriptFile(scriptPath);
    scriptFile << pythonScript;
    scriptFile.close();

    // Ejecutar script Python
    std::string outputCSV = "/tmp/image_points_" + tableName + ".csv";
    std::string command = "python3 " + scriptPath + " \"" + filepath + "\" \"" + outputCSV + "\" 2>&1";

    addLog("Ejecutando conversión de imagen...");

    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        addLog("Error: No se pudo ejecutar Python");
        return;
    }

    char buffer[256];
    std::string pyOutput;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        pyOutput += buffer;
    }
    int status = pclose(pipe);

    if (status != 0 || pyOutput.find("ERROR") != std::string::npos) {
        addLog("Error en conversión: " + pyOutput);
        return;
    }

    addLog("Imagen convertida. Importando puntos...");

    // Importar el CSV generado
    handleCSVImport(outputCSV, tableName);

    // Limpiar archivo temporal
    std::remove(outputCSV.c_str());
}

// Instanciación explícita de templates
template class SpatialGUI<int>;
template class SpatialGUI<double>;

} // namespace gui
