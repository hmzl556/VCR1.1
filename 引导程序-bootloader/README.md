# Bootloader 工程说明（小白版）

本文说明如何在 Keil 里**从零建立** 16KB Bootloader 工程、如何编译、如何烧录，以及它和 APP 的关系。

---

## 一、Bootloader 是干什么的？

可以把 Flash 想象成一条长走廊，里面按顺序放着几段程序：

```
0x01000000  ┌─────────────────┐
            │  Bootloader     │  16KB  上电先跑这里（“门卫”）
0x01004000  ├─────────────────┤
            │  APP1（用户程序）│  200KB  平时运行的 BLE/CAT1/OTA 程序
0x01036000  ├─────────────────┤
            │  APP2（备份区）  │  200KB  OTA 时把新固件先下载到这里
0x01068000  ├─────────────────┤
            │  OTA 配置 4KB    │  升级标志、完成 marker
0x01069000  ├─────────────────┤
            │  BLE 绑定 4KB    │  配对信息
            └─────────────────┘
```

**OTA 升级完整流程（简化）：**

1. 平时：Bootloader 发现“没有待升级标志” → 直接跳进 **APP1** 正常运行。
2. OTA 时：APP 把新固件下载到 **APP2**，校验通过后写标志 `0x12345678`，然后复位。
3. 复位后：Bootloader 发现标志有效 → 把 **APP2 复制到 APP1** → 写“升级完成 marker” → 再跳进新的 APP1。
4. 新 APP 启动：读到 marker → **LED1 闪 3 秒** 表示升级成功。

Bootloader **不会**再次校验 APP2 整包（APP 在步骤 9 已经校验过）。

---

## 二、本目录里有什么？

```
bootloader/
├── README.md          ← 本说明
├── prj/
│   ├── boot.uvprojx   ← 可直接用 Keil 打开的工程（已配好源文件与 scatter）
│   └── boot.sct       ← 链接脚本：程序必须落在 0x01000000，最大 16KB
└── src/
    └── boot_main.c    ← Bootloader 主程序（上电入口，含 co_log_init）
```

**还会复用 APP 工程里已有的文件**（不用复制，Keil 里“添加现有文件”即可）：

| 文件 | 作用 |
|------|------|
| `total-随时更新/bsp/common/src/co_printf.c` | LPUART 日志（115200，PB1 Tx，与 APP 一致） |
| `total-随时更新/bsp/driver/src/gpio.c` | co_printf 依赖 |
| `total-随时更新/bsp/driver/src/lpuart.c` | co_printf 依赖 |
| `total-随时更新/bsp/driver/src/rcc.c` | co_printf 依赖 |
| `total-随时更新/bleuart/cat1/OTA/udp_bootload.c` | Flash 擦写、APP2→APP1 复制 |
| `total-随时更新/bleuart/cat1/OTA/flash_layout.h` | 分区地址宏 |
| `total-随时更新/bsp/driver/src/qflash.c` | Flash 驱动 |
| `total-随时更新/bleuart/prj/startup_br1601x.s` | 芯片启动汇编 |
| `total-随时更新/bleuart/prj/system_br1601x.c` | SystemInit |
| `total-随时更新/cs/src/cs.lib` | 芯片底层库（SystemInitCore、delay 等） |

---

## 三、准备工作

1. 已安装 **Keil MDK5**（与编译 APP 同一版本即可，例如 MDK 5.36）。
2. **APP 工程已能成功编译**  
   路径：`total-随时更新/bleuart/prj/ble_dtss.uvprojx`  
   确认 APP 已链接在 **`0x01004000`**（scatter 里 `ER_IROM1 0x01004000 0x32000`）。
3. 烧录器/工具与平时烧 APP 相同（J-Link、Barrot 下载工具等）。

---

## 四、打开现成工程并编译（推荐）

已提供 **`bootloader/prj/boot.uvprojx`**，无需从零新建。

1. 双击或用 Keil 打开：  
   `VCR1-UDP-bootloader 文件夹/bootloader/prj/boot.uvprojx`
2. 确认左上角 Target 为 **`boot`**。
3. 点击 **Rebuild**（或 F7）。
4. 成功后在 `prj/Objects/` 生成 **`boot.hex`**。
5. 用 **Download（F8）** 下载到芯片（hex 内地址为 `0x01000000`，只写 Boot 区）。

> 日常只改 APP 时，打开 `ble_dtss.uvprojx` 按 F8 即可（APP @ `0x01004000`）。  
> 两个工程各下各的 hex，**不能**用一个工程 F8 同时更新 Boot 和 APP。

### 若需从零新建（可选）

以下为手动建工程步骤，与 `boot.uvprojx` 等价，一般可跳过。

## 五、在 Keil 里手动新建 Bootloader 工程（逐步操作）

### 步骤 1：新建工程

1. 打开 Keil μVision。
2. 菜单 **Project → New μVision Project…**
3. 保存路径选：  
   `VCR1-UDP-bootloader 文件夹/bootloader/prj/`  
   文件名例如：**`boot.uvprojx`**
