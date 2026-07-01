## 1. 硬件模块概览 (Functional Blocks)

* **图纸总述**: 本图纸为“天空星-筑基学习板”的 SPI DEVICE 模块原理图（版本V1.0），主要实现 SPI 与 I2S 总线的物理引脚切换，并集成了板载 SPI Flash 与 6轴 IMU 传感器模块。主控性能、频率、内存等参数未标明。
* **包含的核心模块**:
* 信号复用切换模块 (由 U31, U33, U34 模拟开关组成)
* 外部 SPI 扩展接口 (H8 排针)
* 板载 SPI Flash 存储电路 (U30: W25Q128JVSIQ)
* 板载 6轴姿态传感器电路 (U32: ICM-42688-P)



## 2. 核心主控/外设引脚映射表 (Pinout Mapping)

| 芯片/模块 | 引脚号 | 引脚名称 | 物理网络标号 (Net) | 连接对象及说明 |
| --- | --- | --- | --- | --- |
| **U31 (复用开关)** | 4 | A | SPI2_SCK, PB10 | 来自 MCU 的 SPI2 时钟信号 |
|  | 1 | B2 | ES8388_SCLK | 连接至板载 ES8388 I2S 时钟 |
|  | 3 | B1 | SPI_CLK | 连接至公共 SPI 时钟总线 |
|  | 6 | S | SPI_OR_I2S_SWITCH | 状态控制端 (低电平->B1导通) |
| **U33 (复用开关)** | 4 | A | SPI2_MOSI, PC3 | 来自 MCU 的 SPI2 MOSI 信号 |
|  | 1 | B2 | ES8388_SD | 连接至板载 ES8388 数据端 |
|  | 3 | B1 | SPI_MOSI | 连接至公共 SPI MOSI 总线 |
|  | 6 | S | SPI_OR_I2S_SWITCH | 状态控制端 (低电平->B1导通) |
| **U34 (复用开关)** | 4 | A | SPI2_MISO, PC2 | 来自 MCU 的 SPI2 MISO 信号 |
|  | 1 | B2 | ES8388_EXT_SD | 连接至板载 ES8388 扩展数据端 |
|  | 3 | B1 | SPI_MISO | 连接至公共 SPI MISO 总线 |
|  | 6 | S | SPI_OR_I2S_SWITCH | 状态控制端 (低电平->B1导通) |
| **U30 (Flash)** | 1 | CS# | SPI_FLASH_CS | 来自 MCU_PE4，串接 R46 (10kΩ) 上拉至 VCC |
|  | 2 | DO | SPI_MISO | 连接至公共 SPI MISO 总线 |
|  | 5 | DI | SPI_MOSI | 连接至公共 SPI MOSI 总线 |
|  | 6 | CLK | SPI_CLK | 连接至公共 SPI 时钟总线 |
| **U32 (IMU)** | 12 | AP_CS | SPI_IMU_CS | 来自 MCU_PE7，串接 R47 (10kΩ) 上拉至 VCC |
|  | 13 | AP_SCL/AP_SCLK | SPI_CLK | 连接至公共 SPI 时钟总线 |
|  | 14 | AP_SDA/AP_SDI | SPI_MOSI | 连接至公共 SPI MOSI 总线 |
|  | 1 | AP_SDO/AP_AD0 | SPI_MISO | 连接至公共 SPI MISO 总线 |
| **H8 (扩展排针)** | 4 | 4 | EXT_SPI_CS0 | 扩展片选0，直连 MCU_PD11 |
|  | 6 | 6 | EXT_SPI_CS1 | 扩展片选1，直连 MCU_PE0 |
|  | 8 | 8 | EXT_SPI_CS2 | 扩展片选2，直连 MCU_PE15 |

## 3. 关键通信总线拓扑 (Bus Topology)

