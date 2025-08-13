#pragma once
#include "device_ctrl.h"
#include "json_utils.h"
#include <pthread.h>
#include <string.h>
#include <stdio.h>    // snprintf


// 자동제어 default 값(reset)
#define AMBIENT_DEFAULT rainbow  // 엠비언트 기본값 rainbow 색상

//int aircon_set();
void* auto_sensor_control_thread(void* arg);
void* emergency_control_thread(void* arg);