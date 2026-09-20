# 03 HID 与按键扫描 — 复盘笔记

> 代码：[ble_hid_kbd.c](bleuart/src/ble_hid_kbd.c)（HID over GATT 服务）、[app_keyscan.c](bleuart/src/app_keyscan.c)（按键硬件层）、[task_keyscan.c](bleuart/src/task_keyscan.c)（按键业务层状态机）

## 一句话定位

遥控器的核心业务链路：**按键矩阵 → 消抖/长按状态机 → 键码映射 → HID 报告 → BLE notify → 机顶盒**。HID 服务表（HOGP）是手动搭的 GATT 表，键值映射对标国标 GY/T 407-2024 的机顶盒遥控器标准键值。

## 一、数据链路（按键到 STB 的完整路径）

```
按键矩阵(44键) → KEYSCAN 硬件外设(自动扫描+DTS硬件消抖+WTS) → EXTI13 中断
  → ISR 查 keyscan_map 表 → key_receive 全局变量
  → task_keyscan_process 状态机(IDLE→DEBOUNCE 20ms→PRESSED)
  → hid_kbd_map_key_to_usage(键号→HID Usage) → 组装 8 字节报告
  → gatts notify(Report Input 0x2A4D 优先 / Boot Input 0x2A22 兜底) → STB
```

## 二、手写的 HID 服务表（HOGP / HID over GATT）

HID Service 0x1812，按规范顺序手动用 `gatts_chr_add` 创建：

| 特征 | UUID | 属性 | 作用 |
|---|---|---|---|
| HID Information | 0x2A4A | READ | bcdHID/国家码/标志（远程唤醒能力声明） |
| HID Report Map | 0x2A4B | READ | 报告描述符，**支持 offset 分段读取**（描述符 >ATT_MTU 时分多次读） |
| HID Protocol Mode | 0x2A4E | READ+WRITE | Boot(0)/Report(1) 模式切换，由主机写入 |
| HID Control Point | 0x2A4C | WRITE | 主机发 Suspend/Exit Suspend |
| Report Input ×2 | 0x2A4D | READ+NOTIFY+CCCD | 键盘报告 + Consumer 报告，各挂 CCCD + Report Reference |
| Boot Keyboard Input | 0x2A22 | READ+NOTIFY+CCCD | Boot 模式输入报告（兼容 BIOS/无驱动环境） |
| Boot Keyboard Output | 0x2A32 | READ+WRITE+W/O RSP | 主机下发 LED 状态（NumLock 等） |

**Report Map 三种报告**：
- **ID=1 键盘**：8 字节 = 修饰键位图(1B) + 保留(1B) + **6 键数组**（NKRO 6 键同按）；
- **ID=2 Consumer**：1 字节位图，bit0=Mute / bit1=Vol+ / bit2=Vol-；
- **ID=3 厂商自定义**：20 字节（预留）。

⚠️ 诚实性提示：代码注释明确写着 Report Map 描述符"从参考工程原封不动复制、当前阶段不要改动"——**面试时别声称手写描述符**，但要能逐行读懂（描述符每个字节的含义见文件内注释，已被详细注释过）。

## 三、按键扫描层（硬件层 + 业务层）

### 硬件层（app_keyscan.c）
- BR1601 KEYSCAN 外设：**8 个 IO（PA0-3,6 + PB8-10）支撑 44 键矩阵**，开漏复用 AF5；
- 硬件消抖：DTS_10MS（消抖时间）+ WTS_32MS（扫描时间），自动扫描模式（MODE_FIXED_INTV）；
- 按键变化触发 EXTI13 中断 → ISR 内读键值查表 → 置 key_receive 上报主循环（**ISR 只做轻活**，重活交给主循环）；
- 低功耗扫描模式预留（MODE_PRESS_TRIG：按键触发扫一轮后回休眠，20 键）；
- ISR 里处理休眠唤醒瞬间亮 LED1（g_pm_sleeping 标志，先恢复 GPIOB 时钟再点灯）。

