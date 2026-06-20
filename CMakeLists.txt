cmake_minimum_required(VERSION 3.10)
project(varm C)

set(CMAKE_C_STANDARD 11)

# Tell CMake where your header files live
include_directories(include)

# ONE single definition for the varm binary containing all your source assets
add_executable(varm
    src/main.c
    src/hle_kernel.c
    varm-gxm/src/hle/gxm.c
)

# Keep your standalone dumper utility separate
add_executable(varm-dump
    src/varm-dump.c
)
