# WineDirectALSA

Wine/Proton용 초저지연 ASIO 드라이버: Windows ASIO를 Linux ALSA에 직접 연결합니다. 주로 리눅스 리듬 게이머들을 위해 만들어졌습니다.

## 설치 방법 (최종 사용자용)

### 종속성

1. 파이썬 3.x
2. umu-launcher
3. vdf 파이썬 패키지

아치 리눅스에서는 다음과 같이 설치합니다. (`multilib` 필수)

```shell
sudo pacman -S python python-vdf umu-launcher
```

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

