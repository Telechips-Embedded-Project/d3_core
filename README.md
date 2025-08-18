# topst_device_driver

임베디드 리눅스(텔레칩스 TOPST D3 계열)에서 차량 내 장치 (엠비언트, 와이퍼, 창문, 에어컨, 헤드램프)을 제어하기 위한 **커널 드라이버 + 유저 공간 데몬/도구** 모음입니다.<br />
커널은 `/dev/*` 장치를 제공하고, 유저 공간 데몬이 ioctl로 상태를 읽어 실제 PWM/SPI 등 하드웨어 신호를 구동합니다.<br />
기본 테스트용 setter 유틸도 포함되어 있습니다.

---

## 구성 (Repository Layout)

```
topst_device_driver/
├─ driver/         # .ko 파일 (보드 scp용)
|    └─ code/      # 커널 모듈 소스, Makefile, Kconfig
└─ user/           # 유저 공간 데몬 실행파일 (보드 scp용)
     └─ code/      # 유저 공간 데몬 코드, 테스트용 setter
```

- `driver/` → 보드의 커널 모듈 디렉터리(`/lib/modules/$(uname -r)/extra/`)에 배치<br />
- `user/` → `*_daemon`, `*_setter` 실행 파일 포함

---

## 지원 기능 (모듈별 개요)

- **ambient_driver** : SPI/WS281x 등 엠비언트 라이트 제어 (색상/밝기, rainbow 등)<br />
- **wiper_driver** : 와이퍼 모드 제어 (slow/fast), 유저 데몬이 반복 각도/PWM 제어<br />
- **window_driver** : 창문 구동 (up/down/stop)<br />
- **aircon_driver** : 팬 레벨/부스트, 유저 데몬이 주기적 PWM 반영<br />
- **headlamp_driver** : 전조등 on/off/레벨<br />

---

## 빌드

### 커널 모듈 (.ko)
```bash
cd driver
make
```
→ 산출물(.ko)을 대상 보드로 복사 후 `/lib/modules/$(uname -r)/extra/`에 배치

### 유저 공간 도구
```bash
cd user
$CC -O2 -o [실행파일명 - aircon_daemon] [.c 파일명 - aircon_daemon.c]
```
→ `ambient_daemon`, `aircon_daemon`, `wiper_daemon`, 각 `*_setter` 생성

---

## 설치 (Install)

1) 커널 모듈 복사
```bash
KV=$(uname -r)
sudo mkdir -p /lib/modules/$KV/extra
sudo cp driver/*.ko /lib/modules/$KV/extra/
sudo depmod -a
```

2) 부팅 시 자동 로드
```bash
sudo tee /etc/modules-load.d/topst.conf >/dev/null <<'EOF'
ambient_driver
wiper_driver
window_driver
aircon_driver
headlamp_driver
EOF
```

수동 로드 예시:
```bash
sudo modprobe ambient_driver
```

---

## 실행 (런타임)

### 데몬 실행
```bash
./user/aircon_daemon   &
./user/ambient_daemon  &
./user/wiper_daemon    &
```

### 수동 제어 (테스트)
```bash
# 에어컨 레벨
./user/aircon_setter low
./user/aircon_setter mid
./user/aircon_setter high

# 엠비언트 색상
./user/ambient_setter color red
./user/ambient_setter color green
./user/ambient_setter color rainbow

# 엠비언트 밝기
./user/ambient_setter brightness 0
./user/ambient_setter brightness 100

# 와이퍼
./user/wiper_setter slow
./user/wiper_setter fast
```
---

## 장치 인터페이스 요약

- 각 드라이버는 `/dev/ambient_dev`, `/dev/aircon_dev` 등 character device 제공<br />
- ioctl() 기반 SET/GET 명령 지원<br />
- 지속 효과(Rainbow, 부스트 타이밍 등)는 데몬 루프에서 구현<br />
- 와이퍼는 PWM 왕복 제어를 위해 데몬이 루프 유지

---

---