### 业务层（task_keyscan.c）
- 状态机：IDLE → DEBOUNCE(20ms) → PRESSED；消抖期间键值乱跳则重新计时；释放判定（无键 + 20ms 无事件）；
- **短按即发**：消抖完成立刻发按下报告（延迟最小化）；释放发全 0 报告；
- **长按 50ms 仅触发一次**：KEY15 语音键长按启动语音采集、松开停止；
- 电源键 KEY1 + 回连态 → 切唤醒广播；
- **工厂复位状态机**（KEY8+KEY14 长按 3s → PENDING 窗口 LED2 快闪 10s → 松手后按 KEY4 确认/按其他取消 → 擦绑定 + 进休眠），OTA 期间忽略；
- **组合键互斥调度**：工厂复位空闲时简单组合键（配对/解配）才工作，反之亦然；
- **BLE/CAT1 串行化**：CAT1 的 AT 命令繁忙时 HID 按下/释放报告全部延后（s_hid_at_defer），AT 结束后补发——单核系统里避免 UART 收包窗口内发 BLE notify 拖垮串口消费；
- 断连时 `task_keyscan_reset_on_ble_disconnect()` 重置全部状态（防残留状态干扰下次连接）。

## 四、与 PM 状态机 / 其他模块的联动

- 消抖完成 → `pm_activity_notify(PM_ACTIVITY_KEY)` → 待机态**秒回工作态**；
- STB 写 Boot Output（LED 状态）→ `pm_activity_notify(PM_ACTIVITY_PEER_DATA)` → 对端数据也算"活动"；
- `g_last_ble_ms` 记录 BLE 最后活动时间 → task_ble 主循环据此做 BLE/CAT1 错峰调度（距上次 BLE 活动 ≥50ms 才开 CAT1 新业务）；
- KEY15 的语音 GATT 通知与 RC-KEY notify 错开发送（先让语音 chr indicate 入队，再发 RC notify）。

## 五、面试考点（自测题）

1. HOGP 服务表有哪些特征？每个的作用？（背上面的表）
2. Report Map 为什么支持 offset 分段读取？GATT 长属性读取流程？
3. Boot 模式 vs Report 模式的区别？为什么遥控器要保留 Boot 模式？
4. 键盘 8 字节报告格式？6 键 NKRO 怎么实现？修饰键位图？
5. Consumer 报告和键盘报告的区别（16-bit Usage、位图 vs 数组）？
6. CCCD 的机制？为什么 STB 不写 CCCD 就发不出 notify？force enable 是解决什么兼容性问题？
7. 按键消抖怎么做？硬件消抖（DTS/WTS）和软件消抖（20ms）各管什么？
8. 为什么键值要映射成 HID Usage 而不是自定义编码？（国标 GY/T 407-2024 定义机顶盒标准键值；跨设备通用性）
9. 8 个 IO 怎么支撑 44 键？矩阵键盘原理？（行/列交叉点短路检测，开漏+上拉）
10. 长按、短按、连击怎么区分？本实现各是什么行为？
11. BLE 和 CAT1 为什么要串行化？延后 HID 的设计解决什么问题？
12. ISR 里为什么只读键值不做处理？（中断里做重活会阻塞 BLE 栈/其它中断）
13. 按键扫描从"持续扫描"改成"按键触发扫一轮"能省多少电？（低功耗扫描模式，与休眠态配合）

## 六、与硬件岗的关联点

- 矩阵键盘原理与消抖（软件+硬件双重消抖）；
- KEYSCAN 外设的功耗：自动扫描模式持续耗电 vs 触发模式 + 休眠（这就是低功耗状态机里"按键唤醒"的硬件基础，EXTI13）；
- 开漏输出的矩阵扫描为什么用开漏（多个按键同时按下时避免输出冲突/短路）；
- LED 指示电路（两个 LED 表达连接/配对/复位状态）。
