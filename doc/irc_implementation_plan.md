# IRC 红外功能实施计划（硬件 IRC 方案）

> **状态**：随项目进展持续更新  
> **基础文档**：[ir_implementation_plan.md](ir_implementation_plan.md)（TIM 方案，保留参考）  
> **方案变更原因**：CAT1 UART 从 PB4/PB5 迁移到 PA4/PA5（USART1），释放 PB4/PB5 给硬件 IRC

---

## 一、硬件引脚（已确定）

| 引脚 | 功能 | 外设 | 说明 |
|------|------|------|------|
| **PB4** | IRTX | IRC（硬件自动接管） | 38kHz 红外发射 |
| **PB5** | IRRX | IRC / EXTI | 发送时=IRC RX，学习时=GPIO+EXTI |
| PA4 | USART1 TX（CAT1） | AF3 | 原 PB4 功能迁移 |
| PA5 | USART1 RX（CAT1） | AF3 | 原 PB5 功能迁移 |
| PB6 | 电池 ADC CH5 | ADC | 不变 |
| PB7 | LED2 | GPIO OUTPUT | 不变 |

---

## 二、IRC 外设概述（BR1601x 用户手册第 20 章）

| 寄存器 | 偏移 | 位宽 | 功能 |
|--------|------|------|------|
| FREQ_CARRIER_ON | 0x00 | 10bit | 载波导通 ticks |
| FREQ_CARRIER_OFF | 0x04 | 10bit | 载波关断 ticks |
| LOGIC_ONE_TIME | 0x08 | MARK[15:8]+SPACE[7:0] | 逻辑 1 时序 |
| LOGIC_ZERO_TIME | 0x0C | MARK[15:8]+SPACE[7:0] | 逻辑 0 时序 |
| CTRL | 0x10 | — | 控制（ENABLE/TX_START/FIFO_RESET 等） |
| STATUS | 0x14 | — | 状态（BUSY/FIFO_WORDS/IRQ_FLAG） |
| REPEAT_TIME | 0x18 | 16bit | 重复码间隔 |
| CODE_FIFO | 0x1C | 21bit | 数据码 FIFO |
| REPEAT_FIFO | 0x20 | 21bit | 重复码 FIFO |

- 基地址：`0x40021C00`（AHB 总线）
- 时钟：`RCC_AHB_PERIPH_IRC`（bit 14）
- 中断：`IRC_IRQn = 31`

> ⚠️ **待确认**（需查手册第 20 章）：IRC 时钟源、CODE_FIFO 位定义、引导码生成方式

---

## 三、软件模块

```
bleuart/IR/
├── irc.h              IRC 驱动 API
├── irc.c              IRC 初始化 + NEC 发送
├── ir_key_map.h       按键映射表（表 B.1）+ 默认键值
├── ir_learn.h         红外学习状态机 API
├── ir_learn.c         红外学习（EXTI + TIM）
├── ir_learn_store.h   学习键值 NV 存储 API
└── ir_learn_store.c   Flash 读写 + 缓存管理
```

---

## 四、实施步骤

### Phase 1：IRC 驱动（`IR/irc.h` + `IR/irc.c`） ✅

- [x] 确认 IRC 时钟源（AHB = HSI = 64MHz 直供）
- [x] 确认 CODE_FIFO 21-bit 格式（Digital / Paint Mark / Paint Space）
- [x] 确认引导码/停止位处理方式（Paint 消息手工组装）
- [x] 实现 `irc_init()` — 时钟 + 载波 + 时序 + 使能
- [x] 实现 `irc_send(user_code, cmd_code)` — 组装 NEC 帧 → CODE_FIFO → TX_START
- [x] 实现 `irc_send_repeat()` — CODE_FIFO Paint 消息（9ms+2.25ms+600μs）
- [x] 实现 `irc_stop()` / `irc_is_busy()`
- [x] 实现 `IRC_IRQHandler()`

### Phase 2：按键映射（`IR/ir_key_map.h`） ✅

- [x] 定义 `ir_default_cmd_table[18]` = GY/T 407-2024 表 B.1 红外键值（按 KEYSCAN_KEY1..17 索引）
- [x] 物理按键 KEYSCAN_KEYx → 默认键值直接对应（与 app_keyscan.h 一致）
- [x] 查找接口 `ir_default_cmd()` / `ir_default_user()`（学习值优先逻辑在 ir_learn_store）

### Phase 3：学习键值 NV 存储（`IR/ir_learn_store.h` + `.c`） ✅

