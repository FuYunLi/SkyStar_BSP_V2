# SkyStar_BSP_HAL 工程辅助技能使用规范

本规范旨在为开发人员与 AI 助手提供一套清晰、标准化的工程治理、诊断与分析技能使用指南，确保在 **STM32F4** 和 **Keil MDK** 环境下具备一致的高效开发体验，并严格规范 AI 助手的行为边界。

---

## 1. 设计源起与自适应探测机制

本项目引入的工程辅助技能（位于 **.agents/skills** 目录）均针对 **ARM Cortex-M4** 架构以及 **Keil MDK (uVision)** 工具链进行了深度适配。

这套技能具备高度的**自检测与零人工配置**能力，具体表现在以下几个方面：

### 1.1 工具链与 IDE 路径自探测
所有与编译、调试、分析相关的技能均支持对物理环境的智能检索：
* 脚本会自底向上扫描系统磁盘分区，自动定位 **Keil MDK (UV4.exe)** 的安装目录。
* 成功识别后，系统会将探测出的工具链绝对路径写入并缓存在工程根目录下的配置文件 **.em_skill.json** 中（该文件已加入 **.gitignore** 忽略名单，每个开发者的本地路径各自独立，互不污染）。
* 后续的技能调用会秒级读取 **.em_skill.json** 中的缓存路径，无需重复探测。

### 1.2 调试仿真器物理探针自识别
在烧录和调试过程中，脚本会尝试利用 OpenOCD 发送低开销的握手指令，自动在 **stlink**、**jlink**、**cmsis-dap** 中优选当前已插入的物理调试器，并自动加载对应的接口配置文件。

### 1.3 串口设备智能优选
串口监视技能（**serial-monitor**）通过列出系统中的所有可用 COM 端口，通过描述文本的特征优先级匹配算法，自动优选并锁定板载串口芯片，彻底告别频繁手动查找端口号的繁琐流程。

---

## 2. 核心技能模块使用指南

### 2.1 build-keil (工程自动化构建)
* **设计意图**：提供脱离 IDE 界面限制的命令行编译能力，自动定位 Keil uVision 交叉编译链并执行高效构建。
* **常用命令**：
  * **自探测并保存配置**：
    ```bash
    python .agents/skills/build-keil/scripts/keil_builder.py --detect --save-config
    ```
  * **一键编译指定工程目标**：
    ```bash
    python .agents/skills/build-keil/scripts/keil_builder.py --detect --project MDK-ARM/SkyStar_BSP_HAL.uvprojx --target SkyStar_BSP_HAL
    ```

### 2.2 memory-analysis (固件内存与 Section 空间分析)
* **设计意图**：分析编译生成的 AXF 或 MAP 文件，统计各数据段大小。
* **常用命令**：
  * **分析指定 AXF 空间明细**：
    ```bash
    python .agents/skills/memory-analysis/scripts/memory_analyzer.py --elf MDK-ARM/SkyStar_BSP_HAL/SkyStar_BSP_HAL.axf
    ```

### 2.3 flash-keil / flash-openocd (自动化烧录)
* **设计意图**：配合 Keil 命令行或 OpenOCD 完成一键固件下发。
* **常用命令**：
  * **通过 Keil 内置调试器执行下载**：
    ```bash
    python .agents/skills/flash-keil/scripts/keil_flasher.py --project MDK-ARM/SkyStar_BSP_HAL.uvprojx
    ```
  * **通过 OpenOCD 烧录 AXF 固件**：
    ```bash
    python .agents/skills/flash-openocd/scripts/openocd_flasher.py --artifact MDK-ARM/SkyStar_BSP_HAL/SkyStar_BSP_HAL.axf
    ```

### 2.4 debug-gdb-openocd (GDB 调试与崩溃现场分析)
* **设计意图**：提供命令行调试与崩溃诊断。针对 **Cortex-M4** 的异常处理，支持解析 SCB (System Control Block) 相关的寄存器现场（如 HardFault 等）。
* **常用命令**：
  * **崩溃现场快照分析 (crash-context)**：
    当芯片运行挂起、串口无响应或怀疑触发 HardFault 时，执行此命令。脚本将自动向 GDB 注入分析序列，读取异常现场寄存器：
    ```bash
    python .agents/skills/debug-gdb-openocd/scripts/gdb_debugger.py --elf MDK-ARM/SkyStar_BSP_HAL/SkyStar_BSP_HAL.axf --mode crash-context
    ```

