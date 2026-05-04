# SkyStar BSP V2 开发路径规划

## 修订记录

| 日期         | 版本   | 修订内容                                                                                    |
| ---------- | ---- | --------------------------------------------------------------------------------------- |
| 2025-05-04 | v1.0 | 初始版本                                                                                    |
| 2025-05-04 | v1.1 | 阶段零完成后修订：删除 HAL 句柄映射宏（改为 Interface 层数组映射），删除 Utils 目录（合并到 bsp\_board.h），MultiTimer 提前引入 |

***

## V1 路径复盘

| 阶段   | V1 的问题                                          |
| ---- | ----------------------------------------------- |
| 起步   | 初版框架后直接 I2C，没有统一规范基础提交                          |
| 顺序   | 串口接口在 GPIO 前完成，依赖关系倒置                           |
| 混用   | SPI 接口与 ST7789 驱动同一提交，粒度太粗                      |
| 节奏   | LVGL 在 v0.19 就引入，串口 DMA 高级功能在 v0.23 才做，顺序颠倒     |
| 中间件  | Letter Shell 在 v0.28 才有，导致前期所有"测试"都靠注释开关，无法持续验收 |
| Demo | 无独立 Demo 层，测试代码混在 bsp\_board.c 的 init 序列里       |
| 提交信息 | `v0.25 修复IDLE 中断 DMA 数据长度计算` 是 fix，但挂在版本号下无法区分  |

***

## V2 路径设计原则

1. **规范先行**：第一个提交只建框架和规范，不写一行业务代码
2. **Shell 最早引入**：第三个里程碑就移植 Letter Shell，此后所有验收通过 Shell 命令执行
3. **接口→驱动→Demo 严格分层**：同一总线的接口层完成后，立刻做一个最简 Demo 验收，再进入下一总线
4. **EasyLogger 紧随 Shell**：日志和交互工具都到位后，后续所有驱动开发才有完整的调试手段
5. **按硬件复杂度升序**：GPIO → UART → SPI → I2C → PWM → 传感器 → 电机 → 音频 → 网络

***

## 推荐提交消息格式（Conventional Commits）

```
<type>(<scope>): <中文简述>

type:  feat / fix / refactor / docs / test
scope: arch / interface / driver / middleware / demo / app

示例：
feat(interface): port_gpio - GPIO 逻辑映射层
feat(driver):    dev_led - 板载 LED 驱动
feat(demo):      demo_ws2812 - 彩虹渐变 Shell 命令
fix(driver):     dev_ws2812 - DMA 传输完成标志竞态
```

***

## 阶段零：架构基础（1 提交）

**@\[CubeMX配置] 全局参数检查**
为了彻底解决 V1 代码耦合和配置遗漏问题，请在建立工程时严格检查以下全局参数：

1. **System Core -> RCC (复位与时钟控制)**:
   - **HSE (High Speed Clock)**: 选择 `Crystal/Ceramic Resonator` (开发板板载 8MHz 外部晶振)。
   - **LSE (Low Speed Clock)**: 保持 `Disable` (使用外部高精度 SD3078，无需 MCU 内部低速时钟)。
2. **System Core -> SYS (系统)**:
   - **Debug**: 选择 `Serial Wire` (SWD 下载与调试)。
   - **Timebase Source**: 选择 `SysTick` (裸机默认即可)。
3. **Clock Configuration (时钟树配置)**:
   - **Input frequency**: 填写 `8` MHz。
   - **PLL Source Mux**: 选择 `HSE`。
   - **Main PLL**: 配置 `/M` = 4, `*N` = 168, `/P` = 2, `/Q` = 4。
   - **System Clock Mux**: 选择 `PLLCLK`。
   - **APB1 Prescaler**: 选择 `/4` (输出 42MHz，因最大限制为 42MHz)。
   - **APB2 Prescaler**: 选择 `/2` (输出 84MHz，因最大限制为 84MHz)。
   - 确认核心总线 **HCLK** 为 `168` MHz。
4. **Project Manager -> Project**:
   - **Toolchain / IDE**: 选择 `MDK-ARM`。
   - **Linker Settings**: `Minimum Heap Size` 设为 `0x1000` (4KB)，`Minimum Stack Size` 设为 `0x1000` (4KB)。为后续打印、缓冲预留空间。
5. **Project Manager -> Code Generator**:
   - 勾选 **Copy only the necessary library files** (只拷贝需要的库文件，缩减体积)。
   - **\[极度重要]** 强制勾选 **Generate peripheral initialization as a pair of '.c/.h' files per peripheral** (为每个外设独立生成 `.c/.h` 文件。这是实现 V2 代码模块化、保持 `main.c` 清爽的绝对基础！)。

**目标**：建立整个 V2 的规范地基，后续所有文件都遵循这一套标准。

```
feat(arch): V2 项目框架、规范基础与通用 Utils
```

**内容清单**：

- 目录结构：`BSP/{Board,Driver,Interface}` / `App/{demos,tasks}` / `Middleware/` / `Docs/`
- `BSP/Board/bsp_board.h`：`bsp_status_t` 枚举、`extern "C"` 守卫、断言宏、常用宏（**此后所有头文件模板**）
- `BSP/Board/bsp_board.c`：`bsp_status_str()` 状态码字符串转换
- `BSP/Board/bsp_config.h`：系统时钟配置（**HAL 句柄映射改为 Interface 层数组映射**）
- `BSP/Interface/port_tick.h/c`：系统滴答接口
- `App/app_main.h/c`：应用入口
- `Middleware/MultiTimer/`：软件定时器（**提前引入**）
- `.clang-format`：Allman 风格配置 v1.3.0
- `Docs/代码规范.md`：项目代码规范
- `README.md`：模块状态表

**验收**：项目能空编译通过，无任何业务代码。

***

## 阶段零（补充）：架构增强设计

> **背景**：基于 V1 代码审查，发现以下四个架构层面的不足。V2 应在正式开发前完成这些设计决策，确保代码质量达到商业级产品标准。

