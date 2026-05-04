# SkyStar BSP V2

基于 STM32F407 的板级支持包，采用三层架构设计。

## 目录结构

```
SkyStar_BSP_HAL/
├── BSP/
│   ├── Board/           # 板级服务层（状态码、断言、常用宏）
│   ├── Driver/          # 设备驱动层
│   └── Interface/       # 硬件抽象层（HAL 句柄映射）
├── App/
│   ├── demos/           # 独立 Demo
│   ├── tasks/           # 常驻后台任务
│   └── app_main.c/h     # 应用入口
├── Middleware/
│   └── MultiTimer/      # 软件定时器
├── Core/                # CubeMX 生成代码
├── Drivers/             # CMSIS + HAL 库
└── Docs/                # 文档
```

## 架构设计

### 三层架构

| 层级 | 命名前缀 | 职责 |
|------|----------|------|
| **Board** | `bsp_` | 板级业务封装 |
| **Driver** | `dev_` | 设备驱动实现 |
| **Interface** | `port_` | 硬件抽象层（含 HAL 句柄映射） |

### 事件驱动

- **main.c**：唯一 while(1)，只调基础设施
- **app_main.c**：注册任务/Demo，无循环
- **MultiTimer**：统一调度，回调驱动

## 核心文件

| 文件 | 说明 |
|------|------|
| `BSP/Board/bsp_board.h` | 状态码、断言宏、常用宏 |
| `BSP/Board/bsp_config.h` | 系统时钟配置 |
| `Middleware/MultiTimer/` | 软件定时器调度 |

## 模块状态

| 模块 | 状态 | 说明 |
|------|------|------|
| BSP 框架 | ✅ | 阶段零完成 |
| MultiTimer | ✅ | 已移植 |
| UART | ⏳ | 待开发 |
| Shell | ⏳ | 待移植 |
| EasyLogger | ⏳ | 待移植 |

## 开发规范

详见 [代码规范.md](Docs/代码规范.md)

## 开发路径

详见 [SkyStar_BSP_V2开发路径规划.md](Docs/SkyStar_BSP_V2开发路径规划.md)

## 依赖

- STM32CubeF4 HAL 库
- MultiTimer (MIT)
- Letter Shell (MIT) - 待移植
- EasyLogger (MIT) - 待移植

## 许可证

MIT License
