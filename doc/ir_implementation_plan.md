# 红外（IR）遥控功能实施计划

## TL;DR

在 BR1601x MCU 上实现符合 **GY/T 407-2024** 标准的红外遥控发射与学习功能。核心工作：基于当前工程的 TIM 驱动重写 NEC 协议编码层（参考驱动不可用），新增按键映射表、学习键值 NV 存储、工厂复位联动。硬件未定，所有引脚通过宏配置。

---

## 一、GY/T 407-2024 红外功能要求速查

### 1.1 载波信号（§6.1.1，第 7 页）

| 参数     | 要求                                       |
| -------- | ------------------------------------------ |
| 载波频率 | **38kHz** ±2%                       |
| 占空比   | **T1/Tc = 1/3**（≈33%，不是 50%！） |
| 测量方法 | §8.3（第 20 页）                          |

### 1.2 位波形（§6.1.2，第 7 页）

- 脉冲距离调制（Pulse Distance Modulation）
- 位 0 和位 1 通过脉冲时间间隔区分

### 1.3 数据格式（§6.1.3，第 7-8 页）

```
引导码 + 8bit用户码 + 8bit用户反码 + 8bit数据码 + 8bit数据反码 + 停止位
```

| 字段               | 说明                                                                  |
| ------------------ | --------------------------------------------------------------------- |
| 用户码（STB 默认） | **0x3A**（备用 0x3C）                                           |
| 用户码（TV 默认）  | **0x51**（备用 0x53）                                           |
| 位序               | **LSB first**（低位先发）                                       |
| 用户码切换         | 音量+ + 主页 3s → 切换 STB 用户码；音量+ + 菜单 3s → 切换 TV 用户码 |

### 1.4 重复码（§6.1.4，第 8 页）

- 按住按键不放：发完整帧后，**每隔 108ms 发一次重复码**（不是重新发完整帧！）

### 1.5 红外学习（§6.2，第 9 页）

- 触发：长按**电视机电源键 5s**，指示灯常亮
- 操作：按学习型按键（指示灯闪烁）→ 按被学习遥控器对应键
- 结果：慢闪 3 下 = 成功，快闪 5 下 = 失败
- 超时：15s 无操作退出
- 学习对象：**电视机遥控器**（仅 TV 端的按键）

### 1.6 按键码表（附录 B 表 B.1，第 30-31 页）

| 序号  | 按键       | 红外键值                        | 必选 |
| ----- | ---------- | ------------------------------- | ---- |
| 1     | 电源       | 0xDC                            | 必选 |
| 2     | 直播业务   | 0x3D                            | 必选 |
| 3     | 电视机业务 | 0x88                            | 必选 |
| 4     | 主页       | 0x88                            | 必选 |
| 5     | 确定       | 0xCE                            | 必选 |
| 6     | 返回       | 0x95                            | 必选 |
| 7     | 菜单       | 0x82                            | 必选 |
| 8     | 上         | 0xCA                            | 必选 |
| 9     | 下         | 0xD2                            | 必选 |
| 10    | 左         | 0x99                            | 必选 |
| 11    | 右         | 0xC1                            | 必选 |
| 12    | 信源       | 0x57                            | 可选 |
| 13    | 语音       | 0xE5                            | 可选 |
| 14    | 音量+      | 0x80                            | 必选 |
| 15    | 音量-      | 0x81                            | 必选 |
| 16    | 静音       | 0x9C                            | 可选 |
| 17    | 设置       | 0x8D                            | 可选 |
| 18    | 频道+      | 0x85                            | 必选 |
| 19    | 频道-      | 0x86                            | 必选 |
| 20    | 回看       | 0x37                            | 可选 |
| 21-30 | 数字 0-9   | 0x92/93/CC/8E/8F/C8/8A/8B/C4/87 | 可选 |

> 蓝牙/星闪键值：厂商识别码 0xC753，产品识别码 0x527D（附录 B 脚注 a）

### 1.7 键值更新（§7.4，第 10 页）

> "按键键值可通过下载方式或红外学习方式更新，**键值应以最后更新的为准**"

