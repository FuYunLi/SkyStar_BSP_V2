# SkyStar BSP V2 - CubeMX SDIO 与 FatFS 配置说明

本篇文档用于详细记录和说明 SkyStar BSP V2 工程中关于 SDIO (TF 卡外设) 与 FatFS 文件系统中间件的 CubeMX 原始配置、引脚定义、DMA 关系，以及移植与配置过程中必须注意的技术细节与踩坑点。

---

## 1. 物理引脚分配 (Pinout)

板载 TF 卡座通过标准 SDIO 总线驱动，并配备了物理在位检测引脚。引脚定义如下：

*   **PC8** -> **SDIO_D0** (数据线 0)
*   **PC9** -> **SDIO_D1** (数据线 1)
*   **PC10** -> **SDIO_D2** (数据线 2)
*   **PC11** -> **SDIO_D3** (数据线 3)
*   **PC12** -> **SDIO_CK** (时钟线)
*   **PD2** -> **SDIO_CMD** (命令线)
*   **PD3** -> **TF_DET** (TF卡在位检测引脚)
    *   **GPIO 属性**：输入模式 (**GPIO_Input**)，使能内部上拉电阻 (**Pull-up**)。
    *   **在位逻辑**：当卡插入卡槽时，TF 卡座内部金属触点接地，使 **PD3** 引脚电平被拉低（物理低电平表示卡在位）。开机检测到低电平时，板载硬件会自动闭合总线模拟开关，导通 SDIO 总线。

---

## 2. SDIO 外设参数配置 (Parameter Settings)

在 CubeMX 的 Connectivity 列表中选择 **SDIO**，配置如下：

*   **Mode**：选择 **4-bit Wide bus** (4线宽总线模式)
*   **Clock transition**：**Rising edge** (上升沿锁存)
*   **Clock bypass**：**Disable** (禁用时钟旁路)
*   **Clock power save**：**Disable** (禁用时钟省电，保持时钟连续)
*   **Hardware flow control**：**Disable** (禁用硬件流控)
*   **Clock Div**：配置为 **8**
    *   **频率计算**：SDIO 时钟源来自 APB2 外设时钟（F407 上一般为 48MHz）。主频分频公式为：SDIO_CLK = SDIOCLK / (ClockDiv + 2)。
    *   当时钟分频系数设为 8 时，SDIO 物理时钟线输出频率为 48MHz / 10 = 4.8MHz。该保守频率可大幅提升长导线或普通开发板排线下的抗干扰能力，完全满足稳定性的物理需求。

---

## 3. DMA 与中断配置 (DMA & NVIC)

为实现大文件高速搬运且不占用 CPU，必须为 SDIO 配置双通道 DMA：

*   **DMA2 Stream 6**：
    *   **Channel**：Channel 4
    *   **Direction**：**Peripheral to Memory** (外设到内存，用于读卡)
    *   **Mode**：**Peripheral Flow Controller** (外设流控模式，由 SDIO 控制器决定传输边界)
    *   **Priority**：Low (低优先级，可根据需要调整)
    *   **Increment Address**：Memory 开启自增 (Memory Inc)，Peripheral 禁用自增
    *   **Data Width**：均配置为 **Word** (32位双字)
*   **DMA2 Stream 3**：
    *   **Channel**：Channel 4
    *   **Direction**：**Memory to Peripheral** (内存到外设，用于写卡)
    *   **Mode**：**Peripheral Flow Controller** (外设流控模式)
    *   **Priority**：Low
    *   **Increment Address**：Memory 开启自增，Peripheral 禁用自增
    *   **Data Width**：均配置为 **Word**
*   **NVIC 中断**：
    *   必须在 NVIC 界面中勾选 **SDIO global interrupt**。
    *   为了配合 DMA 读写完成回调，中断优先级建议设为较高优先级（在抢占式多任务中须注意与操作系统优先级隔离）。

---

## 4. FatFS 中间件配置 (FATFS Settings)

在 Middleware 列表中选择 **FATFS**，配置如下：

*   **Mode**：勾选 **SD Card** 选项。
*   **Configuration 参数配置**（**ffconf.h** 核心属性）：
    *   **CODE_PAGE (Character Set)**：选择 **936**（支持简体中文 GBK 编码）。
    *   **USE_LFN (Long File Name)**：选择 **2 (Enabled with LFN working buffer on the Heap)**
        *   **说明**：开启长文件名支持，且将长文件名缓存区开辟在系统堆上，防止因分配大文件名导致栈区溢出（**Stack Overflow**）爆死。
    *   **MAX_SS (Maximum Sector Size)**：配置为 **512** 字节。
    *   **FS_REENTRANT (Reentrancy)**：若后续在多线程 RTOS 中运行，建议开启以支持文件系统信号量互斥保护。

---

## 5. 核心移植避错经验与踩坑记录

### A. 初始总线宽度锁死 4-bit 导致握手失败
*   **问题表现**：CubeMX 初始化中把 **hsd.Init.BusWide** 默认写死了 **SDIO_BUS_WIDE_4B**。系统上电执行 **HAL_SD_Init** 时，由于物理卡未经过协议握手还处于 1-bit 状态，主控用 4-bit 强行通信会导致卡无法识别，直接抛出超时或响应 CRC 错误，**BSP_SD_Init** 永久返回错误码。
*   **解决方法**：在调用 **MX_SDIO_SD_Init()** 之后、调用任何物理识别 API 之前，手动强制将 **hsd.Init.BusWide** 重设为 **SDIO_BUS_WIDE_1B**。卡在 1-bit 下成功识别后，底层库会自动通过 ACMD6 将卡和主机同步提升至 4-bit 高速模式。

### B. 在位检测安全锁
*   **问题表现**：若直接调用 **f_mount**，在未插卡的情况下，底层 HAL 库会执行大量的重试与超时等待逻辑，导致开机初始化直接卡顿 2-3 秒，极大降低用户体验。
*   **解决方法**：在 **port_sdio.c** 的物理接口初始化中引入预检：
    ```c
    if (!port_sdio_is_present())
    {
        return BSP_ENODEV;
    }
    ```
    通过检测 **TF_DET (PD3)** 引脚的物理状态，无卡时直接终止初始化流程，保证开机秒开，后续有卡插入时再允许通过 Shell 随时手动挂载。
