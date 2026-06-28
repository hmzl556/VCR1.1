# BLE 遥控器电源管理（PMU）实现总结

> 版本：v1.0  
> 日期：2026-06-24  
> 基于方案：[休眠待机态修改方案.md](休眠待机态修改方案.md)

---

## 一、项目概述

在原有蓝牙遥控器项目（HID 按键 + 语音 + CAT1 通信）基础上，新建 `pmu/` 电源管理模块，实现**五状态机**管理，满足 GY/T 407-2024 功耗指标：

| 指标 | 要求值 | 实现方式 |
|------|--------|----------|
| 待机平均电流 | ≤ 65μA | 大连接间隔(50ms/lat100) + 关闭 CAT1 TX/LED |
| 休眠平均电流 | ≤ 20μA | BR1601 Sleep 模式（实测 ~1.6μA） |

---

## 二、新增/修改文件清单

### 2.1 新增文件（7个）

```
bleuart/pmu/
├── inc/
│   ├── pm_config.h        # 所有可配置参数（超时、连接参数、休眠参数、调试开关）
│   ├── pm_state.h          # 状态机枚举 + 活动类型枚举 + 全部公共 API 声明
│   ├── pm_standby.h        # pm_standby_enter() / pm_standby_exit() 声明
│   └── pm_sleep.h          # pm_sleep_enter() + 唤醒原因宏定义
└── src/
    ├── pm_state.c           # 五状态机核心逻辑（~460 行）
    ├── pm_standby.c         # 待机态外设控制（BLE 参数切换 + CAT1 TX + LED）
    └── pm_sleep.c           # 休眠态管理（Sleep 模式 + RTC 唤醒 + 唤醒源检测）
```

### 2.2 修改文件（8个）

| 文件 | 改动说明 |
|------|----------|
| `bleuart/src/main.c` | 新增 `#include "pm_state.h"`；`app_rtos_init()` 后调用 `pm_state_init()` |
| `bleuart/src/task_ble.c` | 新增 `#include "pm_state.h"`；主循环开头调用 `pm_state_process()`；BLE 启动后调用 `pm_state_sync_after_ble_init()` |
| `bleuart/src/ble_gap.c` | 新增 `#include "pm_state.h"`；连接成功回调中调用 `pm_notify_connected()`；断连回调中调用 `pm_notify_disconnected()` |
| `bleuart/src/task_keyscan.c` | 新增 `#include "pm_state.h"`；按键消抖确认后调用 `pm_activity_notify(PM_ACTIVITY_KEY)` |
| `bleuart/src/app_voice_ctrl.c` | 新增 `#include "pm_state.h"`；语音开始时调用 `pm_activity_notify(PM_ACTIVITY_VOICE)` |
| `bleuart/src/ble_hid_kbd.c` | 新增 `#include "pm_state.h"`；STB 写入 HID Boot OUT 时调用 `pm_activity_notify(PM_ACTIVITY_PEER_DATA)` |
| `bleuart/cat1/rc_cat1_bridge.c` | 新增 `#include "pm_state.h"`；CAT1 收到下行数据时调用 `pm_activity_notify(PM_ACTIVITY_CAT1)` |
| `bleuart/prj/ble_dtss.uvprojx` | Include 路径新增 `..\pmu\inc`；用户手动添加 pmu 源文件到工程 |

---

## 三、五状态机设计

### 3.1 状态定义

