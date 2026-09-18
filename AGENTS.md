# AGENTS.md — TXW82x FPV SDK v2.7.1.7 (build 45228)

Vendor SDK (TXsemi/HugeIC) for the TXW82x dual-CPU C-SKY SoC, currently used as the base for the
**AI 数码相框 (AI digital photo frame)** project — see `doc/` and the "AI photo frame project" section below.
Under git since 2026-09-18 — see "Version control" below. C only, UTF-8 (CRLF), Chinese comments throughout.
Toolchain: `csky-elfabiv2-gcc` (minilibc).

## Layout

- `project/txw82xApp/` — CPU0 application firmware. Entry: `main.c` (`main()` → `main_wk` os_work).
  Also `device.c` (static device instances + `*_attach`), `syscfg.c`, `project_config.h`, `sys_config.h`.
- `project/txw82xCore/` — CPU1 Wi-Fi/BLE core (LMAC). Builds first; produces `txw82xcore.bin` used by App.
  Do not touch for app features.
- `project/txw82x.cdkws` — CDK workspace containing both projects.
- `sdk/include/` — all public headers (`basic_include.h` is the umbrella; `devid.h` = device IDs).
- `sdk/app/` — application modules (app_lcd, screen, ui, video_app, encode/decode, spook (RTSP), ...).
- `sdk/demo/` — per-product reference apps + their config headers (selected via CUSTOMER_ID).
- `sdk/lib/` — middleware source (lvgl, freetype, fs, net/lwip, multimedia MSI, rpc/cpurpc, lcd panels).
- `sdk/hal/` + `sdk/driver/` — peripheral HAL (one .c per peripheral) and chip drivers (`hg*` files).
- `sdk/chip/txw82x/` — startup vectors, per-CPU system init, trap handler.
- `libs/` — prebuilt closed `.a` (wifi, h264, lcdc, isp, boot, pmu...). `0`/`1` suffix = CPU0/CPU1 build. No source in tree.
- `csky/`, `ohos/` — two RTOS backends: AliOS Rhino (csky/) and LiteOS-M (ohos/), each with a `txsemi/` OSAL adapter.
- `tools/` — crash/stack analysis helpers (`cpu.txt` register tables, `alios_stack.exe`).

## Build

Vendor build is T-Head CDK (`.cdkproj`), NOT CMake — despite "Cmake" in the workspace path there is no
CMakeLists.txt in this tree. The CMake migration lives in the sibling repo `D:\work\Dual_Screen_Cmake\TXW82x_FPV`.
SDK release notes live there too. Project-specific docs live in `doc/` (see AI photo frame section).

- Build order: txw82xCore FIRST, then txw82xApp — the core post-build rewrites the app linker script
  (`gcc_csky.ld` from `gcc_csky.ld.i`, patching CORECODE_SIZE/COREBSS_END) and supplies `txw82xcore.bin`.
- Outputs: per-project `Obj/*.elf|*.ihex`, `Lst/*.map` → post-build (`BuildBIN.sh`, `makecode`) produces
  the flashable `APP.bin` in `project/txw82xApp/`.
- Flash: `project/txw82xApp/CSKYFlashProgramerConsole.bat` (programs APP.bin at 0x0).
- Output bin naming: `sdktools makecode` (via makecode.ini) also emits a copy named
  `txw82xApp_{sdk ver}_{svn}_app-{APP_VERSION}_{date}_{suffix}.bin`, where `{suffix}` is parsed from
  the **comment text next to the active CUSTOMER_ID in project_config.h** — keep that comment empty/ASCII-only
  (e.g. `* 10`), otherwise Chinese/brackets end up in the filename.

## Configuration chain (no Kconfig)

`project/txw82xApp/project_config.h` → `CUSTOMER_ID` (currently **5** = `sdk/demo/ipc_720p_demo/ipc_720p_config.h`,
defines `IPC_720P_DEMO` so main.c calls `ipc_720p_demo_init()`; 6 = lcd_720p demo, 1–3 = AI demos...).
Feature macros live in the demo config header; `sys_config.h` holds SDK defaults — override in the demo
config, never edit sys_config.h defaults.

`pin_param.h` is generated from `config.cfg` by `pin_bin.exe`/`makecode` — entries may only be appended at
the end, never reordered. Board pin tables: `project/txw82xApp/cfg/*.cfg`.

