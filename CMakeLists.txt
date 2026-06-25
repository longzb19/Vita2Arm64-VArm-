cmake_minimum_required(VERSION 3.10)
project(varm C)

set(CMAKE_C_STANDARD 11)

# Include directories for both main and submodule subdirectories
include_directories(include)
include_directories(varm-gxm/include)

# Unified target structure
add_executable(varm
    src/main.c
    src/hle_kernel.c
    src/hle_module.c
    src/varm_menu.c
    src/varm_input.c
    src/varm_graphics.c
    src/varm_cheats.c
    src/varm_system.c
    src/varm_gxm_backend.c
)

# Link dynamic loading hooks and explicit OpenGL ES 2.0 graphics libraries safely
target_link_libraries(varm ${CMAKE_DL_LIBS} GLESv2)
