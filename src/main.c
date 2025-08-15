#include "../include/main.h"

system_status_t *shm_ptr = NULL;
pthread_mutex_t shm_mutex = PTHREAD_MUTEX_INITIALIZER;
int shmid;
int tid_size;


int main() {
    // 공유 메모리 초기화 (shm_manager)
    if (shm_init(SHM_KEY, &shmid, &shm_ptr) != 0) exit(1);
    printf("shm_ptr=%p, &shm_mutex=%p, shmid=%d\n", (void*)shm_ptr, (void*)&shm_mutex, shmid);

    // DB 초기화
    if(!init_db("settings.db")) {
        return 1;
    }
    // qt로 들어가야 함.
    // 프로필 저장// save_user_profile("user1", ac, window, wiper, ambient_color);
    // 프로필 불러오기 // load_user_profile("user1", ac, window, wiper, ambient_color);
    // 유저 리스트 // get_user_list();

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGQUIT, signal_handler);

    pthread_mutex_lock(&shm_mutex);
    memset(shm_ptr, 0, sizeof(system_status_t));
    pthread_mutex_unlock(&shm_mutex);

    pthread_t tid[10];
    pthread_create(&tid[0], NULL, thread_fifo_rx, shm_ptr);
    pthread_create(&tid[1], NULL, tcp_rx_thread, NULL);
    pthread_create(&tid[2], NULL, can_rx_thread, NULL);
    /////pthread_create(&tid[3], NULL, tcp_tx_thread, NULL);
    pthread_create(&tid[4], NULL, shm_monitor_thread, NULL);
    pthread_create(&tid[5], NULL, auto_sensor_control_thread, NULL);
    ////pthread_create(&tid[5], NULL, emergency_control_thread, NULL);
    tid_size = 7;

    for (int i = 0; i < tid_size; i++)
        pthread_join(tid[i], NULL);

    shm_detach(shm_ptr);
    shm_destroy(shmid); // 필요 시

    return 0;
}
