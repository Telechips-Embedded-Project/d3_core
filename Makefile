CFLAGS = -O2 -Wall -pthread -lcjson -lasound -Iinclude -lsqlite3

SRCS = src/main.c \
       src/shm_manager.c \
       src/device_ctrl.c \
       src/json_utils.c \
       src/fifo_thread.c \
       src/tts_utils.c \
       src/network_threads.c \
       src/sensor_ctrl.c \
       src/mydb.c

OBJS = src/main.o \
       src/shm_manager.o \
       src/device_ctrl.o \
       src/json_utils.o \
       src/fifo_thread.o \
       src/tts_utils.o \
       src/network_threads.o \
       src/sensor_ctrl.o \
       src/mydb.o

all: topst_core

topst_core: $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -f *.o topst_core src/*.o settings.db