### 1.8 恢复出厂设置（§5.1.1.4，第 3 页）

> "主页 + 音量- 3s → 指示灯快闪 → 10s 内按确认 → 恢复至出厂默认状态"

恢复出厂应将 IR 学习键值清除。

### 1.9 配对请求红外码（§7.2，第 9 页）

- 码值：**0x7D**
- 用途：发起配对时发送

### 1.10 红外基础信息文件（附录 I，第 86 页，资料性）

- PW 编码和 BP 编码示例
- 用于蓝牙/星闪下发红外配置的格式定义

---

## 二、架构设计

### 2.1 数据流

```
按键按下 (task_keyscan.c)
    │
    ▼
keyscan_map[] → KEYSCAN_KEYx 枚举 (16 个物理按键)
    │
    ├─→ BLE 已连接？ → ble_hid_kbd_send()  ← 优先 BLE HID（工作态/待机态）
    │
    └─→ BLE 未连接？ → IR 发送路径（配对/回连/休眠唤醒）
         │
         ▼
    ir_key_map[] → 表B.1序号 (1~30)
         │
         ├─→ g_ir_learn_cache[]    (NV Flash 学习数据)  ← 第1优先级
         ├─→ g_ir_download_cache[] (NV Flash 下载数据)  ← 第2优先级（附录I）
         └─→ ir_default_rom[]      (ROM 默认表 B.1)     ← 第3优先级
         │
         ▼
    ir_send_encode(user_code, cmd_code)  → NEC 帧缓冲区
         │
         ▼
    ir_send() → TIM1 PWM (38kHz, 1/3占空比) + TIM3 (位时序)
         │
         ▼
    ir_repeat_timer() → sdk_timer 108ms → 重复码
```

> **IR 与 BLE 路由原则**：`pm_is_connected()` 为 true（WORKING/STANDBY 态）时走 BLE HID；
> false（PAIRING/RECONNECTING/SLEEP 唤醒）时走 IR。
> 例外：配对请求红外码（0x7D）、电源键开机——始终走 IR。

### 2.2 模块划分

```
bleuart/
├── inc/
├── IR/                       [新增] IR 功能模块（与 cat1/ 并列，不分 inc/src）
│   ├── ir_send.h             NEC 协议编码 + 38kHz 发送 API
│   ├── ir_send.c             NEC 编码 + TIM PWM 发送驱动
│   ├── ir_key_map.h          按键映射表 + 默认键值（纯头文件）
│   ├── ir_learn_store.h      学习键值 NV 存储 API
│   ├── ir_learn_store.c      Flash 读写 + 缓存管理
│   ├── ir_learn.h            红外学习状态机 API
│   └── ir_learn.c            学习流程 (EXTI + TIM 捕获)
├── bsp/driver/
│   ├── inc/tim.h             [已有] TIM 驱动（PWM 输出比较）
│   └── src/tim.c             [已有]
│   ├── inc/exti.h            [已有] 外部中断驱动（学习用）
│   └── src/exti.c            [已有]
├── prj/
│   └── ble_dtss.sct          [已有] NV 区在代码区外
└── cat1/cat1/OTA/
    └── flash_layout.h        [修改] 新增 FLASH_IR_LEARN_START/SIZE
```

### 2.3 硬件资源分配

BR1601x 有 TIM1、TIM3、TIM6（**没有 TIMER0**——参考驱动使用的 TIMER0 是其他 MCU 的外设）。

| 资源 | 用途 | 说明 |
|------|------|------|
| **TIM1 CHx** | 38kHz PWM 载波 | 硬件自动产生载波，无需 CPU 干预 |
| **TIM3** | NEC 位时序中断 | 每 bit 触发一次 ISR，切换 PWM 通断 |
| **EXTI y** | IR 学习信号捕获 | 下降沿触发，TIM6 辅助计时 |
| **TIM6** | IR 学习脉冲计时 | 仅学习模式启用，测量脉冲间隔 |
| **TIM6** | IR 学习脉冲计时 | 16位，PSC=48 → 1MHz (1μs)，ISR 中软件扩展高位防溢出 |
| **sdk_timer** | 108ms 重复码定时 | 工程已有的 SDK 软件定时器（1ms 精度） |

