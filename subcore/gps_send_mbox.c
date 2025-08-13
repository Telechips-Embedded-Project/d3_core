// gps_send_mbox.c (HEADER-ONLY 전송 버전)
// UART(/dev/ttyAMA2)에서 NMEA 읽어 위경도를 /dev/cb_mbox_test로 송신
// 빌드: gcc -O2 -Wall -o gps_mbox gps_send_mbox.c

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <ctype.h>

#define GPS_UART_DEV   "/dev/ttyAMA2"
#define GPS_BAUDRATE   B9600
#define MBOX_DEV       "/dev/cb_mbox_test"

#undef  MBOX_CMD_FIFO_SIZE
#define MBOX_CMD_FIFO_SIZE 6     // 커널과 동일
#undef  MBOX_DATA_FIFO_SIZE
#define MBOX_DATA_FIFO_SIZE 64

struct tcc_mbox_data {
    uint32_t cmd[MBOX_CMD_FIFO_SIZE]; // 6 * 4 = 24
    uint32_t data_len;                // words
    uint32_t data[MBOX_DATA_FIFO_SIZE];
};

static volatile int running = 1;
static void on_signal(int s){ (void)s; running = 0; }

// double → int32 (deg * 1e7)
static int32_t to_i1e7(double d){
    long long v = (long long)(d * 10000000.0);
    if (v >  2147483647LL) v =  2147483647LL;
    if (v < -2147483648LL) v = -2147483648LL;
    return (int32_t)v;
}

static int uart_open_config(const char *dev){
    int fd = open(dev, O_RDWR | O_NOCTTY);
    if (fd < 0){ perror("open uart"); return -1; }
    struct termios tio;
    if (tcgetattr(fd, &tio) < 0){ perror("tcgetattr"); close(fd); return -1; }
    cfmakeraw(&tio);
    cfsetispeed(&tio, GPS_BAUDRATE);
    cfsetospeed(&tio, GPS_BAUDRATE);
    tio.c_cflag |= (CLOCAL | CREAD);
    tio.c_cflag &= ~CRTSCTS; tio.c_cflag &= ~CSTOPB; tio.c_cflag &= ~PARENB;
    tio.c_cflag &= ~CSIZE;   tio.c_cflag |= CS8;
    tio.c_iflag &= ~(IXON | IXOFF | IXANY);
    tio.c_cc[VMIN] = 1; tio.c_cc[VTIME] = 0;
    tcflush(fd, TCIFLUSH);
    if (tcsetattr(fd, TCSANOW, &tio) != 0){ perror("tcsetattr"); close(fd); return -1; }
    return fd;
}

static int mbox_open(const char *dev){
    int fd = open(dev, O_WRONLY);
    if (fd < 0){ perror("open mbox"); return -1; }
    return fd;
}

static char* strtrim(char *s){
    char *e;
    while (*s && isspace((unsigned char)*s)) s++;
    if (!*s) return s;
    e = s + strlen(s) - 1;
    while (e > s && isspace((unsigned char)*e)) *e-- = '\0';
    return s;
}

// NMEA ddmm.mmmm / dddmm.mmmm → decimal degrees
static int nmea_degmin_to_deg(const char *dm, char hemi, double *out){
    if (!dm || !*dm) return -1;
    const char *dot = strchr(dm, '.');
    int len = dot ? (int)(dot - dm) : (int)strlen(dm);
    if (len < 3) return -1;
    int deg_digits = (len > 4) ? (len - 2) : 2; // 위도 2자리, 경도 3자리 자동 판별
    if (deg_digits < 2) deg_digits = 2;

    char dgbuf[8] = {0};
    if (deg_digits >= (int)sizeof(dgbuf)) return -1;
    memcpy(dgbuf, dm, deg_digits);
    int deg = atoi(dgbuf);
    double min = atof(dm + deg_digits);

    double dec = deg + (min / 60.0);
    if (hemi == 'S' || hemi == 'W') dec = -dec;
    *out = dec;
    return 0;
}

