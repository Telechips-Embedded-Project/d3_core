#ifndef SYSTEM_STATUS_H
#define SYSTEM_STATUS_H

#include <stdbool.h>
#include <stdint.h>   // 추가

#define SHM_KEY 0x8889

typedef struct {
    int brightness_level;   // 0: off, 1: low, 2: mid, 3: high
    char color[16];         // "red", "yellow", etc.
    bool error_flag;
} ambient_state_t;

typedef struct {
    int level;              // 0: off (장치별 모드/강도 등)
    bool error_flag;
} device_state_t;

typedef struct {
    uint8_t temperature;      // degC
    uint8_t humidity;         // %
    uint8_t CO2_flag;            
    uint8_t Headlight_flag;
    uint8_t Wiper_flag;
    // === CAN 원시/플래그 ===
    uint8_t temp_raw;       // 0..63 (0x700 하위 6bit)
    uint8_t humi_raw;       // 0..255 (0x700 data[1])
} sensor_state_t;

typedef struct {
    uint8_t FS_flag;        // Front Supersonic (0x300)
    uint8_t FO_flag;        // Front Object    (0x600)
    uint8_t LS_flag;        // Left  Supersonic(0x301)
    uint8_t RS_flag;        // Right Supersonic(0x302)
} notion_state_t;

typedef struct{
    int aircon_autoflag;
    int aircon_val;
    int ambient_autoflag;   
    char ambient_color[16];
    int window_autoflag;
    int wiper_autoflag;
    int wallpaper_num;
    int wallpaper_flag;
} user_state_t;

typedef struct {
    ambient_state_t ambient;
    device_state_t  aircon;
    device_state_t  window;
    device_state_t  headlamp;
    device_state_t  wiper;
    sensor_state_t  sensor;
    notion_state_t  notion;     //추가
    uint8_t         speed;      //추가 (0x200)
    user_state_t    user;       // 추가
} system_status_t;

#endif 