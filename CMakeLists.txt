cmake_minimum_required(VERSION 3.22)
project(varm C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Configure compiler flags
if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-Wall -Wextra -O2)
endif()

# Include directories
include_directories(include)

# Define our dump utility executable
add_executable(varm src/main.c)
add_executable(varm-dump src/varm-dump.c)

# Set the output directory for the compiled binary to our local bin/ folder
set_target_properties(varm-dump PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