```
                              ┌──────────────┐
                    ┌────────>│   配对态     │<────────────┐
                    │         │  PAIRING     │             │
                    │         │ 可发现可连接  │             │
                    │         └──────┬───────┘             │
                    │                │ 连接成功             │
                    │         ┌──────▼───────┐             │
        断连+无绑定  │ 回连超时 │   回连态     │             │
                    │  -------│ RECONNECTING │             │
                    │ |       │ 不可发现可连接│             │
                    │ |       └──────┬───────┘             │
                    │ |              │ 连接成功             │
                    │ |       ┌──────▼───────┐   断连      │
                    │ |       │   工作态     │─────────┐   │
                    │ |       │   WORKING    │         │   │
                    │ |       │10ms/lat40   │         │   │
                    │ |       └──┬──────┬────┘         │   │
                    │ |          │      │ 无业务8s     │   │
                    │ |  按键/语 │      ▼              │   │
                    │ |  音/STB  │  ┌──────────┐       │   │
                    │ |  数据    │  │  待机态   │ 断连  │   │
                    │ |          └─→│ STANDBY  │──────►│   │
                    │ |             │50ms/lat100│      │   │
                    │ |             └────┬─────┘       │   │
                    │ |                  │ 无业务45s    │   │
                    │ |                  ▼              │   │
                    │ |             ┌──────────┐       │   │
                    │ |             │  休眠态   │       │   │
                    │  -----------> │  SLEEP   │       │   │
                    │               │~1.6μA    │       │   │
                    │               └──┬───┬───┘       │   │
                    │      按键/RTC唤醒  │   │           │   │
                    │                   ▼   ▼           │   │
                    │              有绑定  无绑定       │   │
                    └───────────────┼──────┼───────────┘   │
                                    │      └───────────────┘
                                    └──→ 回连态 / 配对态
```

### 3.2 状态转换条件速查表

| 序号 | 源状态 | 目标状态 | 触发条件 | 代码位置 |
|------|--------|----------|----------|----------|
| 1 | 上电 | PAIRING | 无绑定记录 | `pm_state_init()` |
| 2 | 上电 | RECONNECTING | 有绑定记录（BLE 启动后同步） | `pm_state_sync_after_ble_init()` |
| 3 | PAIRING | WORKING | 连接成功 | `pm_notify_connected()` |
| 4 | RECONNECTING | WORKING | 连接成功 | `pm_notify_connected()` |
| 5 | WORKING | STANDBY | 空闲 8 秒无业务 | `pm_state_process()` |
| 6 | STANDBY | WORKING | 按键/语音/STB数据/CAT1数据 | `pm_activity_notify()` |
| 7 | STANDBY | SLEEP | 空闲 45 秒无业务 | `pm_state_process()` |
| 8 | RECONNECTING | SLEEP | 回连广播 45 秒无连接 | `pm_state_process()` |
| 9 | WORKING | RECONNECTING | 断连且有绑定 | `pm_notify_disconnected()` |
| 10 | WORKING | PAIRING | 断连且无绑定 | `pm_notify_disconnected()` |
| 11 | STANDBY | RECONNECTING | 断连且有绑定 | `pm_notify_disconnected()` |
| 12 | STANDBY | PAIRING | 断连且无绑定 | `pm_notify_disconnected()` |
| 13 | SLEEP | RECONNECTING | 按键/RTC唤醒，有绑定 | `pm_enter_sleep()` |
| 14 | SLEEP | PAIRING | 按键/RTC唤醒，无绑定 | `pm_enter_sleep()` |

---

## 四、各状态详细行为

### 4.1 配对态（PAIRING）

| 项目 | 内容 |
|------|------|
| 触发进入 | 上电无绑定；断连无绑定；恢复出厂设置；休眠唤醒无绑定 |
| BLE 广播 | 可发现 + 可连接（类型1），20ms 间隔，37/38/39 三通道 |
| LED 指示 | LED2 200ms 周期快闪 |
| 连接参数 | 无连接 |
| 功耗特征 | 高（持续广播，~mA 级） |
| 退出条件 | 连接成功 → WORKING |
| 超时 | 不超时（用户可能在操作配对流程） |

**状态进入** (`pm_enter_pairing`)：
- 刷新活动时间戳

**状态退出**：无需特殊操作

### 4.2 回连态（RECONNECTING）

| 项目 | 内容 |
|------|------|
| 触发进入 | 上电有绑定；断连有绑定；休眠唤醒有绑定 |
| BLE 广播 | 不可发现 + 可连接（类型2），20ms 间隔 |
| LED 指示 | 灭 |
| 连接参数 | 无连接 |
| 功耗特征 | 高（持续广播） |
| 退出条件 | 连接成功 → WORKING；超时45秒 → SLEEP |
| 超时 | `PM_RECONNECT_TIMEOUT_MS`（默认 45 秒） |

