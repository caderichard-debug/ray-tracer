cmake_minimum_required(VERSION 3.10)
project(RayTracer VERSION 1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler flags for optimization and SIMD
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native -mavx2 -mfma -ffast-math")
elseif(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /arch:AVX2 /fp:fast")
endif()

# Find OpenMP
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    target_link_libraries(raytracer PUBLIC OpenMP::OpenMP_CXX)
endif()

# Source files
set(SOURCES
    src/main.cpp
)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# Create executable
add_executable(raytracer ${SOURCES})

# Link OpenMP if found
if(OpenMP_CXX_FOUND)
    target_link_libraries(raytracer PUBLIC OpenMP::OpenMP_CXX)
endif()

# Set build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
if(OpenMP_CXX_FOUND)
    message(STATUS "OpenMP Found: YES")
else()
    message(STATUS "OpenMP Found: NO")
endif()
