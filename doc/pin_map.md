# ai_album 硬件引脚映射 (pin_map)

> **数据来源**：`docs/TXW827-RGB888_GQ_XC001 V1.1原理图.pdf`
> **主控**：U5 = TXW827-C08（CSKY e804df，69-pin QFN，含中心 EPAD）
> **状态**：空白模板（按引脚顺序）。芯片引脚名已填，**外部信号 / 功能两列待按原理图填入**。

---

## 1. 主控 U5 (TXW827-C08) 引脚表（按引脚顺序）

| 引脚号 | 芯片引脚名称 (内部) | 外部信号 / 网络名 | 功能说明 |
| :---: | :--- | :--- | :--- |
| 1 | VCCPA | VCC_3V3 |  |
| 2 | VCCRF | VCC_3V3 |  |
| 3 | PC1 | B7 |  |
| 4 | PC0 | B6 |  |
| 5 | PB15 | B5 |  |
| 6 | PB14 | B4 |  |
| 7 | PB13 | B3 |  |
| 8 | PB12 | B2 |  |
| 9 | PB11 | B1 |  |
| 10 | PB10 | B0 |  |
| 11 | PB9 | G7 |  |
| 12 | PB8 | G6 |  |
| 13 | PB7 | S_CLK |  |
| 14 | PB6 | PB6_BAT_ADC |  |
| 15 | VCCAU27 | AUDIO_VDD |  |
| 16 | MIC_P | MIC_P |  |
| 17 | MIC_N | MIC_N |  |
| 18 | VCCAU33 | VCCAU33 |  |
| 19 | VCMAU | VCMAU |  |
| 20 | LOUT_N | LOUT_N |  |
| 21 | LOUT_P | LOUT_P |  |
| 22 | AGND | AGND |  |
| 23 | PC13 | G5 |  |
| 24 | PC12 | CSI_SDA |  |
| 25 | PC11 | CSI_SCL | 与 SD 无关；PC14/PC15 是 ADC 模拟脚、不在数字电源域（SD_D0 实际接 PC11） |
| 26 | PC10 | G4 |  |
| 27 | PC9 | G3 |  |
| 28 | VCAM2/PC8 | VCAM_2 |  |
| 29 | VCC18 | VCC18 |  |
| 30 | PB5 | QSPI_MISO |  |
| 31 | PB4 | QSPI_CS |  |
| 32 | PB3 | QSPI_WP |  |
| 33 | PB2 | QSPI_HOLD |  |
| 34 | PB1 | QSPI_CLK |  |
| 35 | PB0 | QSPI_MOSI |  |
| 36 | VCC_FLS | VCC_FLS |  |
| 37 | FSDP/PD13 | VADD_EN |  |
| 38 | FSDM/PD12 | DAC_EN |  |
| 39 | PD7 | G2 |  |
| 40 | PD6 | G1 |  |
| 41 | PD5 | G0 |  |
| 42 | PD4 | R0 |  |
| 43 | PD3 | R1 |  |
| 44 | PD2 | R2 |  |
| 45 | PD1 | R3 |  |
| 46 | PD0 | R4 |  |
| 47 | PA0 | R5 |  |
| 48 | PA1 | R6 |  |
| 49 | PA2 | R7 |  |
| 50 | PA3 | DLCK |  |
| 51 | PA4 | DE |  |
| 52 | PA5 | PA5_PWR_ON/OFF |  |
| 53 | HSDP/PC6 | PC6 |  |
| 54 | HSDM/PC7 | PC7 |  |
| 55 | VCC | VCC_3V3 |  |
| 56 | VCC1 | VCC_3V3 |  |
| 57 | VCAM/PA13 | VCAM |  |
| 58 | VCCD/VCCA | VCC_3V3 |  |
| 59 | PA6/PA9 | PA9_TMS (PA9_LCD_LED) |  |
| 60 | PA7/PA10 | PA10_TCK(PA10_USB_DET) |  |
| 61 | PA15 | PA15_ADKEY |  |
| 62 | VDD | VDD1V15 |  |
| 63 | XI | XI |  |
| 64 | XO | XO |  |
| 65 | VDD15O | VDD15 |  |
| 66 | VDD15L | VDD15 |  |
| 67 | VDD15R | VDD15 |  |
| 68 | ANT | ANT |  |
| 69 | EPAD |  |  |

---

## 2. LCD 排线 J2 (FPC50，0.5mm，翻盖下接) — RGB888 1024×600

> 50 个信号脚 + 2 个屏蔽脚(Shd) = 52 位。「主控对应引脚」只填 §1 已确认的 RGB/同步位，其余待补。

