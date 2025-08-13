// recv_mbox.c - Telechips multi-mailbox receiver (user space)
// 빌드: aarch64-telechips-linux-gcc -O2 -o recv_mbox recv_mbox.c
// 사용: ./recv_mbox [/dev/cb_mbox_test] [-n COUNT]

#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>

#ifndef MBOX_CMD_FIFO_SIZE
#define MBOX_CMD_FIFO_SIZE 6
#endif
#ifndef MBOX_DATA_FIFO_SIZE
#define MBOX_DATA_FIFO_SIZE 64
#endif

struct tcc_mbox_data {
    uint32_t cmd[MBOX_CMD_FIFO_SIZE]; // 6 * 4 = 24
    uint32_t data_len;                // words
    uint32_t data[MBOX_DATA_FIFO_SIZE];
};

static void print_timestamp(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm;
    localtime_r(&ts.tv_sec, &tm);
    char buf[64];
    strftime(buf, sizeof(buf), "%F %T", &tm);
    printf("[%s.%03ld] ", buf, ts.tv_nsec/1000000);
}

static void print_msg(const struct tcc_mbox_data *m, ssize_t msg_bytes) {
    size_t hdr_sz = sizeof(m->cmd) + sizeof(m->data_len);
    size_t payload_bytes = (msg_bytes > (ssize_t)hdr_sz) ? (msg_bytes - hdr_sz) : 0;
    unsigned payload_words = (payload_bytes / 4);

    print_timestamp();
    printf("RX: cmd[0..5]=[");

    for (int i = 0; i < MBOX_CMD_FIFO_SIZE; i++) {
        printf("0x%08X", m->cmd[i]);
        if (i != MBOX_CMD_FIFO_SIZE-1) printf(", ");
    }
    printf("], data_len=%u", m->data_len);

    if (payload_words) {
        printf(", data[0..%u):", payload_words);
        for (unsigned i = 0; i < payload_words; i++) {
            if ((i % 8) == 0) printf("\n    ");
            printf(" 0x%08X", m->data[i]);
        }
    }
// print_msg() 안, 마지막 printf("\n") 직전에 추가
if (m->cmd[0] == 0x47505331 /* "GPS1" */) {
    int32_t lat_i = (int32_t)m->cmd[2];
    int32_t lon_i = (int32_t)m->cmd[3];
    printf("  [GPS/HDR] fix=%u lat=%.7f lon=%.7f",
           m->cmd[1], lat_i/1e7, lon_i/1e7);
}
    printf("\n");
}

int main(int argc, char **argv) {
    const char *dev = "/dev/cb_mbox_test";
    int count = -1; // 무한
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-n") && i+1 < argc) {
            count = atoi(argv[++i]);
        } else if (argv[i][0] == '/') {
            dev = argv[i];
        } else {
            fprintf(stderr, "사용: %s [/dev/xxx] [-n COUNT]\n", argv[0]);
            return 1;
        }
    }

    int fd = open(dev, O_RDONLY);
    if (fd < 0) { perror("open"); return 2; }

    struct tcc_mbox_data msg;
    size_t max_len = sizeof(msg);
    size_t hdr_sz = sizeof(msg.cmd) + sizeof(msg.data_len);

    while (count != 0) {
        ssize_t r = read(fd, &msg, max_len);
        if (r < 0) {
            if (errno == EINTR) continue;
            perror("read");
            break;
        }
        if (r < (ssize_t)hdr_sz) {
            fprintf(stderr, "short read: %zd\n", r);
            continue;
        }
        // 유효성(드라이버가 msg_size만 돌려주므로 아래 체크는 참고용)
        size_t payload_bytes = (size_t)r - hdr_sz;
        unsigned payload_words = payload_bytes / 4;
        if (payload_words != msg.data_len) {
            fprintf(stderr, "warn: payload words mismatch (read=%u, header=%u)\n",
                    payload_words, msg.data_len);
        }
        print_msg(&msg, r);
        if (count > 0) count--;
    }

    close(fd);
    return 0;
}