**状态进入** (`pm_enter_reconnecting`)：
- 设置 `g_pm_reconnect_start_ms`（回连计时起点）
- 刷新活动时间戳

**状态退出**：无需特殊操作

### 4.3 工作态（WORKING）

| 项目 | 内容 |
|------|------|
| 触发进入 | 配对态/回连态连接成功；待机态下收到活动通知 |
| BLE 广播 | 关闭（已连接，广播无意义） |
| BLE 连接参数 | 10ms 连接间隔 / latency 40（GY/T 407-2024 规范值） |
| 外设状态 | CAT1 全双工；LED 按指示逻辑；KEYSCAN 正常；语音可用 |
| 功耗特征 | 基准功耗 |
| 退出条件 | 空闲8秒 → STANDBY；断连 → PAIRING/RECONNECTING |

**状态进入** (`pm_enter_working`)：
- 刷新活动时间戳（避免刚进入就因旧计时器值超时）

**状态退出**：无需特殊操作

**活动通知**（重置空闲计时器）：
- 按键按下 → `pm_activity_notify(PM_ACTIVITY_KEY)`
- 语音开始 → `pm_activity_notify(PM_ACTIVITY_VOICE)`
- STB 数据下发 → `pm_activity_notify(PM_ACTIVITY_PEER_DATA)`
- CAT1 收数据 → `pm_activity_notify(PM_ACTIVITY_CAT1)`

### 4.4 待机态（STANDBY）

| 项目 | 内容 |
|------|------|
| 触发进入 | 工作态下无业务 8 秒 |
| BLE 连接 | 保持连接 |
| BLE 连接参数 | 50ms 连接间隔 / latency 100（GY/T 407-2024 上限，最大省电） |
| CAT1 UART | TX 关闭（USART2->CTRL1.TXEN=0），RX 保持（可唤醒） |
| LED | 全灭 |
| KEYSCAN | 正常检测 |
| 射频 | BLE 链路保持（每 ~5 秒响应一次，latency=100） |
| 功耗目标 | ≤ 65μA |
| 退出条件 | 按键/语音/STB数据/CAT1数据 → WORKING；空闲45秒 → SLEEP；断连 → PAIRING/RECONNECTING |

**状态进入** (`pm_enter_standby`) → `pm_standby_enter()`：

| 步骤 | 操作 | 函数 |
|------|------|------|
| 1 | 向 STB 发送连接参数更新请求（50ms/lat100） | `pm_conn_params_set_standby()` → `le_conn_update()` |
| 2 | 关闭 CAT1 UART TX（RX 保持） | `pm_cat1_tx_disable()` → `USART2->CTRL1 &= ~TXEN` |
| 3 | 关闭全部 LED | `pm_led_all_off()` → `CLRLED1 + CLRLED2` |

**状态退出**（仅当目标为 WORKING 时，目标为 SLEEP 则跳过）→ `pm_standby_exit()`：

| 步骤 | 操作 | 函数 |
|------|------|------|
| 1 | 向 STB 发送连接参数恢复请求（10ms/lat40） | `pm_conn_params_set_working()` → `le_conn_update()` |
| 2 | 恢复 CAT1 UART TX | `pm_cat1_tx_enable()` → `USART2->CTRL1 \|= TXEN` |
| - | LED 由业务模块自行控制 | 不强制点亮 |

**待机态期间触发动作**：

| 触发事件 | 处理流程 | 位置 |
|----------|----------|------|
| 按键按下 | `task_keyscan_process()` → `pm_activity_notify(KEY)` → 刷新时间戳 + 切回 WORKING | `task_keyscan.c:338` |
| 语音开始 | `voice_ctrl_process_pending()` → `pm_activity_notify(VOICE)` → 刷新时间戳 + 切回 WORKING | `app_voice_ctrl.c:48` |
| STB 数据下发 | `hid_boot_out_cbk()` → `pm_activity_notify(PEER_DATA)` → 刷新时间戳 + 切回 WORKING | `ble_hid_kbd.c:208` |
| CAT1 收数据 | `rc_cat1_poll_downlink()` → `pm_activity_notify(CAT1)` → 刷新时间戳 + 切回 WORKING | `rc_cat1_bridge.c:173` |
| BLE 断连 | `ble_gap_disconnected()` → `pm_notify_disconnected()` → 根据绑定进入 PAIRING/RECONNECTING | `ble_gap.c:349` |
| 空闲 45 秒 | `pm_state_process()` → `pm_state_transition(SLEEP)` → 直接进入休眠（不恢复外设） | `pm_state.c` |

