# WineDirectALSA

An ultra-low-latency ASIO driver for Wine/Proton that connects Windows ASIO directly to Linux ALSA. It is mainly designed for Linux rhythm game players.

## Installation (for end users)

### Dependencies

#### Required

1. Python 3.x
2. umu-launcher
3. The `vdf` Python package

On Arch Linux, install them as follows. (`multilib` is required.)

```shell
sudo pacman -S python python-vdf umu-launcher
```

#### Optional

1. RealtimeKit (`rtkit-daemon`) (Recommended)

### Installation steps

> [!caution]
> This project only supports Wine/Proton 11.0 or later.

1. Download the latest tarball from the [Releases](https://github.com/na1307/WineDirectALSA/releases/latest) page.
2. Extract the downloaded tarball.
3. Copy the `x86_64-{unix|windows}` directories to one of the following locations:
   * `/opt/WineDirectALSA/` (requires root privileges)
   * `$HOME/.local/lib/wine/`
4. Run the included `wda-install` script and follow its instructions.
5. In the game's Steam properties, set the launch options as follows:
   ```shell
   WINEDLLPATH=/opt/WineDirectALSA %command% # or WINEDLLPATH=~/.local/lib/wine/ %command%
   ```
6. Launch the game!

## Limitations

WineDirectALSA accesses the selected ALSA `hw:` device directly and does not perform sample-format conversion or resampling. The device must therefore natively support stereo, 48 kHz, signed 32-bit little-endian PCM (`SND_PCM_FORMAT_S32_LE`).

`S32_LE` describes the sample container format and does not necessarily mean that the device provides true 32-bit conversion resolution. Many onboard HDA devices support this format—the Realtek ALC897 used for testing does—but some low-cost USB audio devices only support 16-bit or 24-bit formats.

To test playback compatibility, replace `hw:0,0` with your ALSA device:

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

If capture is required, test the input device as well:

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

If both commands are completed without errors, the device supports the key hardware requirements of WineDirectALSA.

## Benchmark

Test system: 48 kHz, 64-frame ASIO buffer, RTL Utility 0.5.2, Realtek ALC897

| Driver         |           Reported RTL |           Measured RTL |             Difference |
| -------------- | ---------------------: | ---------------------: | ---------------------: |
| WineDirectALSA | 160 samples / 3.333 ms | 174 samples / 3.625 ms |  14 samples / 0.292 ms |
| PipeASIO       | 128 samples / 2.667 ms | 290 samples / 6.033 ms | 162 samples / 3.367 ms |

10 consecutive measurements:

* WineDirectALSA: 174 / 174 / 174 / 174 / 174 / 174 / 174 / 174 / 174 / 174
* PipeASIO: 286 / 285 / 289 / 290 / 289 / 292 / 292 / 292 / 291 / 290

## Building

### Dependencies

#### Required

1. CMake
2. Ninja
3. [xwin](https://github.com/Jake-Shadle/xwin)
4. Wine 11.0 or later

### Build steps

1. First, use xwin to download the Windows SDK:
   ```shell
   xwin --arch x86_64 --accept-license splat --include-debug-libs --preserve-ms-arch-notation --output <path>
   ```
2. Configure the project with CMake:
   ```shell
   cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DXWIN_SPLAT_PATH=<path>
   ```
3. Build the project:
   ```shell
   cmake --build build
   ```

## License

WineDirectALSA itself is licensed under GPL-3.0-only. Dependencies used by this project may be licensed differently.