| 排线脚 | 网络名 | 功能 | 主控对应引脚 |
| :---: | :--- | :--- | :--- |
| 1 | VLED+ | 背光 LED 正（升压后） | — |
| 2 | VLED+ | 背光 LED 正 | — |
| 3 | VLED- | 背光 LED 负 | — |
| 4 | VLED- | 背光 LED 负 | — |
| 5 | GND | 地 | — |
| 6 | Vcom | 公共电压 | — |
| 7 | DVdd | 数字电源 3.3V（LCD_3V3） | — |
| 8 | MODE | 模式选择（DE/HV） |  |
| 9 | DE | 数据使能 | PA4 |
| 10 | VS | 场同步 |  |
| 11 | HS | 行同步 |  |
| 12 | B7 | 蓝 B7 | PC1 |
| 13 | B6 | 蓝 B6 | PC0 |
| 14 | B5 | 蓝 B5 | PB15 |
| 15 | B4 | 蓝 B4 | PB14 |
| 16 | B3 | 蓝 B3 | PB13 |
| 17 | B2 | 蓝 B2 | PB12 |
| 18 | B1 | 蓝 B1 | PB11 |
| 19 | B0 | 蓝 B0 | PB10 |
| 20 | G7 | 绿 G7 | PB9 |
| 21 | G6 | 绿 G6 | PB8 |
| 22 | G5 | 绿 G5 | PC13 |
| 23 | G4 | 绿 G4 | PC10 |
| 24 | G3 | 绿 G3 | PC9 |
| 25 | G2 | 绿 G2 | PD7 |
| 26 | G1 | 绿 G1 | PD6 |
| 27 | G0 | 绿 G0 | PD5 |
| 28 | R7 | 红 R7 | PA2 |
| 29 | R6 | 红 R6 | PA1 |
| 30 | R5 | 红 R5 | PA0 |
| 31 | R4 | 红 R4 | PD0 |
| 32 | R3 | 红 R3 | PD1 |
| 33 | R2 | 红 R2 | PD2 |
| 34 | R1 | 红 R1 | PD3 |
| 35 | R0 | 红 R0 | PD4 |
| 36 | GND | 地 | — |
| 37 | DLCK | 像素时钟 PCLK | PA3 |
| 38 | GND | 地 | — |
| 39 | L/R | 左右翻转 |  |
| 40 | U/D | 上下翻转 |  |
| 41 | Vgh | 栅极高压 | —（vg_EN 使能） |
| 42 | Vgl | 栅极低压 | —（vg_EN 使能） |
| 43 | Avdd | 模拟电源 | —（AVDD_EN 使能） |
| 44 | RESET | 屏复位 |  |
| 45 | NC | 空 | — |
| 46 | Vcom | 公共电压 | — |
| 47 | DLTHB | — |  |
| 48 | GND | 地 | — |
| 49 | NC | 空 | — |
| 50 | NC | 空 | — |
| 51 | Shd | 屏蔽地 | — |
| 52 | Shd | 屏蔽地 | — |

---

## 3. LCD 电源 / 背光控制

> 开机序：AVDD → VGH/VGL → 背光；关机反向；各路间隔 ≥50ms（原理图注释）。

| 电源 / 控制 | 网络 / 作用 | 使能控制 | 主控对应引脚 |
| :--- | :--- | :--- | :--- |
| VLED+ / VLED- | 背光 LED（U1 MT3608L 升压 + Q1 开关） | LCD_LED | PA9 |
| AVDD | 模拟电源 ~5V（U6 MT3608L） | AVDD_EN | PD13 |
| Vgh | 栅极高压 18V（U3 MT3608L） |  |  |
| Vgl | 栅极低压 -6V（U4 TP7660H） |          |  |
| DVdd / LCD_3V3 | 数字逻辑 3.3V | LCD_3V3 | VCC_3V3 |
| 总电源开关 | PA5_PWR_ON/OFF | PA5 | PA5 |

---

## 4. SD 卡座 (microSD，JSD13PCS)

| 卡座脚 | 网络名 | 功能 | 主控对应引脚 |
| :---: | :--- | :--- | :--- |
| 1 | DAT2 | 数据 2（4-bit 模式） |  |
| 2 | CD/DAT3 | 数据 3 / 卡检测 |  |
| 3 | CMD | SD_CMD 命令 | PC12 |
| 4 | VDD | 3.3V 供电 | — |
| 5 | CLK | SD_CLK 时钟 | PB7 |
| 6 | VSS | 地 | — |
| 7 | DAT0 | SD_D0 数据 0 | PC11 |
| 8 | DAT1 | 数据 1（4-bit 模式） |  |
| 9 | C/D | 卡插入检测 |  |
| 10~13 | GND1/2/3 | 外壳地 | — |

> 1-bit SDIO 实测确认（2026-08-19）：CLK=PB7、CMD=PC12、DAT0=**PC11**（DAT1~3 未接）。
> DAT0 旧记 PC15 有误（模拟脚不可用）；PC11 未出现在 §1 表中，封装引脚号待对照原理图补录。

---

## 5. USB-C (TYPE-C 6-PIN，座子 TYPE1)

