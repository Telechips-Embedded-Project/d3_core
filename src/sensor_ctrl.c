#include "../include/sensor_ctrl.h"
#include <unistd.h>
#include <string.h>   // strcpy, strncpy
#ifndef WIPER_OFF
#define WIPER_OFF   0
#endif
#ifndef WIPER_SLOW
#define WIPER_SLOW  1
#endif
#ifndef WIPER_FAST
#define WIPER_FAST  2
#endif

extern command_t cmd;

// 현재 온도에 따른 에어컨 세기 결정 (level: 0~3)
static int aircon_set_from_target(int target_temp, const system_status_t *shm)
{
    int cur = (int)shm->sensor.temperature;
    int diff = cur - target_temp; // 양수면 목표보다 더움 → 더 세게
    int level = 0;

    if      (diff >= 4) level = 3;
    else if (diff >= 2) level = 2;
    else if (diff >= 1) level = 1;
    else                level = 0;

    return level;
}

void* auto_sensor_control_thread(void* arg)
{
    // 자동 개입 상태/복귀용
    int aircon_active   = 0, aircon_prev   = 0;
    int window_active   = 0, window_prev   = 0;
    int wiper_prev     = 0;

    // “사용자 자동 플래그”의 전이 감지용
    int aircon_was_auto = 0;
    int window_was_auto = 0;
    int amb_was_auto    = 0;

    // 앰비언트 복원용
    char ambient_prev_color[16] = {0};


    while (1) {
        // ------- 스냅샷 읽기 -------
        uint8_t CO2_flag, Headlight_flag, Wiper_flag;
        int aircon_lvl, window_lvl;
        char ambient_color_str[16] = {0};
        user_state_t user;

        pthread_mutex_lock(&shm_mutex);

        CO2_flag       = shm_ptr->sensor.CO2_flag;
        Headlight_flag = shm_ptr->sensor.Headlight_flag;
        Wiper_flag     = shm_ptr->sensor.Wiper_flag;

        aircon_lvl   = shm_ptr->aircon.level;
        window_lvl   = shm_ptr->window.level;

        //strncpy(ambient_color_str, shm_ptr->ambient.color, sizeof(ambient_color_str)-1);
        snprintf(ambient_color_str, sizeof(ambient_color_str), "%s", shm_ptr->ambient.color);

        user = shm_ptr->user;
        pthread_mutex_unlock(&shm_mutex);
        // --------------------------------

        // ===== AMBIENT =====
        if (user.ambient_autoflag && !amb_was_auto) {
            //strncpy(ambient_prev_color, ambient_color_str, sizeof(ambient_prev_color)-1);
            snprintf(ambient_prev_color, sizeof(ambient_prev_color), "%s", ambient_color_str);
            //ambient_prev_brightness = ambient_brightness_now; // 0, 1, 2
        }
        if (user.ambient_autoflag) {
            ambient_control(0, user.ambient_color, shm_ptr, 0);
        }
        if (!user.ambient_autoflag && amb_was_auto) {
            ambient_control(0, ambient_prev_color, shm_ptr, 0);
        }
        amb_was_auto = user.ambient_autoflag;

        // ===== AIRCON =====
        if (user.aircon_autoflag && !aircon_was_auto) {
            aircon_prev = aircon_lvl;
        }
        if (user.aircon_autoflag) {
            int target = (user.aircon_val > 0) ? user.aircon_val : 24;
            int new_lvl = aircon_set_from_target(target, shm_ptr);
            if (!aircon_active || aircon_lvl != new_lvl) {
                aircon_control(new_lvl, shm_ptr, 1);
                aircon_lvl = new_lvl;
                aircon_active = (new_lvl > 0) ? 1 : 0;
            }
        } else {
            if (aircon_was_auto && aircon_active) {
                aircon_control(aircon_prev, shm_ptr, 1);
            }
            aircon_active = 0;
        }
        aircon_was_auto = user.aircon_autoflag;

        // ===== WINDOW =====
        if (user.window_autoflag && !window_was_auto) {
            // 자동 진입 시 현재 상태 저장
            window_prev = window_lvl;
            // 자동 시작할 때는 아직 개입 안 한 상태로 둠
            // window_active = 0;  // 필요 시 명시
        }

        if (user.window_autoflag) {
            // 조건: CO2_flag == 2일 때만 창문을 연다. (한 번만 실행)
            if (CO2_flag == 2 && !window_active) {
                window_control(1, shm_ptr, 1);   // OPEN (프로젝트 매핑에 맞게 조정)
                window_active = 1;
            }
            // CO2_flag != 2인 동안에는 아무 것도 하지 않음(현재 상태 유지)
        } else {
            // autoflag 1 -> 0 전이 시, 자동으로 열어둔 상태였다면 prev로 복구
            if (window_was_auto && window_active) {
                window_control(window_prev, shm_ptr, 0);
            }
            window_active = 0;
        }

       // ===== WIPER =====
        if (user.wiper_autoflag) {
            if (Wiper_flag == 2) {
                wiper_control(WIPER_FAST, shm_ptr, 1);
            } else if (Wiper_flag == 1) {
                wiper_control(WIPER_SLOW, shm_ptr, 1);
            } else { // Wiper_flag == 0 → OFF
                int notify_off = (wiper_prev > 0 && Wiper_flag == 0) ? 1 : 0; // 1→0 하강엣지에만 1
                wiper_control(WIPER_OFF, shm_ptr, notify_off);
            }
        } else {
            int notify_off = (wiper_prev > 0 && Wiper_flag == 0) ? 1 : 0;
            wiper_control(WIPER_OFF, shm_ptr, notify_off);
        }
        wiper_prev     = Wiper_flag;

        // ===== HEADLAMP =====
        static int headlight_prev = -1;  // 미정 상태
        if (headlight_prev == -1) {
            headlight_prev = Headlight_flag;
            headlamp_control(Headlight_flag, shm_ptr, 0);
        } else if (Headlight_flag != headlight_prev) {
            // 상태가 바뀐 경우에만 호출
            int notify_off = (headlight_prev == 1 && Headlight_flag == 0); // 1→0일 때만 알림
            headlamp_control(Headlight_flag, shm_ptr, notify_off);
            headlight_prev = Headlight_flag;
        }
    }

    return NULL;
}
