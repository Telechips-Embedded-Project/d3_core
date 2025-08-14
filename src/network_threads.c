#include "../include/network_threads.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <cjson/cJSON.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <pthread.h>
#include <errno.h>

#include "../include/json_utils.h"
#include "../include/network_threads.h" // shm_ptr, shm_mutex 정의 포함

#define CAN_INTERFACE "can0"
// --- Pi1로부터 BitNet 답변 수신---
void *tcp_rx_thread(void *arg) {
    char message[TCP_BUF_SIZE];
    int hServSock, hClntSock;
    ssize_t strLen;
    struct sockaddr_in servAdr, clntAdr;
    socklen_t clntAdrSize;

    hServSock = socket(PF_INET, SOCK_STREAM, 0);
    if (hServSock < 0) { perror("socket() failed"); pthread_exit(NULL); }

    int opt = 1;
    setsockopt(hServSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&servAdr, 0, sizeof(servAdr));
    servAdr.sin_family = AF_INET;
    servAdr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAdr.sin_port = htons(60003);

    if (bind(hServSock, (struct sockaddr*)&servAdr, sizeof(servAdr)) < 0) {
        perror("TCP RX bind() failed"); close(hServSock); pthread_exit(NULL);
    }
    if (listen(hServSock, 5) < 0) {
        perror("listen() failed"); close(hServSock); pthread_exit(NULL);
    }

    printf("[TCP RX] Listening on port %d...\n", 60003); fflush(stdout);

    for (;;) {
        clntAdrSize = sizeof(clntAdr);  // ★ accept() 직전마다 초기화
        hClntSock = accept(hServSock, (struct sockaddr*)&clntAdr, &clntAdrSize);
        if (hClntSock < 0) {
            if (errno == EINTR) continue;  // 시그널이면 재시도
            perror("accept() failed");     // 치명적 에러 아니면 계속 listen
            continue;
        }
        printf("[TCP RX LLM] LLM connected!\n"); fflush(stdout);

        for (;;) {
            strLen = recv(hClntSock, message, TCP_BUF_SIZE - 1, 0);
            if (strLen <= 0) {
                // 클라이언트 종료(0) 또는 에러(<0) -> 소켓 닫고 새 연결 대기
                if (strLen < 0) perror("recv() failed");
                printf("[TCP RX] Client disconnected\n"); fflush(stdout);
                close(hClntSock);
                break;  // ★ 바깥 for로 나가서 accept() 재진입
            }

            message[strLen] = '\0';
            printf("[TCP RX: BitNet Answer] recv: %s\n", message); fflush(stdout);

            // TTS가 블로킹이면 recv 타이밍에 영향 -> 필요시 비동기/스레드로
            run_piper(message);

            pthread_mutex_lock(&shm_mutex);
            handle_device_control(message);
            pthread_mutex_unlock(&shm_mutex);

            usleep(10000); // 10ms
        }
    }

    // 도달하지 않지만, 정리 코드 예시
    // close(hServSock);
    // pthread_exit(NULL);
}


// --- CAN 소켓 생성 ---
static int setup_can_socket(void) {
    int s;
    struct ifreq ifr;
    struct sockaddr_can addr;

    s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (s < 0) {
        perror("CAN socket");
        return -1;
    }

    strcpy(ifr.ifr_name, CAN_INTERFACE);
    if (ioctl(s, SIOCGIFINDEX, &ifr) < 0) {
        perror("SIOCGIFINDEX");
        close(s);
        return -1;
    }

    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("CAN bind");
        close(s);
        return -1;
    }

    return s;
}

static void print_all(unsigned char CO2_flag,
                      unsigned char Wiper_flag,
                      unsigned char Headlight_flag,
                      unsigned char LS_flag,
                      unsigned char RS_flag,
                      unsigned char FS_flag,
                      unsigned char FO_flag,
                      unsigned char temperature,
                      unsigned char humidity,
                      unsigned char Speed) {
    printf("CF: %d, WF: %d, HF: %d, LF: %d, RF: %d, FS: %d, FO: %d, temp: %2d, humi: %2d, Sp: %3d\n",
           CO2_flag, Wiper_flag, Headlight_flag, LS_flag, RS_flag, FS_flag, FO_flag, temperature, humidity, Speed);
}

