# WineDirectALSA

Wine/Proton용 초저지연 ASIO 드라이버: Windows ASIO를 Linux ALSA에 직접 연결합니다. 주로 리눅스 리듬 게이머들을 위해 만들어졌습니다.

## 설치 방법 (최종 사용자용)

### 종속성

#### 필수

1. 파이썬 3.x
2. umu-launcher
3. vdf 파이썬 패키지

아치 리눅스에서는 다음과 같이 설치합니다. (`multilib` 필수)

```shell
sudo pacman -S python python-vdf umu-launcher
```

#### 선택

1. RealtimeKit (`rtkit-daemon`) (권장)

### 설치 과정

> [!caution]
> 이 프로젝트는 와인/프로톤 11.0 이상만 지원합니다.

1. [Release](https://github.com/na1307/WineDirectALSA/releases/latest) 페이지에서 최신 tarball을 다운받습니다.
2. 다운받은 tarball의 압축을 풉니다.
3. `x86_64-{unix|windows}` 폴더를 다음 두곳 중 한곳으로 복사합니다.
   * `/opt/WineDirectALSA/` (root 권한 필요)
   * `$HOME/.local/lib/wine/`
4. 동봉된 `wda-install`을 실행합니다. 설치 스크립트의 지시를 따르세요.
5. 스팀의 게임 속성에서 실행 옵션을 다음으로 설정합니다.
   ```shell
   WINEDLLPATH=/opt/WineDirectALSA %command% # 혹은 WINEDLLPATH=~/.local/lib/wine/ %command%
   ```
6. 이제 게임을 실행하세요!

## 제한 사항

WineDirectALSA는 선택한 ALSA 하드웨어 장치에 직접 접근하며, 샘플 형식 변환이나 리샘플링을 수행하지 않습니다. 따라서 장치 자체에서 스테레오, 48 kHz, 32비트 부호 있는 리틀 엔디안 PCM 형식(`SND_PCM_FORMAT_S32_LE`)을 지원해야 합니다.

`S32_LE`는 샘플을 저장하는 컨테이너 형식을 뜻하며, 장치의 실제 변환 정밀도가 32비트라는 의미는 아닙니다. 테스트에 사용된 Realtek ALC897을 비롯한 많은 온보드 HDA 장치가 이 형식을 지원하지만, 일부 저가형 USB 오디오 장치는 16비트 또는 24비트 형식만 지원합니다.

### 장치 호환성 확인

재생 장치의 호환성을 확인하려면 다음 명령에서 `hw:0,0`을 사용할 ALSA 장치로 바꾸어 실행하십시오.

```shell
aplay \
  --device=hw:0,0 \
  --file-type=raw \
  --format=S32_LE \
  --channels=2 \
  --rate=48000 \
  --period-size=32 \
  --buffer-size=128 \
  --duration=5 \
  --dump-hw-params \
  /dev/zero
```

입력 기능을 사용하려면 캡처 장치도 같은 방법으로 확인하십시오.

```shell
arecord \
  --device=hw:0,0 \
  --file-type=raw \
  --format=S32_LE \
  --channels=2 \
  --rate=48000 \
  --period-size=32 \
  --buffer-size=128 \
  --duration=5 \
  --dump-hw-params \
  /dev/null
```

두 명령이 오류 없이 완료되면 해당 장치는 WineDirectALSA가 요구하는 주요 하드웨어 조건을 지원합니다.

## Benchmark

Test system: 48 kHz, 64-frame ASIO buffer, RTL Utility 0.5.2, Realtek ALC897

| Driver         |           Reported RTL |           Measured RTL |             Difference |
| -------------- | ---------------------: | ---------------------: | ---------------------: |
| WineDirectALSA | 160 samples / 3.333 ms | 174 samples / 3.625 ms |  14 samples / 0.292 ms |
| PipeASIO       | 128 samples / 2.667 ms | 290 samples / 6.033 ms | 162 samples / 3.367 ms |


10 consecutive measurements:

* WineDirectALSA: 174 / 174 / 174 / 174 / 174 / 174 / 174 / 174 / 174 / 174
* PipeASIO: 286 / 285 / 289 / 290 / 289 / 292 / 292 / 292 / 291 / 290

## 빌드 방법

### 종속성

#### 필수

1. CMake
2. Ninja
3. [xwin](https://github.com/Jake-Shadle/xwin)
4. Wine 11.0 이상

### 빌드 과정

1. 먼저 xwin을 이용하여 윈도우 SDK를 다운받습니다.
   ```shell
   xwin --arch x86_64 --accept-license splat --include-debug-libs --preserve-ms-arch-notation --output <경로>
   ```
2. CMake를 초기화합니다.
   ```shell
   cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DXWIN_SPLAT_PATH=<경로>
   ```
3. 빌드합니다.
   ```shell
   cmake --build build
   ```

## License
WineDirectALSA 자체는 GPL-3.0-only 라이선스입니다. 이 프로젝트가 사용하는 종속성은 다를 수 있습니다.