| 座子脚 | 网络名 | 功能 | 主控对应引脚 |
| :---: | :--- | :--- | :--- |
| 1 | VBUS | +5V 输入 | — |
| 2 | VBUS | +5V 输入 | — |
| 3 | CC1 | Type-C 配置（R62 5K1 下拉） | — |
| 4 | CC2 | Type-C 配置（R59 5K1 下拉） | — |
| 5 | GND | 地 | — |
| 6 | GND | 地 | — |

| 相关信号 | 网络名 | 功能 | 主控对应引脚 |
| :--- | :--- | :--- | :--- |
| USB D+ | HSDP | USB 数据 + | PC6 |
| USB D- | HSDM | USB 数据 - | PC7 |
| USB 接入检测 | USB_DET | VBUS 分压检测 | PA10 |

> 注：6-PIN Type-C 通常只有电源 + CC；D+/D- 是否走该座子待确认（§1 把 PC6/PC7 标为 USB 数据）。

---

## 6. 音频 (麦克风 / 功放 / 喇叭)

| 信号 | 网络名 | 功能 | 主控对应引脚 |
| :--- | :--- | :--- | :--- |
| 麦克风 + | MIC_P | MIC1 座子正（2P 1.25mm） | MIC_P（脚 16） |
| 麦克风 - | MIC_N | MIC1 座子负 | MIC_N（脚 17） |
| 音频出 + | LOUT_P | 功放 U7(8002D) 输入 + | LOUT_P（脚 21） |
| 音频出 - | LOUT_N | 功放 U7(8002D) 输入 - | LOUT_N（脚 20） |
| 喇叭 + | VOP | 功放输出 → SP1 座子 + | —（U7 输出） |
| 喇叭 - | VON | 功放输出 → SP1 座子 - | —（U7 输出） |
| 功放使能 | SHUTDOWN | 功放开关 | PC6(1en\0dis) |

---

## 7. 按键

> 6 个 AD 按键共用 PA15（电阻分压矩阵）；SW7 / SW8 为独立按键。

| 按键 | 分压电压 | 网络名 | 主控对应引脚 |
| :---: | :---: | :--- | :--- |
| OK | 0.67 V | PA15_ADKEY | PA15 |
| →（右） | 1.32 V | PA15_ADKEY | PA15 |
| M（菜单） | 1.62 V | PA15_ADKEY | PA15 |
| ←（左） | 1.98 V | PA15_ADKEY | PA15 |
| ↑（上） | 2.31 V | PA15_ADKEY | PA15 |
| ↓（下） | 2.65 V | PA15_ADKEY | PA15 |
| 断电复位（SW7） | — | 独立硬复位 | — |
| Ai / 开机键（SW8） | — | SYS_EN | — |

> ⚠️ 按键 ↔ 电压对应按原理图坐标推断，需实测确认。需求文档定义 7 类按键：开关 / M / 上 / 下 / 左 / 右 / OK。

---

## 8. 调试接口 (JTAG / 串口)

| 信号 | 网络名 | 功能 | 主控对应引脚 |
| :--- | :--- | :--- | :--- |
| TMS | PA9_TMS | JTAG 模式选择 | PA9 |
| TCK | PA10_TCK | JTAG 时钟 | PA10 |
| 调试串口 TX | TX1 | 调试 UART 发送 | PC6 |
| 调试串口 RX | RX1 | 调试 UART 接收 | PC7 |
| VCC | VCC_3V3 | 调试供电 | — |
| GND | GND | 地 | — |

> DEBUG 排针 J8~J11（4 脚）+ 调试串口 TX1/RX1；UART 引脚待确认。

---

## 9. Flash / 其它

### QSPI Flash U8 (PY25D32SH，32Mbit，SOP8)

| 信号 | 网络名 | 功能 | 主控对应引脚 |
| :--- | :--- | :--- | :--- |
| CS# | QSPI_CS | 片选 | PB4          |
| SI (IO0) | QSPI_MOSI | 主出从入 | PB0 |
| SO (IO1) | QSPI_MISO | 主入从出 | PB5 |
| SCLK | QSPI_CLK | 时钟 | PB1 |
| WP# (IO2) | QSPI_WP | 写保护 | PB3 |
| HOLD# (IO3) | QSPI_HOLD | 保持 | PB2 |
| VCC | VCC_3V3 | 电源 | — |
| VSS | GND | 地 | — |

### 其它板上模块

| 模块 | 网络 / 作用 | 主控对应引脚 |
| :--- | :--- | :--- |
| 40MHz 温补晶振 X1 | 主时钟 | XI（脚 63）/ XO（脚 64） |
| WiFi/BT 射频 | ANT → 匹配网络 → PCB 天线 | ANT（脚 68） |
| 电池电量检测 | PB6_BAT_ADC | PB6（脚 14） |
| 锂电充电 | TP4056 (U10)，USB VBUS → 电池 | —（独立电源管理） |
| 充电 / 电源指示灯 | 充电灯 / 电源灯 | — |