**BLE 中断屏蔽策略**：

BR1601x **没有统一的 `BLE_IRQn`**（参考驱动用的 `BLE_IRQn=0` 是其他芯片平台的），BLE 中断分散为 10 个独立 IRQ（`BLE_SW_IRQn`, `BLE_HSLOT_IRQn`, `BLE_FINETGT_IRQn` 等）。IR 发送 ISR 中 PWM 寄存器更新使用工程已有的 **`GLOBAL_INT_DISABLE()`/`GLOBAL_INT_RESTORE()`** 宏（`qflash.c` 同款），仅屏蔽 2~3 个寄存器写操作（约 200ns），而非逐个屏蔽 BLE IRQ。

**中断优先级方案**（不与现有冲突）：

```
USART2 (Cat.1)   = 0  ← 最高（已有）
DMA (PCM音频)     = 1  ← 已有
TIM3 (IR位时序)   = 2  ← 新增
TIM1 (PWM)       = 2  ← 新增（或无需IRQ，纯硬件PWM）
KEYSCAN          = 3  ← 已有
EXTI (IR学习)    = 3  ← 新增
TIM6 (学习计时)  = 3  ← 新增
```

> IR 发送 ISR 中用 `GLOBAL_INT_DISABLE()` 保护 PWM 寄存器更新（约 200ns），
> 参考工程 `qflash.c` 的 Flash 操作保护模式。

### 2.4 Flash 布局

```
0x01004000  ER_IROM1 (200KB)          固件 + const 默认键值
0x01036000  APP2 (OTA备份 200KB)
0x01068000  OTA Config (4KB)
0x01069000  BLE Bonding (4KB)
0x0106A000  User Cloud Policy (4KB)
0x0106B000  → IR Learn Sector (4KB)  [新增]
0x0106C000  (空闲)
```

### 2.5 代码注释风格

严格遵循工程现有 Doxygen 风格（参考 `pmu/src/pm_state.c`），不开 Barrot 版权头：

---

## 三、与参考驱动的差异（⚠️ 关键）

| 项目     | 参考 driver_ir_send.c   | GY/T 407-2024 要求                            | 实现策略                        |
| -------- | ----------------------- | --------------------------------------------- | ------------------------------- |
| 载波频率 | 36k/38k/56k 三种        | 仅 38kHz                                      | 只实现 38kHz                    |
| 占空比   | 50%                     | **1/3 (≈33%)**                         | TIM 输出比较 Pulse 设为 total/3 |
| 数据帧   | 引导码+裸数据+停止位    | 引导码+**用户码+反码+数据+反码**+停止位 | 重新实现 NEC 编码               |
| 重复机制 | 循环重发完整帧          | 108ms 间隔重复码                              | 独立重复码逻辑                  |
| 用户码   | 无                      | STB:0x3A/0x3C, TV:0x51/0x53                   | 全局变量+切换                   |
| PWM 驱动 | driver_pwm.h（其他MCU） | BR1601x TIM 输出比较                          | 用当前工程 tim.c                |
| OS 抽象  | BR1601x native OS       | FreeRTOS                                      | 用 FreeRTOS API                 |
| 学习存储 | 存原始时序（每键 800B） | 存解码 NEC 值（每键 4B）                      | 用 qflash.c                     |

---

## 四、实施步骤

### Phase 1：基础设施（无硬件依赖）

**Step 1.1** — Flash 布局定义

- 文件：`cat1/cat1/OTA/flash_layout.h`
- 新增：`FLASH_IR_LEARN_START` (0x0106B000) 和 `FLASH_IR_LEARN_SIZE` (0x1000)
- 依赖：无

**Step 1.2** — NEC 协议编码模块

- 文件：`IR/ir_send.h` + `IR/ir_send.c`
- 实现：`ir_send_encode(user_code, cmd_code, *out_buf, *out_len)` — 组装 NEC 帧时序数组
- 依赖：无（纯算法）

**Step 1.3** — 默认键值 ROM 表

