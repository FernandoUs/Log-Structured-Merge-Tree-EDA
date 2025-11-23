cmake_minimum_required(VERSION 3.10)
project(LSM_Spatial_DB VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(MSVC)
    add_compile_options(/W4 /WX-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

include_directories(${PROJECT_SOURCE_DIR}/include)

# Buscar dependencias para la GUI
find_package(OpenGL REQUIRED)
find_package(glfw3 3.3 REQUIRED)

# ImGui sources
set(IMGUI_DIR ${PROJECT_SOURCE_DIR}/external/imgui)
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

include_directories(
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

# CLI version (sin GUI)
set(SOURCES
    src/main.cpp
)

set(HEADERS
    include/spatial/Point.h
    include/spatial/MBR.h
    include/spatial/SpatialComparators.h
    include/spatial/RTree.h
    include/lsm/LSMComponent.h
    include/lsm/LSMTree.h
    include/lsm/MergePolicy.h
    include/lsm/PartitioningStrategy.h
    include/sql/QueryExecutor.h
    include/cli/CLI.h
    include/workload/Workload.h
)

add_executable(lsm_spatial_db ${SOURCES} ${HEADERS})

set_target_properties(lsm_spatial_db PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# GUI version
set(GUI_SOURCES
    src/main_gui.cpp
    src/gui/GUI.cpp
    ${IMGUI_SOURCES}
)

set(GUI_HEADERS
    ${HEADERS}
    include/gui/GUI.h
)

add_executable(lsm_spatial_db_gui ${GUI_SOURCES} ${GUI_HEADERS})

set_target_properties(lsm_spatial_db_gui PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_link_libraries(lsm_spatial_db_gui
    OpenGL::GL
    glfw
)

if(APPLE)
    target_link_libraries(lsm_spatial_db_gui
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
    )
endif()

install(TARGETS lsm_spatial_db lsm_spatial_db_gui DESTINATION bin)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