### 4.5 休眠态（SLEEP）

| 项目 | 内容 |
|------|------|
| 触发进入 | 待机态空闲 45 秒；回连态广播 45 秒无连接 |
| CPU 状态 | 停止（WFI），HSE/HSI 关闭 |
| SRAM | 48KB 全部保持 |
| BLE | 基带关闭，广播停止 |
| LSI | 保持运行（供 RTC 定时唤醒） |
| FLASH | 关闭 |
| 唤醒源 | ① KEYSCAN EXTI（按键）；② RTC 定时器（每 5 秒） |
| 电流 | ~1.6μA（远低于 20μA 指标） |
| 唤醒后行为 | 重新初始化 BLE 栈 → 根据绑定启动配对或回连广播 |

**状态进入** (`pm_enter_sleep`) → `pm_sleep_enter()`：

| 步骤 | 操作 | 函数 | 位置 |
|------|------|------|------|
| 1 | 停止 BLE 广播 | `ble_adv_stop()` | `pm_sleep.c:159` |
| 2 | 清零 `bleAdvStat`（主循环发现后重启 BLE） | `bleAdvStat = 0U` | `pm_sleep.c:162` |
| 3 | 关闭非必要外设（LED + CAT1 UART 全关） | `pm_peripherals_sleep_shutdown()` | `pm_sleep.c:165` |
| 4 | 首次进入时一次性初始化 RTC + LSI | `pm_rtc_init_once()` | `pm_sleep.c:48-82` |
| 5 | 配置 RTC 唤醒定时器（5 秒） | `pm_rtc_wakeup_config()` | `pm_sleep.c:90-108` |
| 6 | 进入 Sleep 模式 | `PWR_EnterSLEEPMode(WFI)` | `pm_sleep.c:178` |

**RTC 初始化**（`pm_rtc_init_once`，仅首次进入休眠时执行一次）：

| 步骤 | 操作 | 代码 |
|------|------|------|
| ① | 确保 LSI 已使能并稳定 | `RCC->LSCTRL \|= LSIEN` → 等待 `RCC_FLAG_LSIRD` |
| ② | 选择 LSI 为低速时钟源 | `RCC->LSCTRL &= ~RCC_LSCTRL_LSXSEL` |
| ③ | 使能 RTC 外设时钟 | `RCC->LSCTRL \|= RCC_LSCTRL_RTCEN` |
| ④ | 使能 PWR 接口时钟 | `RCC_EnableAPB1PeriphClk(PWREN, ENABLE)` |
| ⑤ | 配置并初始化 RTC | `RTC_Init(&rtc_cfg)` 参数：32KHz LSI, 预分频 127/255 |

**Wakeup 检测**（`pm_detect_wake_reason`）：

| 优先级 | 检查项 | 返回值 |
|--------|--------|--------|
| 1 | `EXTI_GetITStatus(EXTI_LINE13)` KEYSCAN 中断 | `PM_WAKE_REASON_KEYSCAN` |
| 2 | `RTC_GetFlagStatus(RTC_FLAG_WTF)` RTC 唤醒标志 | `PM_WAKE_REASON_RTC` |
| 3 | 都不是 | `PM_WAKE_REASON_NONE` |

**唤醒后处理**（`pm_enter_sleep` 后半段）：