### E1 — 统一状态码与防御性编程规范

**V1 问题回顾**：

- Driver 层存在 `void` 返回值函数（如 `lcd_init()`, `lcd_set_rotation()`）
- 参数检查不够全面，部分函数只做简单 return 而不返回错误码
- 缺乏统一的断言机制，问题往往在 HardFault 后才暴露

**V2 设计决策**：

| 规范项            | 要求                                           |
| -------------- | -------------------------------------------- |
| Interface 层返回值 | 所有 API 必须返回 `bsp_status_t`（除极特殊的底层读写）        |
| Driver 层返回值    | 所有 API 必须返回 `bsp_status_t`，禁止 `void`         |
| 参数检查           | 所有公开 API 入口处检查指针有效性、参数范围                     |
| 错误码语义          | `BSP_EINVAL` 用于参数错误，`BSP_ENOMEM` 用于内存不足，禁止混用 |

**新增断言宏**（放入 `bsp_board.h`）：

```c
/* 轻量级断言：Release 模式下可配置为空操作 */
#ifdef BSP_DEBUG
    #define BSP_ASSERT(expr)                                    \
        do {                                                     \
            if (!(expr)) {                                       \
                log_e("Assert failed: %s, %s:%d", #expr,        \
                      __FILE__, __LINE__);                       \
                return BSP_EINVAL;                               \
            }                                                    \
        } while(0)
#else
    #define BSP_ASSERT(expr) ((void)0)
#endif
```

**验收**：所有 Driver 层 API 返回 `bsp_status_t`，空指针入参触发 `BSP_EINVAL`。

***

### E2 — 依赖倒置强化

**V1 问题回顾**：

- Driver 层基本达标，但存在一处瑕疵：`dev_st7789.c` 使用了 `HAL_MAX_DELAY`
- 这意味着 Driver 层"知道"了 HAL 库的存在，破坏了抽象

**V2 设计决策**：

| 层级              | 允许依赖                                                     | 禁止依赖                            |
| --------------- | -------------------------------------------------------- | ------------------------------- |
| **Driver 层**    | `port_spi.h`, `port_i2c.h`, `port_gpio.h`, `bsp_board.h` | `stm32f4xx_hal.h`, 任何 MCU 厂商头文件 |
| **Interface 层** | HAL 库、`bsp_board.h`                                      | 应用层头文件                          |

**抽象层常量封装**（放入 `bsp_board.h`）：

```c
/* 超时常量：封装 HAL 依赖 */
#define BSP_WAIT_FOREVER  0xFFFFFFFFU  /* 替代 HAL_MAX_DELAY */
#define BSP_NO_WAIT       0U           /* 立即返回 */
```

**验收**：Driver 层代码搜索 `stm32f4xx_hal`、`HAL_`、`TIM[0-9]`、`SPI[0-9]` 等关键字，结果应为空。

***

### E3 — 中断回调统一分发机制

**V1 问题回顾**：

- 各 Interface 层有回调机制，但分散管理
- `port_uart` 有 `register_callbacks`，但 `port_spi` 没有
- HAL 中断回调直接在 Interface 层实现，应用层无法灵活注册

**V2 设计决策**：

**统一回调类型定义**（放入 `bsp_board.h`）：

```c
/* 通用事件回调类型 */
typedef void (*bsp_event_cb_t)(void *user_data);

/* DMA 传输完成回调 */
typedef void (*bsp_dma_cb_t)(bool success, void *user_data);

/* GPIO 中断回调 */
typedef void (*bsp_gpio_isr_cb_t)(uint8_t pin_id, void *user_data);
```

**Interface 层统一 API 模式**：

```c
/* 每个 Interface 必须提供以下注册接口 */
bsp_status_t port_xxx_register_callback(port_xxx_event_t event, 
                                         bsp_xxx_cb_t cb, 
                                         void *user_data);
```

**HAL 中断路由模式**（以 SPI 为例）：

```c
/* port_spi.c 内部 */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    port_spi_id_t bus = get_bus_id(hspi);
    if (spi_ctx[bus].cb) {
        spi_ctx[bus].cb(true, spi_ctx[bus].user_data);
    }
}
```

**验收**：应用层可通过 `port_xxx_register_callback()` 监听任意 Interface 层事件，无需修改 Interface 源码。

***

### E4 — 硬件资源仲裁器（核心难点）

**V1 问题回顾**：

- **完全缺失**，这是 V1 最大的架构缺陷
- SkyStar 硬件存在 SPI2/I2S2 复用、TF卡/排针互斥等场景
- 无仲裁机制会导致总线冲突、数据错乱

**SkyStar 硬件互斥矩阵**：

| 资源        | 互斥对象      | 切换方式           |
| --------- | --------- | -------------- |
| **SPI2**  | I2S2（音频）  | PCA9555 控制模拟开关 |
| **I2S2**  | SPI2（IMU） | 同上             |
| **TF 卡槽** | 排针 SPI    | 物理互斥，软件检测      |
| **I2C1**  | 多设备共享     | 无硬件互斥，需软件仲裁    |

**V2 设计决策**：

**总线资源枚举**（放入 `bsp_board.h`）：

```c
/* 共享总线资源 ID */
typedef enum
{
    BSP_BUS_SPI2_I2S2 = 0,  /* SPI2 与 I2S2 复用 */
    BSP_BUS_SDIO_SPI,       /* SDIO 与排针 SPI 互斥 */
    BSP_BUS_I2C1,           /* I2C1 多设备共享 */
    BSP_BUS_MAX
} bsp_bus_t;

/* 总线用途 */
typedef enum
{
    BSP_BUS_USE_SPI2_IMU,   /* SPI2 用于 IMU */
    BSP_BUS_USE_I2S2_AUDIO, /* I2S2 用于音频 */
    BSP_BUS_USE_SDIO_SD,    /* SDIO 用于 SD 卡 */
    BSP_BUS_USE_SPI_EXT,    /* SPI 用于扩展排针 */
} bsp_bus_use_t;
```

