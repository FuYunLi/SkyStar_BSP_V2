# SkyStar BSP V2 架构地图

> 版本：1.0.1
> 更新日期：2026-09-20
> 说明：面向开发者（含 AI 助手）的工程全局索引。新会话/新成员先读本文即可建立总体认知，细节再按图索骥。每合并一批功能须同步更新。

---

## 1. 工程定位

基于 **STM32F407VET6**（立创天空星核心板 + 自研筑基底板）的裸机分层 BSP 框架。设计目标：**接口抽象实现跨平台适配与中间件快速集成**。当前状态：基础框架与主流外设驱动已落地，阶段八音频子系统（I2S + ES8388 + WAV 播放）已上板跑通，正在收口串口文件传输通路。

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
4. **交给 DMA 后端的缓冲必须 4 字节对齐（硬约束，违反则静默位移）**
   SDIO IDMA 与 SPI DMA 只按 32 位取指，会丢弃地址低 2 位；而 FatFS 对整扇区读写是把用户指针直达 `disk_write/disk_read`（`ff.c` direct-write 路径，无中间拷贝）。因此任何走 DMA 存储后端的缓冲区（含中间件帧缓冲的载荷偏移、取证工具的读窗）都必须 4 字节对齐，否则数据整体位移且 **无任何错误上报**（CRC 由外设对实际发出的字节生成）。已在 `app_ymodem_demo.c` 用对齐中转缓冲规避；详见 `Docs/40-records/串口框架与Ymodem移植记录-20260922.md` §3

### Interface 层模块清单

| 模块 | 职责 | 要点 |
|---|---|---|
| `port_gpio` | 逻辑引脚读写/翻转 + EXTI 注册 | 触发沿 + 回调注册，key_irq 的基础 |
| `port_uart` | 串口抽象 | RX：循环 DMA + IDLE 搬运 LwRB；TX：队列/直连双模式；错误恢复 |
| `port_spi` | SPI1/2 阻塞 + DMA 异步 | 异步走 `port_async_cb_t` |
| `port_i2s` | I2S2 循环 DMA 流式发送 | 运行时自建句柄（SPI2/I2S2 同外设实例），半/全传输回调路由 |
| `port_i2c` | 硬/软 I2C 统一编址 | 软件通道以 `0x80` 偏置进同一 ID 空间 |
| `port_pwm` | PWM 逻辑通道（蜂鸣器/WS2812/背光） | 占空比千分比制；**已知缺陷：set_freq 时钟域写死 APB1** |
| `port_encoder` | TIM4 正交编码器计数 | `s_tim_map` 解耦范本 |
| `port_adc` | 片上 ADC（电位器 PC0） | LSB / mV 双粒度 |
| `port_sdio` | SDIO/TF 卡抽象 | 在位预检（PD3）+ 容量查询；卡识别须 `MX_SDIO_SD_Init()` + 强制 1-bit 总线宽度，勿删 |
| `port_tick` / `port_dwt` | 毫秒时基 / DWT 微秒延时 | 高精度时序的基础 |
| `port_critical` | 临界区 | PRIMASK 保存恢复，RTOS 可替换 |
| `bsp_bus`（Board） | SPI2/I2S2 复用总线仲裁 | PCA9555 软件切换模拟开关（BIT3），acquire/release + 挂起 IMU |
| `bsp_audio`（Board） | WAV 播放业务封装 | 双缓冲 + MultiTimer 填充，编排仲裁/I2S/codec/功放 |
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

## 8. 开发任务台账

> 当前开发方向由用户在会话中提出，新任务确定后在此登记：任务名 / 涉及模块 / 状态。
> 已知问题见第 9 节；历史批次记录（zcode 分支 RocketPi 实验）随 zcode 分支留存，不在本分支维护。

当前批次：**阶段八后续收口——存储链路完整性与串口框架升级**（方案与实测见 `Docs/40-records/串口框架与Ymodem移植记录-20260922.md`，分支 `fix/ymodem-write-corruption`；阶段八本体见同目录 `阶段八音频调试记录-20260920.md`，已合入 zcode_bsp）

