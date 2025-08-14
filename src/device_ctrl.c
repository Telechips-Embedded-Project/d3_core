#include "device_ctrl.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

// 디바이스 제어 함수 
// 에어컨
void aircon_control(int level, system_status_t *shm) {
    int fd = open(AIRCON_DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        shm->aircon.error_flag = true;
        return;
    }
    if (ioctl(fd, AIRCON_SET_LEVEL, &level) < 0) {
        shm->aircon.error_flag = true;
        close(fd);
        return;
    }
    int cur_level = 0;
    if (ioctl(fd, AIRCON_GET_LEVEL, &cur_level) < 0) {
        shm->aircon.error_flag = true;
    } else {
        shm->aircon.level = cur_level;
        shm->aircon.error_flag = false;
    }
    shm->user.aircon_autoflag = 0;
    close(fd);
}

// 창문
void window_control(int value, system_status_t *shm) {
    int fd = open(WINDOW_DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        shm->window.error_flag = true;
        return;
    }
    if (ioctl(fd, WINDOW_SET_STATE, &value) < 0) {
        shm->window.error_flag = true;
    } else {
        int state = 0;
        if (ioctl(fd, WINDOW_GET_STATE, &state) == 0) {
            shm->window.level = state;
            shm->window.error_flag = false;
        } else {
            shm->window.error_flag = true;
        }
    }
    shm->user.window_autoflag = 0;
    close(fd);
}

// 와이퍼 
void wiper_control(int mode, system_status_t *shm) {
    int fd = open(WIPER_DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        shm->wiper.error_flag = true;
        return;
    }
    if (ioctl(fd, WIPER_SET_MODE, &mode) < 0) {
        shm->wiper.error_flag = true;
        close(fd);
        return;
    }
    int cur_mode = 0;
    if (ioctl(fd, WIPER_GET_MODE, &cur_mode) < 0) {
        shm->wiper.error_flag = true;
    } else {
        shm->wiper.level = cur_mode;
        shm->wiper.error_flag = false;
    }
    shm->user.wiper_autoflag = 0;
    close(fd);
}

// 헤드램프 
void headlamp_control(int on, system_status_t *shm) {
    int fd = open(HEADLAMP_DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        shm->headlamp.error_flag = true;
        return;
    }
    int value = (on ? 1 : 0);
    int ret = ioctl(fd, HEADLAMP_SET_STATE, &value);
    if (ret < 0) {
        shm->headlamp.error_flag = true;
        close(fd);
        return;
    }
    int state = 0;
    if (ioctl(fd, HEADLAMP_GET_STATE, &state) < 0) {
        shm->headlamp.error_flag = true;
    } else {
        shm->headlamp.level = state;
        shm->headlamp.error_flag = false;
    }
    close(fd);
}

// 앰비언트 // "low|mid|high" 고정 매핑
static int map_fixed_brightness(const char *sv) {
    if (!sv) return 50;
    if (!strcasecmp(sv, "low"))  return 20;
    if (!strcasecmp(sv, "mid"))  return 50;
    if (!strcasecmp(sv, "high")) return 100;
    return 50; // 디폴트
}

static const char* map_color_any(const char *sv) {
    // color는 문자열만 사용 (숫자 경로 제거)
    if (sv && *sv) return sv;
    return "off";
}

// AMBIENT
// value 제거 버전
void ambient_control(int command_type, const char *svalue, system_status_t *shm) {
    int fd = open(AMBIENT_DEVICE_PATH, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        shm->ambient.error_flag = true;
        return;
    }

    int ret = -1;
    if (command_type == 0) {
        // color
        const char *mode_str = map_color_any(svalue);
        ret = ioctl(fd, AMBIENT_SET_MODE, (void*)mode_str);
    } else if (command_type == 1) {
        // brightness (low/mid/high)
        int bright = map_fixed_brightness(svalue);
        ret = ioctl(fd, AMBIENT_SET_BRIGHTNESS, &bright);
    } else {
        close(fd);
        shm->ambient.error_flag = true;
        return;
    }

    if (ret < 0) {
        shm->ambient.error_flag = true;
        close(fd);
        return;
    }

    // 상태 동기화
    char cur_mode[32] = {0};
    int  cur_brightness = 0;

    if (ioctl(fd, AMBIENT_GET_MODE, cur_mode) < 0 ||
        ioctl(fd, AMBIENT_GET_BRIGHTNESS, &cur_brightness) < 0) {
        shm->ambient.error_flag = true;
        close(fd);
        return;
    }

    shm->ambient.error_flag = false;
    strncpy(shm->ambient.color, cur_mode, sizeof(shm->ambient.color)-1);

    // 20/50/100 기준으로 1/2/3 매핑
    if      (cur_brightness <= 20)  shm->ambient.brightness_level = 1; // low
    else if (cur_brightness <= 60)  shm->ambient.brightness_level = 2; // mid
    else                            shm->ambient.brightness_level = 3; // high

    shm->user.ambient_autoflag = 0;
    close(fd);
}

// 종료 처리 함수들
void cleanup() {
    printf("\n[SHUTDOWN] Cleaning up and turning off devices...\n");
    pthread_mutex_lock(&shm_mutex);
    aircon_control(0, shm_ptr);
    window_control(0, shm_ptr);
    headlamp_control(0, shm_ptr);
    wiper_control(0, shm_ptr);
    ambient_control(0, "off", shm_ptr);
    ambient_control(1, "low", shm_ptr);
    pthread_mutex_unlock(&shm_mutex);
}

void signal_handler(int sig) {
    cleanup();
    exit(0);
}
