#pragma once
#include "system_status.h"
#include <sys/types.h>
#include <pthread.h>

extern system_status_t *shm_ptr;
extern pthread_mutex_t shm_mutex;
extern int shmid;

// 키로 공유메모리 생성/연결, 성공시 0
int shm_init(key_t key, int *shmid, system_status_t **shm_ptr);
// Detach/destroy
void shm_detach(system_status_t *shm_ptr);
void shm_destroy(int shmid);