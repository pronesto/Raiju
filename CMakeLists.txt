cmake_minimum_required(VERSION 3.14)
project(LatticeStaticAnalyzer VERSION 1.0 LANGUAGES CXX)

# Force C++17 or higher (essential for enum class, std::vector, etc.)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==========================================
# 1. Main Application Target
# ==========================================

# Include directory for header files
include_directories(src)

# Define the source files for the analyzer core
set(ANALYZER_SOURCES
    src/AbstractValue.cpp
)

# Executable for the main static analyzer tool
add_executable(lattice_analyzer src/main.cpp ${ANALYZER_SOURCES})

# ==========================================
# 2. Testing Target (Using Catch2 via FetchContent)
# ==========================================

include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.4.0 # Stable Catch2 version
)
FetchContent_MakeAvailable(Catch2)

# Define the test executable
add_executable(analyzer_tests 
    tests/test_abstract_value.cpp 
    ${ANALYZER_SOURCES} # Link the core implementation files to the test
)

# Link Catch2's main entry point and library to the test executable
target_link_libraries(analyzer_tests PRIVATE Catch2::Catch2WithMain)

# Enable ctest command line utility
enable_testing()
add_test(NAME LatticeTests COMMAND analyzer_tests)
