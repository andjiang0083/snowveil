// ────────────────────────────────────────────────────────────
// Snowveil v0.0.3a — M5StickS3
// 雪夜 · romantic snow night
// ────────────────────────────────────────────────────────────

#include <hal/gpio_types.h>
#include <M5Unified.h>
#include <cmath>
#include <cstdlib>

// ═══════════════════════════════════════════════════════════
//  工具函数
// ═══════════════════════════════════════════════════════════

static inline uint16_t c565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r>>3)<<11) | ((g>>2)<<5) | (b>>3);
}
static inline float frnd() { return (float)rand()/(float)RAND_MAX; }
static inline float clampf(float v, float lo, float hi) {
    if (v < lo) return lo; if (v > hi) return hi; return v;
}
// ST7789 面板 sprite 内存为字节交换存储，直接写 buffer 时需翻转
static inline uint16_t swap16(uint16_t v) { return (v << 8) | (v >> 8); }

// ═══════════════════════════════════════════════════════════
//  常量
// ═══════════════════════════════════════════════════════════

static const int W = 240, H = 135;
static const float FOCAL = 65.0f;
static const int N_SNOW = 1500;

// 球形雪花场：相机在球心，任何方向看都有雪（360° 无穿帮）
static const float R_SNOW = 400.0f;

// ═══════════════════════════════════════════════════════════
//  雪花粒子
// ═══════════════════════════════════════════════════════════

struct Flake {
    float wx, wy, wz;
    float speed;
    float swayPhase;
    uint8_t size;    // 1~4
    float bright;    // 个性亮度 0.55~1.0 (每颗雪有自己的明暗)
};

static Flake g_snow[N_SNOW];

// 球内均匀分布（上半球：从上方出现）
static void spawnFlake(Flake& f, bool upperOnly) {
    float r = R_SNOW * powf(frnd(), 0.333f);
    float theta = frnd() * M_PI * 2;
    float cosp;
    if (upperOnly) {
        cosp = frnd();                              // phi ∈ [0, π/2]
    } else {
        cosp = 2.0f * frnd() - 1.0f;                // 全球
    }
    float sinp = sqrtf(1.0f - cosp * cosp);
    f.wx = r * sinp * cosf(theta);
    f.wz = r * sinp * sinf(theta);
    f.wy = r * cosp;
    f.speed = 0.3f + frnd() * 1.2f;
    f.swayPhase = frnd() * M_PI * 2;
    f.size = 1 + rand() % 4;
    f.bright = 0.55f + frnd() * 0.45f;
}

static void initSnow() {
    for (int i = 0; i < N_SNOW; i++) {
        spawnFlake(g_snow[i], false);
    }
}

// ═══════════════════════════════════════════════════════════
//  亮度查色表 (预计算 256 级雪花颜色，替代每帧浮点计算)
// ═══════════════════════════════════════════════════════════

static uint16_t g_snowLUT[256];

static void initSnowLUT() {
    for (int i = 0; i < 256; i++) {
        float bright = (float)i / 255.0f;
        // 白带淡蓝 (与 v0.0.1b 完全一致的计算)
        float r = 240.0f * bright * 0.003f * 255.0f;
        float g = 248.0f * bright * 0.003f * 255.0f;
        float b = 255.0f * bright * 0.003f * 255.0f;
        if (r < 20) r = bright * 60.0f;
        if (g < 20) g = bright * 65.0f;
        if (b < 20) b = bright * 80.0f;
        g_snowLUT[i] = c565((uint8_t)clampf(r,0,255),
                            (uint8_t)clampf(g,0,255),
                            (uint8_t)clampf(b,0,255));
    }
}

// ═══════════════════════════════════════════════════════════
//  全局
// ═══════════════════════════════════════════════════════════

static lgfx::LGFX_Sprite canvas(&M5.Display);
static float g_time = 0;
static uint32_t g_lastTick = 0;
static int g_brightIdx = 4;
static const uint8_t BRIGHT[] = {3,13,26,76,128,204,255};

