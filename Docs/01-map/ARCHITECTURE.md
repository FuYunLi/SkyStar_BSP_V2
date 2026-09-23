# SkyStar BSP V2 架构地图

> 版本：1.0.2
> 更新日期：2026-09-23
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
4. **交给 DMA 后端的缓冲必须满足地址不变式（硬约束，违反则静默位移）**
   不变式不是“缓冲 4 字节对齐”，而是 **`buf ≡ 当前文件位置 (mod 4)`**：SDIO IDMA 只按 32 位取指会丢弃地址低 2 位，
   而 FatFS 对整扇区读写把用户指针直达 `disk_write/disk_read`（`ff.c` direct 路径无中间拷贝），且会先把指针
   **推进到扇区边界**（推进量 ≡ -fptr mod 4）。实测两个必现场：Ymodem 载荷 `frame_buf+3`（恒 4n+3）、
   data 块起始 78 字节的 WAV（缓冲本身对齐但交出 `buf+434`）。违规后果：数据整体位移且**无任何错误上报**
   （CRC 由外设对实际发出的字节生成）。已升格为架构级保障（不再靠调用方自觉）：
   L1 `sd_diskio.c` 入口只拒绝不中转（非对齐/CCM 不可达 → `RES_PARERR`）；
   L2 `bsp_file` 统一兜底（段首段尾走 FatFS 窗口、段体批量整扇区经 1KB 对齐暂存区）；两个计数器供自检取证。
   SPI 侧规则不同：`DataSize=8BIT` 时 DMA 可按字节搬运、无对齐约束（16 位时需 2 字节对齐），但同样要求 DMA 可达；
   **CCM(0x10000000) 对所有 DMA 不可达**，放 CCM 的缓冲不得交给任何 DMA 路径。详见 `Docs/40-records/DMA对齐契约全局化-20260923.md`（根因定案过程另见同目录 `串口框架与Ymodem移植记录-20260922.md` §3）。

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

当前批次：**阶段八后续收口——存储链路完整性、串口框架与对齐契约**（本轮实测见 `Docs/40-records/DMA对齐契约全局化-20260923.md`，分支 `feature/dma-alignment-contract`；上一批见同目录 `串口框架与Ymodem移植记录-20260922.md`；阶段八本体见 `阶段八音频调试记录-20260920.md`）

| 任务 | 里程碑 | 涉及模块 | 状态 |
|---|---|---|---|
| 串口框架升级至 V2.1 + Ymodem 升级至 V3 | M32+ | `port_uart`、`Middleware/Ymodem`（替换）；`app_ymodem_demo`（ops 适配 + 连续性守卫）；`stm32f4xx_it.c`（去重复委托） | 上板验收通过（Ymodem→SD/flash 字节级一致且可播放），已合入 zcode_bsp |
| I2S2 接口层 + SPI2/I2S2 总线仲裁 | M30 | `port_i2s`、`bsp_bus`（新建）；`bsp_imu`、`Core/Src/stm32f4xx_it.c`（修改） | 上板验收通过，已合入 zcode_bsp |
| ES8388 编解码驱动 + HT6872 功放使能 | M31 | `dev_es8388`、`dev_ht6872`（新建）；`dev_pca9555`、`port_i2c`（复用） | 上板验收通过，已合入 zcode_bsp |
| WAV 音乐播放器 Demo | M32 | `bsp_audio`、`app_audio_demo`（新建/扩充）；`bsp_file` 补 read/size 接口 | 上板验收通过（读卡器导入 WAV 正常出声），已合入 zcode_bsp |
| DMA 对齐契约全局化（L1 拒绝 + L2 中转 + L3 入契约） | 收口批次 | `FATFS/Target/sd_diskio.c`、`BSP/Board/bsp_file.c`（+计数器）、`port_spi.c`、`bsp_audio.c`（吞错改上抛+停播收尾）、`app_fatfs_demo.c`（双向矩阵）、`app_ymodem_demo.c`（删点状规避） | 上板验收通过（align 9 轮全 PASS、两后端 CRC=PC、44 块双遍一致、78 字节头 WAV 可播且释放总线），待提交 |
| 存储健壮性第一批：dev_w25q 上抛超时、VFS remove/rename、Ymodem 临时名提交 | 收口批次 | `dev_w25q`、`bsp_file`、`app_ymodem_demo` | 已合入 zcode_bsp（f8f564b）；本轮补齐"中止传输不破坏同名好文件"两后端验证 |
| W25Q/LittleFS 接入 SPI2 总线仲裁 | 收口批次 | `bsp_lfs`、`bsp_bus`、`app_flash_demo` | 已合入 zcode_bsp（47fcba8）；本轮实测：音频持总线时 flash 快速失败 `ret=-5 BSP_BUSY` 不卡死，切回后 CRC 复原 |
| SDIO 传输边界缺陷：背靠背单扇区写错 2 字节 / 36 字节岛 | 待定 | `sd_diskio`/`bsp_driver_sd`/HAL SDIO 数据路径 | 未定位（指针合法的 raw 路径仍间歇复现），已入第 9 节 |
| SDIO 卡识别回归修复（M30 调试副产） | M32 | `port_sdio`（修复）；`app_fatfs_demo`、`bsp_audio`（诊断日志） | 已合入 zcode_bsp（逻辑错误码已在接口层翻译） |

