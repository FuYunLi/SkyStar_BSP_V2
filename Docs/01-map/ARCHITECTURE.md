# SkyStar BSP V2 架构地图

> 版本：1.0.0
> 更新日期：2026-09-19
> 说明：面向开发者（含 AI 助手）的工程全局索引。新会话/新成员先读本文即可建立总体认知，细节再按图索骥。每合并一批功能须同步更新。

---

## 1. 工程定位

基于 **STM32F407VET6**（立创天空星核心板 + 自研筑基底板）的裸机分层 BSP 框架。设计目标：**接口抽象实现跨平台适配与中间件快速集成**。当前状态：基础框架与主流外设驱动已落地，正在按 RocketPi 教程工程逐例补齐应用层。

## 2. 目录地图

| 目录 | 性质 | 说明 |
|---|---|---|
| `BSP/Interface` | **手写** | 硬件抽象层，`port_` 前缀，HAL 类型止步于此 |
| `BSP/Driver` | **手写** | 设备驱动层，`dev_` 前缀；含 LibDriver 适配（AHT20/AT24Cxx/INA226） |
| `BSP/Board` | **手写** | 板级服务层，`bsp_` 前缀，契约中枢与业务封装 |
| `APP` | **手写** | 应用层：`app_main` + `demos/`（自检演示）+ `tasks/`（常驻任务） |
| `Middleware` | 第三方 | MultiTimer/LwRB/EasyLogger/letter-shell/MultiButton/Ymodem/LittleFS/LVGL |
| `Middlewares` | 第三方 | FatFs（ST 官方包结构） |
| `Core` | 生成 | CubeMX 生成代码（main/gpio/usart/tim 等） |
| `Drivers` | 生成 | CMSIS + STM32F4xx HAL 库 |
| `FATFS` | 手写薄层 | FatFs 用户配置（App/Target） |
| `MDK-ARM` | 工程 | Keil 工程文件与启动文件 |
| `Docs` | 手写 | 规范/规划/移植报告/板级资料 |

体量：手写核心约 1.5 万行；生成与第三方约 9.4 万行。

## 3. 三层架构

```
APP (app_main / demos / tasks)
        │  仅调用 Board 层 API + Shell 导出验收
        ▼
Board (bsp_xxx)   板级业务封装
        │  仅调用 Driver 层
        ▼
Driver (dev_xxx)  设备驱动实现
        │  仅调用 Interface 层
        ▼
Interface (port_xxx)  硬件抽象层：逻辑 ID + 静态映射表
        │  唯一允许出现 HAL 类型/句柄的层
        ▼
Core (CubeMX 生成) + HAL
```

**铁律**：依赖只允许自上而下；HAL 类型与错误码不出 Interface 层；上层只认识逻辑 ID 与 `bsp_status_t`。

## 4. 关键契约（全工程的"宪法"，改前必读）

1. **状态码体系**（`BSP/Board/bsp_board.h`）
   `bsp_status_t`（BSP_OK/EINVAL/ETIMEOUT/BUSY…）统一全工程错误语义；`hal_to_bsp_status()` 以 `static inline` 形式在进入体系的瞬间翻译 HAL 错误码。
2. **统一异步回调**（`bsp_board.h`）
   `typedef void (*port_async_cb_t)(uint8_t bus_id, bsp_status_t result, void *user_ctx);`
   所有异步操作（SPI DMA、UART TX/Error 等）共用此签名；`user_ctx` 透明指针原样回传。
3. **逻辑 ID + 静态映射表**
   每支 port 用枚举逻辑通道 + 指定初始化器映射表（如 `port_pwm.c` 的 `pwm_mapping`、`port_uart.c` 的 `s_uart_map`、`port_encoder.c` 的 `s_tim_map`）对接物理外设；未使能通道句柄置 NULL 做运行时守卫。换板 = 改表。

### Interface 层模块清单

