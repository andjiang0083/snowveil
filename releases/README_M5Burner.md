# Flashing Snowveil with M5Burner / 用 M5Burner 刷入 Snowveil

## English

1. Download `snowveil-vX.Y-merged.bin` from the [GitHub releases](https://github.com/andjiang0083/snowveil/releases)
   (or pick the **Snowveil** entry in the M5Burner store).
2. Open [M5Burner](https://shop.m5stack.com/en/burner/) (desktop app) or the
   [web burner](https://burner.m5stack.com/).
3. Connect the M5StickS3 via USB, pick device **M5StickS3**, then **Burn**.
4. When the screen shows the `SNOWVEIL` splash, **keep the device still for ~3 s**
   — the gyroscope is calibrating. Snow starts falling after that.

> Use the file whose name ends in **`-merged.bin`**. The plain `firmware.bin`
> (app-only) does **not** boot when written at 0x0.

## 中文

1. 从 [GitHub Releases](https://github.com/andjiang0083/snowveil/releases) 下载
   `snowveil-vX.Y-merged.bin`（或在 M5Burner 商店直接选 **Snowveil** 条目）。
2. 打开 [M5Burner](https://shop.m5stack.com/en/burner/)（桌面版）或
   [网页版烧录器](https://burner.m5stack.com/)。
3. M5StickS3 接 USB，设备选 **M5StickS3**，点 **烧录 / Burn**。
4. 屏幕出现 `SNOWVEIL` 开机画面后，**保持设备静止约 3 秒**（陀螺仪在校准），
   随后雪花开始飘落。

> 务必使用文件名以 **`-merged.bin`** 结尾的全镜像；
> 普通 `firmware.bin`（只有应用）写到 0x0 无法启动。

## 操作 / Controls

| 输入 | 功能 |
|---|---|
| 转动设备 | 360° 自由视角 |
| A 键 | 锁定 / 解锁视角 |
| B 键 | 亮度 7 级 |
| 摇一摇 | 10 秒大风雪 |
