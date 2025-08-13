#pragma once
#include "device_ctrl.h"
#include "tts_utils.h"
#include "json_utils.h"
#include "config.h"

void *tcp_rx_thread(void *arg);
//void *tcp_tx_thread(void *arg);
void *can_rx_thread(void *arg);
//void* udp_rx_thread(void* arg);
void* shm_monitor_thread(void* arg);