待办：LittleFS 无锁与 4 KB 小堆、CCM 40 KB 空闲未规划、IMU 读不出（`imu_read` ret=-1）、FatFS LFN 开启、SDIO 传输边界缺陷，详见第 9 节。

## 9. 已知问题清单（在 develop 基点代码中核实过，修一个删一行）

- [ ] `port_uart.c` RX 依赖纯 IDLE 快照：两次 IDLE 间连流超过 DMA 缓冲会静默覆写；ISR 内 `lwrb_write` 溢出无统计。修复方向：保留传输完成中断兜底 + 溢出计数
- [ ] `bsp_uart.c` `uart_rx_data_cb` 为空：推送通知链路已建未用，上层为拉模式
- [ ] `port_gpio.c` `HAL_GPIO_EXTI_Callback` 路由仅比对引脚号不比对端口（PE8 按键与 PB8 LED 同为 pin 8），现靠回调 NULL 检查兜底；根治方案是从 SYSCFG_EXTICR 反查端口归属

- [x] ~~Ymodem 写文件内容损坏~~ —— **已定案修复（2026-09-22）**：根因是 Ymodem 载荷指针 `&frame_buf[3]` 非 4 字节对齐，经 FatFS 直达路径交给 SDIO IDMA，而 IDMA 丢弃地址低 2 位 → 整块位移（含帧头 `02 01 FE`），且因 CRC 由外设对实际发出字节生成而全程无错。V3 的 ctx 布局使 `frame_buf` 偏移从 9（碰巧对齐）变为 16（必然非对齐），因而必现。修复：`app_ymodem_demo.c` 落盘前经对齐中转缓冲 + 偏移连续性守卫。验证：`tour.wav` 176478 字节板端 CRC32 与 PC 一致（7a6fd6f4），43 块双趟读全一致，可正常播放。详见 `Docs/40-records/串口框架与Ymodem移植记录-20260922.md`
- [x] ~~LittleFS/W25Q 写路径存在块级内容损坏~~ —— **已推翻（2026-09-22）**：裸 `dev_w25q_read` 77 次重复读 0 差异（含跨 4KB 边界地址），LittleFS 单命令内 8 遍 hash 完全一致，干净重传后两后端连测 4 次全部 = PC 基准且 `short=0`。SPI2/W25Q/电气/驱动均无罪
- [x] ~~Ymodem 接收失败/中止会留下**无从发现的坏文件**~~ —— **已修并上板验证（2026-09-23）**：接收统一写固定临时名 `__ymodem.tmp`（避开 FatFs 8.3 限制），提交条件比"协议报 OK"更严：实收字节数 == 声明大小 且 close 无错才 `bsp_file_rename`，否则 `bsp_file_remove` 丢弃；会话中断有兜底收尾，逐文件复位判定位。实测（刻意少发的 `ymodem_short_sender.py`）：声明 176478 实收 10240 → `FAILED bytes=10240/176478 commit=0 (incomplete file discarded)`，且已存在的同名好文件 CRC 不变、两后端无 `.tmp` 残留
- [ ] LittleFS 实例被多使用者无锁共用（`lv_port_fs` + 各 demo + 开机写 `boot.txt`）：`bsp_lfs.c` 的 `lfs_cfg` 未提供 `.lock/.unlock`（`LFS_LOCK` 实为空操作）；且未定义 `LFS_NO_MALLOC`，每次 `lfs_file_open`/`lfs_dir_open` 都要向 C 堆要 256 字节，而实测**堆最大连续可用仅 3584 字节**。修法：给 LittleFS 配专用静态内存池 + 协作式 busy 锁（不长时间关中断），并把 LFS 错误码经 `bsp_file` 透传（现统一压成 `BSP_ERROR`，看不出 `NOSPC`）
- [ ] `imu_read` 持续 `ret = -1`（ICM42688 读不出），而同一 SPI2 上的 W25Q 读写全部正常 ⇒ 独立缺陷，暂候选：`bsp_imu` 的 suspend/resume 链未重新初始化器件（`bsp_bus` 切到 I2S2 时会挂起 IMU）。零成本判据：`imu_read` → `play_wav` → `imu_read`
- [x] ~~DMA 缓冲 4 字节对齐契约仅在一处点状规避~~ —— **已升格为架构级保障（2026-09-23）**：不变式实为 `buf ≡ 文件位置 (mod 4)`（FatFS 会把用户指针推进到扇区边界），落地为 L1 `sd_diskio` 入口拒绝（非对齐/CCM 不可达 → `RES_PARERR`）+ L2 `bsp_file` 统一分段中转（段首尾走窗口、段体批量整扇区经 1KB 对齐暂存区）+ L3 写入第 4 节契约；两个计数器与 `fatfs_test align` 双向矩阵作为可复验凭据。附带修正：`port_spi` 按 `DataSize` 动态定对齐要求（8BIT 无约束，不行误伤）；`bsp_audio` 填充期吞错已改为上报+停播释放总线（旧行为下一次坏读永久占住 SPI2/I2S2）。详见 `Docs/40-records/DMA对齐契约全局化-20260923.md`
- [ ] **SDIO 传输边界缺陷（新发现，未定位）**：在**指针完全合法**（已 4 字节对齐、文件位置扇区对齐）的 raw `f_write` 上，历史签名 `mismatches=36 bad@8..43` 仍间歇复现（本轮 3 跑中 2 次，后续 2 跑 0 次）；另有更严重的确定性形式：L2 暂存区取 512（拆成连续两笔单扇区写）时 Ymodem→SD **必现**自第二笔开头错开 2 字节（偏移 532 起整体位移），改回 1KB 批量整扇区下发后消失。即**传输形状/边界影响结果**，方向在 `WriteStatus` 完成语义（DMA/数据结束中断 vs 卡实际编程完成）与 SDIO FIFO 复位，而非信号质量（上拉、SW7、时钟已逐项排除；栈溢出假设也已用 `Stack_Size=0x2000` 实测否证）。取证入口：`fatfs_test align`（raw 轮为负向用例）、`crcmap`、`ymodem_short_sender.py`
- [ ] FatFs 未开启长文件名：`ffconf.h` `_USE_LFN = 0`，文件名超 8.3 格式时 `f_open` 直接失败（Ymodem 接收报 Code 5）。修复方向：`_USE_LFN = 1` + 静态工作缓冲，需评估 RAM 开销
- [ ] `port_pwm.c` `port_pwm_set_freq()` 定时器时钟域写死 APB1（`HAL_RCC_GetPCLK1Freq()` + `PPRE1` 判 ×2，恒得 84MHz），而 `pwm_mapping` 混挂了 APB2 的 `htim10`（LCD 背光，实际 168MHz）：ARR 算少一半，输出频率为目标的 2 倍。当前潜伏——全工程仅 `dev_buzzer`(TIM13/APB1) 与 `dev_ws2812`(TIM5/APB1) 调该函数，背光只走 `set_duty`（CCR/ARR 比值，与时钟无关）。修复方向：`port_pwm_map_t` 增加总线归属字段，`set_freq` 查表取时钟，禁止运行时猜 `RCC->CFGR`（换板只改表）
- [ ] `bsp_backlight.c` 亮度语义与板级极性相反：`LCD_BLK_PWM` 网络硬件为低电平点亮（依据 `Docs/00-board_info/EC11_LCD_KEY描述.md`），而 TIM10 CH1 配为 PWM1 + `OCPOLARITY_HIGH`、上层按高电平占比等于亮度写 CCR，导致 `backlight 0` 最亮、`backlight 100` 熄灭。修复方向：`port_pwm_map_t` 增加有效电平标记，由 `set_duty` 统一反相，使 Board 层对上维持 0=灭、100=最亮的直觉语义。连带隐患：`bsp_backlight_init()` 是先 `port_pwm_start()` 再 `bsp_backlight_set()`，而 CubeMX 初始 `Pulse=0` 在低有效硬件上等于全亮，定时器启动到设亮度之间可能短暂闪一下最亮（未实测，修反相时应改成先写 CCR 再 start）

（注：本项早先以 zcode 分支自引入自修复、develop 无此代码为由不列入清单；经核实本分支基点 zcode_bsp 的 `pwm_mapping` 已含 APB2 通道 `htim10`，问题真实存在，已上移为上方正式待修项。重写 port_pwm 时直接按"按实例地址归属总线动态判定"实现。）

## 10. 维护约定

1. 新增 port/dev/demo 模块后，在对应清单表加一行
2. 修复已知问题后，勾掉第 9 节对应项
3. 移植批次状态变化时更新第 8 节
4. 契约（第 4 节）变更属于架构决策，须在 commit 正文说明原因