// ── 风暴状态 ──
static const float STORM_SECONDS = 10.0f;
static float g_storm = 0;       // 风暴强度 0~1 (平滑过渡)
static float g_stormTimer = 0;  // 剩余风暴时间 (秒)

// ═══════════════════════════════════════════════════════════
//  姿态：纯陀螺仪四元数积分 (360° 自由，无卡点无回弹)
//  自动回正：静止 2s 后缓慢归位 (雪花朝下)
// ═══════════════════════════════════════════════════════════

static float g_qw = 1.0f, g_qx = 0, g_qy = 0, g_qz = 0;   // 设备姿态四元数
static float g_biasX = 0, g_biasY = 0, g_biasZ = 0;       // 陀螺仪零偏
static uint32_t g_lastIMU = 0;
static uint8_t g_shakeFrames = 0;
static float g_quiescentTime = 0;   // 静止累计时间 (秒)
static bool g_recentering = false;  // 正在自动回正
static bool g_viewLocked = false;   // 视角锁定 (BtnA)
// 回正目标姿态 (雪花垂直下落 = 世界下方向对齐屏幕下方)
static float g_tqw = 1.0f, g_tqx = 0, g_tqy = 0, g_tqz = 0;
static bool g_alignSet = false;     // 回正目标是否已计算

// 校准零偏：splash 期间静止采样 (只累计加速度≈1g 的静止样本)
static void calibrateGyroBias() {
    const int N = 120;  // 约 2.4 秒
    int count = 0;
    float sx = 0, sy = 0, sz = 0;
    for (int i = 0; i < N; i++) {
        M5.Imu.update();
        float ax, ay, az, gx, gy, gz;
        M5.Imu.getAccel(&ax, &ay, &az);
        M5.Imu.getGyro(&gx, &gy, &gz);
        float am = sqrtf(ax*ax + ay*ay + az*az);
        if (am > 0.95f && am < 1.05f) {  // 静止样本
            sx += gx; sy += gy; sz += gz;
            count++;
        }
        delay(20);
    }
    if (count > 0) {
        g_biasX = sx / count;
        g_biasY = sy / count;
        g_biasZ = sz / count;
    }
}

static void updateIMU() {
    M5.Imu.update();
    float gx, gy, gz;
    M5.Imu.getGyro(&gx, &gy, &gz);
    uint32_t now = millis();
    float dt = (now - g_lastIMU) / 1000.0f;
    if (dt <= 0) dt = 0.01f;
    if (dt > 0.05f) dt = 0.05f;
    g_lastIMU = now;

    // 去零偏 → 弧度/秒
    float wx = (gx - g_biasX) * DEG_TO_RAD;
    float wy = (gy - g_biasY) * DEG_TO_RAD;
    float wz = (gz - g_biasZ) * DEG_TO_RAD;

    // ── 视角未锁定时：四元数积分 + 自动回正 ──
    if (!g_viewLocked) {
        // 四元数积分: q' = q + 0.5·Ω⊗q·dt
        float h = 0.5f * dt;
        float nqw = g_qw - h * (g_qx*wx + g_qy*wy + g_qz*wz);
        float nqx = g_qx + h * (g_qw*wx + g_qy*wz - g_qz*wy);
        float nqy = g_qy + h * (g_qw*wy + g_qz*wx - g_qx*wz);
        float nqz = g_qz + h * (g_qw*wz + g_qx*wy - g_qy*wx);
        float norm = sqrtf(nqw*nqw + nqx*nqx + nqy*nqy + nqz*nqz);
        if (norm > 1e-6f) {
            norm = 1.0f / norm;
            g_qw = nqw*norm; g_qx = nqx*norm; g_qy = nqy*norm; g_qz = nqz*norm;
        }

        // 静止检测 → 自动回正
        float wmag = sqrtf(wx*wx + wy*wy + wz*wz);
        if (wmag < 0.06f) {           // 角速度 < ~3.4°/s 视为静止
            g_quiescentTime += dt;
            if (g_quiescentTime > 2.0f) g_recentering = true;
        } else {
            g_quiescentTime = 0;      // 动了一下，静止计时清零
        }
        // 取消回正：滞回——回正进行中需明显转动 (~14°/s) 才取消，
        // 手的自然微抖不再打断回正
        if (g_recentering && wmag > 0.25f) {
            g_recentering = false;
            g_alignSet = false;       // 下次回正重新计算目标
        }
    }

    // ── 摇晃检测 → 触发大风雪 (连续 2 帧 ±1.2g 以上) ──
    float ax, ay, az;
    M5.Imu.getAccel(&ax, &ay, &az);
    float am = sqrtf(ax*ax + ay*ay + az*az);
    if (am > 2.2f || am < 0.3f) {
        if (++g_shakeFrames >= 2) {
            g_stormTimer = STORM_SECONDS;
            g_shakeFrames = 0;
        }
    } else {
        g_shakeFrames = 0;
    }
}

