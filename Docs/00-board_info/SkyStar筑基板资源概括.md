## 1. 硬件模块概览 (Functional Blocks)
- [cite_start]**图纸总述**: 天空星筑基学习板是一款专为STM32天空星(当前为STM32F407VET6版)核心板设计的全能型扩展底板，支持8V-24V宽压输入，并提供多总线通信、运动控制及多种传感器采集评估 [cite: 7]。
- [cite_start]**包含的核心模块**: 宽电压输入与多级稳压电路（含防反接与过压保护）、双隔离电源、以太网(LAN8720A)、隔离CAN(TJA1042T)与隔离RS485(SP485EEN)、传感器(六轴IMU ICM-42688-P, 温湿度 AHT20-F, 高精度RTC SD3078, 24位ADC HX711)、音频编解码(ES8388)及D类功放(HT6872x2)、多路电机驱动(双直流AT8236, 步进TMC2209, 外部光耦步进接口)、IO与I2C扩展缓冲(PCA9555PW, PCA9517)、RGB全彩指示灯(WS2812) [cite: 7, 125, 126, 128, 130]。

## 2. 核心主控/外设引脚映射表 (Pinout Mapping)
| 芯片/模块 | 引脚号 | 引脚名称 | 物理网络标号 (Net) | 连接对象及说明 |
| :--- | :--- | :--- | :--- | :--- |
| MCU_Core | 未标明 | PA6 (TIM13_CH1) | BUZZER_PWM | [cite_start]接单刀双掷模拟开关(U67)，分时复用驱动有源或无源蜂鸣器 [cite: 256, 291]。 |
| MCU_Core | 未标明 | PD3 | TF_OR_EXT_IO_SWITCH | [cite_start]TF卡插入检测脚，上拉默认高电平，插入后拉低，控制模拟开关切换SDIO总线流向 [cite: 881, 892]。 |
| MCU_Core | 未标明 | PC12, PD2, PC8-PC11 | SDIO_CLK, CMD, D0-D3 | [cite_start]SDIO通信引脚，受PD3控制分流至板载TF卡或外部排针 [cite: 874-881, 889-891]。 |
| MCU_Core | 未标明 | PA4, PA5, PA6, PA7 | SPI_CS, CLK, MISO, MOSI | [cite_start]核心板SPI1总线，专用于屏幕驱动，核心板板载Flash被屏蔽禁用 [cite: 882-886, 893]。 |
| MCU_Core | 未标明 | PB14, PB15 | TIM12_CH1, TIM12_CH2 | [cite_start]经模拟开关复用于舵机PWM控制或双路直流电机中的电机2控制 [cite: 1316]。 |

## 3. 关键通信总线拓扑 (Bus Topology)
- **I2C1**:
  - [cite_start]**SCL/SDA**: 核心板引出后连接至 PCA9517 (I2C中继缓冲器) 以降低总线电容、增强驱动能力并加快上升沿至约600ns [cite: 1132, 1134][cite_start]。随后挂载 ES8388 (0x10), PCA9555PW (0x20), SD3078 (0x32), AHT20-F (0x38), INA226 (0x40), AT24C02 (0x50) [cite: 1124-1125][cite_start]。PCA9555PW 内部有 100kΩ 弱上拉电阻 [cite: 330]。
- **SPI2 / I2S2**:
  - [cite_start]**SCK/MOSI/MISO**: 挂载 W25Q128 (Flash) 与 ICM-42688-P (IMU) [cite: 964-967][cite_start]。通过模拟开关与音频芯片 ES8388 的 I2S2 数据流实现引脚的物理级分时复用 [cite: 388, 961]。
- **CAN总线**:
  - [cite_start]**TX/RX**: MCU_CAN_TX/RX -> CA-IS3722CHS (数字隔离器) -> TJA1042T (CAN收发器) -> 总线 [cite: 778][cite_start]。隔离器两侧分别由核心板3.3V与隔离电源模块(IPS1_VCC_5V)供电 [cite: 778][cite_start]。外设端包含跳线帽(JUMP3)用于选择接入 120Ω 终端电阻，并配有 PESD2CAN 防静电保护二极管 [cite: 780, 782]。
- **RS485总线**:
  - [cite_start]**TX/RX/DE**: UART信号及方向控制脚(RE/DE)通过两颗隔离芯片实现双重隔离 -> SP485EEN -> 总线 [cite: 786, 787][cite_start]。外设端配置 R40 终端匹配电阻(跳线帽选择)及 CDSOT23-SM712 TVS保护管 [cite: 789]。

## 4. 电源树结构 (Power Tree)
- **供电路径**: 
  - [cite_start]**主降压链路**: 8V~24V输入 (DC插座/接线端子, 拨动开关二选一) -> 防反接(单向TVS)与自恢复保险丝(5A) -> 共模滤波(L2) -> INA226电流采样(15mΩ) -> XL4015E1 DCDC -> 5V (5A MAX) [cite: 626, 628, 630]。
  - [cite_start]**主稳压与保护链路**: DCDC 5V输出 -> TL431+PMOS 过压保护电路(输出电压超5.3V自动关断) -> DCDC_OUT_VCC_5V 节点 [cite: 632]。
  - [cite_start]**3.3V分支**: DCDC_OUT_VCC_5V 经三路独立的 SCJA1117B-3.3 (LDO) 搭配 MT9700 限流芯片，分别产生 SKYSTAR_+3V3 (核心板专属)、ONBOARD_+3V3 (板载外设) 和 EXT_+3V3_OUT (外部排针) [cite: 509, 511-513]。
  - [cite_start]**电机直驱链路**: 8V~24V电源经滤波后直接供给 TMC2209 步进驱动与 AT8236 双直流驱动芯片 [cite: 499-501]。
  - [cite_start]**隔离链路**: DCDC_OUT_VCC_5V -> 双路 5V 隔离电源模块 -> 分别为CAN/RS485/继电器及外部步进电机光耦接口独立供电 [cite: 508, 662]。
