#pragma once

// 디바이스 경로/매직넘버/IOCTL
#define AIRCON_DEVICE_PATH           "/dev/aircon_dev"
#define AIRCON_MAGIC                 'A'
#define AIRCON_SET_LEVEL             _IOW(AIRCON_MAGIC, 1, int)
#define AIRCON_GET_LEVEL             _IOR(AIRCON_MAGIC, 2, int)

#define WINDOW_DEVICE_PATH        "/dev/window_dev"
#define WINDOW_MAGIC              'M'
#define WINDOW_SET_STATE          _IOW(WINDOW_MAGIC, 0, int)
#define WINDOW_GET_STATE          _IOR(WINDOW_MAGIC, 1, int)

#define WIPER_DEVICE_PATH         "/dev/wiper_dev"
#define WIPER_MAGIC               'W'
#define WIPER_SET_MODE            _IOW(WIPER_MAGIC, 1, int)
#define WIPER_GET_MODE            _IOR(WIPER_MAGIC, 2, int)

#define HEADLAMP_DEVICE_PATH      "/dev/headlamp_dev"
#define HEADLAMP_MAGIC            'H'
#define HEADLAMP_SET_STATE        _IOW(HEADLAMP_MAGIC, 0, int)
#define HEADLAMP_GET_STATE        _IOR(HEADLAMP_MAGIC, 1, int)

#define AMBIENT_DEVICE_PATH            "/dev/ambient_dev"
#define AMBIENT_MAGIC                  'L'
#define AMBIENT_SET_MODE           _IOW(AMBIENT_MAGIC, 1, char *)
#define AMBIENT_GET_MODE           _IOR(AMBIENT_MAGIC, 2, char *)
#define AMBIENT_SET_BRIGHTNESS    _IOW(AMBIENT_MAGIC, 3, int)
#define AMBIENT_GET_BRIGHTNESS    _IOR(AMBIENT_MAGIC, 4, int)

// SHM/네트워크/FIFO
#define SHM_KEY     0x8889
#define FIFO_PATH   "/tmp/qt_fifo"
#define TCP_PORT    60003
#define UDP_PORT    5006

// 기타 유틸
#define TCP_BUF_SIZE    512
#define WAV_HEADER_SIZE 44
#define BUFFER_SIZE     2048
