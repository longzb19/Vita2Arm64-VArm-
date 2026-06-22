#ifndef VARM_SYSTEM_H
#define VARM_SYSTEM_H

void varm_system_init(void);
void varm_system_set_cpu_clock(int mhz);
void varm_system_set_gpu_clock(int mhz);
int varm_system_get_cpu_clock(void);
int varm_system_get_gpu_clock(void);

#endif // VARM_SYSTEM_H