**仲裁 API 设计**：

```c
/**
 * @brief 申请总线资源
 * @param bus   总线 ID
 * @param use   申请用途
 * @param timeout_ms 超时时间（BSP_WAIT_FOREVER 表示永久等待）
 * @return BSP_OK 成功，BSP_TIMEOUT 超时，BSP_BUSY 被占用
 */
bsp_status_t bsp_bus_acquire(bsp_bus_t bus, bsp_bus_use_t use, uint32_t timeout_ms);

/**
 * @brief 释放总线资源
 * @param bus   总线 ID
 * @return BSP_OK 成功
 */
bsp_status_t bsp_bus_release(bsp_bus_t bus);

/**
 * @brief 查询总线当前状态
 * @param bus   总线 ID
 * @return 当前用途，若空闲返回 -1
 */
int bsp_bus_get_state(bsp_bus_t bus);
```

**SPI2/I2S2 切换实现示例**：

```c
bsp_status_t bsp_bus_acquire(bsp_bus_t bus, bsp_bus_use_t use, uint32_t timeout_ms)
{
    if (bus == BSP_BUS_SPI2_I2S2)
    {
        /* 等待当前使用者释放 */
        if (bus_state[bus] != BSP_BUS_USE_NONE) {
            /* 可选：实现等待队列或直接返回 BSP_BUSY */
        }
        
        if (use == BSP_BUS_USE_I2S2_AUDIO) {
            /* 1. 停止 SPI2 */
            HAL_SPI_DeInit(&hspi2);
            /* 2. 切换 PCA9555 模拟开关 */
            dev_pca9555_write_pin(0, ANALOG_SWITCH_PIN, 1);
            /* 3. 初始化 I2S2 */
            HAL_I2S_Init(&hi2s2);
        }
        else if (use == BSP_BUS_USE_SPI2_IMU) {
            /* 反向切换 */
            HAL_I2S_DeInit(&hi2s2);
            dev_pca9555_write_pin(0, ANALOG_SWITCH_PIN, 0);
            HAL_SPI_Init(&hspi2);
        }
        
        bus_state[bus] = use;
    }
    return BSP_OK;
}
```

**验收**：

- 音频播放前调用 `bsp_bus_acquire(BSP_BUS_SPI2_I2S2, BSP_BUS_USE_I2S2_AUDIO, ...)`
- IMU 读取前调用 `bsp_bus_acquire(BSP_BUS_SPI2_I2S2, BSP_BUS_USE_SPI2_IMU, ...)`
- 两者不会同时占用总线

***

### E5 — 事件驱动架构（核心架构）

**V1 问题回顾**：

- `bsp_board_process()` 职责膨胀，混合按键、LED、LVGL、Shell 等多种轮询
- `bsp_board.c` 调用 `app_main.c` 的 `led_flow()`，**依赖倒置**
- 应用层暴露轮询接口给下层，架构泄漏
- 无统一的事件调度机制，每个模块各自管理定时器

**V2 设计决策**：

#### 1. 架构分层与职责

```
┌─────────────────────────────────────────────────────────┐
│                      main.c                             │
│                                                         │
│   while(1) {                                            │
│       multi_timer_process();  ──► 统一调度所有定时器    │
│       bsp_shell_process();     ──► Shell 交互           │
│       task_xxx_process();      ──► 协作式轮询（可选）   │
│   }                                                     │
└─────────────────────────────────────────────────────────┘
```

| 层级               | 职责               | 是否有 while(1) |
| ---------------- | ---------------- | ------------ |
| **main.c**       | 系统心脏，唯一主循环       | ✅ 唯一         |
| **bsp\_board.c** | 硬件初始化，无业务逻辑      | ❌            |
| **app\_main.c**  | 注册任务/Demo，不轮询    | ❌            |
| **task\_xxx.c**  | 常驻后台任务，回调驱动      | ❌            |
| **demo\_xxx.c**  | 独立 Demo，Shell 触发 | ❌            |

#### 2. 引入 MultiTimer 中间件

**开源地址**：<https://github.com/0x1abin/MultiTimer>

**特性**：

- 单文件实现，轻量级（\~200 行）
- 支持链表管理的软件定时器
- 支持单次/周期模式
- 回调机制，携带用户数据
- 防漂移设计

**API 设计**：

```c
/* MultiTimer 接口适配 */
typedef struct multi_timer multi_timer_t;
typedef void (*multi_timer_cb_t)(multi_timer_t *timer, void *arg);

struct multi_timer
{
    uint32_t          start;      /* 启动时间戳 */
    uint32_t          interval;   /* 定时间隔 */
    multi_timer_cb_t  callback;   /* 回调函数 */
    void             *arg;        /* 用户数据 */
    bool              repeat;     /* true=周期, false=单次 */
    multi_timer_t    *next;       /* 链表指针 */
};

/* 初始化调度器 */
void multi_timer_init(void);

/* 添加定时器 */
multi_timer_t *multi_timer_add(multi_timer_t *timer, uint32_t interval_ms,
                                multi_timer_cb_t cb, void *arg, bool repeat);

/* 删除定时器 */
void multi_timer_del(multi_timer_t *timer);

/* 统一调度（main while(1) 中调用） */
void multi_timer_process(void);
```

#### 3. 应用层代码形态

```c
/* app_main.c — 注册即忘，无 while(1) */

static multi_timer_t timer_led;
static multi_timer_t timer_key;

static void led_callback(multi_timer_t *timer, void *arg)
{
    dev_led_toggle(LED_CORE);
}

static void key_callback(multi_timer_t *timer, void *arg)
{
    bsp_key_scan();
}

void app_main_init(void)
{
    /* 注册回调，之后不再关心 */
    multi_timer_add(&timer_led, 500, led_callback, NULL, true);
    multi_timer_add(&timer_key, 5, key_callback, NULL, true);
    
    /* 函数结束，控制权交回 main.c */
}
```