- 文件：`IR/inc/ir_key_map.h`
- 定义：`ir_default_rom[30]` = 表 B.1 红外键值数组
- 定义：`ir_key_map[]` — 物理按键 KEYSCAN_KEYx → 表 B.1 序号
- 定义：用户码常量
- 依赖：Step 1.2

**Step 1.4** — 学习键值 NV 存储

- 文件：`IR/inc/ir_learn_store.h` + `IR/src/ir_learn_store.c`
- 实现：`ir_learn_load()`, `ir_learn_save()`, `ir_learn_clear_all()`
- 协议：Magic + version + entries[] + CRC32
- 依赖：Step 1.1, qflash API

**Step 1.5** — 键值查找统一接口

- 文件：`IR/inc/ir_key_map.h` 中增加
- 实现：`ir_lookup(key_index, *user_code)` → cmd_code（三级查找）
- 依赖：Step 1.3, Step 1.4

### Phase 2：PWM 发送（需硬件 TIM，引脚通过宏配置）

**Step 2.1** — IR 引脚配置宏

- 文件：`IR/ir_send.h`
- `IR_SEND_TIM` — PWM 载波定时器（默认 TIM1）
- `IR_SEND_TIM_CH` — PWM 输出通道（默认 CH1）
- `IR_SEND_PIN_PORT`, `IR_SEND_PIN_BIT` — PWM 输出 GPIO
- `IR_SEND_BIT_TIM` — 位时序定时器（默认 TIM3）
- `IR_LEARN_EXTI_PIN_PORT`, `IR_LEARN_EXTI_PIN_BIT`, `IR_LEARN_EXTI_LINE`
- `IR_LEARN_EN_PIN_PORT`, `IR_LEARN_EN_PIN_BIT`
- 依赖：无（硬件确定后仅修改宏）

**Step 2.2** — NEC 发送驱动（双定时器架构）

- 文件：`IR/ir_send.c`
- **TIM1 CHx**：硬件 PWM 产生 38kHz 1/3 占空比载波（`TIM_InitTimeBase` + `TIM_InitOc1`）
- **TIM3**：位时序中断，在 ISR 中切换 TIM1 PWM 通断（`TIM_InitTimeBase` 单次/周期模式）
- PWM 频率配置：48MHz / 38kHz = 1263 ticks，Pulse = 1263/3 = 421（1/3 占空比）
- `ir_send(user_code, cmd_code)` — 编码 + 启动双定时器发送
- `ir_send_stop()` — 停止 PWM + 停止 TIM3 + 恢复引脚
- `ir_send_init()` — 初始化 TIM1/TIM3/GPIO（`ir_send_init()` 在 `main.c` 调用）
- ISR 中 PWM 寄存器更新使用 `GLOBAL_INT_DISABLE()/GLOBAL_INT_RESTORE()` 保护（参考 `qflash.c`）
- 依赖：Step 2.1, Step 1.2, tim.c

**Step 2.3** — 重复码定时器（SDK 软件定时器）

- 文件：`IR/ir_send.c`
- 使用工程已有的 **`sdk_timer_add()`**（不是 FreeRTOS xTimer！工程未用 FreeRTOS 软件定时器）
- 发完完整帧后注册 108ms 单次定时器 → 回调中发送重复码
- 按键松开 → `sdk_timer_del()` 取消定时器 → `ir_send_stop()`
- 依赖：Step 2.2

### Phase 3：红外学习（需 EXTI + TIM6，引脚通过宏配置）

**Step 3.1** — 红外学习状态机

- 文件：`IR/ir_learn.h` + `IR/ir_learn.c`
- 状态：IDLE → LEARNING → SUCCESS / FAILED / TIMEOUT
- LED 指示：常亮(学习中) / 慢闪3下(成功) / 快闪5下(失败)
- 超时：15s（用 `sdk_timer_add` 实现）
- 依赖：Step 2.1, exti.h

**Step 3.2** — EXTI 捕获 + TIM6 计时

