#!/bin/bash

# Script para descargar e instalar ImGui y sus backends

EXTERNAL_DIR="external"
IMGUI_VERSION="v1.90.1"

echo "Descargando ImGui ${IMGUI_VERSION}..."

# Crear directorio si no existe
mkdir -p ${EXTERNAL_DIR}

cd ${EXTERNAL_DIR}

# Descargar ImGui desde GitHub
if [ ! -d "imgui" ]; then
    echo "Clonando repositorio de ImGui..."
    git clone --depth 1 --branch ${IMGUI_VERSION} https://github.com/ocornut/imgui.git
else
    echo "ImGui ya existe, omitiendo descarga..."
fi

echo "✅ ImGui instalado correctamente en ${EXTERNAL_DIR}/imgui"
echo ""
echo "Archivos necesarios:"
echo "  - imgui/*.cpp"
echo "  - imgui/backends/imgui_impl_glfw.cpp"
echo "  - imgui/backends/imgui_impl_opengl3.cpp"
