 
#ifndef VARM_JIT_H
#define VARM_JIT_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initializes the JIT translation context and compiles/rebuilds
 * the static block caches before entering the main execution thread.
 * @param game_path Path to the game executable binary.
 */
void varm_jit_init(const char* game_path);

/**
 * @brief Advances the virtual execution environment by running active
 * compiled translation blocks for a cycle step.
 */
void varm_jit_execute_cycle(void);

#endif // VARM_JIT_H
