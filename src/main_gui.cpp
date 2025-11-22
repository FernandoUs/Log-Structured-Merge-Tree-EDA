#include <iostream>
#include <exception>
#include "../include/cli/CLI.h"
#include "../include/gui/GUI.h"

using namespace std;

int main(int argc, char* argv[]) {
    try {
        // Crear instancia de CLI (backend)
        cli::CLI<int> app;

        // Crear y configurar GUI
        gui::SpatialGUI<int> visualizer(&app);

        if (!visualizer.initialize(1600, 900)) {
            cerr << "Error al inicializar la interfaz gráfica" << endl;
            return 1;
        }

        cout << "🚀 LSM-Tree Spatial Database GUI iniciada" << endl;
        cout << "Presiona ESC o cierra la ventana para salir" << endl;

        // Ejecutar loop principal
        visualizer.run();

        cout << "👋 Gracias por usar el visualizador" << endl;

    } catch (const exception& e) {
        cerr << "\n[CRITICAL ERROR]: " << e.what() << endl;
        return 1;
    }

    return 0;
}
