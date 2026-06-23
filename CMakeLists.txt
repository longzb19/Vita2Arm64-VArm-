cmake_minimum_required(VERSION 3.10)
project(varm C)

set(CMAKE_C_STANDARD 11)

# Include directories for both main and submodule subdirectories
include_directories(include)
include_directories(varm-gxm/include)

# Look for your executable target configuration and add the new file:
add_executable(varm
    src/main.c
    src/hle_kernel.c
    src/hle_module.c             # <-- Drop our new modular registry framework here
    src/varm_menu.c
    src/varm_input.c
    src/varm_graphics.c         # <-- Added here
    src/varm_cheats.c           # <-- Added here
    src/varm_system.c           # <-- Added here
    varm-gxm/src/hle/gxm.c
)

# Crucial: This links the system loader library so dlopen works flawlessly on ARM64 Linux
target_link_libraries(varm ${CMAKE_DL_LIBS})