- 文件：`IR/ir_learn.c`
- **EXTI**：下降沿触发，在 ISR 中读 TIM6 当前计数值 → 得到脉冲间隔
- **TIM6**：学习期间持续自由运行（48MHz / 48 = 1MHz → 1μs 分辨率），供 EXTI ISR 读取
- 脉冲缓冲区：**动态分配**，最大 200 个 uint16_t（400 字节），避免 12KB 静态分配撑爆 RAM
- 解码：分析脉冲间隔 → 检测载波频率（38kHz ± 容差）→ 提取 NEC 用户码+数据码
- 依赖：Step 3.1, Step 1.2, exti.c, tim.c (TIM6)

**Step 3.3** — 学习结果保存

- 调用：`ir_learn_save()` → NV Flash 存储
- 依赖：Step 3.2, Step 1.4

### Phase 4：系统集成

**Step 4.1** — 按键系统对接

- 文件：`src/task_keyscan.c` / `src/main.c`
- **路由判断**：`pm_is_connected()` → true 走 BLE HID，false 走 IR
- 例外：配对红外码 0x7D、电源键开机 → 始终走 IR
- 按键按下 → `ir_lookup()` 获取键值 → `ir_send(user_code, cmd_code)`
- 按键保持检测 → 触发重复码定时器
- 按键松开 → `sdk_timer_del()` 取消重复码 → `ir_send_stop()`
- 依赖：Phase 2, Step 1.5, `pm_state.h`

**Step 4.2** — PM 状态机联动

- IR 发送期间禁止进入 SLEEP（通过 `pm_activity_notify()` 刷新活动时间戳）
- IR 发送完成后恢复正常的空闲超时检测
- 依赖：Step 4.1, `pm_state.c`

**Step 4.3** — OTA 会话互斥

- OTA 会话期间（`udp_ota_session_busy() != 0`）：禁止 IR 学习、禁止用户码切换
- IR 发送不受限（OTA 期间仍需红外开机/配对）
- 依赖：Step 4.1, `udp_ota.h`

**Step 4.4** — 用户码切换组合键

- 实现：音量+ + 主页 3s → LED 指示 + 切换 STB 用户码（0x3A ↔ 0x3C）
- 实现：音量+ + 菜单 3s → LED 指示 + 切换 TV 用户码（0x51 ↔ 0x53）
- OTA 会话期间禁止切换
- 依赖：Step 1.3

**Step 4.5** — 学习触发组合键

- 实现：长按电视机电源键 5s → 进入学习模式
- 依赖：Phase 3

**Step 4.6** — 工厂复位联动

- 文件：`src/task_keyscan.c` → `FACTORY_RST_CONFIRMED` 分支
- 新增：`ir_learn_clear_all()` — 擦除 IR Learn 扇区 + 复位用户码为默认值
- 依赖：Step 1.4

**Step 4.7** — 初始化调用

- 文件：`src/main.c`
- `ir_send_init()` — TIM1 PWM + TIM3 位时序 + GPIO 初始化
- `ir_learn_load()` — 从 Flash 恢复学习键值到 RAM 缓存
- 依赖：Step 2.2, Step 1.4

---

## 五、关键数据结构

```c
/* ir_learn_store.h */
#define IR_LEARN_MAGIC   0x49524C4Eu  /* 'IRLN' */

typedef struct {
    uint8_t  key_index;    /* 表 B.1 序号 1~30, 0=无效 */
    uint8_t  user_code;    /* NEC 用户码 */
    uint8_t  cmd_code;     /* NEC 数据码 */
    uint8_t  reserved;
} ir_learn_entry_t;        /* 4 字节 */

typedef struct {
    uint32_t magic;
    uint8_t  version;
    uint8_t  entry_count;
    uint8_t  reserved[2];
    uint32_t crc32;
    ir_learn_entry_t entries[30];  /* 120 字节 */
} ir_learn_sector_t;       /* 136 字节 → 1 个 4KB 扇区 */

/* ir_key_map.h */
typedef struct {
    uint8_t  b1_index;          /* 表 B.1 序号, 0=未映射 */
    uint8_t  default_ir_code;   /* 默认红外键值 */
    bool     is_required;
} ir_key_map_entry_t;

/* ir_send.h */
typedef struct {
    uint32_t *timing_buf;   /* 时序数组（高低电平 μs） */
    uint16_t  timing_cnt;   /* 时序个数 */
    uint8_t   user_code;
    uint8_t   cmd_code;
    bool      is_sending;
    bool      key_held;     /* 按键保持中 → 发重复码 */
} ir_send_ctx_t;
```

