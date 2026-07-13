#ifndef HLE_THREAD_H
#define HLE_THREAD_H

#include <stdint.h>
#include <stdbool.h> //
#include <pthread.h> //

typedef int SceUID; //

// Simplified structure to track a virtual guest thread mapped to a host pthread
typedef struct {
    SceUID uid;
    char name[32];
    pthread_t host_thread;
    void *entry_point; // 🎯 Added to track guest binary execution target address
    bool active;
} Varm_ThreadEntry;

// Simplified structure to track guest mutexes mapped to host mutexes
typedef struct {
    SceUID uid;
    char name[32];
    pthread_mutex_t host_mutex;
    bool active;
} Varm_MutexEntry;

// HLE Function Hooks for the registry
SceUID hle_kernel_create_thread(const char *name, void *entry, int initPriority, int stackSize, uint32_t attr, int cpuMask, void *option);
int    hle_kernel_start_thread(SceUID thid, uint32_t args, void *argp);
SceUID hle_kernel_create_mutex(const char *name, uint32_t attr, int initCount, void *option);
int    hle_kernel_lock_mutex(SceUID mutexid, int lockCount, uint32_t *timeout);
int    hle_kernel_unlock_mutex(SceUID mutexid, int unlockCount);

void hle_thread_init(void);

#endif // HLE_THREAD_H