#### 4. 目录结构

```
App/
├── demos/                  # 独立 Demo，Shell 触发
│   ├── demo_led_flow.c/h
│   ├── demo_ws2812.c/h
│   └── demo_sensor.c/h
├── tasks/                  # 常驻后台任务
│   ├── task_led_status.c/h
│   ├── task_key_monitor.c/h
│   └── task_lvgl.c/h
└── app_main.c/h            # 应用入口：注册 Demo + 启动 Task
```

#### 5. 协作式轮询（重度任务）

对于 LVGL 等重度计算任务，采用非阻塞状态机：

```c
/* task_lvgl.c — 协作式轮询 */

void task_lvgl_process(void)
{
    lv_timer_handler();  /* 每次处理一小块工作 */
}

/* main.c */
while(1)
{
    multi_timer_process();
    bsp_shell_process();
    task_lvgl_process();  /* 协作式，非阻塞 */
}
```

**验收**：

- `bsp_board.c` 不含任何业务逻辑或测试代码
- `app_main.c` 不暴露任何轮询接口
- 所有常驻任务通过 MultiTimer 回调驱动
- Demo 通过 Shell 命令触发

***

### E6 — 推荐开源库集成

#### 6.1 已确认引入

| 库名               | 用途         | 开源地址                                           | 引入阶段    |
| ---------------- | ---------- | ---------------------------------------------- | ------- |
| **MultiTimer**   | 软件定时器调度    | <https://github.com/0x1abin/MultiTimer>        | 阶段零     |
| **Letter Shell** | 串口交互终端     | <https://github.com/NevermindZZT/letter-shell> | 阶段一 M2  |
| **EasyLogger**   | 异步日志系统     | <https://github.com/armink/EasyLogger>         | 阶段一 M3  |
| **LittleFS**     | Flash 文件系统 | <https://github.com/littlefs-project/littlefs> | 阶段四 M16 |
| **LVGL**         | 图形界面库      | <https://github.com/lvgl/lvgl>                 | 阶段六     |

#### 6.2 推荐引入

| 库名              | 用途         | 开源地址                                     | 引入阶段   | 说明                 |
| --------------- | ---------- | ---------------------------------------- | ------ | ------------------ |
| **MultiButton** | 多按键管理      | <https://github.com/0x1abin/MultiButton> | 阶段二 M8 | 状态机按键，支持单击/双击/长按   |
| **CmBacktrace** | 故障诊断       | <https://github.com/armink/CmBacktrace>  | 阶段零    | HardFault 时打印调用栈   |
| **SFUD**        | Flash 统一驱动 | <https://github.com/armink/Sfud>         | 阶段四    | 支持 W25Q 等多种 Flash  |
| **EasyFlash**   | 参数存储       | <https://github.com/armink/EasyFlash>    | 阶段四    | KV 存储，替代裸 LittleFS |
| **cJSON**       | JSON 解析    | <https://github.com/DaveGamble/cJSON>    | 阶段八    | 配置文件/网络数据解析        |
| **TinyCRC**     | CRC 校验     | <https://github.com/noibyy/crc>          | 按需     | 通信校验               |

#### 6.3 可选引入

| 库名           | 用途         | 开源地址                                         | 适用场景     |
| ------------ | ---------- | -------------------------------------------- | -------- |
| **FreeRTOS** | 实时操作系统     | <https://www.freertos.org>                   | 任务复杂时升级  |
| **FatFs**    | FAT 文件系统   | <http://elm-chan.org/fsw/ff/00index_e.html>  | SD 卡文件系统 |
| **LwIP**     | TCP/IP 协议栈 | <https://savannah.nongnu.org/projects/lwip/> | 网络应用     |
| **WolfSSL**  | SSL/TLS 加密 | <https://www.wolfssl.com/>                   | 安全通信     |

#### 6.4 中间件集成规范

```c
/* Middleware/ 目录结构 */
Middleware/
├── letter-shell/           # Shell 终端
│   ├── src/
│   ├── port/               # 移植层
│   └── shell_cfg.h         # 配置
├── EasyLogger/             # 日志系统
│   ├── src/
│   ├── port/
│   └── elog_cfg.h
├── MultiTimer/             # 软件定时器
│   └── multi_timer.c/h
├── MultiButton/            # 按键管理
│   └── multi_button.c/h
├── LittleFS/               # 文件系统
│   ├── lfs.c/h
│   └── lfs_util.h
└── CmBacktrace/            # 故障诊断
    ├── cmb_flash.c/h       # Flash 移植
    └── cm_backtrace.c/h
```

**验收**：每个中间件有独立的 `xxx_cfg.h` 配置文件，移植代码放在 `port/` 目录。

***

### 架构增强实施时机

| 增强项       | 实施阶段                 | 依赖关系       |
| --------- | -------------------- | ---------- |
| E1 状态码规范  | 阶段零（立即）              | 无          |
| E2 依赖倒置强化 | 阶段零（立即）              | 无          |
| E3 中断回调分发 | 阶段一（随各 Interface 实现） | E1         |
| E4 资源仲裁器  | 阶段四（I2C 完成后）         | PCA9555 驱动 |
| E5 事件驱动架构 | 阶段零（MultiTimer 移植）   | 无          |
| E6 开源库集成  | 按阶段逐步引入              | 各库独立       |

***

## 阶段一：核心接口层 + 调试基础（4 提交）

**@\[CubeMX配置]**

- **UART1 (调试串口)**:
  - **引脚**: `PA9` (TX), `PA10` (RX)。
  - **参数**: Baud Rate `115200`, Word Length `8 Bits`, Parity `None`, Stop Bits `1`。
  - **NVIC**: 开启 `USART1 global interrupt` (优先级抢占0，子优先0)。
  - **DMA**:
    - `USART1_RX`: `DMA2 Stream 2`, Channel 4, Mode `Circular`, Direction `Peripheral to Memory`。
    - `USART1_TX`: `DMA2 Stream 7`, Channel 4, Mode `Normal`, Direction `Memory to Peripheral`。