4. 器件选择：与 APP 一致（工程里为 **Cortex-M0 / ARMCM0**，与 `ble_dtss` 相同即可）。
5. 弹出 “Manage Run-Time Environment” 时，**不要勾选** CMSIS/RTOS 等组件，直接 **Cancel**。

### 步骤 2：添加源码分组与文件

在左侧 **Project** 窗口，对 **Target 1** 右键 → **Manage Project Items…**

建议建 5 个 Group，按下面表格 **Add Files**（路径均相对于 `bootloader/prj/`）：

#### Group: `boot`

| 文件 |
|------|
| `../src/boot_main.c` |

#### Group: `common`

| 文件 |
|------|
| `../../total-随时更新/bsp/common/src/co_printf.c` |

#### Group: `OTA`

| 文件 |
|------|
| `../../total-随时更新/bleuart/cat1/OTA/udp_bootload.c` |

#### Group: `core`

| 文件 |
|------|
| `../../total-随时更新/bleuart/prj/startup_br1601x.s` |
| `../../total-随时更新/bleuart/prj/system_br1601x.c` |

#### Group: `driver`

| 文件 |
|------|
| `../../total-随时更新/bsp/driver/src/qflash.c` |
| `../../total-随时更新/bsp/driver/src/misc.c` |
| `../../total-随时更新/bsp/driver/src/gpio.c` |
| `../../total-随时更新/bsp/driver/src/lpuart.c` |
| `../../total-随时更新/bsp/driver/src/rcc.c` |

#### Group: `cs`

| 文件 |
|------|
| `../../total-随时更新/cs/src/cs.lib` |

> **说明：** `cs.lib` 提供 `SystemInitCore`、`delay_n_10us` 等；`co_printf.c` 提供 `co_printf`/`co_log_init`（`cs.lib` 内部也会调用）；`misc.c` 提供 `assert_param`（qflash 会用到）。Boot 上电会先 `co_log_init()`，串口 **115200 / PB1** 可看到 `[BOOT]` 日志。

**不要**把 APP 的 `main.c`、`udp_ota.c`、FreeRTOS、BLE 等加进 Boot 工程。

### 步骤 3：配置头文件搜索路径（Include Path）

1. 点击工具栏 **魔术棒**（Options for Target）。
2. 打开 **C/C++** 页。
3. 在 **Include Paths** 末尾追加（分号分隔，可直接粘贴）：

```
..\..\total-随时更新\bsp\driver\inc;..\..\total-随时更新\bsp\core\inc;..\..\total-随时更新\bsp\common\inc;..\..\total-随时更新\cs\inc;..\..\total-随时更新\bleuart\cat1\OTA
```

4. 在 **Define** 中填写（与 APP 芯片定义一致）：

```
BR1601A01,USB_STDPERIPH_DRIVER
```

### 步骤 4：配置链接脚本（Scatter，最关键）

1. 仍在 Options for Target → **Linker** 页。
2. 勾选 **Use Memory Layout from Target Dialog** 的 **取消**（即改用 scatter 文件）。
3. **Scatter File** 选择：

```
..\prj\boot.sct
```

   或直接填：`boot.sct`（若工程已在 `prj` 目录下）。

4. `boot.sct` 含义（无需改，除非改分区）：
   - 代码区：`0x01000000`，大小 **0x4000（16KB）**
   - RAM：`0x20004000`，32KB

### 步骤 5：输出 hex 文件

1. **Output** 页 → 勾选 **Create HEX File**。
2. **Output Name** 可改为 **`boot`**（生成 `boot.hex`）。
3. **Listing** 页可勾选 **Linker Map File**（便于查 ROM 是否超过 16KB）。

### 步骤 6：编译

1. 点击 **Rebuild**（全部重新编译）。
2. 期望结果：**0 Error(s)**。

#### 编译成功后检查 ROM 大小

打开 `prj/Listings/boot.map`（名称随 Output Name），搜索 **Total ROM Size** 或看 **ER_IROM1** 使用量：

- **必须 ≤ 16KB（0x4000）**，否则 Bootloader 会侵占 APP1 区域，烧录失败或运行异常。

#### 常见编译/链接错误

| 现象 | 处理 |
|------|------|
| `SystemInitCore` 未定义 | 确认已添加 `cs.lib` |
| `delay_n_10us` 未定义 | Linker → Misc 含 `symdefs.obj`；`boot_main.c` 含 `co_printf.h` |
| `co_printf` / `LPUART_*` 未定义 | 确认已添加 `co_printf.c` 及 `gpio.c`、`lpuart.c`、`rcc.c` |
| `assert_param` 未定义 | 确认已添加 `misc.c` |
| `FLASH_SECTOR_SIZE` 等未定义 | 检查 Include Path 是否含 `bsp\driver\inc` |
| ROM 超过 16KB | 不要加入 BLE/FreeRTOS/OTA 状态机等 APP 文件 |

---

## 六、烧录顺序与地址（非常重要）

Flash 里 **Boot 和 APP 地址不同**，必须分两次（或合并 hex 时严格按地址）烧录。

