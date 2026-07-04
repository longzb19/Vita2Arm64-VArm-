cmake_minimum_required(VERSION 3.10)
project(varm C)

# Ensure strict C11 standard compliance
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Locate the SDL2 package on the system
find_package(SDL2 REQUIRED)

# Define the executable target and its source files
add_executable(varm
    src/main.c
    src/hle_kernel.c
    src/hle_gxm.c
    src/hle_module.c
    src/varm_menu.c
    src/varm_input.c
    src/varm_graphics.c
    src/varm_cheats.c
    src/varm_system.c
    src/varm_gxm_backend.c
    src/varm_gxm_parser.c
    src/varm_gxp_shader.c   # 💡 Change this to varm_gxp_compiler.c if that's what's on disk!
    src/varm_bin_loader.c   # 💡 Change this to varm_elf_loader.c if that's what's on disk!
    src/varm_jit.c
    src/varm_audio.c        # 👈 FIX: Added our brand new audio module to the build queue
)

# Target-scoped include directories
target_include_directories(varm PRIVATE
    include
    varm-gxm/include
    ${SDL2_INCLUDE_DIRS}
)

# Link dynamic loading hooks, OpenGL ES 2.0, and SDL2 for controller/timing support
target_link_libraries(varm ${CMAKE_DL_LIBS} GLESv2 SDL2)
