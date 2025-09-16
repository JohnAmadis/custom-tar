cmake_minimum_required(VERSION 3.10)

# Set the project name
project(CustomTar)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Add the source files
add_executable(custom-tar
    main.cpp
    Archive.cpp
    Tlv.cpp
)

# Include directories if needed
# target_include_directories(custom-tar PRIVATE include)