| 模块 | 职责 | 要点 |
|---|---|---|
| `port_gpio` | 逻辑引脚读写/翻转 + EXTI 注册 | 触发沿 + 回调注册，key_irq 的基础 |
| `port_uart` | 串口抽象 | RX：循环 DMA + IDLE 搬运 LwRB；TX：队列/直连双模式；错误恢复 |
| `port_spi` | SPI1/2 阻塞 + DMA 异步 | 异步走 `port_async_cb_t` |
| `port_i2c` | 硬/软 I2C 统一编址 | 软件通道以 `0x80` 偏置进同一 ID 空间 |
| `port_pwm` | PWM 逻辑通道（蜂鸣器/WS2812/背光） | 占空比千分比制；**已知缺陷：set_freq 时钟域写死 APB1** |
| `port_encoder` | TIM4 正交编码器计数 | `s_tim_map` 解耦范本 |
| `port_adc` | 片上 ADC（电位器 PC0） | LSB / mV 双粒度 |
| `port_tick` / `port_dwt` | 毫秒时基 / DWT 微秒延时 | 高精度时序的基础 |
| `port_critical` | 临界区 | PRIMASK 保存恢复，RTOS 可替换 |
| `soft_i2c` | GPIO 位操作软件 I2C | 触摸屏 FT6336 使用 |

## 5. APP 层组织

- `main.c`（Core）：唯一 `while(1)`，只调 `app_main_init()` / `app_main_process()`
- `app_main.c`：初始化编排 + 注册任务/Demo，**无业务循环**
- `demos/`：独立自检演示，惯例是导出 Shell 命令验收（`SHELL_EXPORT_CMD`），命名 `app_xxx_demo`
- `tasks/`：常驻任务。范本 `app_sys_monitor`：按键回调只 `post_event`，FSM 定时器消费事件，外设表现全部在 MultiTimer 回调——**输入/决策/表现三分离**
- 调度核心：Middleware/MultiTimer（软件定时器链表），回调内自行续期实现周期任务

## 6. 中间件挂接点

| 中间件 | 用途 | 挂接位置 |
|---|---|---|
| MultiTimer | 全局软件定时调度 | `app_main_process` → `multiTimerYield` |
| LwRB | 环形缓冲 | 串口 RX/TX 队列（`port_uart` 内部） |
| EasyLogger | 日志 | `bsp_logger`，模块内 `#define LOG_TAG` 后引头 |
| letter-shell 3.1 | 命令行 | `bsp_shell`，Demo 验收命令入口 |
| MultiButton | 按键事件 | `dev_key` 的回调机制 |
| Ymodem | 串口文件传输 | `app_ymodem_demo` + VFS |
| LittleFS / FatFs | SPI Flash / SD 卡文件系统 | `bsp_lfs` / FatFs，`bsp_file` 提供 VFS 统一路径 |
| LVGL | GUI | 显示/触摸/FS 三套移植接口（见 `Docs/30-porting/LVGL*移植报告.md`） |

## 7. 构建与验证

- IDE 工程：`MDK-ARM/SkyStar_BSP_HAL.uvprojx`（Keil MDK-ARM）
- 命令行构建：`UV4.exe -b`（见 `.agents/skills/build-keil`）
- 索引：`compile_commands.json` + `.clangd`（clangd 跳转/诊断）
- 外设变更：改 `SkyStar_BSP_HAL.ioc` → CubeMX 重新生成 → 目检 Core/ 增量
- 代码风格：`.clang-format`；验收惯例：Shell 命令实测

## 8. 开发任务台账（待填）

> 本节留空。当前开发方向由用户在会话中提出，新任务确定后在此登记：任务名 / 涉及模块 / 状态。
> 已知问题见第 9 节；历史批次记录（zcode 分支 RocketPi 实验）随 zcode 分支留存，不在本分支维护。

## 9. 已知问题清单（在 develop 基点代码中核实过，修一个删一行）

- [ ] `port_uart.c` RX 依赖纯 IDLE 快照：两次 IDLE 间连流超过 DMA 缓冲会静默覆写；ISR 内 `lwrb_write` 溢出无统计。修复方向：保留传输完成中断兜底 + 溢出计数
- [ ] `bsp_uart.c` `uart_rx_data_cb` 为空：推送通知链路已建未用，上层为拉模式
- [ ] `port_gpio.c` `HAL_GPIO_EXTI_Callback` 路由仅比对引脚号不比对端口（PE8 按键与 PB8 LED 同为 pin 8），现靠回调 NULL 检查兜底；根治方案是从 SYSCFG_EXTICR 反查端口归属

（zcode 分支的 `port_pwm_set_freq` APB1 时钟域问题系 zcode 自引入自修复，develop 无此代码，不列。重写 port_pwm 时直接按"按实例地址归属总线动态判定"实现。）

## 10. 维护约定

1. 新增 port/dev/demo 模块后，在对应清单表加一行
2. 修复已知问题后，勾掉第 9 节对应项
3. 移植批次状态变化时更新第 8 节
4. 契约（第 4 节）变更属于架构决策，须在 commit 正文说明原因