| 任务 | 里程碑 | 涉及模块 | 状态 |
|---|---|---|---|
| 串口框架升级至 V2.1 + Ymodem 升级至 V3 | M32+ | `port_uart`、`Middleware/Ymodem`（替换）；`app_ymodem_demo`（ops 适配 + 对齐中转 + 连续性守卫）；`stm32f4xx_it.c`（去重复委托） | 上板验收通过（Ymodem→SD 字节级一致且可播放），待提交 |
| I2S2 接口层 + SPI2/I2S2 总线仲裁 | M30 | `port_i2s`、`bsp_bus`（新建）；`bsp_imu`、`Core/Src/stm32f4xx_it.c`（修改） | 上板验收通过，已合入 zcode_bsp |
| ES8388 编解码驱动 + HT6872 功放使能 | M31 | `dev_es8388`、`dev_ht6872`（新建）；`dev_pca9555`、`port_i2c`（复用） | 上板验收通过，已合入 zcode_bsp |
| WAV 音乐播放器 Demo | M32 | `bsp_audio`、`app_audio_demo`（新建/扩充）；`bsp_file` 补 read/size 接口 | 上板验收通过（读卡器导入 WAV 正常出声），待提交 |
| DMA 对齐契约全局化 + W25Q 写损坏修复 | 收口批次 | `port_sdio`、`port_spi`、`bsp_file`、`dev_w25q`/`bsp_lfs`；方案见 `Docs/20-planning/DMA对齐契约与W25Q写损坏修复方案.md` | 方案已定，待 hotfix/flash-bus-mutex 合入后开工 |
| SDIO 卡识别回归修复（M30 调试副产） | M32 | `port_sdio`（修复）；`app_fatfs_demo`、`bsp_audio`（诊断日志） | 上板验证通过，工作区未提交；诊断代码待收口 |

待办：中止传输残留坏文件、LittleFS 无锁与小堆、IMU 读不出、FatFS LFN 开启、`dev_w25q` 接入总线仲裁、对齐契约由点状规避升格为统一保障，详见第 9 节。

## 9. 已知问题清单（在 develop 基点代码中核实过，修一个删一行）

- [ ] `port_uart.c` RX 依赖纯 IDLE 快照：两次 IDLE 间连流超过 DMA 缓冲会静默覆写；ISR 内 `lwrb_write` 溢出无统计。修复方向：保留传输完成中断兜底 + 溢出计数
- [ ] `bsp_uart.c` `uart_rx_data_cb` 为空：推送通知链路已建未用，上层为拉模式
- [ ] `port_gpio.c` `HAL_GPIO_EXTI_Callback` 路由仅比对引脚号不比对端口（PE8 按键与 PB8 LED 同为 pin 8），现靠回调 NULL 检查兜底；根治方案是从 SYSCFG_EXTICR 反查端口归属