- [cite_start]**关键去耦电容**: XL4015E1 输入与输出端均部署了低ESR固态电容以保障大功率稳定输出 [cite: 124, 629]。

## 5. 被动元件与外围配置 (Passive BOM & Config)
- [cite_start]**晶振电路**: SD3078 内部集成带温度补偿的晶体振荡器(TCXO, 3.8PPM) [cite: 125]。
- **拨码开关/跳线帽**: 
  - [cite_start]**SW7 (8路功能冲突切换拨码)**: 物理连接至 PCA9555PW IO0 及各模拟开关S引脚 [cite: 186, 357]。
    - [cite_start]BIT1: 切换音频功放静音/开启。其下拉电阻特制为 1kΩ (而非10kΩ)，使分压降至 0.032V，满足 HT6872 的绝对关断阈值 (<0.2V) [cite: 335, 336, 340-341]。
    - [cite_start]BIT2~BIT8: 分别负责切换 HX711/ADC、SPI2/I2S2、有源/无源蜂鸣器、双舵机/直流电机2、EC11/电机2编码器、板载RGB/外接灯条、板载步进驱动/外接光耦步进 [cite: 388, 390]。
  - [cite_start]**SW3 (步进电机配置)**: 3位拨码开关，用于配置 TMC2209 的细分步数(MS1, MS2，默认8细分)及工作模式(SPREAD，默认静音模式) [cite: 1221-1222]。
  - [cite_start]**SW2 (EEPROM配置)**: 4位拨码开关，用于手动设定 AT24C02 的硬件写保护(WP)及 I2C 器件地址(A0-A2) [cite: 1128]。
- **LED/按键**: 
  - [cite_start]用户按键SW5/SW6及EC11轴向按键用于输入采集 [cite: 130, 1558]。
  - [cite_start]8枚白色状态LED经由 PCA9555PW 的 IO1(IO1_0~IO1_7) 驱动 [cite: 128, 1556][cite_start]。板载3枚 WS2812 RGB LED，由核心板 3.3V 信号经 SN74LVC1T45DBVR 转换为 5V 电平后驱动 [cite: 1387-1388]。

## 6. 视觉审核警告 (Hardware Warnings)
- **悬空引脚**: 未标明。
- **潜在冲突**: 
  - [cite_start]**电源输入短路风险**: 8V-24V接线端子与DC插座严禁同时上电且不操作选择开关，必须通过顶部的单刀双掷拨动开关物理隔断不需要的输入源 [cite: 215, 217]。
  - [cite_start]**拨码控制电平冲突**: SW7拨码开关与 PCA9555PW 扩展芯片共同控制模拟开关选通。若手动拨码强制拉高(ON)而软件向PCA9555PW对应IO写低，会引发冲突。为此两者之间串联了 1kΩ 限流保护电阻 (R154-R161)，将其短路电流限制在安全的 3.3mA，并确立了软件控制享有绝对覆写优先级 [cite: 346, 350-354]。
  - [cite_start]**TF卡与排针总线互斥**: 当TF卡座被物理插入时，PD3 引脚机械拉低，硬件级强制触发模拟开关断开外部 SDIO排针的网络连接，此时排针引脚完全失效，同时点亮 LED2 警示用户 [cite: 865, 881, 896]。

## 7. 细节外设与防坑指南 (Peripherals & Pitfalls)
- **屏幕与触摸接口**: 
  - **触摸总线**: I2C2 默认分配给外扩屏幕的电容触摸使用。
  - **背光复用**: 控制绿色 LED16 的 GPIO 和 LCD 屏幕背光物理共用引脚。若接屏幕时不需点亮 LED16，需拔掉板上的 JUMP13 短接帽。
- **编码器 Z 相中断处理**: 步进电机编码器在 AB 相外增加 Z 相（零位）信号。因 STM32F4 定时器 ENCODER 模式原生不支持 ABZ 模式，硬件上将 Z 相信号被单独接入了外部中断（EXTI）引脚，闭环步进控制需依赖软件中断配合。
- **独立与扩展接口**: 
  - **独立串口**: UART2 为完全独立串口，通过排针引出，无功能冲突。
  - **SDIO 替代复用**: 未插 TF 卡（模拟开关导通至外部排针）时，外部排针的 SDIO 接口除了接 SD 卡，还可被软件配置复用为 SPI3、UART4 和 UART5。
- **供电安全警告**: 
  - **排针反向供电禁忌**: 底板排针上的 O3V3 和 O5V0 引脚严禁用于对底板反向供电（此处无防反接保护，极易烧板），仅限对外输出。不驱动电机时，首推使用天空星核心板的 Type-C 供电。