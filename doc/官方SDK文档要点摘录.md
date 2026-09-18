# TXW82x 官方在线文档要点摘录（AI 数码相框开发参考）

> 来源：泰芯半导体官方文档站 https://taixin-semi.com/zh/docs/txw82x/latest/
> 摘录日期：2026-09-17（本次补充）；对应官方文档版本：主文档 V1.0（2026-08-15，基于 TXW82x_FPV-v2.7.1.7-44398）、LCD FAQ V1.3（2026-07-15）、硬件设计指南 V1.2（2026-06-23）、架构与配置 / 框架原理 / 视频应用 / 低功耗 / 选型表 均标注适用 v2.7.1.7。新增来源文档：低功耗开发指南、SDK 视频应用功能使用说明、SDK 选型表（含 v2.7.0.7 vs v2.7.1.7 差异）、文档列表页发现的 TXW827/826/828 数据手册及 DEV_BORAD v1.2。
> 本工程 SDK 版本 v2.7.1.7-45228 与文档基线同大版本，条目已抽查核实（标注 ✅ 的为在本工程源码中确认存在）。
> 原文以 Markdown/PDF 提供（页面内有"下载 Markdown 源文件"按钮），二次开发遇到细节应以原文为准。

## 目录

1. [对本项目直接有用的要点（按优先级）](#1-对本项目直接有用的要点按优先级)
2. [框架原理：MSI / TXMPlayer / LCD 显示流 / VFS](#2-框架原理)
3. [架构、配置体系与双核规则](#3-架构配置体系与双核规则)
4. [内存与 Cache/DMA 规则](#4-内存与-cachedma-规则)
5. [LCD FAQ 摘录（RGB 屏相关优先）](#5-lcd-faq-摘录)
6. [网络 / 系统事件 / 参数区 / OTA](#6-网络系统事件参数区-ota)
7. [音频](#7-音频)
8. [硬件设计要点](#8-硬件设计要点)
9. [调试方法论](#9-调试方法论)
10. [官方文档索引（其余在线专题）](#10-官方文档索引)

---

## 1. 对本项目直接有用的要点（按优先级）

### 1.1 AV 堆内存调整的官方宏（后续优化 AV 堆时直接用）✅

官方确认的堆尺寸宏（demo config.h 级覆盖，正是我们推迟的"AV 堆 6MB→7MB"方案的正确抓手）：

| 宏 | 作用 | 说明 |
|---|---|---|
| `CONFIG_PSRAM_AVHEAP_SIZE` | 音视频 PSRAM 堆大小 | 各 demo config.h 均有定义（voice 512KB / vision 1.5MB / ipc_720p 7.5MB）；本工程 photo_frame_config.h 定义为 6.25MB |
| `CONFIG_AVHEAP_SIZE` | 音视频 SRAM 堆大小 | 分辨率越高需求越大；ipc_720p 为 100KB |
| `MORE_SRAM` | 将部分数据转移到 PSRAM | SRAM 不足时用，可能影响性能 |
| `DEFAULT_SYS_CLK` | 系统主频 | 不建议低于 160MHz；可超至 240MHz 但**必须配合硬件 VDD 提到 1.2V** |

⚠ 官方明确：AV 堆**不是额外物理内存，是从系统可用内存划出的专用池**，修改时必须与 CPU1 SKB、lwIP、文件系统、AI、LVGL、普通 PSRAM heap 一起核算 —— 与我们内存地图分析的结论一致（见 WORKLOG 三十六/三十七）。
实现位置 ✅：`sdk/demo/app_mem.c`（`video_psram_init`/`video_sram_init`），720P 方案基线 7MiB+512KiB AV PSRAM + 100KiB AV SRAM。

### 1.2 VFS 三个坑（相册/字体/存储代码都用得上）

- **`fflush()` 当前不执行实际刷盘**，需要同步必须用 `fsync(fileno(fp))`（官方框架文档原文）。
- `fread()/fwrite()` 返回实际字节数——写入后必须检查短写。
- 路径体系：客户代码统一 `/sd0/...`；录像/拍照内部模块用 `0:/...`（FatFS 盘符）；**LVGL 文件系统用 `V:/sd0/...`**（我们 SD bitmap 字体/相册加载器已用对，注意移植代码时别混）。
- `rename()` 不能跨挂载点移动文件；FS_EN=0 时不要调用文件接口。

### 1.3 JPEG 解码放大 → SCALE2 显存 SRAM 公式（图生图/大图缩放直接用）

LCD FAQ 第 12 节给出精确公式（遇到"JPG 解码放大显示条纹/花屏"就是 scale2 buf 不够）：

- Y 需要 SRAM：`0x20 + TW + J0*SRAMBUFLEN*4 + 256`
- U/V 各需 SRAM：`0x10 + TW/2 + J1*SRAMBUFLEN*2 + 256`
- MJPEG/YUV420 输入：`J0=(16*倍数)+2`，`J1=(8*倍数)+2`
- H264/YUV420 输入：`J0=(20*倍数)+2`，`J1=(12*倍数)+2`
- TW = scale2 输出目标宽度；放大倍数越大所需空间越大；两种源并存时按 H264 最大系数申请。
- scale 输出尺寸遵循 **8 字节对齐**；scale_set_in_out_size / scale_set_step / scale_set_start_addr 三件套做中心放大。

### 1.4 显示质量可白嫖的两个硬件功能（RGB 屏直接受益）

- **DITHER 抖动滤波** ✅：`lcdc_dither_linebuf`（3 行即可）+ `lcdc_dither_en` 打开，对 RGB565 色彩单调渐变区（白墙、天空、照片大面积过渡）减少色带断层。代价：占 3 行宽度的 SRAM。API 存在于 `sdk/hal/lcdc.c` + `sdk/include/hal/lcdc.h`。**照片展示类产品值得开启评估。**
- **OSD 透明/半透明** ✅：`lcdc_set_osd_alpha`（0~0x100 整层 alpha）；`lcdc_set_osd_enc_cfg` 设整层透明色（默认 0x000000 黑色穿透到 video 层）；`lcdc_osd_spec_color_alpha(柄,使能,色,α)` 指定特殊色做**半透明菜单栏**效果。

### 1.5 TXMPlayer 播放器（将来加背景音乐/提示音时用，勿自建解码链）

`SUPPORT_TXMPLAYER` 定义时 `main.c Codec_init()` 已自动 `txmplayer_init(0,0,NULL)`，客户直接：

```c
int32 sid = txmplayer_open("/sd0/music.mp3", 0, NULL);  // 本地或 http URL
txmplayer_pause(sid, 1/0);  txmplayer_seek(sid, ms);
txmplayer_set_volume(sid, 80);  txmplayer_set_speed(sid, 2);
int32 st = txmplayer_state(sid);
uint32 total; int32 cur = txmplayer_playtime(sid, &total);
txmplayer_close(sid);
```

音视频同步三模式：`TXMPLAYER_AVSYNC_AUDIO/VIDEO/CLOCK`。`struct txmplayer_param` 可设初始音量、fbQ_size（网络播放调大）、netbuf_size、自定义 vdd/decoder。解码器由 `SUPPORT_DECODER_H264/JPEG` + `*_DEC_CTRL` 决定（值=运行核，如 `PCM_DEC_CTRL=AUCODER_RUN_IN_CPU1` —— 正是我们 brtc TTS 链的配置 ✅）。

### 1.6 网络就绪判定（印证 photo_frame_net.c 现有实现）

官方 CAUTION：**不要在收到 Wi-Fi connected 事件后用固定延时猜网络就绪，必须等 DHCP 完成事件**。默认单网卡名 `w0`（以太网 `e0`）。我们的 brtc 启动挂在 `SYSEVT_LWIP_DHCPC_DONE` 上，与此一致 ✅。

### 1.7 JTAG 与本板引脚的潜在冲突（硬件注意）

官方：JTAG = PA9(TCK)/PA10(TMS)，芯片内部默认 100K 上拉；连 CKLink 时这两脚不能被其他外设强驱动、不能并联大电容。
本板：PA9 = LCD 背光（config.cfg `LCD_BACKLIGHT_IO`）、PA10 = VBUS 检测。**量产不用 JTAG 没问题；但开发期若插 CKLink 调试，会与背光/VBUS 检测引脚打架**——排查背光异常或插拔检测异常时先看是否连着调试器。

### 1.8 第三方库移植红线（brtc 移植已踩过的坑，官方背书）

> CPU0 为 E804DF **硬浮点**，不能直接链接其他架构、其他浮点 ABI 或**其他 SDK 版本**生成的库（建议在当前 SDK 版本生成）。
> 不要用"能链接"判断二进制兼容——浮点 ABI、结构体对齐、编译选项或 C++ 运行库不一致会在运行时**静默破坏栈和数据**。

### 1.9 syscfg 参数区规则（印证持久化做法）

- 参数结构**前部带 magic/CRC 兼容字段，新字段只能追加到尾部**，不能插入/重排，否则量产设备参数区不兼容（与我们踩过的"named syscfg record 失败、tail-append 才行"完全一致 ✅）。
- 修改后调 `syscfg_save()`；需立即生效还要按模块调 `wificfg_flush()/netcfg_flush()` 或重启。
- SSID/密码/密钥/MAC/校准数据**不得打印到普通日志**。

---

## 2. 框架原理

（官方《TXW82x SDK 框架原理说明》V1.0，适用 v2.7.1.7）

### 2.1 MSI 媒体流框架

核心四文件：`sdk/include/lib/multimedia/msi.h`、`framebuff.h`、`sdk/lib/multimedia/msi/msi.c`、`sdk/app/algorithm/stream_define.h`（组件命名）。

**struct msi 关键字段**：`fbQ`（输入帧队列）、**`fb_limits`（在途帧数量限制）**、`type`（帧类型过滤 = mtype<<8|stype）、`enable`、`action`、`priv`。
—— 印证我们的修复：`msi_new` 只初始化 fbQ，`fb_limits` 需手动赋值（如 `msi->fb_limits.counter = 16`，dac_msg.c 模式），否则 `msi_alloc_fb` 永远失败 ✅。

**一帧的流动**：源组件 `msi_output_fb(fb, care)` → 按 mtype/stype 过滤 → 下游 `MSI_CMD_TRANS_FB` 确认 → 入队 → 下游线程 `msi_get_fb()` 处理 → `fb_put()` 引用归零自动释放。

**帧所有权（care 语义，官方原文）**：
- `msi_output_fb(..., care=0)`：返回后调用方**不再持有**该帧；
- `msi_output_fb(..., care=1)`：无下游接收时保留调用方引用；
- 跨线程保存帧前 `fb_get()`，用完 `fb_put()`；帧链 API 必须传首节点。

**客户标准用法**（不必改 MSI 框架）：找到数据源名（AUTO_H264/AUTO_JPG…）→ `msi_new()` 建组件 → `msi_add_output()` 连接 → 工作线程 `msi_get_fb()` → 退出时 `msi_del_output()` + `msi_destroy()`；调试用 `msi_dump()` 看连接与水位。

**控制命令**：`msi_do_cmd(组件, cmd, p1, p2)` 控制单组件；`msi_cmd2(源, cmd, 0, 0)` 向整条下游链广播。

### 2.2 LCD 显示流

分层：LVGL(RGB565) → LVGL OSD MSI → `R_OSD_ENCODE`（OSD 硬件编码）→ `R_LCD_OSD` → LCDC 三层混合（+视频层 `R_VIDEO_P0/P1`）→ 面板。
`app_lcd_init()` 内部建四个 R_* MSI 组件并启动刷新线程；`app_lvgl_init(main_ui, input_mask)` 注册 LVGL 内存钩子后 UI 直接受 OSD 链托管，客户只写 LVGL 对象。
视频层开关：`msi_cmd(R_VIDEO_P0, MSI_CMD_LCD_VIDEO, MSI_VIDEO_ENABLE, 1/0)`。
播放器上屏走 VDD：TXMPlayer → LCD Virtual Display → R_VIDEO_P0（`SUPPORT_LCD` 时 Codec_init 自动注册）。
换屏 checklist：方案屏宏 + lcdstruct 时序分辨率 + 复位/背光/电源引脚 + 颜色格式/旋转/LVGL 分辨率五件套同步改。

### 2.3 VFS

初始化链：`vfs_init()` → `vfs_fatfs_register()` → `fatfs_sd0_init()` → `vfs_mount('0:','/sd0','fatfs')`。最长前缀匹配。SD 热插拔由系统事件 + `mount.c` 处理，业务必须监听挂载事件而非"检测到卡"。**新代码不要再使用 osal_fopen/osal_fwrite 等兼容接口。**

---

## 3. 架构、配置体系与双核规则

（官方《TXW82x SDK 架构与配置说明》V1.0）

### 3.1 配置生效顺序

`sys_config.h` 首先包含 `project_config.h`（含 CUSTOMER_ID → 方案 config.h），再用 `#ifndef` 补默认值。
规则：改当前方案 config.h，不改 sys_config.h 默认值；一个固件一个 CUSTOMER_ID；**宏开了 ≠ 功能通了，还要确认方案初始化代码调用了对应模块**。

### 3.2 CUSTOMER_ID 方案表（1-9）

| ID | 方案 | 配置文件 | 用途 |
|---|---|---|---|
| 1 | AI 语音 | ai_dialogue/voice_config.h | Coze 语音对话 |
| 2 | AI 视觉 | ai_dialogue/vision_config.h | MIPI LCD+LVGL，默认无摄像头 |
| 3 | AI 闹钟 | ai_alarm_clock_config.h | SPI LCD、触摸、Flash 文件系统 |
| 4 | ISP 调试 | isp_tuning_config.h | 画质调试，不量产 |
| 5 | IPC 720P | ipc_720p_config.h | 默认方案 |
| 6 | LCD 720P | lcd_720p_config.h | MIPI LCD+LVGL+播放器 |
| 7 | IPC Sleep 720P | ipc_720p_sleep_config.h | 低功耗 |
| 8 | IPC 1080P | ipc_1080p_config.h | 高清 |
| 9 | 电池相机 1080P | battery_camera_1080p_config.h | 电源域控制 |

（本工程 CUSTOMER_ID=10 photo_frame_demo 为自建方案，不在官方表内。）

### 3.3 双核规则（重要边界，官方第 8 章全文要点）

- 双核 cpurpc 及邮箱 **SDK 已初始化，客户不要再调 `cpu_rpc_init()`，不要自建第二套核间通信**。
- CPU1 客户代码只允许**纯软件运算**：输入明确数值/只读数据，输出结果，**不访问硬件、不动调度**。CPU1 **无硬浮点**，别在 CPU1 做浮点。
- 两个受控入口：
  - `cpu1_run_func(func, p1, p2, p3)`（rpc0.c 封装，同步执行单函数）；
  - `cpu1_new_task(name, func, arg, prio, time, stack, stack_size)`（原型未在公共头文件，需 extern；优先级建议 `OS_TASK_PRIORITY_NORMAL` 起，不得高于 Wi-Fi/LMAC）。
  - 函数体必须位于 CPU1 可取指地址；arg/名字符串地址两核可见且生命周期足够；**禁用 os_msgqueue/event/sem/mutex/task/irq 等 OSAL 对象**。
- CPU spinlock：`cpu_splock_lock/unlock(id)`（勿再调 init/resume，两核 device.c 已做）。锁号从 `CPU_SPLOCK_ID_0` 顺序分配并**登记"锁号-资源"对应表**；`CPU_SPLOCK_ID_11_PMU/12_SYSCTRL/13_DMA2D/14_EFUSE/15_DCACHE` 平台保留。同一资源两核必须同一锁号。**spinlock 只做互斥，不解决缓存一致性**，volatile 也不能替代。
- RPC ID 表在 `sdk/include/chip/txw82x/rpc.h`，两端必须匹配，不自改已有 ID 顺序。

> 对本项目的意义：brtc/AI 全部在 CPU0；若日后要把相册滤镜、缩略图哈希等纯计算卸载到 CPU1，只能走上述两个入口 + spinlock 模板，且临界区短小无阻塞。

### 3.4 常用配置宏速查

| 类 | 宏 | 备注 |
|---|---|---|
| 内存 | `CONFIG_PSRAM_AVHEAP_SIZE` / `CONFIG_AVHEAP_SIZE` / `MORE_SRAM` | 见 §1.1 |
| 码流 | `SUB_STREAM_EN/WIDTH`、`JPG_NODE_COUNT`(30)、`MP4_MAX_SINGLE_SIZE`(100MB) | 副流需 VPP BUF1 + h264 副流 + 下游子类型三者齐 |
| 网络 | `WIFI_MODE_DEFAULT`、`SYS_APP_DHCPD/SNTP/BLENC(0/1/2)`、`WIFI_TX/RX_AGG_EN` | RX 聚合需 RX buffer ≥18KiB |
| 存储 | `FS_EN`、`SDH_EN`、`FLASHDISK_EN`(内部Flash文件系统，AI闹钟用)、`STARTUP_OTA`、`USE_FAT_CACHE`(录像建议开) | |
| 显示 | `SUPPORT_LCD`、`DMA2D_EN`、`LCD_*_EN`、`LVGL_INPUTDEV_SUPPORT` | |
| 播放器 | `SUPPORT_TXMPLAYER`、`SUPPORT_DECODER_H264/JPEG`、`*_DEC_CTRL` | `_DEC_CTRL` 值=运行核 |

---

## 4. 内存与 Cache/DMA 规则

（官方主文档第 9 章）

内存池一览：

| 区域 | 接口 | 用途 |
|---|---|---|
| CPU0 SRAM heap | `os_malloc/os_zalloc` | 任务、控制对象、低延迟小块 |
| CPU0 PSRAM heap | `os_malloc_psram` | 网络大缓存、一般大对象 |
| AV SRAM heap | `video_sram_init()` | VPP/H.264 时序敏感数据 |
| AV PSRAM heap | `video_psram_init()` | 帧、码流、显示缓存 |
| CPU1 SRAM | CPU0 启动时分配 | heap 40KiB + LMAC RX 10KiB（`CONFIG_CORE_HEAP_SIZE/RXBUF_SIZE`）|
| CPU1 PSRAM | CPU0 启动时分配 | SKB pool 默认 200KiB（本工程裁到 128KiB）|
| Audio RPC PSRAM | `aurpc_psram_heap_init()` | 音频模块缓存，默认 30KiB |

地址边界：CPU0 XIP 0x10000000；CPU1 XIP 0x10001000（前 4KiB CPU0 向量）；PSRAM DBUS 映射 0x28000000；CoreSetting/Loader/固件信息占用保留 SRAM。**一切以本次 project.map 为准，不要按文档地址扩段。**

Cache/DMA/所有权 WARNING（随机花屏/码流损坏的三大来源）：
1. DMA 缓冲满足对齐、地址域、生命周期；CPU→DMA 前 **clean**，DMA→CPU 前 **invalidate**；
2. ISR 中禁大内存分配、等锁、文件系统、网络阻塞调用；
3. MSI/Framebuff 遵守引用计数与"谁释放"约定，**帧交给下游后不要再写原帧**；
4. 大数据放 PSRAM 省 SRAM 但增加延迟/总线压力，视频热点数据实测。

看门狗：不要任务里无条件喂狗；应由健康监控确认关键任务、存储、双核状态后统一喂狗。

---

## 5. LCD FAQ 摘录

（官方《TXW82x LCD FAQ》V1.3；§5.1-5.2 对本板 RGB 屏最相关）

### 5.1 RGB/MIPI 屏抖动处理链（按顺序试）

1. **dclk 补全**：`lcd_hardware_init565` 中打开（处理帧间隔停 CLK 抖动），rgb/mipi 均建议开；
2. **缩短帧间隔**：video 旋转 0/180° 时把 linebuf 64 行 → 32/16/8；
3. **DMA burst 调整**：`device.c` 中 `DMA2AHB_BURST_CH_ROTATE_IN_RD` 对 PSRAM 占用效率再平衡（旋转 0/180° 可设最大；90/270° 遵循 pingpang，burst 32、linebuf 32×2=64）；
4. **OSD fifo**：`app_lcd.c lcd_driver_init` 中添加（对 osd 抖动有优化，fifo 可加大）；
5. **OTA/写 Flash 期间抖动**：空口升级或保存参数 holdcpu 时开 **lcd autokick**；
6. 大屏 video+osd 频繁解码仍抖（伴随 lch 打印）：把 **osd_fifo 和 video line_buf 挪到 SRAM 最后**，减少内存竞争。

### 5.2 新面板驱动配置要点（lcdstruct / 面板 .c）

- 新驱动仿 st7735（SPI）/st7701s_mipi（DSI）另拷 .c，覆盖 `__weak lcdstruct`；`LCD_*_EN` 只开一个（本工程 hx8282 ✅）。
- screen_w/h=实际显存；video/osd_w/h ≤ screen；偏移 x/y 不能出界。
- **技巧**：同驱动不同尺寸屏可不动初始化表 —— screen_w/h 设成驱动显存全尺寸，有效区用 video/osd_w/h + x/y 偏移（差值÷2），LCDC 自己做偏移，免改 2a/2b。
- porch（行场消隐）按屏厂；`pclk` 别一味调高——增加功耗和 PSRAM 带宽压力（帧率够用就好）。
- MIPI 偏色（非顺序错）→ GCK 锁存出错：vbp 上下 ±1/2/3 试，hs_inv/vs_inv 0↔1 同 flip。
- MIPI 单 lane 速率：≤480M 选 `DSI_MODULE_CLK480`，>480M 选 `DSI_MODULE_CLK_960M`（mipi_dsi_driver.c）。

### 5.3 其他实用项

- **TE 同步**（MCU/SPI 屏防撕裂）：SDK 默认不处理；app_lcd.c 设 TE 上升沿外中断 → lcd_msi_irq_callback 里按使能选择 kick 时机。
- **帧间命令**（GC9309NA 等特殊屏）：驱动里填 `frame_table`，在 kick 显存前发出（带 TE 在 TE 中断发，不带 TE 在 lcdc_set_start_run 前）。
- **录屏调试模块**：app_lcd.c 开宏，每次 screen_done 中断把 YUV420P 显存存 PSRAM，仿真 dump 后 ffplay 看 —— 区分"应用画错 / 屏参错 / 屏本身错"。
- **OSD 透明**：见 §1.4。
- **VIDEO 层插黑**（宽高比不一致不裁剪）：改 P0 size + P0 location（scale3_normal_msi 输出 size + fb 带 p0 偏移）。
- WR 跳变问题（SDK-43316 之前）：`lcdc_clk_gather_select` 调出数据/锁存时机。
- MCU 屏读 ID：`lcdc_reg_read_data`（命令/读长/寄存器/存址），RD 引脚非 255、LCD_D0~D7 设 inout。

---

## 6. 网络、系统事件、参数区、OTA

- **网络就绪**：等 `DHCP 完成事件`，勿用固定延时；Cat.1 场景等 `SYSEVT_LTE_CONNECTED` 后 events.c 才切 l0 默认路由。USB 枚举成功 ≠ Cat.1 已联网。
- **系统事件**：事件池 32 节点（`sys_event_init(32)` ✅），只传低频状态，**不能承担逐帧视频/逐包音频**；回调只做状态更新/投递 work；`sys_event_take` 后退出要 `sys_event_untake`（每次注册都耗 heap）。
- **默认 Wi-Fi AP**：SSID 前缀 `82X_`+MAC 后三字节，密码 `12345678`，192.168.1.1/24 —— 量产必须改（本工程 STA 模式，仅备忘）。
- **Wi-Fi 宏**：`WIFI_TX_AGG_EN`/`WIFI_RX_AGG_EN`（RX 聚合需 RX buffer ≥18KiB，同时核对 SRAM map）、`WIFI_PREVENT_PS_MODE_EN`（阻止 STA 休眠——省电产品留意）、`WIFI_FEM_CHIP`。
- **BLE 配网**：`SYS_APP_BLENC`；配网成功以 **Wi-Fi 连接+DHCP 事件**为准再 GATT 回通知，不要收到 BLE 写入就当联网完成。配网凭据属敏感数据，量产要加会话认证/重放保护。
- **SD 录写可靠性**（相册写卡同理）：检查每次写返回长度；文件时间戳单调；分段文件 + 掉电恢复；`sdh_loop` 500ms 轮询热插拔（非独立 CD 脚）；`sd_init` 先 1-bit，Host 标志+卡 SCR 支持才切 4-bit —— **配置了 DAT1~3 ≠ 已工作在 4-bit**。
- **OTA**：`STARTUP_OTA=1` 方案开机查 `/sd0/ota.bin`；`app_sd_init()` 进 OTA 时返回错误、产品初始化据此停业务。升级链=校验头/料号/版本→Hash→分块写→候选启动标记→重启→健康确认→失败回退；**升级成功返回值 ≠ 新固件已健康**。

---

## 7. 音频

- 默认 IPC：`app_audio_init(8000, 16000)` —— **ADC 8kHz，Mixer/DAC 16kHz**，并建 30KiB Audio RPC PSRAM heap。（本工程 photo_frame_demo 用 `app_audio_init(16000,16000)` ✅——MIC 需 16k。）
- 组件库：AAC/G.711(Alaw/Ulaw)/AMR/MP3/Opus 编解码 + AEC/ANS/AGC/VAD + 重采样 + I2S/PDM。
- 爆音/断续排查顺序：采样率→位宽→通道→主从时钟→DMA 周期→Cache→buffer 水位→模拟电源/PA 控制。
- PA 时序 WARNING：**PA 上电、MUTE、音量、DAC 数据顺序错会产生爆音**，严重损坏喇叭；调试用低增益+限流电源。
- 硬件红线：**VCMAU 是音频共模输出只能按指南去耦，禁止当其他电路电源**；AGND 与系统地单点连接。

---

## 8. 硬件设计要点

（官方《TXW82x 硬件设计指南》V1.2 + 主文档第 2/3 章，只摘与本板相关/易踩的）

### 8.1 电源

| 电源 | 要求 |
|---|---|
| VDD（内核） | 1.1/1.15/1.18V；DCDC 纹波 ≤30mV；走线压降补偿 +20mV；**240MHz 主频需 1.2V**（找 FAE/方案原理图确认） |
| 3.3V 系统 | 纹波 ≤50mV；主路径 ≥1A、最窄 ≥500mA；星型拓扑；退耦电容近管脚 |
| VCC1 | PB[6:15]+PC[0:2] 的 IO 电压域（1.8/3.3V 可选）——接屏/传感器前先对电压域 |
| VCC_FLS | 芯片输出给 SPI NOR 供电；Flash CS 外部 100K 上拉到 VCC_FLS |
| VDD18 | 内部 LDO，供内部 PSRAM/外部 camera IO |
| VCCAU27 | 内部 LDO 输出 2.7V，MIC 偏置 |
| VCMAU | 音频共模 ~1.0V，105 电容到 AGND，**禁作他用** |

### 8.2 启动模式（PB0/PB1 电平，内部 100K 下拉）

| PB1 PB0 | 介质 |
|---|---|
| 0 0 | 3.3V FLASH（默认） |
| 1 0 | 1.8V FLASH |
| 0 1 | 3.3V SD/eMMC |
| 1 1 | 1.8V SD/eMMC |

### 8.3 时钟

- 40MHz 高速晶振**必选**：负载电容 15pF、温补 ±10ppm，匹配后射频频偏 ≤±10ppm；
- 32.768K 可选：联网产品可网络校时省掉，该脚可复用 GPIO（QFN68/48 注意屏蔽 IO 功能）。

### 8.4 ADKEY（本板 6+1 键方案依据）

一路 ADC 8 通道；量程 **0-3.0V、10bit、1Msps**；分压电阻 **1% 精度**。（本板 PA15 阶梯 mV 实测表见 AGENTS.md。）

### 8.5 调试口

- JTAG：PA9/PA10，内部默认 100K 上拉；连 CKLink 参考电压接 3.3V；**这两脚不能接其他外设/大电容**（本板冲突分析见 §1.7）。
- UART 外接：TX 串 1K；RX 串 1K + 10K 上拉 + 二极管（压降 <1V）防反向漏电导致启动异常。

### 8.6 背光/屏

- 背光肖特基反向击穿电压 > 背光 IC OVP；输出电容耐压 > OVP；FB 限流电阻 1% 精度；
- PWM 背光频率建议 ≥20kHz；背光 IC EN 硬件默认下拉；LCD RST 靠接口端预留 102 电容；
- RGB 走线 ≤13cm、以 clk 为基准误差 ≤180mil、3W 原则、clk 包地+近芯片 RC。

### 8.7 射频/布局速记

RF 50Ω 单端、短直、π 型匹配近芯片、ESD 近天线；晶振近芯片、底部完整地、禁铺铜区；EPAD 多打地过孔（散热+RF 性能）；MIC 类差分包地远离 RF/DCDC/PA（防啸叫）；LOUTP/N 近芯片各串 1K 再类差分走线（差分输出幅度 1.0607Vrms）。

---

## 9. 调试方法论

（官方主文档第 19 章，浓缩）

1. **先找首个错误**——不要拿后续模块的超时日志当根因；保存 CPU0+CPU1 完整日志；对照本次 project.map（勿套别的方案内存结论）；逐层最小化：电源/时钟→驱动→数据流→协议→业务。
2. 构建问题：改代码没生效 → 查是否加入 cdkproj / FLASH 排除 / 同名符号来自 libs/*.a / 烧的是不是本次 APP.bin；Core CRC 失败 → 先清重建 Core 再 App，**链接区溢出/CRC 失败的产物不可烧录**。
3. 启动问题：无日志 → UART 脚由 config.cfg 决定别照搬他板；无 `CPU1 ready!` → Core 镜像/0x10001000/heap/RX/SKB/共享 SRAM/Mailbox；PSRAM/heap 失败 → AI、LVGL、视频方案内存预算**不可互相复用**。
4. 屏显：黑屏先用**纯色 framebuffer**验证面板/电源/复位/时序，再查视频层/OSD/LVGL/播放器；分层定位"硬件面板初始化→LCDC 输出→framebuffer→图层应用"；不要以背光亮判断初始化成功。
5. RTSP 连上无画面：辅流节点建立？Gen420 尺寸？MSI 有帧？SPS/PPS 输出？URL `.../h264?1`？Socket 是否被阻塞。
6. Wi-Fi 连上不能联网：关联→DHCP→默认网卡→网关→DNS→SNTP/TLS→应用鉴权逐级查。

---

## 10. 官方文档索引

官网还有以下专题页（主文档 §21 列出，部分需登录）：

| 文档 | 与本项目关系 |
|---|---|
| TXSDK 开发入门指南 | 入门流程 |
| TXSDK LLM 开发指南 | **AI 对话/LLM 服务接入（brtc/图生图相关，值得读）** |
| TXSDK Wi-Fi / BLE / BLE 配网 开发指南 | 配网功能实现时读 |
| TXSDK 网络应用开发指南 | lwIP/Socket 范式 |
| TXSDK OTA 开发指南 | 做升级时读 |
| TXSDK AT 指令开发指南 / 主控交互指南 | AT 扩展、外接 MCU 协议 |
| 泰芯芯片 FLASH 常见问题 FAQ / TXW 烧录方法与异常排查 | 烧录/Flash 器件问题（量产烧录必读） |
| Wi-Fi 4 SRRC 测试方法 | 认证 |
| Cat.1 模组接入指南 | USB RNDIS 上网卡（本项目暂不用） |
| TXW82x 学习板资料包 V1.2 / TXW826 硬件资料 | ZIP 下载项，需登录 |
| TXW828-E016FL 等数据手册 | 芯片规格以料号对应手册为准，**不可跨料号套用** |

关键入口（官方主文档 §21.2 与本工程一致）：启动 `sdk/chip/txw82x/system0.c|system1.c`；双核 main `project/txw82xApp/main.c` / `project/txw82xCore/main.c`；配置两工程 project_config.h + sys_config.h；板级 device.c/pin_param.h/config.cfg；构建 BuildBIN.sh + makecode.ini；预编译库 libs/。

---
## 11. 新增章节 1 —— 低功耗开发指南要点

**来源**：官方《TXW82x 低功耗开发指南》，适用 v2.7.1.7；[在线地址](https://taixin-semi.com/zh/docs/txw82x/latest/TXW82x 低功耗开发指南)

### 11.1 三种休眠模式与实测电流

| 模式 | 枚举值 | 说明 | 典型电流（电池相机场景）|
|---|---|---|---|
| 1 | Wi-Fi 保活休眠 | 维持射频链路，可被空口唤醒 | ~338 μA |
| 3 | SRAM 保持休眠 | 关大部分电源域，保留 SRAM 上下文 | ~91 μA |
| 5 | RTC 休眠 | 仅 RTC + 唤醒源，最低功耗 | ~43 μA |

> 对本项目的意义：电池供电相册产品待机可选 **模式 3**（91 μA，~8 天/月）或 **模式 5**（43 μA，~15 天/月）；带 Wi-Fi 唤醒的联网相框用 **模式 1**（~338 μA）。

### 11.2 休眠入口与 Hook 框架

- 入口函数：`int32 system_sleep(uint16 type, struct system_sleep_param *args);`（`type` = 上表枚举）。
- 低功耗框架提供 **3 个 Hook 回调点**：唤醒过滤、收包、RF 就绪；应用可在休眠流程关键节点插入自定义逻辑。
- 应用层统一 `suspend/resume` 框架位于 `sdk/app/lowPower_app`，模块按依赖顺序暂停/恢复，**不要自建休眠流程**。

### 11.3 唤醒引脚选择（本板 AD 键设计约束）

- 官方建议：**优先使用 PA0 ~ PA14 作为唤醒引脚**——该范围被 IO0~IO5 全部 6 路唤醒源覆盖，兼容性最好。
- ⚠️ 本板 6+1 AD 键位于 **PA15**（超出 PA0~14 范围）：**AD 键不能直接作唤醒源**；需按键唤醒时改用 IO 中断脚（PA0~PA14 内空闲脚做独立按键），AD 键仅在唤醒后轮询。
- 唤醒后必须重新初始化时钟域与外设（RTC 休眠会停 40MHz 晶振相关域）。

---
## 12. 新增章节 2 —— 视频应用功能（编解码/录放/RTSP 入口）

**来源**：官方《TXW82x SDK 视频应用功能使用说明》，适用 v2.7.1.7；[在线地址](https://taixin-semi.com/zh/docs/txw82x/latest/TXW82x SDK 视频应用功能使用说明)

### 12.1 三大核心入口一览

| 功能 | 入口 | 输出 | 默认存储路径 |
|---|---|---|---|
| 拍照 | `new_takephoto_msi()` + `msi_do_cmd(photo_msi, MSI_CMD_START, 1, 0)` | JPG 原图 + **320×240 缩略图** | `0:/IMG/...JPG` |
| 录像 | `mp4_record_msi_init()`（可配 `rec_time`/`audio_en`/`mode`/`video_fps`） | MP4（H.264+AAC 8kHz/16bit）| `0:/REC/...MP4` |
| RTSP 图传 | `spook_init()` → `rtsp://<IP>:554/h264`（主）或 `.../webcam`（MJPEG 辅） | 双流 | 网络推送 |

- 一键绑定所有 HTTP 应用（AUTO_H264 + AUTO_JPG）：`config_Viidure(80)`。
- 使用前提：摄像头方案需先完成 **Sensor → MIPI-CSI → ISP → VPP → 编码器** 全链路初始化，否则拍照/录像接口均不生效。

### 12.2 分辨率与帧率限制（⚠️ 跨版本差异）

| 版本 | 720P 帧率 | 1080P 帧率 |
|---|---|---|
| v2.7.0.7 | **25 fps** | 25 fps |
| v2.7.1.7（本工程）| 25 fps | **15 fps** |

> 官方原文："1080P 方案的实际帧率可能为 15，不要固定使用示例中的 25。"

### 12.3 编码/容器/音频格式汇总

| 类型 | 格式 |
|---|---|
| 视频编码 | H.264（MP4/AVI 容器）、MJPG（拍照）|
| 音频编码 | AAC / G.711 (Alaw/Ulaw) / AMR / MP3 / Opus |
| 音频采样率 | 默认 8kHz（本板 photo_frame_config.h 用 16kHz）|
| 音频位宽 | 16bit |

### 12.4 常见故障排查顺序（官方）

SD 卡挂载状态 → 帧率不匹配（1080P 15 vs 25）→ SD 写入速度 → Wi-Fi 信号强度 → 输入帧缓冲队列（fbQ）溢出。

---
## 13. 新增章节 3 —— MSI 框架完整 API 与 TXMPlayer 被动输入

**来源**：官方《TXW82x SDK 框架原理说明》V1.0，适用 v2.7.1.7；[在线地址](https://taixin-semi.com/zh/docs/txw82x/latest/TXW82x SDK 框架原理说明)

### 13.1 MSI 组件 API 完整清单（§2.1 未列全）

| API | 作用 |
|---|---|
| `msi_new()` / `msi_destroy()` | 建 / 毁组件 |
| `msi_find(name)` / `msi_find2()` | 按名查找组件 |
| `msi_add_output(src, NULL, dst, NULL)` | 按组件名连接下游 |
| `msi_del_output()` | 断开连接 |
| `msi_output_fb(fb, care)` | 向下游输出帧（care=0 转移所有权；care=1 无下游时保留调用方引用）|
| `msi_get_fb()` | 从输入队列取帧 |
| `msi_do_cmd(组件, cmd, p1, p2)` | 控制单组件 |
| `msi_cmd2(src, cmd, 0, 0)` | 向整条下游链广播命令 |
| `msi_dump()` | 打印组件连接与帧水位（调试）|

framebuff 关键字段：`data`/`len`、`timestamp`、`mtype`/`stype`、`srcID`、`next`（分片链）、`users`（引用计数）。

### 13.2 TXMPlayer 被动输入模式（§1.5 未提及）

进阶用法：可取到播放器内部 MSI 通道，**直接 push framebuff**——适合自定义 demuxer 或私有网络协议（非 http/https 文件源）。本 brtc 音频流接入后可考虑此模式，避免自建解码链。

---
## 14. 新增章节 4 —— 启动顺序与架构微补充

**来源**：官方《TXW82x SDK 架构与配置说明》V1.0，适用 v2.7.1.7；[在线地址](https://taixin-semi.com/zh/docs/txw82x/latest/TXW82x SDK 架构与配置说明)

### 14.1 CPU0 启动初始化顺序（官方原文）

`main()` → 核心初始化 → **`msi_core_init()`** → Codec → 网络 → VFS → `sys_app_init()`。

> ⚠️ 本条与 §3 已有内容互补：`msi_core_init()` 必须在 Codec / VFS 前调用，否则 MSI 组件无法建链。

### 14.2 Spinlock 共享数据约束（§3.3 未列全）

共享数据必须为**定长 POD 类型**，调用方保证生命周期与缓存一致性（与 §4 Cache/DMA clean/invalidate 规则联动）。`cpu_splock_lock/unlock(id)` 只做互斥，**不解决缓存一致性**。

### 14.3 常用配置宏补充（§3.4 未列全）

| 宏 | 说明 |
|---|---|
| `DEV_SENSOR_GC1084` | IPC demo 默认 720P 摄像头型号 |
| `WIFI_MODE_DEFAULT` | Wi-Fi 模式默认值 |
| `FLASHDISK_EN` | 内部 Flash 文件系统（AI 闹钟方案专用，本工程未用）|

---
## 15. 新增章节 5 —— SDK 版本差异（v2.7.0.7 vs v2.7.1.7，选型表）

**来源**：官方《TXW82x SDK 选型表》；[在线地址](https://taixin-semi.com/zh/docs/txw82x/latest/TXW82x SDK选型表)

| 维度 | v2.7.0.7 | v2.7.1.7（本工程基线）|
|---|---|---|
| 单镜头 | 720P / 1080P / 插值 1080P | 同 |
| 主帧率 | 720P@25fps / 1080P@25fps | 720P@25fps / **1080P@15fps** |
| 录像/拍照 | MP4+AAC / MJPG | 同 |
| 文件系统 | — | **FAT32 / EXFAT** 双支持 |
| 蓝牙 | 配对 | 配对 |
| ISP 调色 | 有 | 有 |
| 低功耗 IPC 方案 | 无 | **新增**（§11 对应）|
| TXMPlayer 媒体播放器 | 无 | **新增**（§1.5/§13.2）|
| PSRAM → Flash 升级 | 无 | **新增**（配合 §6 OTA）|
| AI 库（语音合成/识别/视觉理解）| 无 | **新增**（brtc 基础）|
| 不支持设备清单 | 记录仪、拇指相机、对讲机、BabyMonitor、双目 720P IPC | **同**（选型表原文"暂不支持"）|
| 目标场景 | 常电 IPC / 记录仪 / 对讲机 | 低功耗 IPC、AI 闹钟、网络点唱机、AI 摄像头 |

> ⚠️ **对本项目的意义**：本工程是 **v2.7.1.7 低功耗/AI 路线**，不要套用 v2.7.0.7 常电 IPC 的 1080P@25fps 假设或记录仪/拇指相机用例。

---
## 16. 新增章节 6 —— 文档列表页新发现（遗漏文档盘点）

**来源**：官方文档列表 `https://taixin-semi.com/zh/documentList?eol=false&productScope=category%3Atxw82x`（2026-09-17 抓取，共 29 项，页 1+2 均已核对）

### 16.1 芯片数据手册（TXW828/827/826 子型号，官方 V1.8-V1.9）

| 文档 | 版本 | 更新日期 | 用途 |
|---|---|---|---|
| TXW828-E016FL 数据手册 | V1.8 | 2026-08-06 | 828 大料号电气/引脚/RF |
| TXW828-E08FL 数据手册 | V1.9 | 2026-08-06 | 828 小料号 |
| TXW828-C08FL 数据手册 | V1.8 | 2026-08-06 | 828 C 系列 |
| TXW827-C08 数据手册 | V1.8 | 2026-08-06 | 827 中端（**本板 X001 基芯片需对号**）|
| TXW826-824 数据手册 | V1.9 | 2026-08-06 | 826/824 入门级 |

⚠️ 官方红线：不同料号数据手册**不可互相套用**电气/引脚/RF 规格。本板标注 `TXW827-RGB888_GQ_XC001`，具体料号需与 BOM/FAE 确认后再对应手册。

### 16.2 硬件参考资料

| 文档 | 版本 | 更新日期 | 用途 |
|---|---|---|---|
| TXW82x_DEV_BORAD_v1.2（ZIP）| V1.2 | 2025-12-18 | 官方参考设计板原理图/PCB/结构图（需登录）|
| TXW82x 学习板资料包 V1.2 | — | — | 入门学习板资料（需登录）|

### 16.3 其他在 txw82x 分类下的跨族文档

- TXSDK LLM 开发指南（AI 对话/图生图）
- TXSDK Wi-Fi / BLE / BLE 配网 开发指南
- TXSDK 网络应用开发指南
- TXSDK OTA 开发指南
- TXSDK AT 指令开发指南 / 主控交互指南
- 泰芯芯片 FLASH 常见问题 FAQ / TXW 烧录方法与异常排查
- Wi-Fi 4 SRRC 测试方法
- Cat.1 模组接入指南

上述 8 项 §10 已索引，**未发现本次遗漏的新 TXW82x 前缀开发文档**。

---
## 17. 新增章节 7 —— VFS/SD 卡驱动细节

**来源**：官方《TXW82x SDK 框架原理说明》§VFS + 主文档 §VFS/SD 可靠性章节，[在线地址](https://taixin-semi.com/zh/docs/txw82x/latest/TXW82x SDK 框架原理说明)

### 17.1 SD 卡初始化时序（§6 已提要点，此处补全细节）

1. `sd_init` **先 1-bit 探测**；Host 能力标志 + 卡 SCR 支持才切 **4-bit**。
2. ⚠️ 配置了 DAT1~3 ≠ 已工作在 4-bit，需读 SCR 确认。
3. 热插拔：`sdh_loop` 500ms 轮询（**非独立 CD 引脚中断**）；业务必须监听挂载事件而非"检测到卡"。
4. 掉电恢复：分段文件 + 单调时间戳 + 写后调 `fsync(fileno(fp))`（§1.2 的 `fflush()` 坑）。

### 17.2 FatFS 路径体系速查

| 场景 | 路径前缀 | 示例 |
|---|---|---|
| 客户代码（VFS/POSIX）| `/sd0/` | `fopen("/sd0/photo/a.jpg", "rb")` |
| 内部模块（FatFS 盘符）| `0:/` | `0:/REC/20260917_001.MP4` |
| LVGL 文件系统 | `V:/sd0/` | LVGL 内置 vfs 加载 |

> ⚠️ 新代码**禁用 `osal_fopen`/`osal_fwrite`**（官方红字）。

### 17.3 其余外设驱动

I2C / SPI / UART / ADC / DAC / PWM 驱动细节官方未提供独立专题页，详见 `sdk/hal/`（每个外设一 .c）与 `sdk/driver/hg*`；本章不展开。

---
## 18. 新增章节 8 —— 第 3 页发现的两篇遗漏文档

**来源**：官方文档列表 `https://taixin-semi.com/zh/documentList?eol=false&productScope=category%3Atxw82x` 第 3 页（2026-09-17 抓取）

### 18.1 新增文档清单

| 文档 | 版本 | 更新日期 | 用途 | 优先级 |
|---|---|---|---|---|
| TXW826_HardwareDesignDoc_V1.0.0 | V1.0.0 | 2025-12-18 | TXW826 硬件设计参考（与本板原理图对照）| 中（待确认料号后查阅）|
| TXW8xx SDK BLE配网开发指南 | V1.0 | 2023-11-02 | BLE 配网流程（本项目 STA 模式暂不用，但 AI 闹钟类产品需参考）| 低（本项目暂不用）|

### 18.2 29 项总览结论

本次逐页抓取文档列表（page=1, 2, 3）共 29 项。已在 §10 和 §16 汇总索引的文档不再重复列出。

> ⚠️ 文档列表可能有后续分页（29 项已显示完毕），未发现更多 TXW82x 前缀开发文档。

