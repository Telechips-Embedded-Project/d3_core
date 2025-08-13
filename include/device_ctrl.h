#pragma once
#include "config.h"
#include "system_status.h"
#include <pthread.h>

extern system_status_t *shm_ptr;
extern pthread_mutex_t shm_mutex;

// 디바이스 제어 함수
void aircon_control(int level, system_status_t *shm);
void window_control(int value, system_status_t *shm);
void wiper_control(int mode, system_status_t *shm);
void headlamp_control(int on, system_status_t *shm);
void ambient_control(int command_type, const char *svalue, system_status_t *shm);

void cleanup(void);
void signal_handler(int sig);
