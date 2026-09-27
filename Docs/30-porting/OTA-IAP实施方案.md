# OTA-IAP 实施方案（固件经 W25Q128 迁移片上 Flash）

> 状态：阶段 1 实施中（重定位改造）
> 分支：`feature/ota-iap`（基于 zcode_bsp）
> 传输通道：UART Ymodem（已字节级验证）；以太网 OTA 留待 lwIP 时代
> 【未验证】整体方案未上板实测

---

## 1. F407 的三个硬件事实（决定方案形态）

1. **单 bank**：无 A/B 区无缝切换能力
2. **无 QSPI 内存映射**：W25Q128 中的代码不能原地执行（无 XIP）
3. **Cortex-M 固定从 0x08000000 启动**：bootloader 必须占据片上 Flash 起点

→ 方案必然是"**外部下载 → 校验 → 拷回片上 → 跳转**"的经典 IAP 形态。

## 2. 存储布局

### 2.1 片上 Flash 重排（阶段 1 完成）

| 区域 | 地址 | 大小 | 说明 |
|---|---|---|---|
| Bootloader | 0x08000000–0x08007FFF | 32KB（扇区 0-1） | 独立小工程，**永不自擦** |
| App 区 | **0x08008000**–0x0807FFFF | 480KB（扇区 2-7） | 现 App 全量迁入 |

App 现态 364KB < 480KB，容量可行。改动三处（阶段 1 已完成，见 §7）：
scatter 基址、`VECT_TAB_OFFSET=0x8000`、uvprojx IROM 三字段。

### 2.2 W25Q128 分区（好消息：零改动）

| 区域 | 地址 | 大小 | 说明 |
|---|---|---|---|
| **OTA 区** | **0x000000–0x3FFFFF** | **4MB** | 现为全空闲（LittleFS 起始于块 1024 = 4MB 处） |
| LittleFS | 0x400000–0xFFFFFFF | 12MB | **一行不改** |

OTA 区头部 64B + 镜像（≤512KB），4MB 甚至容纳双版本（回滚预留）。

## 3. OTA 头部契约（App 与 Bootloader 共享的"收货清单"）

```c
/* ota_image.h —— 两个工程各包含一份，字段增删属架构决策 */
#define OTA_MAGIC      0x4F544147U   /* "OTAG" */
typedef struct {
    uint32_t magic;      /* OTA_MAGIC，非法即整区无效 */
    uint32_t image_size; /* 镜像字节数 */
    uint32_t image_crc;  /* 镜像 CRC-32 */
    uint32_t version;    /* 固件版本号（语义自定） */
    uint32_t state;      /* 状态机：EMPTY→DOWNLOADING→READY→JUMPED→APP_OK */
    uint32_t boot_count; /* bootloader 写入的启动尝试计数（回滚预留） */
    uint32_t reserved[2];
} ota_header_t;          /* 32B 对齐写入，W25Q 页编程 256B 内完成 */
```

**铁律：头部永远最后写。** 状态机与断电安全的关系：

| 断电时机 | 头部状态 | 重启后 bootloader 行为 |
|---|---|---|
| 传输中 | DOWNLOADING | 视为垃圾，等待重新传输（**无需断点续传**，512KB Ymodem ≈ 90s） |
| 拷贝中 | READY | 幂等重擦重拷 |
| 拷完跳转后 | JUMPED | 正常进 App |
| App 自检过 | APP_OK | 正常；回滚逻辑（v2）据 boot_count 判断 |

## 4. 接口沉淀（传输无关的更新器契约）

```c
/* BSP/Board/bsp_ota.h —— Board 层，传输层永远是外挂 */
bsp_status_t bsp_ota_begin(uint32_t image_size);                 /* 擦 OTA 区 → DOWNLOADING */
bsp_status_t bsp_ota_write(const uint8_t *chunk, uint32_t len);  /* 顺序追加 */
bsp_status_t bsp_ota_commit(void);                               /* CRC → READY，重启生效 */
```

Ymodem demo 的 ops 回调对接此三段式；将来 lwIP 版只换传输，契约不动。

## 5. Bootloader 工程（独立、极简、永不自擦）

- 独立 Keil 工程：链接于 0x08000000/32KB，不依赖本 BSP
- 依赖清单：时钟初始化 + 精简 SPI2 裸收发（轮询无 DMA 无仲裁，~100 行）
  + 软件 CRC-32 + W25Q 基础指令（读/JEDEC ID/扇区擦/页编程）
- 行为：读头部 → magic/state/CRC 三验 → 擦 App 扇区(2-7 按需) → 4KB 块拷贝
  → 读回比对 → 跳转（`__disable_irq` → SysTick 复位 → `SCB->VTOR=0x08008000`
  → `__set_MSP(*(0x08008000))` → 跳 `*(0x08008004)`）

## 6. 五阶段实测计划

| 阶段 | 内容 | 通过标准 | 状态 |
|---|---|---|---|
| 1 | App 重定位到 0x08008000 | 全量 Shell 回归矩阵照常 | **实施中** |
| 2 | Bootloader 骨架：banner + CRC 校验 + 跳转（不拷贝） | 复位 → banner → App `ver` 正常 | 待做 |
| 3 | bsp_ota 三段接口 + Ymodem ops 对接 | dump 头部 CRC 与 PC 端一致 | 待做 |
| 4 | 完整闭环：改版本号 → 传输 → 重启 → 拷贝跳转 | **`ver` 显示新版本号** | 待做 |
| 5 | 断电测试 + APP_OK 回滚标志（v2） | 拷贝中拔电 → 自动恢复 | 预留 |

## 7. 阶段 1 已做的改动（本次提交）

1. `SkyStar_BSP_HAL.sct`：`LR_IROM1`/`ER_IROM1` 基址 0x08000000→0x08008000，
   长度 0x80000→0x78000；RAM 区不变
2. `system_stm32f4xx.c`：启用 `USER_VECT_TAB_ADDRESS`，Flash 分支
   `VECT_TAB_OFFSET` = 0x8000（向量表随 App 搬家）
3. `uvprojx`：`<Cpu>` 的 IROM 描述、`<IROM>`、`<OCR_RVCT4>` 三处同步
   （该文件已退出纳管，按治理约定 `git add -f` 手工补交）

## 8. 风险与注意

- **CubeMX regen 会重写 `system_stm32f4xx.c`**——regen 后必须复查
  `USER_VECT_TAB_ADDRESS` 与偏移仍在（ioc 中无链接配置，sct 为手工
  维护文件不受影响）
- Keil 烧录 App 时下载地址随工程 IROM 自动变为 0x08008000，无需额外配置；
  bootloader 烧录用独立工程
- 调试注意：bootloader 与 App 是两个 axf，跨跳转单步会"飞"，属正常现象
- App 运行期禁止对自身扇区（0x08008000 起的扇区 2-7）做任何擦写
