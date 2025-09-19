cmake_minimum_required(VERSION 3.10)

# Set the project name
project(CustomTar)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set build type to Debug by default if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

add_subdirectory(zlib)

# Add debug flags for better debugging experience
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")

# Set output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Add the source files
add_executable(custom-tar
    main.cpp
    Archive.cpp
    xxhash.c
    FastLZ/fastlz.c
)

# Link libraries
target_link_libraries(custom-tar PRIVATE zlibstatic)

# Include directories if needed
# target_include_directories(custom-tar PRIVATE include)