// RMC: $G?RMC,hhmmss.sss, A/V, lat, NS, lon, EW, ...  반환: 1=유효, 0=무효, -1=형식아님
static int parse_rmc(const char *line, double *plat, double *plon){
    if (!(strncmp(line,"$GPRMC",6)==0 || strncmp(line,"$GNRMC",6)==0)) return -1;
    char buf[256]; strncpy(buf,line,sizeof(buf)-1); buf[sizeof(buf)-1]='\0';
    char *tok[16]={0}, *p=buf; int n=0;
    while (n<16 && (tok[n]=strsep(&p,","))!=NULL) n++;
    if (n < 7) return -1;

    const char *status = tok[2];
    const char *lat = tok[3], *ns = tok[4];
    const char *lon = tok[5], *ew = tok[6];
    if (!status) return 0;
    if (*status != 'A') return 0; // 무효면 0

    if (!lat || !*lat || !ns || !lon || !*lon || !ew) return 0;
    double dlat=0, dlon=0;
    if (nmea_degmin_to_deg(lat, ns[0], &dlat)!=0) return 0;
    if (nmea_degmin_to_deg(lon, ew[0], &dlon)!=0) return 0;
    *plat=dlat; *plon=dlon;
    return 1;
}

int main(void){
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    setvbuf(stdout, NULL, _IONBF, 0);

    int ufd = uart_open_config(GPS_UART_DEV);
    if (ufd < 0) return 1;
    int mfd = mbox_open(MBOX_DEV);
    if (mfd < 0){ close(ufd); return 2; }

    printf("[GPS→MBOX] UART=%s, MBOX=%s\n", GPS_UART_DEV, MBOX_DEV);

    char rbuf[256], line[256]; int lp=0;

    while (running){
        ssize_t n = read(ufd, rbuf, sizeof(rbuf));
        if (n > 0){
            for (ssize_t i=0;i<n;i++){
                char c = rbuf[i];
                if (c=='\r') continue;
                if (c=='\n'){
                    line[lp]='\0';
                    char *ln = strtrim(line);

                    double lat=0.0, lon=0.0;
                    int r = parse_rmc(ln, &lat, &lon); // r==1: 유효 fix
                    if (r >= 0){
                        struct tcc_mbox_data m;
                        memset(&m, 0, sizeof(m));

                        // ★ 헤더-only 28B 포맷
                        m.cmd[0] = 0x47505331;                        // "GPS1"
                        m.cmd[1] = (uint32_t)((r==1) ? 1 : 0);         // fix flag
                        m.cmd[2] = (uint32_t)to_i1e7((r==1)?lat:0.0);  // lat * 1e7
                        m.cmd[3] = (uint32_t)to_i1e7((r==1)?lon:0.0);  // lon * 1e7
                        m.data_len = 0;                                // payload 없음

                        size_t bytes = sizeof(m.cmd) + sizeof(m.data_len); // 28
                        printf("[DBG] hdr-only write: cmd0=0x%08X fix=%u lat_i=%d lon_i=%d bytes=%zu\n",
                               m.cmd[0], m.cmd[1], (int32_t)m.cmd[2], (int32_t)m.cmd[3], bytes);

                        ssize_t wr = write(mfd, &m, bytes);
                        if (wr < 0) perror("write mbox");
                        else printf("[MBOX] write=%zd (expect 28) %s\n",
                                    wr, (r==1) ? "fix=1" : "fix=0");
                    }
                    lp=0;
                }else{
                    if (lp < (int)sizeof(line)-1) line[lp++]=c;
                    else lp=0; // overflow 보호
                }
            }
        }else if (n < 0 && errno != EINTR){
            perror("read uart");
            usleep(10000);
        }
    }

    close(mfd); close(ufd);
    printf("[GPS→MBOX] Stopped.\n");
    return 0;
}