- [x] Flash 扇区：`0x0106B000`（4KB，紧接 User Cloud Policy）
- [x] 存储格式：Magic + version + count + checksum(XOR) + entries[18]
- [x] 实现 `ir_learn_store_load()` / `save()` / `lookup()` / `clear_all()`

### Phase 4：红外学习（`IR/ir_learn.h` + `IR/ir_learn.c`） ✅（驱动层）

- [x] 状态机：IDLE → LEARNING → DONE / FAILED / TIMEOUT / CANCELED
- [x] **TIM3_CH2 双边沿捕获**（PB5 AF2）解码 NEC 格式（EXTI4_12 被协议栈占用，改用 TIM3）
- [ ] LED 指示：常亮(学习中) / 慢闪3下(成功) / 快闪5下(失败)（Phase 5 集成）
- [ ] 学习结果保存到 NV Flash（Phase 5：task_keyscan 回调里调用 store_save）

### Phase 5：系统集成

- [x] `main.c` → `irc_init()` 调用（+ `ir_learn_init()`）
- [x] KEIL 工程加入 IR 模块（`irc.c`/`ir_learn.c`/`ir_learn_store.c` + IncludePath `..\IR`），编译通过
- [x] `task_ble.c` → BLE 栈初始化后 `ir_learn_store_load()`
- [ ] `task_keyscan.c` → 按键触发 IR 发送
- [ ] 用户码切换组合键（音量+ + 主页 / 音量+ + 菜单）
- [ ] 学习触发组合键（长按电视机电源键 5s）
- [ ] 工厂复位 → `ir_learn_clear_all()`
- [ ] PM 联动：IR 发送期间刷新活动时间戳

---

## 五、NEC 协议参数（GY/T 407-2024 §6.1）

| 参数 | 值 | IRC 寄存器 |
|------|-----|-----------|
| 载波频率 | 38kHz ±2% | FREQ_CARRIER_ON/OFF |
| 占空比 | 1/3 | 待时钟确认后计算 |
| 引导码 | 9ms 低 + 4.5ms 高 | 待确认（自动/手动） |
| 逻辑 0 | 560μs + 560μs | LOGIC_ZERO_TIME |
| 逻辑 1 | 560μs + 1680μs | LOGIC_ONE_TIME |
| 停止位 | 600μs 低 + 长高 | 待确认 |
| 重复码间隔 | 108ms | REPEAT_TIME |
| 用户码 (STB) | 0x3A / 0x3C | 软件管理 |
| 用户码 (TV) | 0x51 / 0x53 | 软件管理 |
| 位序 | LSB first | IRC 硬件处理 |

---

## 六、按键码表（GY/T 407-2024 表 B.1）

| 序号 | 按键 | 红外键值 | 必选 |
|------|------|---------|------|
| 1 | 电源 | 0xDC | ✅ |
| 2 | 直播业务 | 0x3D | ✅ |
| 3 | 电视机业务 | 0x88 | ✅ |
| 4 | 主页 | 0x88 | ✅ |
| 5 | 确定 | 0xCE | ✅ |
| 6 | 返回 | 0x95 | ✅ |
| 7 | 菜单 | 0x82 | ✅ |
| 8 | 上 | 0xCA | ✅ |
| 9 | 下 | 0xD2 | ✅ |
| 10 | 左 | 0x99 | ✅ |
| 11 | 右 | 0xC1 | ✅ |
| 12 | 信源 | 0x57 | |
| 13 | 语音 | 0xE5 | |
| 14 | 音量+ | 0x80 | ✅ |
| 15 | 音量- | 0x81 | ✅ |
| 16 | 静音 | 0x9C | |
| 17 | 设置 | 0x8D | |
| 18 | 频道+ | 0x85 | ✅ |
| 19 | 频道- | 0x86 | ✅ |
| 20 | 回看 | 0x37 | |
| 21-30 | 数字 0-9 | 0x92/93/CC/8E/8F/C8/8A/8B/C4/87 | |

---

## 七、变更记录

| 日期 | 变更 |
|------|------|
| 2026-08-12 | 初始版本：IRC 方案替代 TIM 方案，PB4/PB5 专用于 IRC，CAT1 UART 迁移至 PA4/PA5 |
| 2026-08-13 | 完成学习模块：`ir_key_map.h`、`ir_learn_store.h/.c`、`ir_learn.h/.c`。关键修正：手册表 5-12 确认 IRC 仅 IRTX(PB4 AF2) 无 IRRX 复用，PB5= TIM3_CH2(AF2)；EXTI4_12 被协议栈占用，学习改用 TIM3 输入捕获（双边沿，TIM3 时钟 64MHz） |