---

## 六、验证方法

1. **NEC 编码验证**（Phase 1 完成后）：打印编码帧时序，与标准波形图对比
2. **PWM 发送验证**（Phase 2 完成后）：逻辑分析仪接 IR_GPIO，验证 38kHz 1/3 占空比 + 帧时序
3. **学习验证**（Phase 3 完成后）：用已知 NEC 遥控器对学，比对解码结果
4. **NV 存储验证**（Phase 1.4 + 4.4 完成）：学习→断电→上电→验证键值保留；工厂复位→验证清空
5. **系统集成验证**（Phase 4 完成）：全链路测试——按键→发送→机顶盒响应；学习→发送→电视机响应

---

## 七、决策记录

- **为什么不用参考驱动的原始时序存储？** 因为标准只用 NEC 协议，解码存储每键 4 字节 vs 原始时序 800 字节，空间节省 200 倍
- **为什么占空比是 1/3 而非 50%？** 标准 §6.1.1 和图 1 明确要求 T1/Tc=1/3，用 `TIM_InitOc1` 的 Pulse 参数控制
- **为什么学习只针对 TV？** 标准 §6.2 标题就是"红外学习电视机遥控器"，学习型按键仅 TV 端
- **为什么存储区在 0x0106B000？** 紧接现有 NV 区末尾，4KB 扇区对齐，总 Flash 512KB 内仍有 ~80KB 空余
- **硬件引脚未定怎么办？** 所有 GPIO/TIM/EXTI 通过宏定义，集中在 `ir_send.h` 顶部，后续改一处即可
- **IR 文件放在哪里？** `bleuart/IR/` 目录（与 `cat1/` 并列），文件平铺不分 `inc/`/`src/` 子目录
- **发送模块叫什么名字？** `ir_send` 而非 `ir_nec`——名称反映功能（发送）而非协议（NEC）
- **代码注释用什么风格？** Doxygen 风格（`@file`/`@brief`/`@param`/`@return`/`@note`），参考 `pmu/src/pm_state.c`，不开 Barrot 版权头
- **为什么用 TIM1+TIM3 双定时器？** BR1601x 没有 TIMER0（参考驱动的外设）；TIM1 硬件 PWM 产生载波 + TIM3 位时序中断的架构最低 CPU 开销
- **为什么用 sdk_timer 而不是 FreeRTOS Timer？** 工程没有使用 FreeRTOS 软件定时器（xTimerCreate 等均被链接器移除），统一用 `sdk_timer_add()` 保持一致性
- **学习缓冲区为什么动态分配？** RW_IRAM0 仅 9KB（已满载语音/AES/PCM），静态 12KB 学习缓冲区不可行；改为 `malloc` 最大 400 字节
- **IR 发送期间与 PM 如何交互？** 调用 `pm_activity_notify()` 刷新空闲计时器，避免发送中途意外进入 STANDBY/SLEEP
- **OTA 期间 IR 如何处理？** 学习功能和用户码切换被 OTA 锁定；IR 发送不受限（OTA 期间仍需红外控制）
- **BLE 中断如何屏蔽？** BR1601x 无统一 `BLE_IRQn`（参考驱动用的 `BLE_IRQn=0` 是其他平台），改用 `GLOBAL_INT_DISABLE()/GLOBAL_INT_RESTORE()` 保护 PWM 寄存器更新（约 200ns）
- **TIM6 16 位溢出怎么办？** 学习模式下 TIM6 ISR 软件扩展 32 位计数器，每 65535μs 溢出一次，单帧 NEC 学习最多溢出 2 次
- **IR 和 BLE HID 如何分工？** BLE 连接态走 HID（已有 `ble_hid_kbd.c`），BLE 断开态走 IR；电源键和配对码 0x7D 始终走 IR
