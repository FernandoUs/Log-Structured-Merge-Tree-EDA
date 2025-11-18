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

install(TARGETS lsm_spatial_db DESTINATION bin)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
