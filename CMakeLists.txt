cmake_minimum_required(VERSION 3.14)
project(LatticeStaticAnalyzer VERSION 1.0 LANGUAGES CXX)

# Force C++17 or higher
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==========================================
# 1. Main Application Target
# ==========================================

# Executable for the main static analyzer tool
add_executable(lattice_analyzer src/main.cpp src/Constraint.cpp src/Solver.cpp)

# Specify include directories directly on the target
target_include_directories(lattice_analyzer PRIVATE src)

# ==========================================
# 2. Testing Target (Using Catch2 via FetchContent)
# ==========================================

include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.4.0
)
FetchContent_MakeAvailable(Catch2)

# These are the files that we are currently testing using catch2:
add_executable(analyzer_tests
    tests/test_abstract_value.cpp
    tests/test_constraints.cpp
    src/Constraint.cpp
)

# Ensure the test target can also locate headers inside src/
target_include_directories(analyzer_tests PRIVATE src)

# Link Catch2's main entry point and library to the test executable
target_link_libraries(analyzer_tests PRIVATE Catch2::Catch2WithMain)

# Enable ctest command line utility
enable_testing()
add_test(NAME LatticeTests COMMAND analyzer_tests)