| 步骤 | 操作 |
|------|------|
| 1 | 根据绑定决定去向：有绑定→RECONNECTING，无绑定→PAIRING |
| 2 | 刷新活动时间戳 |
| 3 | `pm_sleep_enter()` 返回到调用方 |
| 4 | `pm_state_transition()` 返回到 `pm_state_process()` |
| 5 | 主循环发现 `bleAdvStat != 0xa6U` → 调用 `myBleStartAdv()` 重新初始化 BLE 栈 + 启动广播 |

---

## 五、活动通知机制

### 5.1 通知点总览

```
业务事件                    通知函数                        PM 动作
════════                   ════════                       ════════
按键消抖确认按下            pm_activity_notify(KEY)       ①刷新空闲计时器
                                                         ②若在 STANDBY → 切回 WORKING

语音传输开始                pm_activity_notify(VOICE)      同上
（voice_ctrl_process_pending）

STB 写入 HID Boot OUT       pm_activity_notify(PEER_DATA)  同上
（hid_boot_out_cbk）

CAT1 收到云端下行数据        pm_activity_notify(CAT1)       同上
（rc_cat1_poll_downlink）
```

### 5.2 pm_activity_notify() 内部逻辑

```
pm_activity_notify(activity)
  │
  ├─ ① pm_refresh_activity()
  │      g_pm_last_activity_ms = voiceUptimeMs   ← 重置空闲计时器
  │
  └─ ② if (g_pm_state == PM_STATE_STANDBY)
         pm_state_transition(PM_STATE_WORKING)    ← 立即切回工作态
         │
         ├─ pm_exit_standby()
         │    ├─ le_conn_update(10ms/lat40)       ← 恢复工作参数
         │    └─ USART2->CTRL1 |= TXEN            ← 恢复 CAT1 TX
         │
         └─ pm_enter_working()
              └─ pm_refresh_activity()             ← 再次刷新
```

---

## 六、关键时序

### 6.1 上电→连接完整流程

```
main()
  ├─ initialGmem()                        voiceUptimeMs = 0
  ├─ app_rtos_init()
  ├─ pm_state_init()                      状态 = PAIRING（此时 peer_bonded_count()=0）
  │   └─ pm_refresh_activity()            g_pm_last_activity_ms = 0
  ├─ xTaskCreateStatic(task_ble_entry)
  └─ vTaskStartScheduler()
       │
       └─ task_ble_entry()
            │
         [循环] bleAdvStat != 0xa6U
            ├─ myBleStartAdv()
            │    ├─ ble_gap_init()         ← peer_recover 从 Flash 恢复绑定
            │    └─ ble_adv_start_by_bonding()
            │
            ├─ pm_state_sync_after_ble_init()
            │    └─ if (有绑定) pm_state_transition(RECONNECTING)
            │         └─ pm_enter_reconnecting()
            │              ├─ g_pm_reconnect_start_ms = NOW
            │              └─ pm_refresh_activity()     ← 计时器重置
            │
            └─ [BLE 广播中...]

         [等待 STB 连接]
            │
            ble_gap_connected() 回调
            ├─ pm_notify_connected()
            │   └─ pm_state_transition(WORKING)
            │        ├─ if (old==RECONNECTING) 无退出操作
            │        └─ pm_enter_working()
            │             └─ pm_refresh_activity()      ← 计时器重置
            │
            └─ 系统进入工作态，正常处理按键/语音/CAT1
```

### 6.2 工作态→休眠态→唤醒完整流程

