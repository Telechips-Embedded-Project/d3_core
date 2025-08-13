#include "../include/fifo_thread.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>

command_t cmd;
// --- FIFO 명령 처리 스레드---
void *thread_fifo_rx(void *arg)
{
	system_status_t *shm = (system_status_t *)arg;

	if (access(FIFO_PATH, F_OK) == -1) {
		if (mkfifo(FIFO_PATH, 0666) < 0) {
			perror("mkfifo");
			pthread_exit(NULL);
		}
	}

	int fd = open(FIFO_PATH, O_RDONLY);
	if (fd < 0) {
		perror("open fifo");
		pthread_exit(NULL);
	}

	char buf[512];
	
	// shared memory 사용자 flag 0으로 만들기
	while (1) {
		memset(buf, 0, sizeof(buf));
		int n = read(fd, buf, sizeof(buf) - 1);
		if (n > 0) {
			printf("[fifo_rx] received: %s\n", buf);

			if (parse_command_json(buf, &cmd) == 0) {
				switch (cmd.device) {
				case DEVICE_AIRCON:
					pthread_mutex_lock(&shm_mutex);
					aircon_control(cmd.value, shm);
					pthread_mutex_unlock(&shm_mutex);
					break;
				case DEVICE_WINDOW:
					pthread_mutex_lock(&shm_mutex);
					window_control(cmd.value, shm);
					pthread_mutex_unlock(&shm_mutex);
					break;
				case DEVICE_AMBIENT:
					pthread_mutex_lock(&shm_mutex);
					ambient_control(cmd.command, cmd.svalue, shm);
					pthread_mutex_unlock(&shm_mutex);
					break;
				case DEVICE_HEADLAMP:
					pthread_mutex_lock(&shm_mutex);
					headlamp_control(cmd.value, shm);
					pthread_mutex_unlock(&shm_mutex);
					break;
				case DEVICE_WIPER:
					pthread_mutex_lock(&shm_mutex);
					wiper_control(cmd.value, shm);
					pthread_mutex_unlock(&shm_mutex);
					break;
				case DEVICE_MUSIC:
					// music_control(cmd.value, shm); // 구현해야함
					break;
				default:
					printf("unknown device\n");
					break;
				}
			}
		}
		sleep(1);
	}

	close(fd);
	return NULL;
}