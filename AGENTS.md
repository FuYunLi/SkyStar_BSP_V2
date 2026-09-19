# SkyStar BSP V2 — Agent 工作守则

> 本文件是 AI Agent 在本仓库的入口指令。先读完本文件,再按启动链按需读取,禁止一上来通读全工程。

## 工程摘要(必知)

- STM32F407VET6,立创天空星核心板 + 筑基底板,裸机分层 BSP(无 RTOS),Keil MDK 工程 `MDK-ARM/SkyStar_BSP_HAL.uvprojx`

### 设计理念(agent 须先理解再动手)

本工程是**分层 BSP 架构**:Interface(`port_`)→ Driver(`dev_`)→ Board(`bsp_`)三层,与 Middleware、APP 共同构成四个抽象级。核心目的:

1. **底层隔离**:HAL/标准库的类型、句柄、错误码止步于 Interface 层,经 `hal_to_bsp_status()` 翻译为全工程统一的 `bsp_status_t`;上层只见逻辑 ID 与统一错误语义,硬件与底层问题被隔离在应用层之下。
2. **跨芯片兼容的机制**:每支 port 用"逻辑 ID + 静态映射表"对接物理外设,换板/换芯片 = 改 Interface 映射表,上层代码不动。
3. **三层协作向上构建功能框架**:Interface 只做硬件抽象(含统一异步回调 `port_async_cb_t`),Driver 实现具体器件驱动,Board 是契约中枢(`bsp_board.h`)与板级业务封装;功能化 API 是三层共同向上搭出的,不是 Interface 一层包办。
4. **积极集成开源中间件**(MultiTimer/LwRB/EasyLogger/letter-shell/LVGL 等):接口层的规范抽象为中间件接入提供便捷与性能保证;BSP + Middleware 一起支撑 APP 层实现复杂应用。
5. **为 agent 而设计**:底层被隔离后,APP 层可被 AI 工具安全、高效地参与开发;验收惯例是 Demo 导出 shell 命令(`SHELL_EXPORT_CMD`)实测。

### 铁律(细则见 Docs/01-map/ARCHITECTURE.md 第 3-4 节)

- 依赖只许自上而下;HAL 类型/句柄只许出现在 Interface 层;上层只认逻辑 ID 与 `bsp_status_t`
- 换板 = 改 Interface 层静态映射表;契约变更属架构决策,须在 commit 正文说明原因
- CubeMX 配置 `SkyStar_BSP_HAL.ioc`:重新生成会覆盖 `Core/`,改动前须知悉

## 启动链(按任务读取,禁止全工程通读)

| 本次任务 | 必读 | 说明 |
|---|---|---|
| 任何任务第一步 | `Docs/01-map/ARCHITECTURE.md` | 工程地图:目录/契约/进度/已知问题,130 行 |
| 硬件引脚/外设分配 | `Docs/00-board_info/` | 引脚总表与外设描述,勿凭记忆猜 |
| 写/改代码 | `Docs/10-standards/工程规范.md` 对应章节 + 目标模块头文件 | 命名/注释/头文件规范;只读要改的模块,不通读实现 |
| 提交代码 | `Docs/10-standards/Git规范.md` | Conventional Commits(中文)+ 分支策略 |
| 新一轮开发规划 | `Docs/20-planning/开发规划.md` 第 0 节"当前状态" + `Docs/01-map/ARCHITECTURE.md` 第 8 节进度表 | 规划正文是历史存档,进度冲突以 ARCHITECTURE.md 为准 |
| 编译/烧录/调试/内存分析/串口 | 直接用 `.agents/skills/` 对应技能 | 技能自带流程,无需读文档 |
| 新增外设驱动 | `Docs/10-standards/LibDriver引入与适配规范.md`、`Docs/10-standards/Keil虚拟文件夹规范.md` | |
| 移植对照上游/查参考仓库 | `Docs/01-map/参考仓库索引.md` | 上游地址速查,不必联网搜索 |
| 排查历史问题 | `Docs/30-porting/`、`Docs/40-records/` 按文件名匹配 | 按需读,不预读 |
| GUI 相关 | `Docs/30-porting/LVGL*移植报告.md` | |

定位改动点:先看目标模块接口头文件确认调用关系,再进实现;禁止逐目录浏览全仓库。

## 行为红线(浓缩自 .agents/rules/agent-constraints.md,该文件为完整版)

1. **权限**:默认"本地开发"级——可改本地代码,禁止任何远程操作(push/删分支/改 remote);涉及提交、分支、烧录必须先向用户说明并确认。
2. **Git 禁令**:禁止 push 受保护分支(master/develop)、force push、删远程分支、`git clean -fdx`、`reset --hard origin/*`。
3. **提交**:必须由用户主动发起,Agent 严禁自主 commit;提交日志用中文 Conventional Commits,须体现代码设计意图、总线/外设与技术栈细节;合入 develop 用 `--no-ff`。
4. **编译结果必须报告**:成功报 Flash/RAM/产物/耗时;失败报文件:行号与错误摘要后**停止**——禁止自动改代码循环重试、禁止静默跳过、禁止假设原因;修复方案须用户确认。
5. **最小修改原则**:只改与当前任务直接相关的最少代码;严禁未经要求擅自优化/重构/蔓延改动;修改前明确回滚路径。
6. **例外(允许自动做)**:新增文件时自动维护 Keil 虚拟工程、包含路径、clangd/compile_commands 配置。
7. **文档**:禁止擅自修改 `Docs/00-board_info/`(需确认);架构契约变更属重大决策,须在 commit 正文说明原因。

## 嵌软硬约束(防"硬件死锁/静默无输出"类低级错误)

1. **ISR 显式绑定**:任何中断/DMA 驱动,必须打开 `Core/Src/stm32f4xx_it.c`,在对应 IRQHandler 的 USER CODE 区显式挂载驱动层处理入口(如 `port_uart_irq_handler`);DMA 传输须绑定 `HAL_DMA_IRQHandler`。严禁假设"初始化了就能收发中断"。
2. **WEAK 核对**:未重写的弱定义 ISR 链接时不报错,必须用代码搜索主动核对,不依赖链接器。
3. **端口静默排查 SOP**(自底向上,严禁直接猜时钟):①审计 `main.c` 初始化调用顺序 → ②审计 it.c 中断链与驱动初始化返回值 → ③`HAL_UART_Transmit` 阻塞直发隔离验证 → ④才允许申请检查物理连接与时钟 HSI/HSE 隔离。

## 维护约定

- 新增 port/dev/demo 模块、修复已知问题、移植批次状态变化:同步更新 `Docs/01-map/ARCHITECTURE.md` 对应清单(第 4/8/9 节)
- 里程碑/待办状态变化时同步更新 `Docs/20-planning/开发规划.md` 第 0 节"当前状态"
- 可复用工作流优先沉淀为 `.agents/skills/` 技能,而非写进本文件