### 2.5 serial-monitor (串口日志与审计)
* **设计意图**：不仅是串口监听，还支持通过控制调试器下发复位信号，以近乎零延迟的同步方式捕捉系统上电初期的 Log。
* **常用命令**：
  * **自动连接匹配的串口并监听**：
    ```bash
    python .agents/skills/serial-monitor/scripts/serial_monitor.py --auto
    ```

### 2.6 static-analysis (静态代码扫描)
* **设计意图**：多层缺陷分析，集成 **cppcheck** 与 **clang-tidy**。
* **常用命令**：
  * **使用编译数据库进行全项目静态分析**：
    ```bash
    python .agents/skills/static-analysis/scripts/static_analyzer.py --compile-db compile_commands.json --cppcheck
    ```

---

## 3. 协作开发及防错契约 (AI 强红线约束)

为保证团队协作及本地代码库的安全，AI 助手在项目开发中必须严格遵守以下契约约束：

### 3.1 缓存屏蔽
本地路径配置文件 **.em_skill.json** 属于开发者个人开发机环境，禁止执行 Git 提交与推送。

### 3.2 AI 开发节奏与操作权限边界（防止越权编译与黑盒修改）
* **禁止擅自编译与调试**：AI 助手在修改完代码后，**严禁不经汇报便主动运行编译 (build-keil)、烧录或调试等命令**。修改完成后，必须立即向开发者清晰展示修改的代码 Diff 与设计意图，得到开发者的显式确认后方可进行后续操作。
* **严禁黑盒闭环迭代**：当编译出错或运行异常时，AI 助手**绝对禁止在未与开发者对齐方案前，擅自进入“修改代码 -> 重新编译 -> 再修改 -> 再编译”的闭环黑盒调试流程**。AI 助手必须在第一时间搜集并呈报错误日志、分析根本原因，给出明确的修改方案供开发者审核。
* **保存现场与可恢复性优先**：任何情况下，修改业务代码都必须保障现场的可恢复性，未经授权绝对不能破坏当前已验证的编译配置、宏定义或硬件跳线默认状态。

### 3.3 STM32CubeMX 强红线契约
对于基于 CubeMX 生成的项目（包含 **.ioc** 工程文件的项目），必须遵循以下**高优先级红线约束**：
* **绝对不允许 Agent 主动修改 CubeMX 相关的配置或代码**：
  * 严禁 AI 助手修改 **SkyStar_BSP_HAL.ioc** 描述文件；
  * 严禁 AI 助手修改或编辑由 CubeMX 自动生成的、非用户保护区域（即非 **USER CODE BEGIN** / **USER CODE END** 包裹的区域）的任何 C 语言代码。
* **严格执行“主动汇报，手动修改”流程**：
  * 凡是涉及管脚分配、复用功能选择、总线使能、中断优先级与中断通道开启、时钟树配置等硬件底层变动，AI 助手**必须在对话中主动、清晰地向开发者说明配置需求**（例如：“请在 CubeMX 中将 PE3 配置为下降沿外部中断，使能 EXTI3 中断通道”），引导并提示开发者手动使用 STM32CubeMX 图形化界面进行修改，并点击 **Generate Code** 重新生成。
* **明确“工程辅助”与“黑盒开发”的界限**：
  * **允许的工程辅助操作**：添加外设源文件到 Keil 虚拟工程（如使用 **keil_path** 辅助脚本）、重新生成非侵入式的 **compile_commands.json** 及 **.clangd** 跳转配置文件以提供智能提示。这些操作纯属辅助性工具，不涉及硬件黑盒逻辑。
  * **绝对禁止的开发操作**：擅自更改硬件初始化逻辑、编辑 **.ioc** 配置文件、手动修改非用户保护区的底层初始化代码。

### 3.4 C 代码排版
编写调试验证临时代码时，应严格遵循大括号独立的 **Allman** 排版风格：
```c
void dummy_function(void)
{
    if (condition)
    {
        do_something();
    }
}
```
