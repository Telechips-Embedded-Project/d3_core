# Makefile for mailbox_driver (out-of-tree)
# 사용법:
# 1) Yocto SDK 환경 로드:
#    $ source /path/to/environment-setup-aarch64-telechips-linux
# 2) 빌드:
#    $ make
#    (또는 make -j$(nproc))
# 3) 정리:
#    $ make clean
#
# 필요 시 직접 지정:
#    $ make KDIR=$SDKTARGETSYSROOT/usr/src/kernel

obj-m += mailbox_driver.o

# KDIR: 커널 빌드 트리
#  - SDK 환경이면: $SDKTARGETSYSROOT/usr/src/kernel
#  - KERNEL_SRC가 있으면 그걸 사용
#  - 둘 다 없으면 호스트 커널(/lib/modules/`uname -r`/build)로 폴백
KDIR ?= $(if $(KERNEL_SRC),$(KERNEL_SRC),$(if $(SDKTARGETSYSROOT),$(SDKTARGETSYSROOT)/usr/src/kernel,/lib/modules/$(shell uname -r)/build))
PWD  := $(shell pwd)

# 선택: 경고 표시 강화
ccflags-y += -Wall

.PHONY: all modules clean install

all: modules

modules:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

# 루트fs에 설치하고 싶으면:
#   make install INSTALL_MOD_PATH=/path/to/rootfs
install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install