## Architecture rules

- Dual CPU: CPU0 = application, CPU1 = Wi-Fi core. Cross-CPU calls go through cpurpc
  (`cpu_rpc_init`, mailbox IPC; `sdk/lib/rpc/cpurpc`). Shared state via `CoreSetting` (sys_config.h).
- Layering: app/demo → sdk/lib (MSI middleware) → sdk/hal wrappers → sdk/driver (`hg*`) → registers.
  Apps never touch registers directly; devices are instantiated in `project/txw82xApp/device.c` and
  attached with `hgx_attach(HG_*_DEVID, ...)` (IDs in `sdk/include/devid.h`).
- OSAL only: use `os_*` APIs (`os_task_init`, `OS_TASK_INIT`, `os_sleep_ms`, `os_sem_*`, `os_work`,
  `os_printf`). Never call `LOS_*`/kernel APIs directly from app code.
- Media pipeline is MSI components (`sdk/include/lib/multimedia/msi.h`): `msi_new` + `msi_add_output` +
  `msi_cmd2(msi, MSI_CMD_..., ...)`; components talk via named ports.
- Video frame buffers come from `av_psram_malloc`/`av_malloc` heaps — not plain malloc.

## Display / dual-screen

"Dual screen" here = single LCDC/DSI with two video layers p0/p1 + OSD layer.
`sdk/app/app_lcd/lcd_core.c` (`LCD_CORE_CHAN == 2`) is the channel scheduler multiplexing two clients
onto p0/p1. LVGL renders via `lvgl_osd_msi.c` → `osd_encode_msi.c` → `sdk/driver/osd_enc/hgosd_enc.c`.
Panel drivers in `sdk/lib/bus/spi/lcd/` (ili9881c, st7701s, st7789v, ...): each compiled only when its
`LCD_*_EN` macro is on and overrides the `__weak lcddev_t lcdstruct` from `sdk/lib/lcd/lcd_v3.c` —
exactly one panel must be enabled or the config silently falls back.
`hal/dual_org.c` (DUALORG) is a frame save/recover block for PSRAM, not a second physical screen.

## Conventions

- Quoted includes only (`#include "typesdef.h"`, `"hal/lcdc.h"`, `"dev/lcdc/hglcdc.h"`,
  `"app_lcd/app_lcd.h"`) — angled brackets only for third-party (`"lwip/..."`, csi headers).
  Include roots: `sdk/include`, `sdk/`, `sdk/app`, project dir.
- snake_case; vendor prefixes: `hg*` drivers, `HG_*` device IDs/macros, `os_*` OSAL, `msi_*` media,
  `screen_*` UI framework (`sdk/app/screen/` — Activity-like lifecycle: on_create/on_resume/...).
- Logging: `os_printf(KERN_DEBUG "...\n", ...)` (KERN_DEBUG/INFO/NOTICE/ALERT); verbosity via `print_level(n)`.
- Gotchas: functions that "have no source" live in `libs/*.a`; `__init`/`__initdata` are currently
  no-op macros; some legacy macros contain load-bearing typos (e.g. `CONFI_CORE_UARTDEV`) — do not
  "fix" blindly; generated files (`pin_param.h`, `compile_commands.json` in `.cache`, `psram.bin`,
  `txw82xcore.bin`, `svn_version.h`) must not be hand-edited.

## AI photo frame project (ai_album)

Target product: 1024×600 landscape AI digital photo frame on board TXW827-RGB888_GQ_XC001.
**Work history and current status: see `WORKLOG.md`.** This section holds only durable facts.

Docs in `doc/` (requirements, pin_map, panel datasheet, original panel template). ⚠ pin_map.md is from the V1.1 schematic; `doc/` also holds V2.0 — re-verify any pin change against the V2.0 PDF before trusting V1.1.

Hardware facts (verified on board unless noted):

