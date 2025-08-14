#pragma once
#include "system_status.h"
#include "device_ctrl.h"

// 명령 타입 구조 (enum)
typedef enum {
    DEVICE_AIRCON,
    DEVICE_WINDOW,
    DEVICE_AMBIENT,
    DEVICE_HEADLAMP,
    DEVICE_WIPER,
    DEVICE_MUSIC,
    DEVICE_UNKNOWN
} device_type_t;


typedef struct {
    device_type_t device;
    int  command;          // color=0, brightness=1
    int value;
    char svalue[16];       // "red","yellow","green","rainbow","off" / "low","mid","high"
} command_t;

int parse_command_json(const char *json_str, command_t *cmd);
char* extract_json(const char *input);
void handle_device_control(const char* raw_json);
