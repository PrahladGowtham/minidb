cmake_minimum_required(VERSION 3.20)
project(minidb VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-Wall -Wextra -Wpedantic -g -O2)
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# Core library (storage, catalog, index, parser, execution, recovery, api)
file(GLOB_RECURSE MINIDB_SOURCES
    src/storage/disk/*.cpp
    src/storage/page/*.cpp
    src/storage/buffer/*.cpp
    src/storage/table/*.cpp
    src/index/*.cpp
    src/catalog/*.cpp
    src/parser/*.cpp
    src/execution/*.cpp
    src/recovery/*.cpp
    src/api/*.cpp
)

add_library(minidb_core ${MINIDB_SOURCES})
target_include_directories(minidb_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# CLI Shell Executable
add_executable(minidb_cli cli/main.cpp)
target_link_libraries(minidb_cli PRIVATE minidb_core)

# Enable testing
enable_testing()

add_executable(buffer_pool_test tests/buffer_pool_test.cpp)
target_link_libraries(buffer_pool_test PRIVATE minidb_core)
add_test(NAME BufferPoolTest COMMAND buffer_pool_test)

add_executable(slotted_page_test tests/slotted_page_test.cpp)
target_link_libraries(slotted_page_test PRIVATE minidb_core)
add_test(NAME SlottedPageTest COMMAND slotted_page_test)

add_executable(b_plus_tree_test tests/b_plus_tree_test.cpp)
target_link_libraries(b_plus_tree_test PRIVATE minidb_core)
add_test(NAME BPlusTreeTest COMMAND b_plus_tree_test)

add_executable(catalog_test tests/catalog_test.cpp)
target_link_libraries(catalog_test PRIVATE minidb_core)
add_test(NAME CatalogTest COMMAND catalog_test)

add_executable(sql_execution_test tests/sql_execution_test.cpp)
target_link_libraries(sql_execution_test PRIVATE minidb_core)
add_test(NAME SQLExecutionTest COMMAND sql_execution_test)

add_executable(wal_test tests/wal_test.cpp)
target_link_libraries(wal_test PRIVATE minidb_core)
add_test(NAME WALTest COMMAND wal_test)