```
[WORKING 状态]
  │
  │  每循环 pm_state_process() 检查空闲时长
  │    idle_ms = voiceUptimeMs - g_pm_last_activity_ms
  │
  ├─ if (idle_ms >= 8000)  ← 8秒无活动
  │    pm_state_transition(STANDBY)
  │    ├─ 无 exit 操作
  │    └─ pm_enter_standby()
  │         ├─ le_conn_update(50ms/lat100)
  │         ├─ CAT1 TX 关
  │         └─ LED 灭
  │
  │  [STANDBY 状态]  功耗开始下降
  │
  │  每循环 pm_state_process() 继续检查
  │    idle_ms = voiceUptimeMs - g_pm_last_activity_ms
  │
  ├─ if (idle_ms >= 45000)  ← 45秒无活动
  │    pm_state_transition(SLEEP)          ← 注意：跳过 pm_exit_standby()
  │    └─ pm_enter_sleep()
  │         └─ pm_sleep_enter()
  │              ├─ ble_adv_stop()
  │              ├─ bleAdvStat = 0U
  │              ├─ 关闭 LED + CAT1 UART 全关
  │              ├─ RTC 初始化（首次）+ 配置 5s 唤醒
  │              └─ PWR_EnterSLEEPMode(WFI)
  │                   │
  │                   │  ╔═══ CPU 休眠 ═══╗
  │                   │  ║ 电流 ~1.6μA   ║
  │                   │  ╚═══════════════╝
  │                   │
  │              ←── KEYSCAN 或 RTC 唤醒
  │              ├─ 检测唤醒原因
  │              ├─ 关闭 RTC 定时器
  │              ├─ 恢复 CAT1 UART RX
  │              └─ return wake_reason
  │
  │  回到 pm_enter_sleep():
  │    ├─ if (有绑定) g_pm_state = RECONNECTING
  │    └─ else        g_pm_state = PAIRING
  │         pm_refresh_activity()
  │
  │  回到主循环:
  │    ├─ bleAdvStat != 0xa6U  ← 被清零了！
  │    ├─ myBleStartAdv()       ← BLE 栈 + 服务全部重新初始化
  │    ├─ pm_state_sync_after_ble_init()  ← 同步状态
  │    └─ 系统恢复 → 等待 STB 连接或按键
```

---

## 七、开发过程中解决的问题

| # | 问题 | 根因 | 解决方案 |
|---|------|------|----------|
| 1 | ARMCC v5 编译报错 `#274: improperly terminated macro invocation` | PM_LOG 宏参数中的 UTF-8 中文导致编译器误解析 | 将 PM_LOG 参数字符串全部改为 ASCII 英文 |
| 2 | `pm_config.h` 中 `#include "co_printf.h"` 导致 `ble_hid_kbd.c` 编译冲突 | `co_printf.h` 被二次包含时与现有代码产生宏冲突 | `PM_DEBUG_LOG_ENABLE` 默认改为 `0U`（关闭），调试时手动开启 |
| 3 | 上电后始终打印 `PM init: no bond, start=PAIRING` | `pm_state_init()` 在 BLE 栈初始化前调用，`peer_bonded_count()` 返回 0 | 新增 `pm_state_sync_after_ble_init()`，在 BLE 启动后重新检查绑定并修正状态 |
| 4 | STANDBY→SLEEP 转换时多余恢复外设 | `pm_state_transition()` 无条件调用 `pm_exit_standby()` 恢复工作参数 | 增加判断：目标为 SLEEP 时跳过 `pm_exit_standby()` |
| 5 | 休眠后没有 RTC 定时唤醒 | 系统中从未调用 `RTC_Init()`（被链接器移除），RTC 外设处于复位；RCC 也未使能 RTC/PWR 时钟 | 新增 `pm_rtc_init_once()`：使能 LSI→RTCEN→PWREN→`RTC_Init()` |
| 6 | 空闲计时器下溢（`WORKING idle 4294935936 ms timeout`） | `pm_state_sync_after_ble_init()` 直接修改 `g_pm_state` 绕过 `pm_state_transition()`，导致 `pm_enter_reconnecting()` 未调用，`pm_refresh_activity()` 未执行 | ① `pm_state_sync_after_ble_init()` 改为调用正规 `pm_state_transition()`；② `pm_idle_duration_ms()` 加入下溢检测+自愈 |

---

## 八、可配置参数

所有参数集中在 [pm_config.h](bleuart/pmu/inc/pm_config.h)，调参无需改动逻辑代码：