* **SPI 共享总线 (SPI_CLK / SPI_MOSI / SPI_MISO)**:
* **SPI_CLK**: U31_Pin3(B1) -> U30_Pin6(CLK) -> U32_Pin13(AP_SCLK) -> H8_Pin3
* **SPI_MOSI**: U33_Pin3(B1) -> U30_Pin5(DI) -> U32_Pin14(AP_SDI) -> H8_Pin5
* **SPI_MISO**: U34_Pin3(B1) -> U30_Pin2(DO) -> U32_Pin1(AP_SDO) -> H8_Pin7


* **控制逻辑总线 (SPI_OR_I2S_SWITCH)**:
* MCU 侧控制引脚 (未标明具体引脚号) -> 并行连接至 U31、U33、U34 的 Pin6 (S极)。当其为低电平时，MCU_PB10/PC3/PC2 对应路由至 SPI 共享总线；为高电平时，路由至 ES8388 I2S 节点。


* **独立片选总线 (CS)**:
* MCU_PE4 -> SPI_FLASH_CS -> 节点处并联 R46 (10kΩ上拉至3V3) -> U30_Pin1
* MCU_PE7 -> SPI_IMU_CS -> 节点处并联 R47 (10kΩ上拉至3V3) -> U32_Pin12



## 4. 电源树结构 (Power Tree)

* **供电路径**:
* 主源输入点未标明 -> ONBOARD_+3V3 (系统3.3V网络)
* ONBOARD_+3V3 -> R45 (0Ω, 0603) -> U30(W25Q128) VCC (Pin 8)
* ONBOARD_+3V3 -> R48 (0Ω, 0603) -> U32(ICM-42688) VDD (Pin 8)
* ONBOARD_+3V3 -> 直接供电至 U31/U33/U34 的 VCC 端、U32 的 VDDIO 端以及 H8 排针输出 (EXT_+3V3_OUT)


* **关键去耦电容**:
* **U31/U33/U34**: 各自 VCC 引脚旁就近并联 100nF 去耦电容 (C72, C75, C77)。
* **U30**: VCC 引脚旁并联 C71 (100nF, 0603, X7R)。
* **U32**: VDD 引脚旁并联 C73 (100nF, 0402, X7R) 与 C74 (2.2uF, 0603, X7R) 组合去耦；VDDIO 引脚旁并联 C76 (10nF, 0402, X7R)。



## 5. 被动元件与外围配置 (Passive BOM & Config)

* **晶振电路**: 未标明。
* **拨码开关/跳线帽**: 未标明。
* **LED/按键**: 未标明。
* **硬件强配置项**:
* U30 和 U32 所在 SPI 从机的 CS 引脚均配置了 10kΩ (R46, R47) 的默认上拉电阻，保证上电复位期间从机处于非选中状态。
* U30 与 U32 主供电路径串入 0Ω 电阻 (R45, R48)，预留了调试时断开单独测量功耗的硬件接口。



## 6. 视觉审核警告 (Hardware Warnings)

* **悬空引脚**:
* U30(Flash) 的 Pin3(IO2) 和 Pin7(IO3) 悬空（图纸按标准 SPI 模式设计，未使用 QSPI）。
* U32(IMU) 的中断引脚 Pin4(INT1/INT) 和 Pin9(INT2/FSYNC/CLKIN) 显式标注 NC 悬空，MCU 只能采用轮询方式读取姿态数据，无法使用硬件中断触发。
* U32 的 Pin2, Pin3, Pin6 (RESV) 已显式打叉处理；Pin7, Pin11 (RESV) 接 GND。


* **潜在冲突**:
* **片选冲突风险**：H8 扩展排针直连了核心 SPI 通信引脚。若外部接入模块且未进行严格的片选逻辑管控，可能与板载的 Flash (U30) 或 IMU (U32) 在同一总线上发生数据冲撞。
* **电平不确定性风险**：控制信号网络 `SPI_OR_I2S_SWITCH` 并未在当前图纸中配置外部上/下拉电阻。若 MCU 初始化前此引脚处于高阻/浮空状态，模拟开关 U31/U33/U34 可能出现随机切换或不完全导通，导致总线上产生毛刺信号，干扰外设。