- [x] ~~Ymodem 写文件内容损坏~~ —— **已定案修复（2026-09-22）**：根因是 Ymodem 载荷指针 `&frame_buf[3]` 非 4 字节对齐，经 FatFS 直达路径交给 SDIO IDMA，而 IDMA 丢弃地址低 2 位 → 整块位移（含帧头 `02 01 FE`），且因 CRC 由外设对实际发出字节生成而全程无错。V3 的 ctx 布局使 `frame_buf` 偏移从 9（碰巧对齐）变为 16（必然非对齐），因而必现。修复：`app_ymodem_demo.c` 落盘前经对齐中转缓冲 + 偏移连续性守卫。验证：`tour.wav` 176478 字节板端 CRC32 与 PC 一致（7a6fd6f4），43 块双趟读全一致，可正常播放。详见 `Docs/40-records/串口框架与Ymodem移植记录-20260922.md`
- [x] ~~LittleFS/W25Q 写路径存在块级内容损坏~~ —— **已推翻（2026-09-22）**：裸 `dev_w25q_read` 77 次重复读 0 差异（含跨 4KB 边界地址），LittleFS 单命令内 8 遍 hash 完全一致，干净重传后两后端连测 4 次全部 = PC 基准且 `short=0`。SPI2/W25Q/电气/驱动均无罪
- [ ] Ymodem 接收失败/中止会留下**无从发现的坏文件**：文件仍在、`size` 也正确，但其数据块已被 LittleFS 释放并复用给其它文件，读它时“同一会话内一致、跨会话变化”，且在 256/4096 整数倍处提前返回。修法：接收写临时名 + 成功后 rename，失败路径上 `lfs_remove`/`f_unlink` 并上报（详见 `Docs/40-records/串口框架与Ymodem移植记录-20260922.md` 第 7 节）
- [ ] LittleFS 实例被多使用者无锁共用（`lv_port_fs` + 各 demo + 开机写 `boot.txt`）：`bsp_lfs.c` 的 `lfs_cfg` 未提供 `.lock/.unlock`（`LFS_LOCK` 实为空操作）；且未定义 `LFS_NO_MALLOC`，每次 `lfs_file_open`/`lfs_dir_open` 都要向 C 堆要 256 字节，而实测**堆最大连续可用仅 3584 字节**。修法：给 LittleFS 配专用静态内存池 + 协作式 busy 锁（不长时间关中断），并把 LFS 错误码经 `bsp_file` 透传（现统一压成 `BSP_ERROR`，看不出 `NOSPC`）
- [ ] `imu_read` 持续 `ret = -1`（ICM42688 读不出），而同一 SPI2 上的 W25Q 读写全部正常 ⇒ 独立缺陷，暂候选：`bsp_imu` 的 suspend/resume 链未重新初始化器件（`bsp_bus` 切到 I2S2 时会挂起 IMU）。零成本判据：`imu_read` → `play_wav` → `imu_read`
- [ ] DMA 缓冲 4 字节对齐契约目前仅在 Ymodem 一处点状规避：建议在 `bsp_file`（统一入参对齐校验/兜底中转）或 `port_sdio`+`port_spi`（非对齐则拒绝或内部中转）升格为全局保障，否则任何新调用方传入非对齐指针（如直接传结构体字段）都会重现静默位移
- [ ] FatFs 未开启长文件名：`ffconf.h` `_USE_LFN = 0`，文件名超 8.3 格式时 `f_open` 直接失败（Ymodem 接收报 Code 5）。修复方向：`_USE_LFN = 1` + 静态工作缓冲，需评估 RAM 开销
- [ ] `port_pwm.c` `port_pwm_set_freq()` 定时器时钟域写死 APB1（`HAL_RCC_GetPCLK1Freq()` + `PPRE1` 判 ×2，恒得 84MHz），而 `pwm_mapping` 混挂了 APB2 的 `htim10`（LCD 背光，实际 168MHz）：ARR 算少一半，输出频率为目标的 2 倍。当前潜伏——全工程仅 `dev_buzzer`(TIM13/APB1) 与 `dev_ws2812`(TIM5/APB1) 调该函数，背光只走 `set_duty`（CCR/ARR 比值，与时钟无关）。修复方向：`port_pwm_map_t` 增加总线归属字段，`set_freq` 查表取时钟，禁止运行时猜 `RCC->CFGR`（换板只改表）
- [ ] `bsp_backlight.c` 亮度语义与板级极性相反：`LCD_BLK_PWM` 网络硬件为低电平点亮（依据 `Docs/00-board_info/EC11_LCD_KEY描述.md`），而 TIM10 CH1 配为 PWM1 + `OCPOLARITY_HIGH`、上层按高电平占比等于亮度写 CCR，导致 `backlight 0` 最亮、`backlight 100` 熄灭。修复方向：`port_pwm_map_t` 增加有效电平标记，由 `set_duty` 统一反相，使 Board 层对上维持 0=灭、100=最亮的直觉语义。连带隐患：`bsp_backlight_init()` 是先 `port_pwm_start()` 再 `bsp_backlight_set()`，而 CubeMX 初始 `Pulse=0` 在低有效硬件上等于全亮，定时器启动到设亮度之间可能短暂闪一下最亮（未实测，修反相时应改成先写 CCR 再 start）

（注：本项早先以 zcode 分支自引入自修复、develop 无此代码为由不列入清单；经核实本分支基点 zcode_bsp 的 `pwm_mapping` 已含 APB2 通道 `htim10`，问题真实存在，已上移为上方正式待修项。重写 port_pwm 时直接按"按实例地址归属总线动态判定"实现。）

## 10. 维护约定

1. 新增 port/dev/demo 模块后，在对应清单表加一行
2. 修复已知问题后，勾掉第 9 节对应项
3. 移植批次状态变化时更新第 8 节
4. 契约（第 4 节）变更属于架构决策，须在 commit 正文说明原因
