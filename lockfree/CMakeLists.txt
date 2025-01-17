cmake_minimum_required(VERSION 3.12)
project(lockfree_stack_test)

# Specify C++23 standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable debug symbols
set(CMAKE_BUILD_TYPE Debug)

# Find GTest package
find_package(GTest REQUIRED)

# Add executable
add_executable(lockfree-stack-test lockfree-stack-test.cpp)

# Link against GTest
target_link_libraries(lockfree-stack-test
    PRIVATE
    GTest::GTest
    GTest::Main
)

# Include directories
target_include_directories(lockfree-stack-test
    PRIVATE
    ${GTEST_INCLUDE_DIRS}
)