- **TIM/DWT**: 保留 SysTick 给 HAL 库。若需要基本定时器，可开启 `TIM6` (预分频 `83`，重装载 `999` 产生 1ms 中断)。

### M1 — UART 接口 + printf 重定向

```
feat(interface): port_uart - DMA + 环形队列双缓冲接收
```

- `port_uart.h/c`：全部 API（发送/接收/回调/统计），含 `extern "C"`，数值宏加括号
- `fputc` 重定向到 `port_uart_write_async`
- 移植 `printf` 支持（用于无 Shell 时的调试输出）

**验收**：`printf("Hello V2\r\n")` 从串口输出，示波器/串口助手可见。

***

### M2 — Letter Shell 移植（**最高优先级**，解锁后续所有 Demo 验收）

```
feat(middleware): Letter Shell - 串口交互终端移植
```

- `Middleware/letter-shell/` 移植，port 文件对接 `port_uart`
- `BSP/Board/bsp_shell.c/h`：`bsp_shell_init()` + `bsp_shell_process()`
- 注册第一个 Shell 命令：`version`（输出 BSP 版本号和编译时间）

**验收**：串口输入 `version` 回车，终端输出版本信息。

***

### M3 — EasyLogger 移植（解锁后续所有结构化日志）

```
feat(middleware): EasyLogger - 异步日志系统移植
```

- `Middleware/EasyLogger/` 移植，`elog_port_output` 对接 `port_uart_write_async`
- `BSP/Board/bsp_logger.c/h`：`bsp_logger_init()`
- 宏：`log_i / log_w / log_e`

**验收**：Shell 输入 `log_test`，终端输出带颜色的 I/W/E 三级日志。

***

### M4 — port\_tick + port\_dwt

```
feat(interface): port_tick + port_dwt - 软件定时与精确延时
```

- `port_tick.c/h`：`port_tick_delay_ms()`、`soft_timer_t` 结构体
- `port_dwt.c/h`：`port_dwt_delay_us()`（DWT 精确微秒延时）

**验收**：Shell 命令 `delay_test`，测试 1ms / 100us 延时误差（用 DWT 计时后 log 输出）。

***

## 阶段二：GPIO + 基础设备（4 提交）

**@\[CubeMX配置]**

- **GPIO**:
  - `PB8` 配置为 `GPIO_Output` (核心板 LED，默认低电平)。
  - **【重要修正】** `PA0` (KEY1), `PE8` (KEY2), `PC13` (KEY3) 配置为 `GPIO_Input`。外部硬件已处理上下拉，**无需开启 EXTI 外部中断** (后续将搭配 MultiButton 组件使用 SoftTimer 轮询)。
- **TIM (PWM)**: 开启 `TIM13_CH1` (对应 `PA6` 蜂鸣器复用脚) 的 PWM Generation。
- **DMA (WS2812)**: 针对驱动 WS2812 的 TIM PWM 通道，开启对应 DMA (Normal 或 Circular 模式，Memory to Peripheral，数据宽度设为 `Word` 或 `HalfWord`)。

### M5 — GPIO 接口层

```
feat(interface): port_gpio - GPIO 逻辑 ID 映射层
```

- `port_gpio.h/c`：`port_gpio_id_t` 枚举（含全部 SkyStar 引脚）、`port_gpio_exti_init()`
- 修正 V1 问题：映射数组缩进统一、空指针返回 `BSP_EINVAL` 而非 `BSP_ENOMEM`

**验收**：Shell 命令 `gpio_test LED_CORE HIGH/LOW`，核心板 LED 响应。

***

### M6 — 基础设备驱动（LED + 蜂鸣器）

```
feat(driver): dev_led + dev_buzzer - 板载 LED 与蜂鸣器驱动
```

- `dev_led.c/h`：驱动 PCA9555PW 的 8 路 LED（依赖 M9 的 I2C，先用 GPIO 核心板 LED 占位）
- `dev_buzzer.c/h`：主动/被动切换（修正 V1 函数体内 `#if` 排版问题）
- 注册 Shell 命令：`led_on N` / `led_off N` / `buzzer_beep freq ms`

**验收**：Shell 触发，LED 闪烁，蜂鸣器发声。

***

### M7 — WS2812B 驱动

```
feat(driver): dev_ws2812 - PWM+DMA RGB LED 驱动
```

- `dev_ws2812.c/h`：修正 V1 颜色宏复合字面量兼容问题（改为 `const` 变量）
- 注册 Shell 命令：`rgb_set R G B` / `rgb_rainbow` / `rgb_breathe`

**验收**：Shell 触发，3 颗 WS2812B 呈现彩虹渐变效果。

***

### M8 — SoftTimer + MultiButton 中间件整合

```
feat(middleware): SoftTimer + MultiButton - 软件定时器与多键管理
```

- 标准化 `soft_timer.c/h` 接口（移自 V1）
- 与 `dev_key` 整合，Button 扫描由 SoftTimer 驱动

***

### M9 — Demo: 系统状态监视器

```
feat(demo): demo_sys_monitor - 基础外设综合测试
```

- 综合使用 WS2812 + Logger + SoftTimer + MultiButton。
- 按键切换 WS2812 的显示模式（彩虹、呼吸等），并通过 Logger 打印按键事件。

***

## 阶段三：SPI 接口 + 显示子系统（5 提交）

**@\[CubeMX配置]**

- **SPI1**:
  - **模式**: `Full-Duplex Master` (引脚 `PA5` SCK, `PB5` MOSI)。
  - **参数**: Data Size `8 Bits`, First Bit `MSB First`, Prescaler `2` (分频到最高速), CPOL `Low`, CPHA `1 Edge`。
  - **DMA (SPI1\_TX)**: 开启 `DMA2 Stream 3`, Channel 3, Mode `Normal`, Direction `Memory to Peripheral`, Data Width 根据 ST7789 配置为 `Byte` 或 `Half Word` (推荐 Half Word 加速刷屏)。
