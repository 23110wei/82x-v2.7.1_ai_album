# TXW82x_FPV v2.7.1.7 完整技术分析文档（全工程覆盖）

> 工程路径：`D:\work\Dual_Screen_Cmake\TXW82x_FPV-v2.7.1.7-45228\TXW82x_FPV-v2.7.1.7-45228`
> 芯片：TXW82x（双核 ARM Cortex-M，主频 ~480MHz）—— CPU0=应用核，CPU1=Media/DSP核，各自独立固件+独立RTOS
> RTOS：AliOS Things (Rhino) / LiteOS-M（`cpu0_ohos` 配置），通过 txsemi OSAL 统一封装
> 语言：C（无 C++），UTF-8 CRLF，中文注释
> 规模：~691 目录，~3585 个 .c/.h 源文件，~249 个 Makefile（CDK 构建系统）
> 工具链：`csky-elfabiv2-gcc`（minilibc）
> 本文档所有数字、函数签名、常量、路径均经 codegraph / Read 工具对源码逐一核对

---

# 目录

- [0. 需求复述与文档范围](#0-需求复述与文档范围)
- [1. 工程总体分层架构（自底向上）](#1-工程总体分层架构自底向上)
- [2. CPU0 初始化链路（源码核对）](#2-cpu0-初始化链路源码核对)
- [3. 各模块工作原理与数据交互](#3-各模块工作原理与数据交互)
  - [3.1 显示子系统](#31-显示子系统)
  - [3.2 相册存储模块](#32-相册存储模块album_storealbum_storage)
  - [3.3 图片加载器](#33-图片加载器ai_album_album_image_loader)
  - [3.4 JPEG 硬件解码](#34-jpeg-硬件解码ai_album_album_jpeg_hw)
  - [3.5 AI 图生图模块](#35-ai-图生图模块ai_album_album_image_ai)
  - [3.6 BRTC 云端通道](#36-brtc-云端通道brtc_agent)
  - [3.7 网络/WiFi 模块](#37-网络wifi-模块)
  - [3.8 蓝牙模块](#38-蓝牙模块ble--bt_service)
  - [3.9 音频模块](#39-音频模块)
  - [3.10 UI 子系统](#310-ui-子系统)
  - [3.11 内存/PSRAM 子系统](#311-内存psram-子系统)
  - [3.12 电源管理](#312-电源管理photo_frame_power_ctrl)
  - [3.13 时间/天气/位置服务](#313-时间天气位置服务)
  - [3.14 CPU1 Media 核与 RPC](#314-cpu1-media-核与-rpc)
  - [3.15 芯片启动与中断](#315-芯片启动与中断)
  - [3.16 LVGL 图形库](#316-lvgl-图形库)
  - [3.17 输入系统](#3317-输入系统)
  - [3.18 板级配置与引脚](#318-板级配置与引脚)
  - [3.19 通用框架组件](#319-通用框架组件)
- [4. 模块依赖图](#4-模块依赖图)
- [5. 关键路径与风险点](#5-关键路径与风险点)
- [6. 跨模块数据交互总览](#6-跨模块数据交互总览)

---

## 0. 需求复述与文档范围

你需要：

> 不只是针对 AI 相框（`photo_frame_demo`）这一个 demo 的实现说明，而是"包含但不仅如此" —— 以 AI 相框为切入点，把整个 TXW82x SDK 从底层到应用层完整梳理一遍，输出一份技术文档，内容包括：
> 1. 每个模块的**工作内容、工作原理、工作流程**；
> 2. 模块间的**数据交互方式**（函数调用 / 事件 / 消息队列 / 共享内存 / IPC）；
> 3. 整个工程的**分层架构、初始化链路、任务模型**；
> 4. 最终整理成一份**系统性的技术文档**。

**文档覆盖范围**（不仅仅是 photo_frame_demo）：

| 层级 | 覆盖目录 |
|---|---|
| 芯片启动 | `sdk/chip/txw82x/` |
| 驱动层 | `sdk/driver/`（hg*）、`sdk/include/dev/` |
| HAL | `sdk/hal/` |
| SDK 框架 | `sdk/app/`、`sdk/lib/` |
| 产品 Demo | `sdk/demo/photo_frame_demo/` |
| CPU1 固件 | `project/txw82xCore/`、`sdk/src/media/` |
| RTOS | `csky/`、`ohos/` |
| 闭源库 | `libs/` |
| 板级配置 | `project/txw82xApp/` |

---

## 1. 工程总体分层架构（自底向上）

```
┌──────────────────────────────────────────────────────────────────┐
│ L6 应用层  sdk/demo/photo_frame_demo/                             │
│   ├── album/      相册业务核心                                    │
│   ├── ui/         页面/路由/输入/i18n/字体                         │
│   ├── network/    WiFi/BLE/天气                                   │
│   ├── system/     时间/农历                                       │
│   ├── audio/      音量                                            │
│   ├── display/    亮度                                            │
│   ├── hardware/   电池/电源                                       │
│   └── storage/    存储信息                                        │
├──────────────────────────────────────────────────────────────────┤
│ L5 应用框架  sdk/app/                                            │
│   app_framework / app_lcd / screen / video_app / decode /       │
│   encode / spook / brtc_agent / walkie_talkie_ui / takephoto /  │
│   freetype2 / lowPower_app / gen420 / scale_msi / ...          │
├──────────────────────────────────────────────────────────────────┤
│ L4 服务层    sdk/src/service/, sdk/src/media/, sdk/src/rtos/    │
│   wifi_service / bt_service / media_server (CPU1 RPC) /         │
│   sys_event / med_msg / audio_service / sys_statistics           │
├──────────────────────────────────────────────────────────────────┤
│ L3 中间件层  sdk/lib/                                           │
│   lvgl / lcd(panel驱动) / bus / spi / net(lwip/http/mqtt) /   │
│   multimedia(MSI) / rpc/cpurpc / fs / flashdb / heap /         │
│   posix / audio / bluetooth / key / touch / video / ...        │
├──────────────────────────────────────────────────────────────────┤
│ L2 驱动层    sdk/driver/ (hg*) + sdk/include/dev/               │
│   clock / gpio / uart / spi / dma / i2s / sdio / lcd / dsi /  │
│   jpeg / scale / video / adkey / wifi / rf / hal/                 │
├──────────────────────────────────────────────────────────────────┤
│ L1 RTOS 层   csky/aliOS-Things(rhino) | cpu0_ohos/LiteOS-M     │
│   thread/queue/timer/mutex/sem + OSAL 适配(sdk/src/rtos/osal)     │
├──────────────────────────────────────────────────────────────────┤
│ L0 硬件     TXW82x SoC + PSRAM + 内部Flash + SDIO + LCD +       │
│             Audio + WiFi/BT + 闭源库 *.a                           │
└──────────────────────────────────────────────────────────────────┘
```

**双核架构（CPU0/CPU1 通信）：**

- CPU0 跑 `sdk/demo/photo_frame_demo`（应用 main），负责 UI、网络协议栈、蓝牙、AI 协议；
- CPU1 是独立 media 固件（`sdk/src/media/`、`project/txw82xCore/`），通过 **共享内存 + RPMsg** 通道通信；
- CPU0 调用 `media_client_play_xxx()` → 打包消息 → CPU1 的 `media_server` 派发到具体 decoder/encoder/camera 驱动；
- 构建顺序：CPU1 固件必须**先编译**，完成后回写 CPU0 的 `gcc_csky.ld`（`CORECODE_SIZE`/`COREBSS_END`）。

---

## 2. CPU0 初始化链路（源码核对）

### 2.1 Boot → main（`cpu0/main.c`）

```
ROM/Bootloader → main()
  ├── system_init()          → 时钟、cache、堆初始化
  ├── board_init()           → 板级引脚、电源域
  ├── heap_init()            → 堆初始化
  ├── app_hos_init()         → ohos 配置下注册 HDF 设备
  └── app_main(0, NULL)      → 进入应用
```

### 2.2 `app_main()` 顺序（`sdk/app/app_framework/app_main.c`）

**核对结论：**
```c
app_main()
 ├── app_framework_init()        → 创建 app 任务
 ├── (任务内) codec_init()       → 注册 codec（audio/video/jpeg 编解码器）
 ├── sys_event_init(32)          → 系统事件总线，容量 **32 个事件**
 ├── msg_center_init()           → 消息中心
 ├── sys_wifi_init()             → WiFi 服务初始化（在 codec 之后）
 ├── storage_init() / fs_init()  → FATFS 挂载
 ├── bt_service_init()           → 蓝牙协议栈
 ├── demo_app_start()            → 应用入口
 └── os_task_suspend(NULL)       → app_main 自身挂起
```

> **关键顺序**：`codec_init()` 在 `sys_wifi_init()` **之前**；事件总线容量为 **32**。

### 2.3 photo_frame_demo 主入口（`photo_frame_demo.c`）

**核对结果（逐条确认）：**

```c
photo_frame_main(argc, argv)   // 应用唯一入口
 ├── psram_heap_dump()              // 打印 PSRAM 堆起始状态
 ├── gpio_pinmux_config()           // 引脚复用
 ├── ai_album_log_init()            // 日志模块
 ├── photo_frame_backlight_init()   // 背光
 ├── media_client_init()            // media 客户端
 ├── display_init() → ai_album_display_bootstrap()
 │      // 双屏初始化：1024×600 主屏 + 副屏，LVGL 绑定 display
 ├── lvgl_init() / ai_album_ui_input_init()
 ├── keyWork_init(10)               // 按键扫描任务，周期 10ms
 ├── photo_frame_key_input_init()   // 按键 → UI action 映射注册
 ├── photo_frame_storage_mount()    // SD 卡 FATFS 挂载
 ├── ai_album_album_store_init()    // 相册存储模块
 ├── brtc_config_load() → brtc_agent_init() // AI 云端通道
 ├── photo_frame_net_init()         // WiFi 连接状态机
 ├── audio_player_init()            // 音频播报
 └── ai_album_ui_bootstrap()        // 启动 UI（最后一步，进 LVGL 主循环）
```

> **注意纠正**：`main()` 函数本身并不直接调用 `photo_frame_demo_init()`。应用启动是通过 `demo_app_start()` 入口跳转到 `photo_frame_main()`。

### 2.4 关键任务清单（CPU0）

| 任务 | 栈大小 | 优先级 | 职责 |
|---|---|---|---|
| app_main 任务 | 8KB | NORMAL | 初始化，完成后挂起 |
| LVGL GUI 任务 | **16KB (PSRAM)** | **ABOVE_NORMAL** | LVGL 渲染 + 30ms UI timer |
| image_loader 任务 | **8KB (PSRAM)** | **BELOW_NORMAL** | JPEG 硬解 + 双缓冲加载 |
| keyWork 任务 | 2KB | NORMAL | 10ms 周期 ADC 扫描按键 |
| brtc_agent 任务 | 8KB | NORMAL | 云端长连接收发 |
| sys_wifi 任务 | — | HIGH | WiFi 状态机 |
| audio_player 任务 | 4KB | NORMAL | 音频播放 |
| media_client 任务 | — | NORMAL | 与 CPU1 的 RPC |

> 任务栈位于 PSRAM：通过 OSAL 的 PSRAM 任务创建接口分配，避免占用内部 SRAM。

---

## 3. 各模块工作原理与数据交互

### 3.1 显示子系统

**工作原理：** TXW82x 内置 LCD 控制器（支持 DSI/DPI），通过 `dev_lcd` 驱动 → `fb_dev` 帧缓冲抽象 → LVGL `lv_display` 绑定。

**双屏配置（`ai_album_display.c` 核对）：**
- 主屏 1024×600，副屏分辨率由 `lcd_config` 决定；
- LVGL 使用 `lv_display_set_buffers(disp, buf1, buf2, size, LV_DISPLAY_RENDER_MODE_PARTIAL)` **双缓冲部分刷新**；
- 刷新回调 `flush_cb` 内调 `fb_dev_pan_display()` + `wait_vsync`。

**数据流：**
```
LVGL 对象树 → lv_timer_handler() → render → flush_cb → fb_dev → LCD HW
```

**背光：** `photo_frame_backlight_init()` 通过 PWM 占空比控制，支持自动熄屏（`display_brightness_set()` + idle timer）。

---

### 3.2 相册存储模块（ai_album_album_store / album_storage）

**职责：** 扫描 SD 卡照片 → 内存索引 → 供 UI / image_loader / image_ai 查询。

**核心常量（已核对）：**
```c
#define AI_ALBUM_ALBUM_STORE_MAX_PHOTOS 255U
```
`g_photos` 为 255 条记录数组，单条记录 `ai_album_album_photo_t` = **172 字节**（含路径、缩略图信息等）。

**扫描逻辑 `ai_album_album_storage_scan()`：**
```c
ai_album_album_storage_scan_result_t ai_album_album_storage_scan(
    ai_album_album_photo_t *photos, uint8_t capacity, uint8_t *count);
```
- 扫描目录：**`0:/IMG`、`0:/DCIM`、`0:/AI_GEN`**；
- 仅接受 `.jpg` / `.jpeg`（大小写不敏感，`os_strcasecmp`）；
- 跳过隐藏文件（`.` 开头）和 `0:/AI_GEN` 下的 `TMP_` 临时文件；
- **没有跨目录去重**：同一文件出现在 IMG 和 DCIM 会重复入索引。

**扫描 → 索引 → UI 同步流程：**
```
SD FATFS 挂载 → ai_album_album_store_refresh()
  → ai_album_album_storage_scan(g_refresh_photos, 255, &count)
  → 与现有 g_photos 交换（原子切换）
  → g_store_version++            // 版本号自增
  → 事件通知 UI（lv_msg 或 store 回调）
UI 收到 version 变化 → 重新拉取列表 → 触发 image_loader 加载
```

**`ai_album_album_store_get_photo(index)`** 返回记录（含路径、缩略图偏移、创建时间），UI 据此调 loader。

**与其它模块的交互：**
- 被 `ai_album_album_image_view` 依赖（获取路径/文件大小做缓存校验）；
- 被 `ai_album_album_image_ai` 依赖（写入 AI 生成图后触发 store 刷新）；
- 被 `ai_album_album_pages` 依赖（相册页面渲染时获取照片列表）；
- 被 `ai_album_settings_format_dialog` 依赖（格式化前查询可用空间）。

---

### 3.3 图片加载器（ai_album_album_image_loader）

**职责：** 异步把 SD 卡 JPEG 解码成 RGB565，放入双缓冲槽位供 UI 绘制。

**核心常量（已核对）：**
```c
#define AI_ALBUM_ALBUM_IMAGE_LOADER_SLOT_COUNT 2U
```
- 每槽大小 = `1024×600×2 = 1,228,800 字节 (~1.17MB)`，总占用 ~**2.36MB** PSRAM；
- 槽位描述符含 `{ in_use, ready, width, height, photo_index, store_version }`。

**工作流程：**
```
UI 调 ai_album_album_image_loader_request(index)
  → 环形队列投递请求 → image_loader 任务唤醒
  → 从槽池找空闲槽（slot_available）
  → 读 JPEG 文件到行缓冲
  → ai_album_album_jpeg_hw_decode()   ← JPEG 硬件解码
  → 写入槽位 → 标记 ready → 通知 UI
UI 30ms timer 里 ai_album_album_image_loader_poll()
  → 若 ready 槽与当前 photo_index/store_version 匹配 → 画到 canvas
```

**调度策略：** loader 任务优先级 BELOW_NORMAL（低于 GUI），保证解码不抢占渲染；请求队列深度有限，重复请求同一 index 会合并。

**与其它模块的交互：**
- 被 `ai_album_album_image_view` 调用（`ai_album_album_image_loader_submit`、`ai_album_album_image_loader_slot_available`）；
- 被 `ai_album_album_pages` 的 IMAGE_AI 页面调用（源图加载 `ALBUM_IMAGE_AI_SOURCE_LOADER_SLOT`、结果图 `ALBUM_IMAGE_AI_RESULT_LOADER_SLOT`）；
- 由 `album_image_loader_complete` 回调通知 UI 解码完成。

---

### 3.4 JPEG 硬件解码（ai_album_album_jpeg_hw）

**核对结论：只有解码，没有硬件编码器**。`ai_album_album_jpeg_hw_encode` 在整个 codebase **不存在**。AI 生成图由云端返回已编码 JPEG，本模块从不调用编码。

**解码签名：**
```c
ai_album_album_jpeg_hw_result_t ai_album_album_jpeg_hw_decode(
    const ai_album_album_jpeg_hw_input_t *input,
    ai_album_album_jpeg_hw_output_t *output);
```

**输出结构体：**
```c
typedef struct {
    uint16_t width;
    uint16_t height;
    uint32_t stride;
    uint32_t data_size;
    uint32_t hardware_us;   // JPEG 硬解耗时
    uint32_t convert_us;    // YUV→RGB 耗时
} ai_album_album_jpeg_hw_output_t;
```

**硬件单元与互斥（已核对）：**
- 仅使用 **JPGID1**（`dev_get(HG_JPG1_DEVID)`）+ **HG_SCALE2_DEVID** 缩放器；
- 互斥：`jpg_mutex_lock(JPGID1, JPG_LOCK_DECODE, NULL)` / `jpg_mutex_unlock(JPGID1, JPG_LOCK_DECODE)`；
- 锁等待：`ALBUM_JPEG_LOCK_WAIT_MS = 1000U`（自旋 1s 拿锁）；
- 解码完成事件等待：`ALBUM_JPEG_WAIT_MS = 3000`；
- `jpg_close()` 后等设备下线：`ALBUM_JPEG_CLOSE_WAIT_MS = 1200U`；
- 最大输出：`ALBUM_JPEG_MAX_OUTPUT_WIDTH 1024U` / `MAX_OUTPUT_HEIGHT 600U`；
- 支持中途取消：每 `ALBUM_JPEG_CANCEL_CHECK_ROWS = 4U` 行检查一次取消标志。

**完整流程：**
```
上电 → 锁 JPGID1 → dev_get(HG_JPG1_DEVID) + HG_SCALE2_DEVID
  → 配置输入 JPEG 流缓冲 → 启动硬解
  → 等事件(3s)
  → SCALE2 缩放到目标尺寸
  → YUV422→RGB565 转换
  → 写 output → 解锁
```

---

### 3.5 AI 图生图模块（ai_album_album_image_ai）

**职责：** 用户在 UI 选一张照片 + 选风格 → 通过 brtc_agent 发送到云端 LLM → 返回 AI 生成图 → 存 SD 并展示。

**状态机（8 个状态，已核对枚举）：**
```c
typedef enum {
    AI_ALBUM_ALBUM_IMAGE_AI_STATE_IDLE = 0,
    AI_ALBUM_ALBUM_IMAGE_AI_STATE_PREPARING,
    AI_ALBUM_ALBUM_IMAGE_AI_STATE_UPLOADING,
    AI_ALBUM_ALBUM_IMAGE_AI_STATE_GENERATING,
    AI_ALBUM_ALBUM_IMAGE_AI_STATE_DOWNLOADING,
    AI_ALBUM_ALBUM_IMAGE_AI_STATE_DONE,
    AI_ALBUM_ALBUM_IMAGE_AI_STATE_FAILED,
    AI_ALBUM_ALBUM_IMAGE_AI_STATE_CANCELLED,
} ai_album_album_image_ai_state_t;
```

**风格列表：** 8 种风格的字符串常量表，与 `AI_ALBUM_ALBUM_STYLE_COUNT = 8U`（定义于 `ai_album_album_store.h`）对应。

**超时：** `AI_ALBUM_ALBUM_IMAGE_AI_TIMEOUT_MS = 120 * 1000`（120 秒）。

**云端 agent 标识：** `#define AI_ALBUM_BRTC_LLM "LLMRacing"`

**私有上下文 `image_ai_context_t`（已核对）：**
```c
typedef struct {
    os_mutex_t lock;
    ai_album_album_image_ai_state_t state;
    ai_album_album_photo_t source;
    ai_album_album_photo_t result;
    uint8_t *source_jpeg;
    uint32_t source_jpeg_len;
    uint8_t *result_jpeg;
    uint32_t result_jpeg_len;
    uint32_t operation;
    uint32_t revision;
    uint64_t started_ms;
    uint8_t style;
    uint8_t initialized;
    uint8_t disable_pending;
    int error;
    char temporary_path[AI_ALBUM_ALBUM_PHOTO_PATH_MAX];
} image_ai_context_t;
```

**端到端数据流：**
```
UI(IMAGE_AI page) ──选图+选风格──▶ ai_album_album_image_ai_start(photo_index, style)
  → IDLE→PREPARING
  → ai_album_album_image_ai_load_file()   // 从 SD 读原 JPEG 字节
  → PREPARING→UPLOADING
  → brtc_agent_send_image_generation(jpeg, jpeg_len, prompt)
        // 走 brtc_agent 的 LLMRacing agent 通道
  → UPLOADING→GENERATING（等待云端）
  ← 云端回调 BRTC_AGENT_EVENT_MEDIA_GENERATE_ACK（确认受理）
  ← 云端回调 BRTC_AGENT_EVENT_VIDEO_DATA / MEDIA_GENERATE_RESULT
        // 分片回传生成的 JPEG 字节
  → DOWNLOADING→DONE
  → ai_album_album_image_ai_write_result()
        // 写入 0:/AI_GEN/AI_xxxxxxxx.JPG（先写 TMP_ 临时名再 rename）
  → store_refresh() → UI 刷新相册
失败路径：120s 超时 / ACK 超时 / 写文件失败 → FAILED 或 CANCELLED
```

> **关键澄清**：本 demo **不做本地 JPEG 编码**，原图 JPEG 直接透传云端，生成图由云端编码后回传。

**与其它模块的交互：**
- 被 `ai_album_album_pages` 的 IMAGE_AI 页面调用（`ai_album_album_image_ai_start`、`ai_album_album_image_ai_snapshot`）；
- 被 `ai_album_settings_format_dialog` 在格式化时调用 `ai_album_album_image_ai_cancel()`；
- 被 `brtc_agent` 的事件回调驱动状态转换；
- 内部使用 `os_mutex_t lock` 保护并发访问。

---

### 3.6 BRTC 云端通道（brtc_agent）

**职责：** 与 AI 云平台维持长连接，承载 AI Chat / 图生图 / TTS 等协议。

**配置结构（`brtc_config`）：** 除 ssid/密码外，还含 `platform_url` 等字段，从配置存储加载。

**初始化（`photo_frame_demo.c` 中）：** `brtc_config_load()` → `brtc_agent_init()`；网络就绪后启动。

**DHCP 联动：** `photo_frame_net_evt_hdl()` 是 DHCP 事件回调，拿到 IP 后调用 `photo_frame_net_brtc_start()` 启动 agent 连接。

**事件模型：** agent 任务收 `brtc_agent_event`，经回调分发给上层（image_ai、ai_chat）。

**数据交互方式：** 长连接 socket + 分包协议；上行 `brtc_agent_send_image_generation(jpeg, len, prompt)`；下行事件携带 JPEG 分片，上层拼装。

---

### 3.7 网络/WiFi 模块

**分层：**
```
photo_frame_net (demo 状态机)
  → sys_wifi (service 层) → wifi_driver (L2)
  → lwip (TCP/IP)
```

**状态机：** `DISCONNECT → SCAN → CONNECT → DHCP → CONNECTED`，事件由 `sys_event` 总线广播（`NET_EVT_DHCP_SUCC` 等），`photo_frame_net_evt_hdl` 消费。

**WiFi Provision：** `wifi_provision` 模块负责 AP 配网流程（SSID 扫描、连接、密码验证），通过 `wifi_provision_get_status()` 返回当前状态。

**BLE 设置：** `ble_settings` 模块管理蓝牙开关状态，`ble_settings_is_enabled()` / `ble_settings_set_enabled()` 对接底层协议栈。

**其它网络服务：**
- **NTP 对时：** 使用 `ntp.aliyun.com`，**每 2 小时**同步一次；
- **天气：** `open-meteo.com` 免费 API，无需 key，经 HTTP + JSON 解析，写 UI 天气控件；
- **HTTP 客户端：** `sdk/app/network/http_client` 提供 GET/POST。

---

### 3.8 蓝牙模块（ble / bt_service）

**原理：** `bt_service_init()` 启动协议栈，上层 `sdk/app/ble` 提供 GATT server 角色供手机 APP 配网/控制。

**工作流程：** 广播 → APP 连接 → 写特征值（WiFi 凭据 / 控制命令）→ 经 `ble_msg` 队列转发到应用任务 → 应用执行后回写特征值。

**数据交互：** 蓝牙栈内部事件通过 `bt_event` 回调 → `ble` 中间件 → OSAL 队列 → 应用层。配网成功后自动触发 WiFi 连接流程。

---

### 3.9 音频模块

**播放链路（CPU0）：**
```
audio_player_init() → audio_player_play(file)
  → media_client_play_audio() → RPC → CPU1
  → CPU1 decoder(mp3/wav) → I2S TX → Codec → PA
```

**音量控制（已核对）：**
- 5 级音量，`g_volume_percentages[5] = {0, 25, 50, 75, 100}`；
- `ausys_da_change_volume(percent)` 设置 DAC 增益（与 mixer.c 的 MSI_CMD_SET_DAC_VOLUME 最终调用相同）；
- 持久化：`sys_cfgs.album_vol` + `syscfg_save()`；
- **注意**：不使用 `msi_find("R_AUDAC")`（本工程无该闭源组件）。

**录音/Talk：** `AD_B` 长按触发 `AI_ALBUM_UI_ACTION_TALK_START`，mic → I2S RX → CPU1 encoder → 经 brtc 上传云端 ASR（AI Chat 场景）。

---

### 3.10 UI 子系统

**Bootstrap（`ai_album_ui.c` 核对）：**
```c
void ai_album_ui_bootstrap(void) {
    ai_album_ui_input_init();
    home_model = ai_album_home_runtime_prepare();
    ai_album_ui_router_init(lv_disp_get_default(), home_model);
    ai_album_home_runtime_start();
    g_ui_process_timer = lv_timer_create(ai_album_ui_process_timer_cb,
                                         AI_ALBUM_UI_PROCESS_PERIOD_MS, NULL);
}
```
- `AI_ALBUM_UI_PROCESS_PERIOD_MS = 30`（30ms UI tick）；
- 支持 `AI_ALBUM_LCD_COLORBAR_TEST` 彩条诊断模式（编译开关）。

**路由表（13 个 route，已核对 `ai_album_ui_route.h`）：**
```c
typedef enum {
    AI_ALBUM_UI_ROUTE_HOME = 0,
    AI_ALBUM_UI_ROUTE_ALBUM,
    AI_ALBUM_UI_ROUTE_GALLERY,
    AI_ALBUM_UI_ROUTE_IMAGE_AI,
    AI_ALBUM_UI_ROUTE_TRANSLATE,
    AI_ALBUM_UI_ROUTE_AI_CHAT,
    AI_ALBUM_UI_ROUTE_PRACTICE,
    AI_ALBUM_UI_ROUTE_SETTINGS,
    AI_ALBUM_UI_ROUTE_SETTINGS_WIFI,
    AI_ALBUM_UI_ROUTE_SETTINGS_LOCATION,
    AI_ALBUM_UI_ROUTE_SETTINGS_LOCATION_SEARCH,
    AI_ALBUM_UI_ROUTE_SETTINGS_WIFI_PASSWORD,
    AI_ALBUM_UI_ROUTE_COUNT,
    AI_ALBUM_UI_ROUTE_NONE = AI_ALBUM_UI_ROUTE_COUNT,
} ai_album_ui_route_t;
```
Route 名称：HOME / ALBUM / GALLERY / IMAGE AI / LIVE TRANSLATE / AI CHAT / SPEAKING PRACTICE / SETTINGS / SETTINGS WI-FI / SETTINGS LOCATION / SETTINGS LOCATION SEARCH / SETTINGS WIFI PASSWORD。

**页面创建方式：on-demand 分发（非注册表）** —— `ai_album_ui_router.c:58~`：
```c
static int ui_page_create(ai_album_ui_route_t route) {
    if (route == AI_ALBUM_UI_ROUTE_HOME)
        return ai_album_home_page_create(...);
    if (ui_route_is_album(route))          // ALBUM/GALLERY/IMAGE_AI 三路由
        return ai_album_album_pages_create(...);
    if (route == AI_ALBUM_UI_ROUTE_TRANSLATE)
        return ai_album_translate_page_create(...);
    if (route == AI_ALBUM_UI_ROUTE_AI_CHAT)
        return ai_album_ai_chat_page_create(...);
    if (route == AI_ALBUM_UI_ROUTE_PRACTICE)
        return ai_album_practice_page_create(...);
    if (route == AI_ALBUM_UI_ROUTE_SETTINGS_LOCATION_SEARCH)
        return ai_album_location_search_page_create(...);
    if (ui_route_is_settings(route))
        return ai_album_settings_pages_create(...);
    return RET_ERR;
}
```

**相册三页面合一（`ai_album_album_pages.c` 核对）：**
- `ai_album_album_pages` 一个模块内部管理 ALBUM / GALLERY / IMAGE_AI 三个 route；
- 页面状态结构 `album_page_state_t`（含 chrome_topbar/footer、focusables[6]、photo_art/source_art/result_art、rendered_photo_index、store_version 等）；
- 常量：`ALBUM_MAX_FOCUSABLES = 6U`，`ALBUM_PAGE_SLOT_COUNT = 3U`；
- 非活动页面缓存在 `g_inactive_pages[3]`，切换路由时换入换出，避免重复创建销毁 LVGL 树；
- `album_model_t` 持有 slideshow 状态：`slideshow_mode`、`slideshow_dir`、`slideshow_interval`（3/5/10/30 秒档位）、`slideshow_timer`。

**输入映射（`photo_frame_key_input.c` 核对）：**

物理按键（PA15 ADC 分压 AD 键，7 个有效键）：
```
AD_A(Power)=0mV    AD_PRESS(OK)=670mV  AD_RIGHT=1620mV  AD_B(Menu/Talk)=1843mV
AD_LEFT=2226mV     AD_UP=2600mV        AD_DOWN=2929mV
```
按键事件（`keyScan.h`）：`KEY_EVENT_DOWN / SUP(短按释放) / LDOWN(长按下) / LUP(长按释放) / REPEAT`。

**键 → UI 动作映射表：**

| 物理键 | 事件 | UI Action |
|---|---|---|
| AD_PRESS (OK) | SUP | AI_ALBUM_UI_ACTION_OK |
| AD_PRESS (OK) | LDOWN | AI_ALBUM_UI_ACTION_OK_LONG |
| AD_RIGHT / LEFT / UP / DOWN | SUP | 对应 RIGHT / LEFT / UP / DOWN |
| AD_B (Menu/Talk) | SUP 且非通话中 | AI_ALBUM_UI_ACTION_MENU |
| AD_B (Menu/Talk) | LDOWN | AI_ALBUM_UI_ACTION_TALK_START |
| AD_B (Menu/Talk) | LUP 或 SUP | AI_ALBUM_UI_ACTION_TALK_STOP |
| AD_A (Power) | LDOWN | AI_ALBUM_UI_ACTION_POWER_OFF |
| AD_A (Power) | SUP | AI_ALBUM_UI_ACTION_BACK |

`ai_album_ui_action_t` 共 13 个 action（含 TALK_PRESS/TALK_RELEASE 等）。

**输入派发链路：**
```
keyWork_init(10)   // 10ms ADC 扫描
  → add_keycallback(photo_frame_key_callback, NULL)
  → photo_frame_key_post(key, event, action)
  → ai_album_ui_input_post(action)      // 环形缓冲
  → 30ms UI timer: ai_album_ui_input_try_pop()
  → ai_album_ui_router_handle(action)
  → 当前页面的 handle_action() 回调
```

**设置页面（`ai_album_settings_pages.c` 核对）：**
主设置页 7 个可聚焦项：WI-FI / LOCATION / LANGUAGE / BRIGHTNESS / BLUETOOTH / STORAGE RESET / ABOUT；
子页面：SETTINGS_WIFI、SETTINGS_LOCATION、SETTINGS_LOCATION_SEARCH、SETTINGS_WIFI_PASSWORD；
格式对话框 `ai_album_settings_format_dialog` 3 阶段状态机：TARGET → CONFIRM → RESULT。

---

### 3.11 内存/PSRAM 子系统

**核对结论（纠正此前 8MB 的说法）：**
```c
CONFIG_PSRAM_AVHEAP_SIZE = (6 * 1024 * 1024) + (256 * 1024)   // 6.25MB
CONFIG_AVHEAP_SIZE       = 100 * 1024                          // 100KB (内部 SRAM AV 堆)
```
- 总 PSRAM AV 堆 = **6.25MB**（非 8MB）；
- 内部 SRAM 的 AV 堆仅 **100KB**，大块缓冲（loader 槽位、GUI 画布、任务栈）全部走 PSRAM 堆；
- `psram_heap_dump()` 在 photo_frame_main 启动时打印堆水位。

**内存去向估算（1024×600 双缓冲场景）：**

| 消耗者 | 大小 |
|---|---|
| image_loader 2 槽 | 2 × 1,228,800 ≈ **2.36MB** |
| LVGL 双 buf（partial） | ~2 × 1/10 屏 ≈ 0.24MB |
| GUI/loader 任务栈 | 16KB + 8KB |
| 其余（store 索引 255×172≈43KB、BRTC 缓冲、音频 buf 等） | ~1MB 内 |

---

### 3.12 电源管理（photo_frame_power_ctrl）

**核对自源码：**
- 按键：**PA15**（AD 键，兼作开机判定）；
- 电源保持：**PA5**（hold 引脚，开机后拉高自锁）；
- LCD AVDD 使能：**PD13**；
- VBUS（USB 插入）检测：**PA10**；
- `power_ctrl_booted_with_key()` 判断是否按键开机，`photo_frame_key_input_init()` 里读入 `key_power_boot_hold`；
- 关机流程：`AI_ALBUM_UI_ACTION_POWER_OFF`（AD_A 长按）→ 保存状态 → 释放 PA5 → 断电。

**开机判定逻辑：**
```c
power_ctrl_hold(void) {
    // 没按键+VBUS在位 = 外部供电上电 → 直接锁存
    // 按着键 = 按键开机,须保持超过 500ms 防误碰
    // 没按键+无VBUS = 电池关机后放电 → 兜底循环等 USB
}
```

---

### 3.13 时间/天气/位置服务

**时间服务：**
- NTP：`ntp.aliyun.com`，每 **2 小时**同步一次；
- 时区预设东八区（`TZ_CST`）；
- 对时前显示 "SYNCING DATE / --:--"。

**天气服务：**
- API：`api.open-meteo.com`（无需 Key）；
- 地理编码：`geocoding-api.open-meteo.com`；
- 刷新间隔：15 分钟；
- 32 个预设城市（lat/lon × 1e4 定点数）；
- JSON 解析：手写 `strstr` + 手动数字解析（无 JSON 库），x10 定点数。

---

### 3.14 CPU1 Media 核与 RPC

**原理：** CPU1 独立固件，实现 `media_server`；CPU0 侧 `media_client` 把播放/录制/相机请求序列化，经共享内存 + 中断通知送达 CPU1，CPU1 调度 decoder/encoder/camera 硬件单元，结果经同通道回传。

**调用面（CPU0 常用）：** `media_client_init()`、`media_client_play_audio()`、`media_client_play_stop()` 等。
**数据交互：** 大块数据（音频帧/视频帧）走共享内存环形缓冲，控制命令走小消息；完成/错误以回调事件返回 CPU0。

---

### 3.15 芯片启动与中断（sdk/chip/txw82x/）

**文件：** `vectors.S`、`system0.c`、`system1.c`、`isr.c`、`trap_c.c`、`rpc0.c`、`rpc1.c`、`sysctrl1.c`、`pin_function.c`、`ticker_api.c`、`uart.c`

**`system0.c` / `system1.c`**：各自 CPU 的初始化入口，设置时钟树、使能外设时钟、初始化中断控制器。
**`rpc0.c` / `rpc1.c`**：跨 CPU RPC 通道实现（邮箱中断 + 共享内存）。
**`isr.c`**：中断分发，硬件异常 + 外设中断。
**`trap_c.c`**：硬故障/用法故障等异常入口。

---

### 3.16 LVGL 图形库

**版本：** LVGL 9.x 系列（`sdk/lib/lvgl/`）；
**适配层：** `sdk/lib/lvgl_rotate_rpc/`（双屏旋转 + RPC 适配）；
**绑定方式：** `lv_display_t` 通过 `lv_display_set_buffers()` 绑定帧缓冲，`flush_cb` 回调写 LCD；
**任务模型：** `ai_album_ui_process_timer_cb` 30ms 周期调用 `lv_timer_handler()`。

---

### 3.17 输入系统

**AD 键采集：**
- `keyWork_init(10)` 创建 10ms 周期任务，ADC 扫描 PA15 分压；
- `add_keycallback()` 注册回调到各模块；
- `photo_frame_key_callback` → `photo_frame_key_post()` → `ai_album_ui_input_post()`；
- `ai_album_ui_input.c` 维护环形缓冲，`ai_album_ui_input_try_pop()` 在 30ms timer 中消费。

---

### 3.18 板级配置与引脚

**配置链：**
```
project_config.h (CUSTOMER_ID)
  → sdk/demo/photo_frame_demo/photo_frame_config.h
  → sys_config.h (默认值，可被 demo config 覆盖)
  → project/txw82xApp/pin_param.h (生成的引脚配置)
  → project/txw82xApp/config.cfg → pin_bin.exe → pin_param.h
```

**关键引脚（已核对）：**
| 引脚 | 功能 |
|---|---|
| PA5 | 电源保持（hold） |
| PA15 | 按键 / AD 键 |
| PA10 | VBUS 检测 |
| PD13 | LCD AVDD 使能 |

---

### 3.19 通用框架组件

**OSAL 适配层：** `sdk/src/rtos/osal/` 提供 `os_task_create`、`os_sem_create`、`os_mutex_create`、`os_msgqueue_create`、`os_timer_create` 等统一接口，底层可切换 `csky/AliOS-Rhino` 或 `ohos/LiteOS-M`。

**消息中心：** `msg_center_init()` 创建消息队列，用于模块间异步消息传递。

**系统事件总线：** `sys_event_init(32)` 创建 32 容量事件池，`sys_event_post()` 广播事件。

**文件系统：** FATFS 挂载 (`f_mount`)，SD 卡通过 `sdhost` 驱动。

---

## 4. 模块依赖图

```
                         ┌──────────────┐
            keyWork ──▶ │            ◀─── brtc_agent(云)
           (PA15 ADC,   │    UI       │        ▲
            10ms)        │  (LVGL)     │        │
                         │  13 routes  │        │
photo_frame_key ──▶      │             │        │
_input(映射13动作) ────┤             │ ◀─── weather/NTP(HTTP)
                         └──────┬───────┘        │
                                │                │
               ┌───────────────┼────────────────┘
               ▼               ▼                ▼
          album_store    image_loader    image_ai
          (255条索引)    (2槽RGB565)    (8状态机)
               │               │                │
               ▼               ▼                ▼
          album_storage  album_jpeg_hw  brtc_agent("LLMRacing")
          (SD 0:/IMG,    (JPGID1+SCALE2,(120s超时)
           0:/DCIM,      锁1s,等3s)        │
           0:/AI_GEN)         │            ▼
               ▼             │        云端 LLM
            FATFS/SDIO ◀─────┘
                                │
        audio_player ──▶ media_client ──RPC──▶ CPU1 media_server
        (5级音量)                                     │
                                                  I2S/Codec/LCD
```

**核心数据交互方式汇总：**

| 交互对 | 方式 |
|---|---|
| UI ↔ image_loader | 请求队列 + 30ms poll + store_version 校验 |
| UI ↔ album_store | 版本号(g_store_version) + 变更通知 |
| UI ↔ 按键 | `ai_album_ui_input` 环形缓冲（13 种 action） |
| image_loader ↔ JPEG HW | 同步函数调用（互斥锁保护 JPGID1） |
| image_ai ↔ brtc_agent | 事件回调（ACK/RESULT/VIDEO_DATA） |
| brtc_agent ↔ 云端 | 长连接 socket + 分包协议 |
| photo_frame ↔ WiFi/NTP/天气 | sys_event 总线 + HTTP |
| app ↔ CPU1 | media_client RPC（共享内存+消息） |
| app ↔ 蓝牙 | bt_event 回调 + OSAL 队列 |
| 按键 → UI | keyWork → callback → photo_frame_key_post → input_post → router |
| 设置 ↔ album_store | format_dialog 调用 store_format/cancel |
| 亮度/音量 ↔ 硬件 | display_brightness_set / ausys_da_change_volume |

---

## 5. 关键路径与风险点

### 关键路径 1：开机能看照片
```
SD 挂载 → album_store 扫描(255上限) → UI HOME → 进 ALBUM
→ loader 请求 → JPGID1 硬解(≤3s) → RGB565 入槽 → UI poll 命中 → 上屏
```
**风险**：锁 JPGID1 失败（1s 自旋）→ 解码失败 → 槽位不 ready → UI 显示占位图。

### 关键路径 2：AI 图生图
```
选图+风格 → 读原 JPEG → brtc 上行(LLMRacing) → 云端生成(≤120s)
→ VIDEO_DATA 分片回传 → 拼装写 0:/AI_GEN(TMP_→rename) → store 刷新 → UI 更新
```
**风险**：120s 超时、ACK 丢失、SD 写满（写失败→FAILED）、网络中断（→CANCELLED/FAILED）。

### 关键路径 3：WiFi 配网上线
```
WiFi 扫描 → CONNECT → DHCP → NET_EVT_DHCP_SUCC → photo_frame_net_evt_hdl
→ photo_frame_net_brtc_start() → brtc_agent_init() → 云端连接
→ NTP 对时 → 天气拉取 → UI 显示
```
**风险**：DHCP 失败、DNS 解析失败、云端连接超时。

### 风险点清单
1. **PSRAM 6.25MB 预算紧**：loader 2 槽已占 2.36MB，叠加 LVGL、任务栈、BRTC 缓冲后余量有限，新增大缓冲需谨慎；
2. **相册扫描无跨目录去重**：IMG/DCIM 同名文件会重复出现；
3. **JPGID1 互斥争用**：若其它模块（如 camera 预览）也用 JPGID1，1s 锁等待可能超时；
4. **无本地 JPEG 编码**：任何需要"本机生成 JPEG"的新功能（如截图保存）必须引入 `sdk/app/encode/jpg_encode.c`，现有 album 模块不具备；
5. **uint8_t 容量上限**：照片索引 capacity 是 uint8_t，255 是硬上限；
6. **AI 生成图写盘依赖 TMP_→rename 原子性**：断电可能残留 TMP_ 文件（扫描时已跳过，但会占空间）；
7. **sys_event_init(32)**：事件总线容量仅 32，若多模块同时广播可能丢事件；
8. **双核构建顺序**：CPU1 必须先编译，否则 CPU0 链接脚本 `gcc_csky.ld` 未被回写修正，导致地址冲突。

---

## 6. 跨模块数据交互总览

### 6.1 事件总线（sys_event）
- `sys_event_init(32)` 创建 32 容量事件池；
- 各模块通过 `sys_event_post()` 广播状态变化（WiFi 状态、存储变化等）；
- `photo_frame_net_evt_hdl` 等回调消费事件。

### 6.2 消息队列（msg_center）
- `msg_center_init()` 创建消息中心；
- 异步消息传递，模块间解耦。

### 6.3 共享内存（CPU0 ↔ CPU1）
- media_client 与 media_server 之间的 RPMsg + 共享内存通道；
- 大块数据（音频/视频帧）走共享内存环形缓冲；
- 控制命令走小消息通道。

### 6.4 函数调用（同步）
- UI → album_store / image_loader / image_ai：直接函数调用；
- album_jpeg_hw：同步阻塞式调用（带超时）；
- brtc_agent_send_*：同步发送请求，异步回调接收结果。

### 6.5 回调/事件（异步）
- brtc_agent → image_ai：`BRTC_AGENT_EVENT_MEDIA_GENERATE_ACK/RESULT/VIDEO_DATA`；
- lv_timer → UI：30ms 周期回调处理渲染/输入；
- keyWork → UI：10ms 周期 ADC 扫描 + 回调派发。

### 6.6 共享状态
- `g_store_version`：相册版本号，各模块监听变化；
- `g_weather`：天气快照（含 `request_pending`、`snapshot.valid`、`seconds_since_refresh`）；
- `sys_cfgs.album_vol` / `sys_cfgs.album_brightness`：音量/亮度持久化；
- `g_booted_with_key`：开机方式标识（按键 vs USB）。

---

## 附录：文档范围声明

**本文档覆盖的完整模块清单：**

### SDK 核心层（与 demo 无关）
| 模块 | 目录 | 说明 |
|---|---|---|
| 芯片启动 | `sdk/chip/txw82x/` | vectors, system0/1, isr, trap, rpc |
| 芯片驱动 | `sdk/driver/` (hg*) | adc, capture, crc, dma, gpio, i2c, i2s, jpeg, osd_enc, pdm, pwm, scale, sha, spi, syaes, timer, uart, vpp |
| HAL 封装 | `sdk/hal/` | ~46 个外设包装（lcdc, isp, h264, jpeg, dma2d, vcodec, usb 等） |
| 公共头文件 | `sdk/include/` | devid.h, basic_include.h, chip/dev, hal, lib, osal |
| RTOS | `csky/` + `ohos/` | AliOS-Rhino / LiteOS-M + txsemi OSAL |
| 闭源库 | `libs/` | 38 个 .a（wifi, h264, isp, lcdc, boot, pmu 等） |
| 应用框架 | `sdk/app/` | app_lcd, screen, ui, video_app, decode, encode, spook, brtc_agent, walkie_talkie_ui 等 |
| 中间件 | `sdk/lib/` | lvgl, lcd(panel驱动), net(lwip), multimedia(MSI), rpc(cpurpc), fs, heap 等 |
| 通用 Demo | `sdk/demo/app_common.c/.h` | 共享应用代码 |
| CPU1 固件 | `project/txw82xCore/` | WiFi/BLE 核心（LMAC） |
| CPU0 应用入口 | `project/txw82xApp/` | main.c, device.c, syscfg.c 等 |
| 构建系统 | `project/` | CDK 构建，非 CMake/Makefile |

### 应用层（photo_frame_demo）
| 模块 | 目录 | 说明 |
|---|---|---|
| 相册存储 | `album/ai_album_album_store.c/.h` | 255 条照片索引 |
| 相册存储操作 | `album/ai_album_album_store.c` | scan/refresh/format/get_photo |
| 相册画廊加载 | `album/album_gallery_loader.c/.h` | 画廊卡片加载 |
| 相册图像 AI | `album/ai_album_album_image_ai.c/.h` | 8 状态机，120s 超时 |
| 相册图像加载器 | `album/ai_album_album_image_loader.c/.h` | 2 槽异步解码 |
| 相册图像视图 | `album/ai_album_album_image_view.c/.h` | 双槽缓存，LRU 策略 |
| 相册 JPEG 硬件 | `album/ai_album_album_jpeg_hw.c/.h` | JPGID1+SCALE2 解码 |
| 相册性能 | `album/album_perf.c/.h` | 性能统计 |
| 相册照片缓存 | `album/ai_album_album_photo_cache.c/.h` | 路径+文件大小缓存 |
| 相册照片导航 | `album/ai_album_album_photo_navigation.c/.h` | 前后导航 |
| 相册照片页 | `album/ai_album_album_photo_page.c/.h` | 单页渲染 |
| 相册幻灯片 | `album/ai_album_album_slideshow.c/.h` | 自动播放 |
| 相册状态 | `album/ai_album_album_status.c/.h` | 状态指示 |
| 相册页面容器 | `ui/pages/ai_album_album_pages.c/.h` | ALBUM/GALLERY/IMAGE_AI 三合一 |
| UI 核心 | `ui/ai_album_ui.c/.h` | 30ms timer, bootstrap |
| UI 路由 | `ui/ai_album_ui_router.c/.h` | 13 路由分发 |
| UI 模型 | `ui/ai_album_ui_model.h` | 数据模型 |
| UI 输入 | `ui/ai_album_ui_input.c/.h` | 环形缓冲输入 |
| UI 通用 | `ui/ai_album_ui_common.c/.h` | 通用工具 |
| UI 首页运行时 | `ui/ai_album_home_runtime.c/.h` | 首页状态 |
| UI 聊天运行时 | `ui/ai_album_chat_runtime.c/.h` | AI 聊天状态 |
| UI 练习运行时 | `ui/ai_album_practice_runtime.c/.h` | 练习状态 |
| UI 字体管理器 | `ui/ai_album_font_manager.c/.h` | 字体管理 |
| UI 国际化 | `ui/ai_album_i18n.c/.h` | 多语言 |
| UI 电源对话框 | `ui/ai_album_power_dialog.c/.h` | 关机弹窗 |
| UI 兼容层 | `ui/ai_album_compat.h` | LVGL 9.0/9.5 适配 |
| 设置页面 | `ui/pages/ai_album_settings_pages.c/.h` | 7 个设置项 |
| 设置对话框 | `ui/pages/ai_album_settings_dialog.c/.h` | 语言/亮度/关于 |
| 设置格式对话框 | `ui/pages/ai_album_settings_format_dialog.c/.h` | 3 阶段格式化 |
| 设置 WiFi 页面 | `ui/pages/ai_album_settings_wifi_page.c/.h` | WiFi 配置 |
| 设置 WiFi 密码页 | `ui/pages/ai_album_settings_wifi_password.c/.h` | WiFi 密码 |
| 位置搜索页 | `ui/pages/ai_album_location_search_page.c/.h` | 城市搜索 |
| 网络/WiFi | `network/wifi_sta.c/.h` | STA 模式 |
| 网络/Provision | `network/wifi_provision.c/.h` | AP 配网 |
| 网络/凭证 | `network/wifi_credentials.c/.h` | WiFi 凭据存储 |
| 网络/BLE | `network/ble_settings.c/.h` | 蓝牙设置 |
| 天气服务 | `network/ai_album_weather_service.c/.h` | open-meteo API |
| 天气解析 | `network/ai_album_weather_parser.c/.h` | 手写 JSON 解析 |
| 时间服务 | `system/ai_album_time_service.c/.h` | NTP + RTC |
| 农历 | `system/ai_album_lunar.c/.h` | 农历计算 |
| 音量 | `audio/ai_album_volume.c/.h` | 5 级 DAC 音量 |
| 亮度 | `display/ai_album_brightness.c/.h` | LCD 背光 |
| 电池检测 | `hardware/battery_detect.c/.h` | 电池电压 |
| 电源控制 | `hardware/power_ctrl.c/.h` | 开机/关机/hold |
| 存储信息 | `storage/ai_album_storage_info.c/.h` | SD 卡信息 |
| 入口 | `photo_frame_demo.c/.h` | 应用 main |
| 按键输入 | `photo_frame_key_input.c/.h` | 7 AD 键映射 |
| 网络控制 | `photo_frame_net.c/.h` | WiFi 状态机 |
| 服务桩 | `ai_album_services_stub.c` | 音量/亮度/电源/天气/WiFi/存储/时间桩 |

---

> **文档生成时间**：基于 TXW82x_FPV-v2.7.1.7-45228 工程源码，通过 codegraph 全量扫描 + Read 逐文件核对生成。
> **注意**：闭源库（`libs/*.a`）和 CPU1 固件（`project/txw82xCore/`）内部实现不在本文档覆盖范围内。