// --- CAN RX 스레드 ---
void *can_rx_thread(void *arg) {
    int can_sock = setup_can_socket();
    if (can_sock < 0) {
        pthread_exit(NULL);
    }

    struct can_frame frame;

    // 내부 상태 변수
    unsigned char CO2_flag = 0;
    unsigned char temperature = 0;
    unsigned char humidity = 0;
    unsigned char Wiper_flag = 0;
    unsigned char Headlight_flag = 0;
    unsigned char LS_flag = 0;
    unsigned char RS_flag = 0;
    unsigned char FS_flag = 0;
    unsigned char FO_flag = 0;
    unsigned char Speed = 0;

    while (1) {
        int nbytes = read(can_sock, &frame, sizeof(frame));
        if (nbytes < 0) {
            perror("CAN read");
            continue;
        }

        printf("[CAN RX] CAN ID %03X\n", frame.can_id);

        pthread_mutex_lock(&shm_mutex);
        switch (frame.can_id) {
            case 0x200:
                Speed = frame.data[2];
				shm_ptr->speed = Speed;
                break;
            case 0x300:
                FS_flag = frame.data[0];
                shm_ptr->notion.FS_flag = FS_flag;
                break;
            case 0x301:
                LS_flag = frame.data[0];
                shm_ptr->notion.LS_flag = LS_flag;
                break;
            case 0x302:
                RS_flag = frame.data[0];
                shm_ptr->notion.RS_flag = RS_flag;
                break;
            case 0x500:
                Wiper_flag = frame.data[0] >> 1;
                Headlight_flag = frame.data[0] & 0b1;
                shm_ptr->sensor.Wiper_flag = Wiper_flag;
                shm_ptr->sensor.Headlight_flag = Headlight_flag;
                break;
            case 0x600:
                FO_flag = frame.data[0];
                shm_ptr->notion.FO_flag = FO_flag;
                break;
            case 0x700:
                CO2_flag = frame.data[0] >> 6;
                temperature = frame.data[0] & 0x3f;
                humidity = frame.data[1];
                shm_ptr->sensor.CO2_flag = CO2_flag;
                shm_ptr->sensor.temperature = temperature;
                shm_ptr->sensor.humidity = humidity;
                break;
            default:
                printf("[CAN RX] Invalid CAN ID\n");
                break;
        }
        pthread_mutex_unlock(&shm_mutex);

		// 디버깅용
        print_all(CO2_flag, Wiper_flag, Headlight_flag, LS_flag, RS_flag, FS_flag, FO_flag, temperature, humidity, Speed);

        usleep(10000);  // 10ms
    }

    close(can_sock);
    pthread_exit(NULL);
}
#if 01
//    printf("CF: %d, WF: %d, HF: %d, LF: %d, RF: %d, FS: %d, FO: %d, temp: %2d, humi: %2d, Sp: %3d\n",
//           CO2_flag, Wiper_flag, Headlight_flag, LS_flag, RS_flag, FS_flag, FO_flag, temperature, humidity, Speed);
// --- SHM 모니터 ---
void* shm_monitor_thread(void* arg) {
	while (1) {
		pthread_mutex_lock(&shm_mutex);
		printf("[SHM MON]\n");
		printf(" [SENSOR] temp: %d, humi: %d, co2 flag: %d, headlamp flag: %d, wiper flag: %d\n",
			shm_ptr->sensor.temperature,
			shm_ptr->sensor.humidity,
			shm_ptr->sensor.CO2_flag,
			shm_ptr->sensor.Headlight_flag,
			shm_ptr->sensor.Wiper_flag);
		printf(" [AIRCON] level: %d, error: %d\n",
			shm_ptr->aircon.level,
			shm_ptr->aircon.error_flag);
		printf(" [AMBIENT] color: %s, brightness: %d, error: %d\n",
			shm_ptr->ambient.color,
			shm_ptr->ambient.brightness_level,
			shm_ptr->ambient.error_flag);
		printf(" [WINDOW] level: %d, error: %d\n",
			shm_ptr->window.level,
			shm_ptr->window.error_flag);
		printf(" [HEADLAMP] level: %d, error: %d\n",
			shm_ptr->headlamp.level,
			shm_ptr->headlamp.error_flag);
		printf(" [WIPER] level: %d, error: %d\n",
			shm_ptr->wiper.level,
			shm_ptr->wiper.error_flag);
		pthread_mutex_unlock(&shm_mutex);
		sleep(1);
	}
	return NULL;
}
#endif