- **GPIO**: LCD 附属控制引脚 `PE14` (CS), `PD14` (DC), `PE1` (RST) 配置为 `GPIO_Output`，速度设为 `Very High`。

### M10 — SPI 接口层

```
feat(interface): port_spi - DMA 三模式 SPI 接口
```

- `port_spi.c/h`：同步 / 异步轮询 / 异步回调三模式（V1 已有，规范化重写）

**验收**：Shell 命令 `spi_loopback`，SPI1 自环测试，log 输出收发结果。

***

### M11 — ST7789 LCD 驱动

```
feat(driver): dev_st7789 - SPI DMA LCD 驱动
```

- `dev_st7789.c/h`：规范化（`extern "C"`、宏括号、`const` 入参）
- 注册 Shell 命令：`lcd_fill COLOR` / `lcd_test`

**验收**：Shell `lcd_fill RED`，屏幕整屏红色。

***

### M12 — PWM 接口 + 背光控制

```
feat(interface): port_pwm - 通用 PWM 接口
feat(driver):    bsp_backlight - 屏幕背光 PWM 控制
```

- `port_pwm.c/h`：规范化重写
- `bsp_backlight.c/h`：0\~100% 亮度控制
- 注册 Shell 命令：`backlight N`（0\~100）

***

### M13 — 软件 I2C + FT6336 触摸驱动

```
feat(interface): soft_i2c - GPIO 模拟 I2C
feat(driver):    dev_ft6336 - 电容触摸驱动
```

- `soft_i2c.c/h`：规范化
- `dev_ft6336.c/h`：触摸坐标读取
- 注册 Shell 命令：`touch_poll`（轮询 1 秒坐标输出）

***

### M14 — Demo: 屏幕划线与触摸反馈

```
feat(demo): demo_lcd_touch - 屏幕与触摸综合测试
```

- 结合 LCD 显示和 FT6336 触摸坐标。
- 在屏幕上实时绘制触摸轨迹，按键可清屏。

***

## 阶段四：I2C 接口 + 存储子系统（4 提交）

**@\[CubeMX配置]**

- **I2C1**:
  - **引脚**: `PB6` (SCL), `PB7` (SDA)。
  - **参数**: I2C Speed Mode `Fast Mode`, I2C Clock Speed `400000` Hz。
- **SPI2 (W25Q128 Flash)**:
  - **模式**: `Full-Duplex Master` (引脚 `PB10` SCK, `PC2` MISO, `PC3` MOSI)。
  - **参数**: Data Size `8 Bits`, Prescaler `2`, CPOL `Low`, CPHA `1 Edge`。
- **GPIO**: 为 Touch 等软件 I2C 预留的引脚配置为 `GPIO_Output` (设为 `Open-Drain` 开漏输出，速度 `High` 更稳定)。

### M15 — I2C 接口层

```
feat(interface): port_i2c - 硬件/软件 I2C 统一分发层
```

- `port_i2c.c/h`：规范化重写（加 `extern "C"`，超时参数显式传入）

***

### M16 — W25Q128 驱动 + LittleFS

```
feat(driver):     dev_w25q - W25Q128 SPI Flash 驱动
feat(middleware): LittleFS - Flash 文件系统移植
```

- `dev_w25q.c/h`：规范化，接口对接 LittleFS block device
- `bsp_lfs.c/h`：`bsp_lfs_mount()` / `bsp_lfs_get_handle()`
- 注册 Shell 命令：`flash_id` / `lfs_ls /` / `lfs_boot_count`

**验收**：Shell `lfs_boot_count` 每次输出递增的启动计数。

***

### M17 — PCA9555 IO 扩展驱动（正式接管 8 路 LED）

```
feat(driver): dev_pca9555 - I2C IO 扩展驱动
refactor(driver): dev_led - 切换至 PCA9555 后端
```

- `dev_pca9555.c/h`：规范化（面向对象结构体，修正 V1 "面向过程"版本）
- `dev_led` 后端从 GPIO 切换到 PCA9555，对外 API 不变
- 注册 Shell 命令：`io_read PORT PIN` / `io_write PORT PIN VAL`

***

### M18 — Demo: LittleFS 存储性能测试

```
feat(demo): demo_lfs_bench - W25Q128 + LittleFS 综合测试
```

- 模拟多次断电写入，测试挂载速度、写入/读取速度。
- 结合 PCA9555，在读写时闪烁对应的扩展 IO LED。

***

## 阶段五：传感器组（6 提交）

**@\[CubeMX配置]**

- **EXTI**: 配置传感器中断引脚 (如 SD3078\_INT PE3, ICM42688\_INT 等) 为外部中断输入，并在 NVIC 开启响应线。
- *(传感器大多挂载在已配置的 I2C1 和 SPI2 上，无需新增总线配置，只需确认时钟速率兼容)*

每个传感器独立提交，每个提交都注册对应 Shell 命令。

### M19 — SD3078 RTC

```
feat(driver): dev_sd3078 - 高精度 RTC 驱动（TCXO）
```

- Shell 命令：`rtc_get` / `rtc_set YYYY-MM-DD HH:MM:SS` / `rtc_temp` / `rtc_sram_dump`

***

### M20 — AHT20 温湿度

```
feat(driver): dev_aht20 - I2C 温湿度传感器驱动
```

- 可参考 LibDriver/aht21 移植核心逻辑
- Shell 命令：`aht20_read`

***

### M21 — INA226 功率监测

```
feat(driver): dev_ina226 - I2C 电流/功率监测驱动
```

- Shell 命令：`power_read`（输出电压、电流、功率）

***

### M22 — AT24C02 EEPROM

```
feat(driver): dev_at24c02 - I2C EEPROM 驱动
```

- Shell 命令：`eeprom_write OFFSET DATA` / `eeprom_read OFFSET LEN`

***

### M23 — ICM-42688-P IMU（SPI2）

