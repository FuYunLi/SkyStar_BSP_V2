# SkyStar BSP V2

&#x20;面向32单片机各类底层库的裸机分层 BSP 框架，采用 Interface 抽象 + 极简驱动 + 中间件集成 的三层架构，专注于接口层非阻塞设计与硬件资源管理。当前仓库基于立创天空星(stm32f407vet6主控)筑基板和STM32的HAL库,接口层设计可屏蔽底层差异适配多型号单片机或底层库,驱动层采用单实例极简设计,积极拥抱各类优秀开源中间件/开源通用驱动库,应用层采用事件驱动设计.总体上追求接口非阻塞屏蔽底层可复用,适用于裸机,可接入rtos内核但不适用于各类框架(如RT-Thread,ESP-IDF),若脱离接口层BSP价值较低,仅供学习参考.

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

| 层级            | 命名前缀    | 职责                |
| ------------- | ------- | ----------------- |
| **Board**     | `bsp_`  | 板级业务封装            |
| **Driver**    | `dev_`  | 设备驱动实现            |
| **Interface** | `port_` | 硬件抽象层（含 HAL 句柄映射） |

### 事件驱动

- **main.c**：唯一 while(1)，只调基础设施
- **app\_main.c**：注册任务/Demo，无循环
- **MultiTimer**：统一调度，回调驱动

## 核心文件

| 文件                       | 说明          |
| ------------------------ | ----------- |
| `BSP/Board/bsp_board.h`  | 状态码、断言宏、常用宏 |
| `BSP/Board/bsp_config.h` | 系统时钟配置      |
| `Middleware/MultiTimer/` | 软件定时器调度     |

## 模块状态

| 模块         | 状态 | 说明    |
| ---------- | -- | ----- |
| BSP 框架     | ✅  | 阶段零完成 |
| MultiTimer | ✅  | 已移植   |
| UART       | ⏳  | 待开发   |
| Shell      | ⏳  | 待移植   |
| EasyLogger | ⏳  | 待移植   |

## 开发规范

详见 [代码规范.md](Docs/代码规范.md)

## 开发路径

详见 [SkyStar\_BSP\_V2开发路径规划.md](Docs/SkyStar_BSP_V2开发路径规划.md)

## 依赖

- STM32CubeF4 HAL 库
- MultiTimer (MIT)
- Letter Shell (MIT) - 待移植
- EasyLogger (MIT) - 待移植

## 许可证

MIT License