// 自动回正：让"世界下方向"在屏幕上垂直向下 (雪花从顶部落到底部)
// 目标从当前四元数直接构造，不需要加速度计；插值最短弧，保留绕屏幕法线的朝向

// 计算回正目标姿态 (只算一次，固定目标)
static void computeAlignTarget() {
    // 世界 -y 在相机系 = R(q)ᵀ 第 2 列取负 = -(m01, m11, m21)
    float m01 = 2*(g_qx*g_qy - g_qw*g_qz);
    float m11 = 1 - 2*(g_qx*g_qx + g_qz*g_qz);
    float m21 = 2*(g_qy*g_qz + g_qw*g_qx);
    float vx = -m01, vy = -m11, vz = -m21;
    // 对齐旋转：绕相机系水平轴 (v × 屏幕下方向)，角度 = acos(v·(0,-1,0))
    float ax = vz, az = -vx;              // 轴在 xz 平面 (水平)
    float alen = sqrtf(ax*ax + az*az);
    float ang = acosf(clampf(m11, -1.0f, 1.0f));   // v·(0,-1,0) = -vy = m11
    float ha = ang * 0.5f;
    float aqw, aqx, aqy = 0, aqz;
    if (alen < 1e-4f) {                   // 退化：v 与屏幕下方向共线 (含倒置)
        aqw = cosf(ha); aqx = sinf(ha); aqz = 0;   // 绕 x 轴 (倒置时翻 180°)
    } else {
        ax /= alen; az /= alen;
        aqw = cosf(ha); aqx = ax * sinf(ha); aqz = az * sinf(ha);
    }
    // 目标姿态 = 对齐旋转 ⊗ 当前姿态 (对齐作用在相机系，右乘)
    float nw = aqw*g_qw - aqx*g_qx - aqz*g_qz;
    float nx = aqw*g_qx + aqx*g_qw + aqz*g_qy;
    float ny = aqw*g_qy - aqx*g_qz + aqz*g_qw;
    float nz = aqw*g_qz + aqx*g_qy + aqz*g_qw;
    g_tqw = nw; g_tqx = nx; g_tqy = ny; g_tqz = nz;
    g_alignSet = true;
}

static void updateRecentering() {
    if (!g_recentering) return;
    if (!g_alignSet) computeAlignTarget();   // 目标固定，只算一次

    // 每帧向目标插值 1% (约 7 秒完成)；最短弧：点积为负取 -目标
    float tw = g_tqw, tx = g_tqx, ty = g_tqy, tz = g_tqz;
    if (g_qw*tw + g_qx*tx + g_qy*ty + g_qz*tz < 0) {
        tw = -tw; tx = -tx; ty = -ty; tz = -tz;
    }
    float k = 0.01f;
    g_qw += (tw - g_qw) * k;
    g_qx += (tx - g_qx) * k;
    g_qy += (ty - g_qy) * k;
    g_qz += (tz - g_qz) * k;
    float norm = sqrtf(g_qw*g_qw + g_qx*g_qx + g_qy*g_qy + g_qz*g_qz);
    if (norm > 1e-6f) {
        norm = 1.0f / norm;
        g_qw *= norm; g_qx *= norm; g_qy *= norm; g_qz *= norm;
    }
    // 到位判定：接近目标即自然收尾 (剩余 <0.8° 无感)，不硬赋值避免跳变
    float dot = g_qw*g_tqw + g_qx*g_tqx + g_qy*g_tqy + g_qz*g_tqz;
    if (dot > 0.9999f) {
        g_recentering = false;
        g_alignSet = false;
        g_quiescentTime = 0;
    }
}