```
feat(driver): dev_icm42688 - 六轴 IMU SPI 驱动
```

- Shell 命令：`imu_read`（输出加速度 + 陀螺仪原始数据）
- Shell 命令：`imu_attitude`（输出计算后的俯仰/横滚角，需要简单互补滤波）

***

### M24 — Demo: 裸机传感器监控站

```
feat(demo): demo_sensor_hub - 传感器数据轮询与显示
```

- 纯裸机实现（无 LVGL），在 LCD 上划分区域，实时打印 RTC 时间、温湿度、系统功耗和 IMU 姿态。

***

## 阶段六：输入设备（3 提交）

**@\[CubeMX配置]**

- **TIM (Encoder)**: 开启拥有编码器接口的定时器，设置为 Encoder Mode (对应 EC11 的 A/B 相输入)。
- **EXTI**: 将 EC11 的 Z 相引脚单独配置为 GPIO\_EXTI 中断输入。

### M25 — EC11 编码器

```
feat(driver): dev_ec11 - EC11 旋转编码器驱动
```

- 备注：按键已在第二阶段由 MultiButton 接管，此处仅处理旋转逻辑。
- `dev_ec11.c/h`：旋转方向 + 步数，基于 TIM 定时器编码器模式
- Shell 命令：`ec11_monitor`（输出旋转事件）

***

### M26 — HX711 24 位 ADC（称重传感器）

```
feat(driver): dev_hx711 - 24-bit Delta-Sigma ADC 驱动
```

- Shell 命令：`hx711_tare` / `hx711_read`

***

### M27 — Demo: 电子秤综合展示

```
feat(demo): demo_scale - HX711 + EC11 + LCD 综合测试
```

- EC11 旋转用于选择不同的标定砝码，按下用于去皮 (Tare)。
- LCD 实时显示重量变化。

***

## 阶段七：LVGL 图形界面（3 提交）

**@\[CubeMX配置]**

- **Project Manager**: LVGL 及相关 UI 框架需要极大的内存缓冲。确保 `Stack Size` 维持 `0x1000` (4KB) 以上，`Heap Size` 增至 `0x2000` (8KB) 甚至 `0x4000` (16KB) (若采用 `malloc` 动态分配 LVGL 画布)。

### M28 — LVGL 与 GUI\_Guider 移植 + 触摸集成

```
feat(middleware): LVGL + GUI_Guider - 图形库移植与 UI 框架对接
```

- `lv_port_disp.c`：对接 `lcd_flush_async_cb`
- `lv_port_indev.c`：对接 `dev_ft6336`
- **新增**：移植 NXP `GUI_Guider` 生成的 UI 工程代码作为上层应用基础
- 基础验收：LVGL 内置 Demo 或 GUI Guider 生成的首屏正常运行

***

### M29 — Demo: LVGL 动态时钟

```
feat(demo): demo_rtc_clock - LVGL 实时时钟
```

- 依托 SD3078 提供精准时间，使用 LVGL 绘制表盘或数字时钟。

***

### M30 — Demo: LVGL 环境监测仪表盘

```
feat(demo): demo_env_station - LVGL 综合仪表盘
```

- 将阶段五的裸机数据（AHT20/INA226/IMU）升级为 LVGL 的 Arc / Chart 控件进行高级可视化展示。

***

## 阶段八：电机控制（4 提交）

**@\[CubeMX配置]**

- **TIM (电机 PWM)**: 开启用于直流电机/舵机的定时器 PWM 通道 (如 TIM12\_CH1/CH2 PB14/PB15)。
- **UART (TMC2209)**: 开启一个 UART 实例与 TMC2209 进行寄存器通信。
- **GPIO**: 步进电机的 STEP 和 DIR 控制引脚配置为 GPIO\_Output (可配置推挽，高速翻转)。

### M31 — AT8236 双直流电机

```
feat(driver): dev_at8236 - PWM 双直流电机驱动
```

- Shell 命令：`dc_run MOTOR SPEED DIR` / `dc_stop MOTOR`

***

### M32 — TMC2209 步进驱动

```
feat(driver): dev_tmc2209 - UART 步进电机驱动
```

- TMC2209 通过 UART 配置寄存器，STEP/DIR 控制运动
- Shell 命令：`step_run STEPS SPEED` / `step_stop`

***

### M33 — 编码器闭环

```
feat(driver): dev_encoder - 步进电机 ABZ 编码器 + EXTI Z 相处理
```

- AB 相通过定时器编码器模式，Z 相通过 EXTI 中断
- Shell 命令：`enc_read`（实时位置）/ `enc_reset`

***

### M34 — Demo: 直流电机 PID 闭环控制

```
feat(demo): demo_motor_pid - PID 闭环展示
```

- 结合 AT8236 和 Encoder，实现位置环/速度环 PID 控制。
- 可通过 Shell 动态调节 PID 参数。

***

## 阶段九：高级通信（4 提交）

**@\[CubeMX配置]**

- **CAN**: 开启 CAN (Master)，设置 Prescaler 和 Time Quanta 以匹配目标波特率 (如 500kbps)，在 NVIC 开启 RX0 中断。
- **UART (RS485)**: 开启串口，配置 RS485 模式 (硬件 DE 控制) 或配置独立的 GPIO 充当发送/接收方向使能脚。
- **SDIO**: 开启 SDIO 接口 (4-bit 模式)，开启 SDIO 全局中断，开启 DMA (RX/TX, Circular/Normal 依需求)。
- **FatFS**: 在中间件列表中开启 FatFS，模式为 SD Card，修改 `MAX_SS` 为 4096，开启 DMA 模板支持。

### M35 — 隔离 CAN

```
feat(interface): port_can - 隔离 CAN 总线接口（TJA1042T）
```

- Shell 命令：`can_send ID DATA` / `can_recv`（接收监听 3 秒）

***

### M36 — 隔离 RS485

```
feat(interface): port_rs485 - 隔离 RS485 接口（SP485EEN）
```

