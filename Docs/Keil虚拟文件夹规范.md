# Keil 虚拟文件夹管理与规范

本文档用于规范 SkyStar BSP 项目中 Keil MDK 开发环境的虚拟文件夹（Groups）组织结构。为保证多名开发者协作时工程视图的整洁高效，所有虚拟文件夹的设计与排序须严格遵循本规范。

---

## 1. 核心分类层级

Keil 工程中的虚拟文件夹应按照**从不需手动修改的底层库**到**经常变动的应用层**自上而下排列，整体分为四个核心层级：

### 层级一：底层及只读层（Unused / CubeMX 自动生成）
放置在工程最顶部。这些文件夹由 CubeMX 或芯片厂商提供，日常开发中完全不需要手动展开或修改：
* **Application/MDK-ARM**（启动文件）
* **Application/User/Core**（CubeMX 自动生成的 main.c、stm32f4xx_it.c 等）
* **Drivers/STM32F4xx_HAL_Driver**（ST 官方 HAL 外设库）
* **Drivers/CMSIS**（ARM Cortex-M 核心支持文件）

### 层级二：中间件层（Middleware）
放置在底层库之后，用于收纳项目引入的第三方和自定义中间件组件：
* **Middleware/MultiTimer**
* **Middleware/LwRB**
* **Middleware/LetterShell**
* **Middleware/Easylogger**
* **Middleware/MultiButton**
* **Middleware/LittleFS**
* **Middleware/LVGL**（LVGL 核心源码）
* **Middleware/LVGL/Porting**（LVGL 移植层与配置文件）

### 层级三：板级支持包层（BSP）
放置在中间件之后，收纳所有硬件驱动及板级外设接口。相同前缀的虚拟文件夹必须连续放置，且按依赖层级从低到高排列，严禁被其他非 BSP 分组穿插隔开：
* **BSP/Board**（板级底层初始化，如时钟、基础 GPIO 配置）
* **BSP/Interface**（硬件抽象与总线接口，如 I2C、SPI、UART 协议层封装）
* **BSP/Driver**（外设驱动层，如 LCD、触摸芯片、外部 Flash、EEPROM 驱动）
* **BSP/Driver/LibDriver/AT24Cxx**（特定外部器件库）

### 层级四：应用层（APP）
放置在工程最底部，包含上层业务逻辑 and 任务划分：
* **APP/tasks**（系统业务任务逻辑）
* **APP/demos**（硬件及外设功能自检自测演示模块）

---

## 2. 虚拟文件夹排序与命名规则

### 命名映射
虚拟文件夹的命名应当尽可能反映其物理路径。
例如，物理路径为 **BSP/Driver/dev_st7789.c** 的文件，应放置在名为 **BSP/Driver** 的虚拟文件夹中，保持虚拟视图与真实物理视图的映射一致性。

### 顺序一致性
所有属于 **BSP** 前缀的群组（如 **BSP/Board**、**BSP/Interface**、**BSP/Driver**）在 Keil 中必须连续靠拢放置，禁止被 **Middleware** 或 **APP** 等其他性质的分组中途截断。

---

## 3. 文件包含规范

### 头文件显式引入
为方便开发者在 Keil 中直接双击打开查阅和编辑 API 声明，虚拟文件夹内**不仅需要包含 .c 源文件，还必须将对应的 .h 头文件一并添加进该分组**。
例如，**BSP/Driver** 分组下应同时包含：
* **dev_st7789.c**
* **dev_st7789.h**

### 头文件包含路径配置
所有新增的虚拟文件夹，若其对应的物理文件夹含有头文件，必须在 Keil 的 **C/C++ Options -> Include Paths** 中添加对应的真实物理路径。

---

## 4. LVGL 9.3 专项处理规范

由于 LVGL 9.3.0 源码极其庞大且子文件夹繁多，为了防止 Keil 界面因文件夹过多而爆炸，采用以下折中且清晰的规范处理：

### Middleware/LVGL 核心组
* 该虚拟分组中仅存放 LVGL 核心框架正常编译所需的关键源文件，包括 **src/core**、**src/display**、**src/draw**、**src/font**、**src/indev**、**src/layouts**、**src/libs**、**src/misc**、**src/osal**、**src/stdlib**、**src/themes**、**src/tick**、**src/widgets** 等模块下的 `.c` 文件，无需在 Keil 中为每个子目录创建单独的 Group。

### Middleware/LVGL/Porting 移植组
为了突出最需要频繁修改和查看的代码，将以下文件独立放置到本组中：
* **lv_conf.h**（LVGL 核心配置文件）
* **lv_port_disp.c** / **lv_port_disp.h**（显示屏接口对接）
* **lv_port_indev.c** / **lv_port_indev.h**（触摸板接口对接）