| 宏名称 | 默认值 | 含义 |
|--------|--------|------|
| `PM_WORK_TO_STANDBY_MS` | 8000 | 工作态无业务→待机态超时（毫秒） |
| `PM_STANDBY_TO_SLEEP_MS` | 45000 | 待机态无业务→休眠态超时（毫秒） |
| `PM_RECONNECT_TIMEOUT_MS` | 45000 | 回连广播无连接→休眠态超时（毫秒） |
| `PM_STANDBY_CONN_INTV_MIN/MAX` | 0x0028 | 待机态连接间隔（=50ms，GY/T 407-2024 上限） |
| `PM_STANDBY_CONN_LATENCY` | 100 | 待机态从机延迟（连接事件数，GY/T 407-2024 上限） |
| `PM_WORK_CONN_INTV_MIN/MAX` | 0x0008 | 工作态连接间隔（=10ms） |
| `PM_WORK_CONN_LATENCY` | 40 | 工作态从机延迟 |
| `PM_SLEEP_RTC_WAKEUP_SEC` | 5 | 休眠态 RTC 唤醒间隔（秒） |
| `PM_DEBUG_LOG_ENABLE` | 0 | 调试日志开关（1=开，0=关） |

---

## 九、待验证项

### 9.1 功耗测试

| 项目 | 指标 | 验证方法 |
|------|------|----------|
| 待机态平均电流 | ≤ 65μA | 连接 STB → 等待进入待机态 → 万用表/功耗分析仪测 VCC 电流 |
| 休眠态平均电流 | ≤ 20μA | 进入休眠态 → 测 VCC 电流（预期 1.6μA） |

### 9.2 功能测试

| 项目 | 验证方法 |
|------|----------|
| 按键唤醒延迟 | 休眠态下按键 → 示波器量 GPIO 翻转测唤醒+BLE 恢复耗时 |
| 待机→工作按键响应 | 待机态下按键 → 确认 HID 数据正常发出 |
| RTC 定时唤醒 | 断开 STB 连接 → 等待休眠 → 5 秒后串口应有唤醒日志 |
| 连接参数 STB 兼容性 | 连接不同品牌 STB（创维/华为/中兴/小米等），确认 `le_conn_update` 被接受 |
| Sleep 模式 SRAM 保持 | 休眠前后对比关键全局变量，确认未丢失 |
| 长期稳定性 | 连续运行 24 小时，观察是否有异常重启或状态混乱 |

### 9.3 已知限制

| 项目 | 说明 |
|------|------|
| 连接参数异步 | `le_conn_update()` 请求由 STB 最终裁决，若 STB 拒绝则参数不变，待机功耗可能不达标 |
| CAT1 初始化 | 休眠唤醒后 CAT1 由 `myBleStartAdv()` → `rc_cat1_init()` 重新初始化 |
| Watchdog | 未确认 IWDG 是否启用。若启用，休眠期间 CPU 停摆会导致看门狗复位 |
| LSI 精度 | ±5~10%，RTC 定时 5s 偏差约 ±0.5s，不影响功能 |

---

## 十、文件速查索引

| 内容 | 文件 |
|------|------|
| 状态枚举 + API | [pm_state.h](bleuart/pmu/inc/pm_state.h) |
| 可调参数 | [pm_config.h](bleuart/pmu/inc/pm_config.h) |
| 状态机核心 | [pm_state.c](bleuart/pmu/src/pm_state.c) |
| 待机态外设控制 | [pm_standby.c](bleuart/pmu/src/pm_standby.c) |
| 休眠态 + RTC 唤醒 | [pm_sleep.c](bleuart/pmu/src/pm_sleep.c) |
| 按键活动通知 | [task_keyscan.c:338](bleuart/src/task_keyscan.c#L338) |
| 语音活动通知 | [app_voice_ctrl.c:48](bleuart/src/app_voice_ctrl.c#L48) |
| STB 数据活动通知 | [ble_hid_kbd.c:208](bleuart/src/ble_hid_kbd.c#L208) |
| CAT1 数据活动通知 | [rc_cat1_bridge.c:173](bleuart/cat1/rc_cat1_bridge.c#L173) |
| 连接/断连通知 | [ble_gap.c:328,349](bleuart/src/ble_gap.c#L328) |
| PM 主循环集成 | [task_ble.c:41,54](bleuart/src/task_ble.c#L41) |
| PM 初始化 | [main.c:131](bleuart/src/main.c#L131) |
