# SkyStar BSP V2 迭代日志

## 阶段零：架构基础

### 2025-05-04

#### 创建文件

| 文件 | 说明 |
|------|------|
| `BSP/Board/bsp_board.h` | 状态码枚举、断言宏、常用宏 |
| `BSP/Board/bsp_board.c` | `bsp_status_str()` 状态码字符串转换 |
| `BSP/Board/bsp_config.h` | 系统时钟配置 |
| `BSP/Interface/port_tick.h` | 系统滴答接口头文件 |
| `BSP/Interface/port_tick.c` | 系统滴答接口实现（基于 HAL_GetTick） |
| `App/app_main.h` | 应用入口头文件 |
| `App/app_main.c` | 应用入口实现（含 MultiTimer 示例） |
| `Middleware/MultiTimer/MultiTimer.h` | 原库头文件 |
| `Middleware/MultiTimer/MultiTimer.c` | 原库实现 |
| `Middleware/MultiTimer/multi_timer_port.c` | 平台适配层 |
| `.clang-format` | Allman 风格配置 v1.3.0 |
| `.clangd` | clangd LSP 配置（包含路径、宏定义） |
| `.gitignore` | Git 忽略规则（Keil 编译产物、用户配置） |
| `compile_commands.json` | 编译数据库（IDE 代码跳转支持） |
| `Docs/代码规范.md` | 项目代码规范文档 |
| `README.md` | 项目说明与模块状态表 |

#### 删除文件

| 文件 | 原因 |
|------|------|
| `Middleware/Utils/bsp_utils.h` | 合并到 `bsp_board.h`，避免重复定义 |

#### 修改文件

| 文件 | 修改内容 |
|------|----------|
| `Core/Src/main.c` | 添加 MultiTimer 初始化和调度调用 |
| `MDK-ARM/SkyStar_BSP_HAL.uvprojx` | 添加包含路径、源文件、虚拟目录 |

#### 配置工作

| 配置项 | 内容 |
|--------|------|
| Keil 工程 | 添加 BSP/Board、BSP/Interface、App、Middleware/MultiTimer 包含路径 |
| Keil 工程 | 添加虚拟目录分组 |
| `.clang-format` | 配置 Allman 风格、禁止单行函数、extern "C" 块处理 |
| `.clangd` | 配置 ARM 交叉编译目标、包含路径、预定义宏 |
| `.gitignore` | 配置 Keil 编译产物、用户配置文件、IDE 临时文件忽略规则 |

#### .trae 目录说明

`.trae/` 是 Trae IDE 的技能配置目录，来自于 `https://github.com/LeoKemp223/embed-ai-tool.git`，包含嵌入式开发工具链：

| 技能 | 说明 |
|------|------|
| `build-keil` | Keil MDK 命令行编译 |
| `build-cmake` | CMake 构建支持 |
| `build-idf` | ESP-IDF 构建支持 |
| `clangd_config` | clangd 配置生成（**新增**） |
| `keil_path` | Keil 工程路径管理（**新增**） |
| `flash-keil` | Keil 烧录支持 |
| `flash-jlink` | J-Link 烧录支持 |
| `flash-openocd` | OpenOCD 烧录支持 |
| `debug-jlink` | J-Link GDB 调试 |
| `debug-gdb-openocd` | OpenOCD GDB 调试 |
| `serial-monitor` | 串口监控 |
| `memory-analysis` | 内存分析 |
| `static-analysis` | 静态代码分析 |
| `stm32-hal-development` | STM32 HAL 开发指南 |

**新增技能说明**：

| 技能 | 功能 | 使用场景 |
|------|------|----------|
| `keil_path` | 管理 Keil 工程的源文件、包含路径、虚拟目录 | 新增 BSP 驱动文件后自动添加到工程 |
| `clangd_config` | 生成 `compile_commands.json` 和 `.clangd` | 配置 IDE 代码跳转和智能提示 |

**注意**：`.trae/` 目录由 Trae IDE 自动管理，无需手动编辑。

#### 与规划差异

