# Snowveil ❄️

掌心里的雪夜 — **M5StickS3** 版。
纯黑屏上，上千颗雪花在一个以你视线为中心的球体里飘落。
转动设备，任意方向看都有雪；摇一摇，惊起一场持续 10 秒的大风雪；
静置片刻，镜头会悄悄回正，让雪重新垂直落下。

[English · 英文文档](README.md)

![Snowveil on M5StickS3](releases/cover-1280x720-snowveil-sticks3.png)

## 你会得到什么

- **球形雪花场** — 1500 粒雪花均匀分布在以相机为中心的球体内，
  任何方向看都是雪（没有空角，没有"穿帮墙"）。
- **360° 自由视角** — 板载 IMU 通过纯四元数陀螺积分驱动视角：
  没有卡点，没有橡皮筋回弹。
- **深度 × 个性** — 每颗雪花有自己的亮度；深度衰减叠在上面，画面有层次。
- **摇一摇大风雪** — 摇一下触发 10 秒风暴：落得更快、风更深，然后慢慢平息。
- **自动回正** — 静止 2 秒后镜头缓慢归位到"雪垂直下落"（约 7 秒，无感）；一动即停。
- **双按钮流程** — A 键锁/解锁视角，B 键切 7 级屏幕亮度。没有菜单，无需配置。

## 操作

| 输入 | 功能 |
|------|------|
| 转动 / 倾斜设备 | 360° 自由视角 |
| **BtnA** | 锁定 / 解锁视角（右下角淡色 `LOCK` 提示） |
| **BtnB** | 屏幕亮度，7 级 |
| 摇晃 | 触发 10 秒大风雪 |

## 安装

### 最简单 — M5Burner

从 [Releases](https://github.com/andjiang0083/snowveil/releases) 页面下载
**merged 全镜像**，用 [M5Burner](https://shop.m5stack.com/) 或
[网页版烧录器](https://burner.m5stack.com/) 烧录（设备选 **M5StickS3**）。
逐步说明见 [releases/README_M5Burner.md](releases/README_M5Burner.md)。

### esptool

```bash
esptool.py --port /dev/cu.usbmodemXXXX write_flash 0x0 snowveil-vX.Y-merged.bin
```

merged 全镜像从偏移 **0x0** 写入（bootloader + 分区表 + 应用，一体）。

## 开发

```bash
# 需要 PlatformIO：pip install platformio
pio run -e sticks3 -t upload --upload-port /dev/cu.usbmodemXXXX
```

单文件固件：[`src/main.cpp`](src/main.cpp)（约 470 行 — 粒子、四元数姿态、
渲染、开机画面，全部集中在一处是刻意为之）。`platformio.ini` 里依赖钉到精确版本，
保证全新克隆构建出同样的固件。构建细节、发布产物布局与排障：[BUILDING.md](BUILDING.md)。

## 协议

MIT — 见 [LICENSE](LICENSE)。