// ═══════════════════════════════════════════════════════════
//  物理更新
// ═══════════════════════════════════════════════════════════

static void updateSnow() {
    for (int i = 0; i < N_SNOW; i++) {
        Flake& f = g_snow[i];
        // 风暴时下落加速 (最多 +60%)
        f.wy -= f.speed * (1.0f + g_storm * 0.6f);
        // 深度耦合的横向风：离球心（相机）越近 → 风越强
        float rdist = sqrtf(f.wx*f.wx + f.wy*f.wy + f.wz*f.wz);
        float windFactor = clampf(1.3f - rdist / 250.0f, 0.15f, 1.6f);
        f.wx += sinf(f.swayPhase + g_time * 4.0f) * 2.5f * 0.15f
              + g_storm * windFactor * 1.6f;
        // 落出球面 → 重生到上半球
        if (rdist > R_SNOW) {
            spawnFlake(f, true);
        }
    }
}

// ═══════════════════════════════════════════════════════════
//  渲染
// ═══════════════════════════════════════════════════════════

static void render() {
    float t = g_time;
    float ex = 4.0f*sinf(t*0.15f) + 1.5f*sinf(t*0.4f);
    float ey = 2.0f + 0.8f*sinf(t*0.10f);
    float ez = 2.0f + 1.5f*sinf(t*0.05f);
    // 呼吸感：镜头自身微浮动 (作用在姿态之后，即相机坐标系内)
    float breatheY = 0.04f*sinf(t*0.14f);
    float breatheX = 0.015f*sinf(t*0.08f);

    // 四元数 → 世界→相机旋转矩阵 (R(q)ᵀ)
    float qw = g_qw, qx = g_qx, qy = g_qy, qz = g_qz;
    float m00 = 1 - 2*(qy*qy + qz*qz);
    float m01 = 2*(qx*qy - qw*qz);
    float m02 = 2*(qx*qz + qw*qy);
    float m10 = 2*(qx*qy + qw*qz);
    float m11 = 1 - 2*(qx*qx + qz*qz);
    float m12 = 2*(qy*qz - qw*qx);
    float m20 = 2*(qx*qz - qw*qy);
    float m21 = 2*(qy*qz + qw*qx);
    float m22 = 1 - 2*(qx*qx + qy*qy);

    // 呼吸旋转 (微小，绕相机 y / x 轴)
    float cYb = cosf(breatheY), sYb = sinf(breatheY);
    float cXb = cosf(breatheX), sXb = sinf(breatheX);

    // ── 纯黑背景 ──
    canvas.fillSprite(TFT_BLACK);

    // ── 雪花 (直接写 sprite 内存) ──
    uint16_t* buf = (uint16_t*)canvas.getBuffer();

    for (int i = 0; i < N_SNOW; i++) {
        const Flake& f = g_snow[i];

        float dx = f.wx-ex, dy = f.wy-ey, dz = f.wz-ez;
        // 姿态旋转 (世界→相机)
        float rx = m00*dx + m10*dy + m20*dz;
        float ry = m01*dx + m11*dy + m21*dz;
        float rz = m02*dx + m12*dy + m22*dz;
        // 呼吸微调 (相机系内绕 y 再绕 x)
        float rxb = rx*cYb - rz*sYb;
        float rzb = rx*sYb + rz*cYb;
        float rry = ry*cXb - rzb*sXb;
        float rrz = ry*sXb + rzb*cXb;
        if (rrz >= -0.5f) continue;
        float depth = -rrz;
        float sc = FOCAL / depth;
        int sx = (int)(W/2 + rxb*sc), sy = (int)(H/2 - rry*sc);
        if (sx < 0 || sx >= W || sy < 0 || sy >= H) continue;

        float bright = 1.0f - depth/600.0f;
        if (bright < 0.01f) continue;

        // 大小随深度
        int sz = f.size;
        if (depth > 200) sz = 1;
        else if (depth > 100) sz = (sz > 2) ? 2 : sz;

        // 查表取色 (深度亮度 × 粒子个性亮度)
        float b = bright * f.bright;
        if (b > 1.0f) b = 1.0f;
        uint16_t col = g_snowLUT[(int)(b * 255.0f)];

        if (sz <= 1) {
            buf[sy * W + sx] = swap16(col);
        } else {
            int hx = sz / 2;
            for (int yy = sy - hx; yy <= sy + hx; yy++) {
                if (yy < 0 || yy >= H) continue;
                uint16_t* row = buf + yy * W;
                for (int xx = sx - hx; xx <= sx + hx; xx++) {
                    if (xx < 0 || xx >= W) continue;
                    row[xx] = swap16(col);
                }
            }
        }
    }

    // ── 视角锁定 HUD (右下角，低调提示) ──
    if (g_viewLocked) {
        canvas.setTextColor(c565(45, 75, 115));
        canvas.setTextSize(1);
        canvas.setCursor(W - 34, H - 10);
        canvas.print("LOCK");
    }
}

