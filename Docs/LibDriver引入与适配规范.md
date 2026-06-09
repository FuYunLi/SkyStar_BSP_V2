# LibDriver 引入与适配规范

> 版本：3.0.0  
> 更新日期：2026-06-09  
> 适用范围：所有基于 LibDriver 开源外设驱动库的引入与适配

为了在本项目中高效、规范地引入第三方开源驱动库（LibDriver），并实现应用层与具体硬件芯片的彻底解耦，特制定本规范。

本项目不采用重型的动态设备驱动模型（如 Device Tree），也不采用繁琐的自定义芯片级驱动封装（即无需在 BSP/Driver 下为 LibDriver 外设编写自定义 Wrapper）。我们采用**板级静态类接口抽象（Static Board-Level Class Interface）**方案：应用层仅调用板级通用服务接口，板级服务层负责引入并桥接 LibDriver 的标准封装示例。

---

## 1. 架构设计与调用链

在此模式下，外设驱动的接入与调用链如下图所示：

```
+-------------------------------------------------------+
|                       应用层 (APP)                    |
|           (如 app_main.c, app_shell_demo.c)           |
+-------------------------------------------------------+
                           |
                           v  [仅调用板级通用类 API]
+-------------------------------------------------------+
|                  板级服务层 (BSP Board)               |
|            (如 bsp_storage.c/h, bsp_sensor.c/h)       |
|  - 声明通用静态接口，在内部桥接 LibDriver Basic 接口  |
+-------------------------------------------------------+
                           |
                           v  [调用 LibDriver 包装层]
+-------------------------------------------------------+
|                 LibDriver Example 包装                 |
|            (如 driver_at24cxx_basic.c/h)              |
+-------------------------------------------------------+
                           |
                           v
+-------------------------------------------------------+
|                    LibDriver 核心库                    |
|               (如 driver_at24cxx.c/h)                 |
+-------------------------------------------------------+
                           |
                           v  [回调函数绑定]
+-------------------------------------------------------+
|                  LibDriver 接口适配                   |
|            (如 driver_at24cxx_interface.c)            |
+-------------------------------------------------------+
                           |
                           v  [调用 Port 物理抽象接口]
+-------------------------------------------------------+
|                  物理抽象接口层 (Port)                |
|           (如 port_i2c.h, port_spi.h, port_dwt.h)      |
+-------------------------------------------------------+
```

* **应用层完全解耦**：应用层**绝对禁止**包含任何以 **driver_<chip>** 或 **at24cxx** 命名的头文件，只能调用 **bsp_storage.h**、**bsp_sensor.h** 等具有通用功能属性命名的板级接口。
* **物理抽象与平台移植性**：LibDriver 接口适配文件（`interface/`）中，**绝对禁止**直接包含 STM32 HAL 库头文件（如 `stm32f4xx_hal.h`），必须通过包含并调用本项目的 **Port** 物理接口层 API 完成硬件读写，以实现驱动与 MCU 平台的彻底解耦。

---

## 2. 目录结构规范