| 规划内容 | 实际执行 | 原因 |
|----------|----------|------|
| `Middleware/Utils/` 独立目录 | 合并到 `bsp_board.h` | 避免宏定义重复，减少文件数量 |
| `bsp_config.h` HAL 句柄映射宏 | 删除，改为 Interface 层数组映射 | 参考 V1 做法，数组映射更灵活、类型安全 |
| MultiTimer 阶段一引入 | 阶段零引入 | 事件驱动架构是核心基础，提前引入 |
| `Docs/V2_规范手册.md` | `Docs/代码规范.md` | 文件命名更简洁 |

#### 编译验证

```
✅ 编译成功
  Flash ≈ 3.2 KB
  RAM   ≈ 8.1 KB
```

#### 规划修订

已更新 `Docs/SkyStar_BSP_V2开发路径规划.md` 至 v1.1 版本。

---

## 相较于纯 CubeMX 生成工程的额外工作

### 一、目录结构创建

| 目录 | CubeMX 生成 | 额外创建 |
|------|-------------|----------|
| `Core/` | ✅ | - |
| `Drivers/` | ✅ | - |
| `MDK-ARM/` | ✅ | - |
| `BSP/Board/` | - | ✅ |
| `BSP/Driver/` | - | ✅ |
| `BSP/Interface/` | - | ✅ |
| `App/demos/` | - | ✅ |
| `App/tasks/` | - | ✅ |
| `Middleware/` | - | ✅ |
| `Docs/` | - | ✅ |

### 二、源文件创建

| 文件 | 说明 |
|------|------|
| `BSP/Board/bsp_board.h/c` | 状态码、断言宏、常用宏 |
| `BSP/Board/bsp_config.h` | 系统时钟配置 |
| `BSP/Interface/port_tick.h/c` | 系统滴答接口 |
| `App/app_main.h/c` | 应用入口 |
| `Middleware/MultiTimer/*` | 软件定时器中间件 |

### 三、配置文件创建

| 文件 | 说明 |
|------|------|
| `.gitignore` | Git 忽略规则 |
| `.clang-format` | 代码格式化配置 |
| `.clangd` | clangd LSP 配置 |
| `compile_commands.json` | 编译数据库 |

### 四、文档创建

| 文件 | 说明 |
|------|------|
| `README.md` | 项目说明 |
| `Docs/代码规范.md` | 代码规范文档 |
| `Docs/SkyStar_BSP_V2开发路径规划.md` | 开发路径规划 |
| `Docs/SkyStar_BSP迭代日志.md` | 迭代日志 |

### 五、工程配置修改

| 修改项 | 说明 |
|--------|------|
| Keil 包含路径 | 添加 BSP、App、Middleware 目录 |
| Keil 虚拟目录 | 添加分组便于管理 |
| Keil 源文件 | 添加自定义源文件 |

### 六、main.c 修改

| 修改项 | 说明 |
|--------|------|
| 包含头文件 | `#include "app_main.h"` |
| 初始化调用 | `app_main_init()` |
| 主循环 | `multi_timer_process()` |

---

## 版本控制说明

### 提交格式

采用 Conventional Commits 规范：

```
<type>(<scope>): <中文简述>

type:  feat / fix / refactor / docs / test
scope: arch / interface / driver / middleware / demo / app
```

### 版本号规则

| 版本 | 含义 | 时机 |
|------|------|------|
| **v0.x.0** | 开发阶段版本 | 每个规划阶段完成 |
| **v1.0.0** | 正式发布 | 所有核心功能完成 |
| **v1.x.0** | 功能更新 | 新增重要功能 |

### 阶段零提交建议

```
feat(arch): V2 项目框架、规范基础与 MultiTimer 中间件

- 目录结构：BSP/{Board,Driver,Interface} / App/{demos,tasks} / Middleware/
- bsp_board.h：状态码、断言宏、常用宏
- bsp_config.h：系统时钟配置
- MultiTimer：软件定时器中间件移植
- .clang-format：Allman 风格配置 v1.3.0
- .clangd：clangd LSP 配置
- .gitignore：Git 忽略规则
- compile_commands.json：编译数据库
- Docs/代码规范.md：项目代码规范文档
- README.md：模块状态表

验收：空编译通过，无业务代码
```