// ═══════════════════════════════════════════════════════════
//  Splash
// ═══════════════════════════════════════════════════════════

static void showSplash() {
    canvas.fillSprite(TFT_BLACK);
    canvas.setTextColor(c565(60, 100, 160));
    canvas.setTextSize(2);
    canvas.setCursor(20, 30);
    canvas.print("SNOWVEIL");
    canvas.setTextColor(c565(80, 130, 200));
    canvas.setTextSize(1);
    canvas.setCursor(32, 48);
    canvas.print("snow night");
    canvas.setTextColor(c565(15, 30, 55));
    canvas.setCursor(8, 82);
    canvas.print("v0.0.3a");
    canvas.setCursor(8, 94);
    canvas.print("shake: snowstorm");
    canvas.setCursor(8, 106);
    canvas.print("BtnA: lock view");
    canvas.setCursor(8, 118);
    canvas.print("BtnB: brightness");
    for (int x = 30; x < 210; x++)
        canvas.drawPixel(x, 74, c565(10, 20, 40));
    canvas.pushSprite(0, 0);
    calibrateGyroBias();   // splash 期间校准陀螺仪零偏 (保持静止)
    delay(600);
}

// ═══════════════════════════════════════════════════════════
//  Setup & Loop
// ═══════════════════════════════════════════════════════════

void setup() {
    auto cfg = M5.config();
    cfg.fallback_board = m5::board_t::board_M5StickS3;
    M5.begin(cfg);
    M5.Lcd.setRotation(1);
    canvas.createSprite(W, H);

    initSnow();
    initSnowLUT();
    M5.Imu.begin();
    M5.Lcd.setBrightness(BRIGHT[g_brightIdx]);

    showSplash();
}

void loop() {
    M5.update();
    updateIMU();
    updateRecentering();

    // 真实时间驱动：等效 15ms/帧语义，帧率波动时雪花速度不变
    uint32_t now = millis();
    float dt = (now - g_lastTick) / 1000.0f;
    g_lastTick = now;
    if (dt <= 0) dt = 0.001f;
    if (dt > 0.1f) dt = 0.1f;
    g_time += dt / 3.0f;

    // ── 风暴状态机：惊起快、平息慢 ──
    if (g_stormTimer > 0) {
        g_stormTimer -= dt;
        g_storm += (1.0f - g_storm) * 0.15f;   // ~0.3s 到峰值
    } else if (g_storm > 0.001f) {
        g_storm -= g_storm * 0.015f;            // ~3s 平息
    }

    updateSnow();
    render();
    canvas.pushSprite(0, 0);

    if (M5.BtnB.wasClicked()) {
        g_brightIdx = (g_brightIdx + 1) % 7;
        M5.Lcd.setBrightness(BRIGHT[g_brightIdx]);
    }
    if (M5.BtnA.wasClicked()) {
        g_viewLocked = !g_viewLocked;
        if (g_viewLocked) {       // 锁定时取消回正
            g_recentering = false;
            g_quiescentTime = 0;
            g_alignSet = false;
        }
    }

    delay(15);
}
