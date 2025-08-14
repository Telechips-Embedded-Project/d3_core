// shm_manager.c
#include "../include/system_status.h"
#include "../include/shm_manager.h"
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>


int shm_init(key_t key, int *out_id, system_status_t **out_ptr)
{
    if (!out_id || !out_ptr) return -1;
    *out_id = -1;
    *out_ptr = NULL;

    printf("[SHM LOG] sizeof(system_status_t)=%zu\n", sizeof(system_status_t));
    printf("[SHM LOG] key=%ld\n", (long)key);

    int created = 1;
    int id = shmget(key, sizeof(system_status_t), IPC_CREAT|IPC_EXCL|0666);
    if (id < 0) {
        if (errno == EEXIST) {
            created = 0;
            id = shmget(key, sizeof(system_status_t), 0666);
        }
    }
    if (id < 0) { perror("shmget"); return -1; }

    void *addr = shmat(id, NULL, 0);
    if (addr == (void*)-1) { perror("shmat"); return -1; }

    // out 파라미터 채우기
    *out_id  = id;
    *out_ptr = (system_status_t*)addr;

    // 전역도 함께 세팅(프로젝트에서 전역을 쓰고 있다면 편의상)
    shmid   = id;
    shm_ptr = *out_ptr;

    // 최초 생성자만 0으로 초기화
    pthread_mutex_lock(&shm_mutex);
    if (created) {
        memset(*out_ptr, 0, sizeof(**out_ptr));
        (*out_ptr)->ambient.color[0] = '\0';  // %s 안전
        printf("[SHM LOG] initialized new segment\n");
    }
    pthread_mutex_unlock(&shm_mutex);

    return 0;
}

void shm_detach(system_status_t *p) { 
    if (p) shmdt(p); 
}
void shm_destroy(int id) { 
    if (id >= 0) shmctl(id, IPC_RMID, NULL); 
}