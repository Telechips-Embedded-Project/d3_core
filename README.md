# D3-P Core (GPS Branch)

이 브랜치는 **GPS 기능**에 관련된 내용을 다룹니다.
TOPST D3-P 보드에서 UART5를 통해 GPS 모듈 데이터를 수신하고, 파싱된 정보를 mbox를 통해 수신하여 다른 프로세스(클러스터, 네비게이션 등)에서 활용할 수 있도록 합니다.
<img width="964" height="601" alt="image" src="https://github.com/user-attachments/assets/d74df5f0-63d9-468a-a65c-3f8ad0935632" />

---

## 주요 기능
- **UART(ttyAMA5) 기반 GPS 데이터 수신**
  - NMEA sentence (`$GPGGA`, `$GPRMC` 등) 읽기
- **GPS 데이터 파싱**
  - 위도/경도 (Latitude, Longitude)
  - UTC 시간
  - 속도 (Speed over ground)
  - Fix 상태 및 위성 개수
- **시스템 연동**
  - 파싱된 데이터 중, 필요한 정보를 mbox를 통해 송신
  - 클러스터/네비게이션 프로세스에서 활용 가능

---


## 사용법
```
1. mbox 부분<br />
   mailbox_driver.ko 올리기 <br />
   서브코어는 subcore 폴더, 메인코어는 maincore 폴더에 있는 .ko 올리기
   <br /><br />
  
2. uart 부분<br />
   메인코어, 서브코어 dts 수정하기<br />
   https://www.notion.so/uart-24a459e1ac2b80fc8b86e18502192b55
```