- Shell 命令：`485_send DATA` / `485_recv`

***

### M37 — TF 卡 SDIO + FatFS

```
feat(interface): port_sdio - SDIO 接口（TF 卡）
feat(middleware): FatFS - SD 卡文件系统移植
```

- Shell 命令：`sd_ls /` / `sd_cat FILE` / `sd_bench`（读写速度测试）

***

### M38 — Demo: 存储综合测试台

```
feat(demo): demo_storage_bench - SD 卡与 Flash 对比测试
```

- 使用 FatFS 对 SD 卡进行读写压测，与 W25Q LittleFS 进行速度和稳定性对比。

***

## 阶段十：音频子系统（4 提交）

> **注意**：SPI2 和 I2S2 共用引脚，需通过 PCA9555 控制模拟开关切换，这是 SkyStar 最复杂的硬件互斥场景。

**@\[CubeMX配置]**

- **I2S2**: 开启 I2S2 (Half-Duplex Master)，配置音频标准 (Philips) 与采样率 (如 44.1kHz / 48kHz)，16-bit 格式。
- **DMA (I2S2\_TX)**: 开启 DMA (Circular 模式，Memory to Peripheral，数据宽度 HalfWord 16-bit)。

### M39 — I2S 接口

```
feat(interface): port_i2s - I2S2 音频数据流接口
```

- 切换模拟开关前必须停止 SPI2（ICM-42688-P 操作），移交总线给 I2S2

***

### M40 — ES8388 音频编解码

```
feat(driver): dev_es8388 - I2C 配置 + I2S 数据流音频驱动
```

- I2C1 配置寄存器，I2S2 传输 PCM 数据
- Shell 命令：`audio_tone FREQ MS`（播放单音调）

***

### M41 — HT6872 功放

```
feat(driver): dev_ht6872 - D 类功放启用控制
```

- 开启对应 GPIO 引脚控制功放芯片使能。

***

### M42 — Demo: TF 卡音乐播放器

```
feat(demo): demo_audio_player - 完整音频播放链路
```

- 从 FatFS (SD卡) 读取 WAV/PCM 文件，通过 I2S 输出到 ES8388 并用 HT6872 放大播放。
- Shell 命令：`audio_play FILE`

***

## 阶段十一：以太网（选修）（2 提交）

**@\[CubeMX配置]**

- **ETH**: 开启 RMII 模式，配置 PHY 地址 (LAN8720A 默认为 0 或 1)。开启 ETH 全局中断。
- **LwIP**: 开启 LwIP 中间件，按需启用 DHCP 或静态 IP。
- **Project Manager**: 协议栈十分耗费堆栈空间，建议将 `Stack Size` 与 `Heap Size` 均提升至 `0x4000` (16KB) 以上，或使用 RTOS 分配专属任务栈。

### M43 — LAN8720A 以太网

```
feat(interface): port_eth - LAN8720A RMII 以太网驱动
feat(middleware): LwIP - 轻量级 TCP/IP 协议栈移植
```

- Shell 命令：`eth_ping IP` / `eth_info`（MAC/IP）

***

### M44 — Demo: 基础网络回环

```
feat(demo): demo_tcp_echo - 以太网基础联通性测试
```

- LwIP 搭建简单 TCP Echo Server 测试网络连通性。

***

## 阶段十二：全系统联调与最终发布

**@\[CubeMX配置]**

- **NVIC (中断优先级调度)**: 最终检查！为了保证所有 Demo 与底层驱动组合后的体验，需对优先级分组 (建议 4 位抢占优先)。
  - **最高优先级**：SDIO DMA、I2S 音频 DMA (防止音频爆音、SD卡数据溢出)。
  - **次高优先级**：SPI DMA (屏幕刷图)。
  - **中低优先级**：UART 接收、各类外设 EXTI、I2C 传感器中断。
- *备注：所有独立 Demo 已经分散至第一至第十一阶段进行随板验收迭代。*

***

## V1 vs V2 路径对比

| 对比项        | V1                         | V2                                                    |
| ---------- | -------------------------- | ----------------------------------------------------- |
| 规范基础       | 无独立提交                      | 第 0 提交即建立                                             |
| Shell 引入时机 | v0.28（第 28 个提交）            | M2（第 3 个提交）                                           |
| EasyLogger | v0.29                      | M3（第 4 个提交）                                           |
| 测试验收方式     | 注释 + 开机自动运行                | Shell 命令随时触发                                          |
| 提交粒度       | 接口+驱动混合                    | 严格分层，每层独立提交                                           |
| 总线接口完成度    | 无 CAN / RS485 / I2S / SDIO | 全覆盖                                                   |
| 传感器覆盖      | SD3078                     | SD3078 + AHT20 + ICM-42688 + INA226 + AT24C02 + HX711 |
| 电机驱动       | 无                          | AT8236 + TMC2209 + 编码器闭环                              |
| 音频         | 无                          | ES8388 + HT6872 完整链路                                  |
| Demo 层     | 代码混在 bsp\_board.c          | 独立 `App/demos/`，Shell 触发                              |

### 架构增强对比

| 架构项        | V1 状态                         | V2 改进                                   |
| ---------- | ----------------------------- | --------------------------------------- |
| **状态码规范**  | ⚠️ Driver 层存在 `void` 返回值      | ✅ 全部 API 返回 `bsp_status_t`              |
| **防御性编程**  | ⚠️ 部分参数检查缺失                   | ✅ 统一 `BSP_ASSERT` 断言机制                  |
| **依赖倒置**   | ⚠️ Driver 层偶现 `HAL_MAX_DELAY` | ✅ 封装 `BSP_WAIT_FOREVER`，彻底隔离            |
| **中断回调分发** | ⚠️ 分散管理，无统一注册机制               | ✅ 统一 `port_xxx_register_callback()` API |
| **硬件资源仲裁** | ❌ **完全缺失**                    | ✅ `bsp_bus_acquire/release()` 互斥机制      |