| 次序 | 文件 | 烧录地址 | 说明 |
|------|------|----------|------|
| **1** | `boot.hex` | **`0x01000000`** | 先烧 Bootloader |
| **2** | `ble.hex`（APP 编译产物） | **`0x01004000`** | 再烧用户程序 |

### 操作要点

- 使用 J-Link / 厂家工具时，加载 hex 后确认 **Start Address**：
  - Boot：`0x01000000`
  - APP：`0x01004000`
- **不要**把 APP 烧到 `0x01000000`，否则会覆盖 Bootloader。
- **首次**部署 OTA 方案：建议 **全片擦除** 或按上述顺序各烧一次，避免旧 APP（链接在 `0x01000000`）残留。

### 仅更新 APP、不改动 Boot 时

日常开发只改 APP：只烧 **`ble.hex` → `0x01004000`** 即可，**不必**每次重烧 Boot。

### 仅更新 Boot 时

只烧 **`boot.hex` → `0x01000000`**，注意不要误擦 APP 区（视烧录工具选项而定）。

---

## 七、Bootloader 运行逻辑（对照代码）

对应 `src/boot_main.c` + `udp_bootload.c`：

| 步骤 | 动作 |
|------|------|
| 1 | 上电，`Qflash_Init()` |
| 2 | 读 `0x01068000` 的 `BOOTLOAD_PARA` |
| 3 | 若 `prog_success_flag != 0x12345678` → **直接跳转 APP1**（`0x01004000`） |
| 4 | 将 APP2（`0x01036000`）复制到 APP1（`0x01004000`），最多重试 10 次 |
| 5 | **复制成功后**擦除 OTA 配置扇区 `0x01068000`（4KB），清除升级 flag |
| 6 | 在 `0x01068100` 写入完成 marker **`0x12345678`** |
| 7 | 跳转 APP1；新 APP 里 `udp_ota_boot_indicate_poll()` 让 **LED1 闪 3 秒** |

复制失败 → **保留 flag**，下次上电 Boot 重试；仍跳转当前 APP1。marker 失败 → 跳转 APP1（新固件已在 APP1，仅 LED 指示可能缺失）。

---

## 八、如何验证 Boot 是否工作？

### 7.1 不上 OTA，仅验证“能跳进 APP”

1. 烧 Boot + 烧 APP。
2. 上电：设备应正常广播、CAT1、BLE 等功能与仅 APP 时一致。
3. 此时 Boot 每次发现无升级标志，瞬间跳转 APP，**感觉不到 Boot 存在**。

### 7.2 验证 OTA 复制（完整流程）

1. Boot @ `0x01000000` + APP @ `0x01004000` 已烧录。
2. 用 UDP OTA 工具或平台完成一次升级（APP 下载到 APP2 → 校验 → 复位）。
3. 复位后应看到：
   - 新程序功能正常；
   - **LED1 以约 500ms 周期闪烁约 3 秒**（升级成功指示）。

联调脚本见仓库：`tools/ota_platform_sim.py`（需 PC 与设备 UDP 互通）。

---

## 九、与 APP 工程的关系（避免混淆）

| 项目 | 工程文件 | 链接地址 | 产物 |
|------|----------|----------|------|
| **Bootloader** | `bootloader/prj/boot.uvprojx` | `0x01000000`，≤16KB | `boot.hex` |
| **用户 APP** | `total-随时更新/bleuart/prj/ble_dtss.uvprojx` | `0x01004000`，≤200KB | `ble.hex` |

两个工程 **互相独立编译**，通过 Flash 分区衔接；**源码仅共用** `udp_bootload.c`、`flash_layout.h`、BSP 驱动等，**不要**把两个工程合成一个 Target。

---

## 十、目录路径速查（复制用）

假设工程文件在：`bootloader/prj/boot.uvprojx`

**Include Path：**

```
..\..\total-随时更新\bsp\driver\inc;..\..\total-随时更新\bsp\core\inc;..\..\total-随时更新\bsp\common\inc;..\..\total-随时更新\cs\inc;..\..\total-随时更新\bleuart\cat1\OTA
```

**Scatter：**

```
boot.sct
```

**Define：**

```
BR1601A01,USB_STDPERIPH_DRIVER
```

---

## 十一、遇到问题怎么办？

1. **编译错误**：把 Keil **Build Output** 全文复制保存，对照第四节“常见错误”表。
2. **烧录后无反应**：检查 APP 是否烧在 **`0x01004000`**，Boot 是否在 **`0x01000000`**。
3. **OTA 后仍跑旧程序**：确认 Boot 已烧录；确认 APP 步骤 9 已写 `0x12345678`；用读 Flash 工具看 APP1 内容是否变化。
4. **LED 不闪 3 秒**：确认 Boot 复制成功后写了 `0x01068100` marker；确认 APP 里 `task_ble.c` 调用了 `udp_ota_boot_indicate_poll()`。

---

## 十二、相关文档

- APP OTA 协议与状态机：仓库内 `OTA升级协议文档-0618更新.pdf`
- APP 集成说明：计划文件 `udp_ota_远程备份_81645972.plan.md`
- Python 联调：`tools/ota_platform_sim.py`