- Display: panel driver `sdk/lib/bus/spi/lcd/hx8282.c` (`LCD_HX8282_EN`, colrarray=0 verified). LCD pin map lives in `config.cfg`: LCD_D0-D7=B, D8-D15=G, D16-D23=R, DE=PA_4, DOTCLK=PA_3, VS/HS/TE=255. All FPV-board peripherals colliding with these pins (DVP/MIPI camera, DSI, touch, FEM, PDM, SPI1/SPI2, GMAC) are set to 255 — do not re-enable.
- Panel power: AVDD_EN PD13 → 50 ms → backlight PA_9 (`LCD_BACKLIGHT_IO` param) after first LCD frame (`lcd_ready_callback`). PD12 = DAC/amp enable, NOT panel VGH/VGL (panel self-generates rails from AVDD).
- Keys: 6+1 AD keys on PA15 ladder. Board key table in `sdk/lib/key/adkey.c` under `#elif defined(PHOTO_FRAME_DEMO)` (measured mV: power 0 / OK 670 / right 1620 / M 1843 / left 2226 / up 2600 / down 2929); action mapping in `sdk/demo/photo_frame_demo/photo_frame_key_input.c`.
- SD card: 1-bit SDIO CLK=PB7, CMD=PC12, DAT0=PC11. Audio: analog MIC_P/N, 8002D amp (enable on PC6 — shared with debug UART; watch when audio lands). Battery ADC PB6.
- Debug UART: UART0 on PC6/PC7.

UI architecture (ported from sibling `D:\work\Dual_Screen_Cmake\TXW82x_FPV\applications\ai_album`):

- ALL pages are ported under `sdk/demo/photo_frame_demo/` (CUSTOMER_ID 10): home, album×3, translate, AI chat, practice, settings family, power dialog, plus the original router. Page code is verbatim; the only 9.0/9.5 adaptation layer is `ui/ai_album_compat.h` (display/btnmatrix/del-cnt/image renames) — include it instead of bare `lvgl.h`, and extend it rather than editing ported pages.
- Service layer is STUBBED at three boundaries — replace stubs one-by-one with real implementations (port from sibling `src/`): `album/ai_album_album_engine_stub.c` (SD store / JPEG HW decode / async loader / image-AI / gallery loader), `brtc_agent/brtc_agent_stub.c` (voice agent — always returns UNINITIALIZED), `ai_album_services_stub.c` (volume/brightness/power/weather/wifi/ble/storage/time + ball-test page). Ported album logic (art/navigation/slideshow/photo_cache/status/perf) lives in `album/*.c`.
- Keys bypass the vendor LVGL keypad indev: keyWork → `photo_frame_key_input.c` → `ai_album_ui_input_post()`; UI entry is `ai_album_ui_bootstrap` via `app_lvgl_init(…, 0)`; UI processing is a 30 ms lv_timer (`ui/ai_album_ui.c`).
- Excluded: `ui/pages/ai_album_ball_test_page.c` (needs 9.5 layer/triangle draw API; its functions are stubbed) and the placeholder page (superseded by full router). Default UI language is English (embedded CJK font is a ~430-char subset; full fonts come with the SD bitmap-font system later).

Relevant SDK building blocks: `sdk/demo/ai_demo/coze_demo/ai_dialogue` (voice AI dialogue + LVGL UI, base for AI 对话), `ai_alarm_clock` (clock-style product UI), `sdk/lib/net` (http/curl/mqtt/llm for AI + weather), `sdk/lib/audio_proc_lib` (AEC/ANS/VAD), `sdk/app/decode` (JPEG decode).

## Version control

- Baseline commit `eb4d29c` (2026-09-18) tracks the whole tree, prebuilt `libs/*.a` included.
  CDK build output, generated `.bin/.elf/.map` and `debug/` board logs are ignored — see `.gitignore`.
  Remote `origin` = `git@github.com:23110wei/82x-v2.7.1_ai_album.git` (SSH).
- `.gitattributes` sets `* -text` because the tree deliberately mixes CRLF (C sources) and LF
  (`BuildBIN.sh`, `precompile.sh`, ...); files must stay byte-exact, so do not add EOL conversion.
- Commit each unit of work separately and append a numbered section to `WORKLOG.md`.

## Key entry points

`project/txw82xApp/main.c` · `project/txw82xApp/device.c` · `project/txw82xApp/project_config.h` ·
`sdk/include/basic_include.h` · `sdk/include/devid.h` · `sdk/demo/ipc_720p_demo/ipc_720p_config.h` ·
`sdk/app/app_lcd/app_lcd.c` + `lcd_core.c` · `sdk/app/screen/screen_manager.h` ·
`sdk/include/lib/multimedia/msi.h`