所有 LibDriver 驱动源文件统一存放至 **BSP/Driver/LibDriver/** 文件夹中（因根目录下的 **Drivers** 是 CubeMX 自动生成与维护的目录，不应存放自定义第三方驱动），并按外设芯片型号建立二级子目录，包含 `src`、`interface` 与 `example` 目录：

```
BSP/Driver/LibDriver/
└── <chip_name>/                    # 例如 at24c02 (库名称一般为 at24cxx)
    ├── src/                        # 【完全冻结】LibDriver 官方核心驱动
    │   ├── driver_<chip_name>.c
    │   └── driver_<chip_name>.h
    ├── interface/                  # 【适配修改】对外物理层适配接口实现
    │   ├── driver_<chip_name>_interface.c
    │   └── driver_<chip_name>_interface.h
    └── example/                    # 【完全冻结】自带的基本应用封装
        ├── driver_<chip_name>_basic.c
        └── driver_<chip_name>_basic.h
```

* **src/** 与 **example/** 目录下的代码文件在项目中**禁止**进行任何修改，保持其原汁原味的开源状态。
* 仅修改 **interface/** 下的函数实现，以对接我们的物理 **Port** 接口。

---

## 3. 接口适配规范 (Interface Mapping)

在 `interface/driver_<chip_name>_interface.c` 中，必须通过调用本项目的 **Port** 接口来实现 LibDriver 的底层回调，常见映射关系如下：

### 3.1 延时接口
* 微秒级延时：调用 **port_dwt_delay_us**。
* 毫秒级延时：调用 **port_dwt_delay_ms**。

### 3.2 I2C 读写适配
* 寄存器级读写：调用 **port_i2c_mem_read** 与 **port_i2c_mem_write**。
* 原始数据流读写：调用 **port_i2c_read** 与 **port_i2c_write**。

### 3.3 SPI 读写适配
* 片选 CS 引脚拉低/拉高：调用 **port_gpio_write**。
* 数据传输：调用 **port_spi_transmit** / **port_spi_receive** / **port_spi_transmit_receive**。

### 3.4 调试打印
* 调试输出接口：统一调用 EasyLogger 提供的 **log_i**、**log_e** 等标准打印函数。

---

## 4. 板级类抽象设计示例 (BSP Board)

板级层文件放置于 **BSP/Board/** 下，负责为应用层提供不依赖具体芯片型号的抽象服务。

### 4.1 头文件 `bsp_storage.h`
```c
#ifndef __BSP_STORAGE_H
#define __BSP_STORAGE_H

#include "bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

bsp_status_t bsp_storage_init(void);
bsp_status_t bsp_storage_read(uint32_t address, uint8_t *buf, uint32_t len);
bsp_status_t bsp_storage_write(uint32_t address, const uint8_t *buf, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_STORAGE_H */
```

### 4.2 源文件 `bsp_storage.c`
```c
#include "bsp_storage.h"
#include "driver_at24cxx_basic.h"

bsp_status_t bsp_storage_init(void)
{
    /* 桥接到 LibDriver 官方的基本初始化函数 */
    uint8_t res = at24cxx_basic_init(AT24C02, AT24CXX_ADDRESS_A000);
    if (res != 0)
    {
        return BSP_ERR;
    }
    return BSP_OK;
}

bsp_status_t bsp_storage_read(uint32_t address, uint8_t *buf, uint32_t len)
{
    /* 限制地址范围，防止越界 */
    if (address + len > 256)
    {
        return BSP_ERR;
    }
    
    uint8_t res = at24cxx_basic_read((uint16_t)address, buf, (uint16_t)len);
    if (res != 0)
    {
        return BSP_ERR;
    }
    return BSP_OK;
}

bsp_status_t bsp_storage_write(uint32_t address, const uint8_t *buf, uint32_t len)
{
    if (address + len > 256)
    {
        return BSP_ERR;
    }
    
    /* 桥接写操作，由于是 const buf 指针，进行必要的类型转换 */
    uint8_t res = at24cxx_basic_write((uint16_t)address, (uint8_t *)buf, (uint16_t)len);
    if (res != 0)
    {
        return BSP_ERR;
    }
    return BSP_OK;
}
```

---

## 5. 引入与适配步骤清单

当在新的里程碑中引入新的 LibDriver 外设时，按以下步骤进行集成：

1. **获取官方驱动**：将 LibDriver 仓库的 `src`、`interface` 与 `example` 目录拷贝至项目中的 **BSP/Driver/LibDriver/<chip_name>/** 目录下。
2. **实现接口映射**：修改 `interface/driver_<chip_name>_interface.c`，引入 `port_i2c.h` 或 `port_spi.h` 等 Port 层头文件，完成底层读写和延时回调的对接。
3. **导入 Keil 工程**：将 `src/`、`interface/` 与 `example/` 中的源文件导入 Keil MDK 虚拟目录 `BSP/Driver/LibDriver/<chip_name>` 中。**注意：在该虚拟目录下直接挂载上述所有源文件即可，无需按子文件夹（src、interface、example）建立多级虚拟文件夹**。同时在工程配置的 `Include Paths` 中添加它们的物理头文件搜索路径。
4. **编写板级服务**：在 **BSP/Board/** 目录下创建类服务头文件与源文件（如 `bsp_storage.h/c`），在 `.c` 文件内部包含 `driver_<chip>_basic.h`，实现桥接。
5. **编译验证**：使用 Keil 构建工具编译工程，确保无任何编译错误或警告。
6. **在 APP 层调用验证**：在 `app_main.c` 中只包含板级服务头文件（如 `bsp_storage.h`），在初始化中调用板级初始化，并编写对应的 Shell 调试命令在物理板上进行读写验证。
