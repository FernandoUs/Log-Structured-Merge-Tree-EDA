#include "../../include/gui/GUI.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <thread>

#if defined(__APPLE__)
#define GL_SILENCE_DEPRECATION
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
               << " (id INT, location POINT, value DOUBLE) with policy "
               << policies[currentPolicy] << " " << policyParam
               << " COMPARATOR " << comparators[currentComparator];
            executeSQL(ss.str());
            memset(tableNameBuffer, 0, sizeof(tableNameBuffer));
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
            viewScale = std::max(0.1f, std::min(viewScale, 10.0f));
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
                executeSQL(ss.str());
                loadTableData();
            }

            ImGui::Spacing();

            static int numRandomPoints = 1000;
            ImGui::InputInt("Puntos Aleatorios", &numRandomPoints);
            if (ImGui::Button("🎲 Insertar Aleatorios", ImVec2(-1, 0))) {
                for (int i = 0; i < numRandomPoints; ++i) {
                    float rx = (rand() % 2000 - 1000) / 10.0f;
                    float ry = (rand() % 2000 - 1000) / 10.0f;
                    int rv = rand() % 1000;
                    std::stringstream ss;
                    ss << "INSERT INTO " << selectedTable << " VALUES ("
                       << rx << ", " << ry << ", " << rv << ")";
                    executeSQL(ss.str());
                }
                loadTableData();
                addLog("Insertados " + std::to_string(numRandomPoints) + " puntos aleatorios");
            }
        }
    }

    if (ImGui::CollapsingHeader("Benchmark")) {
        if (selectedTable.empty()) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Selecciona una tabla primero");
        } else {
            static int benchmarkSize = 100000;
            ImGui::InputInt("Registros", &benchmarkSize);

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
            executeSQL(std::string(sqlCommandBuffer));
        }
    }

    if (ImGui::CollapsingHeader("Acciones")) {
        if (selectedTable.empty()) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Selecciona una tabla primero");
        } else {
            if (ImGui::Button("🗑️ Limpiar Tabla", ImVec2(-1, 0))) {
                std::string result = cli->executeCommand("clean " + selectedTable);
                addLog(result);
                loadTableData();
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
            ImGui::Text("%d", rec.data);
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

    // Obtener todos los datos (consulta completa)
    sp::Point minP({-1e6, -1e6});
    sp::Point maxP({1e6, 1e6});
    sp::MBR fullBox(minP, maxP);

    currentTableData = tree->spatialRangeQuery(fullBox);

    addLog("Tabla cargada: " + std::to_string(currentTableData.size()) + " registros");
}

template<typename T>
void SpatialGUI<T>::executeSQL(const std::string& sql) {
    std::string result = cli->executeCommand(sql);
    addLog("SQL: " + sql);
    addLog("Resultado: " + result);
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
    std::string cmd = "benchmark " + selectedTable;
    std::string result = cli->executeCommand(cmd);
    benchmarkStatus = "Completado: " + result;
    addLog("Benchmark finalizado: " + result);
    loadTableData();
}

// Instanciación explícita de templates
template class SpatialGUI<int>;
template class SpatialGUI<double>;

} // namespace gui
