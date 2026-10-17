# Building & Flashing

[中文说明见下方](#构建与烧录-中文)

## Prerequisites

- [PlatformIO Core](https://platformio.org/installation) (`pip install platformio`)
- M5StickS3 connected over USB
- Dependencies are **pinned to exact versions** in `platformio.ini`
  (M5Unified 0.2.25 + M5GFX 0.2.32) so a fresh clone builds the same firmware.

## Build & flash (development)

```bash
pio run -e sticks3 -t upload --upload-port /dev/cu.usbmodemXXXX
```

PlatformIO writes the four sections at their own offsets
(bootloader / partitions / boot_app0 / app) — this is the normal dev workflow.

## Release asset: the merged image

M5Burner and `esptool write_flash 0x0` expect **one file containing every section**
(bootloader @ 0x0, partition table @ 0x8000, boot_app0 @ 0xe000, app @ 0x10000).
Build it from the same `.pio/build/sticks3/` output:

```bash
pio run -e sticks3        # first, so .pio/build/sticks3/*.bin exist

python3 -m esptool --chip esp32s3 merge-bin \
  -o releases/snowveil-v$(cat VERSION)-merged.bin \
  --flash-mode dio --flash-freq 80m --flash-size 8MB \
  0x00000 .pio/build/sticks3/bootloader.bin \
  0x08000 .pio/build/sticks3/partitions.bin \
  0xe0000 ~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin \
  0x10000 .pio/build/sticks3/firmware.bin
```

> ⚠️ Two traps that cost real time:
> - `flash_mode` must be **dio** everywhere (M5StickS3 hardware requirement); a QIO
>   bootloader boots to a black screen on the device.
> - Do **not** upload the app-only `firmware.bin` to M5Burner — it lands at 0x0 and
>   overwrites the bootloader.

Verify a merged image (header + section layout):

```bash
xxd -l 16 releases/snowveil-v*-merged.bin
# expect: e9 03 02 3f ...   (magic e9, 3 sections, DIO, 80 MHz + 8 MB)
```

## Troubleshooting

| Symptom | Fix |
|---|---|
| Black screen after M5Burner flash | You flashed the app-only bin; use the `-merged.bin` |
| `Failed to connect: No serial data` | Hold **BtnA**, tap **RST** on the back, keep holding BtnA through the flash |
| Wrong viewpoint at boot | Keep the device still for the first ~3 s (gyro bias calibration runs during the splash screen) |

---

## 构建与烧录（中文）

### 前置

- PlatformIO Core（`pip install platformio`）
- M5StickS3 接 USB
- 依赖已钉到精确版本（M5Unified 0.2.25 + M5GFX 0.2.32），全新克隆可复现同一固件

### 开发刷机

```bash
pio run -e sticks3 -t upload --upload-port /dev/cu.usbmodemXXXX
```

### 发布用 merged 全镜像

M5Burner / `esptool write_flash 0x0` 需要**一个文件包含所有段**
（bootloader @ 0x0、分区表 @ 0x8000、boot_app0 @ 0xe000、应用 @ 0x10000）：

```bash
pio run -e sticks3
python3 -m esptool --chip esp32s3 merge-bin \
  -o releases/snowveil-v$(cat VERSION)-merged.bin \
  --flash-mode dio --flash-freq 80m --flash-size 8MB \
  0x00000 .pio/build/sticks3/bootloader.bin \
  0x08000 .pio/build/sticks3/partitions.bin \
  0xe0000 ~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin \
  0x10000 .pio/build/sticks3/firmware.bin
```

> ⚠️ 两个坑：`flash_mode` 必须全程 **dio**（QIO bootloader 上真机 = 黑屏）；
> M5Burner 只能刷 `-merged.bin`，刷 app-only 的 `firmware.bin` 会覆盖 bootloader。

### 排障

| 现象 | 处理 |
|---|---|
| M5Burner 刷后黑屏 | 刷的是 app-only bin，换 `-merged.bin` |
| `No serial data received` | 按住 **BtnA** → 按一下背面 **RST** → 保持按住完成刷机 |
| 开机视角不对 | 开机前 ~3 秒保持设备静止（开机画面期间校准陀螺仪零偏） |
