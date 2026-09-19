# USB CDC/MSC 移植笔记（M45 规划稿）

> 状态：设计稿（未接入工程，等待 CubeMX regen）
> 对标：RocketPi usb_cdc / usb_msc
> 【未验证】本文档为指导笔记，未在板上实测

---

## 1. 硬件资源

- USB OTG FS 内置于 F407（无需外部 PHY），PA11=DM、PA12=DP，直连核心板 Type-C
- VBUS 感测可关闭（自供电设备），PA9 的 VBUS 引脚不接

## 2. ⚠️ 时钟前置条件（现有工程的真雷）

USB OTG FS 要求精确 **48MHz** 内核时钟（来自主 PLL 的 /Q 分频）。

| 参数 | 当前工程 | USB 需要 |
|---|---|---|
| PLL | /M=4 ×N=168 /P=2 → 168MHz | 不变 |
| **PLLQ** | **4 → 336/4 = 84MHz ❌** | **7 → 336/7 = 48MHz ✅** |

启用 USB 时 CubeMX 会强制把 PLLQ 改为 7。注意 PLLQ 同时供给 **SDIO 与 RNG**：
SDIO 基础时钟由 84MHz 变 48MHz 后，SDIO 时钟分频链会自动适配（CardClock =
48MHz/(2×CLKDIV)），FatFS 驱动无需改动，但**实测 TF 卡读写速度会变化**，移植后回归测试 SDIO 即可。

## 3. CubeMX 配置清单

1. Connectivity → USB_OTG_FS：Mode = Device_Only；VBUS Sense = 关闭
2. Middleware → USB_DEVICE：Class For FS IP = **CDC (Virtual Port Com)**（MSC 为第二里程碑）
3. 时钟树确认 PLLQ=7、48MHz 树点亮
4. NVIC：USB_OTG_FS_IRQn 使能（CubeMX 自动）

## 4. 生成文件清单（CubeMX 产出，均不手改）

```
Core/Src/usb_otg.c                  ← OTG_FS 内核初始化（GPIO/时钟/PCD 句柄）
USB_DEVICE/App/usb_device.c         ← 协议栈总入口 MX_USB_DEVICE_Init
USB_DEVICE/App/usbd_desc.c          ← 描述符（VID/PID 在此改）
USB_DEVICE/App/usbd_cdc_if.c        ← ★ 唯一需要桥接的文件（见 §5）
USB_DEVICE/Target/usbd_conf.c/.h    ← 栈配置（含 PCD 回调转发）
Middlewares/ST/STM32_USB_Device_Library/
  ├── Core/src/usbd_core.c          ← 枚举状态机、标准请求
  ├── Core/src/usbd_ctlreq.c        ← 控制传输处理
  ├── Core/src/usbd_ioreq.c         ← 数据阶段搬运
  └── Class/CDC/src/usbd_cdc.c      ← CDC 类实现（EP 大小/端点配置）
```

## 5. BSP 对接架构（桥接设计）

ST 栈自成体系，与三层 BSP 的边界收敛在 `usbd_cdc_if.c` 一个文件上：

```
PC ⇄ USB OTG FS ⇄ PCD(HAL) ⇄ usbd_core ⇄ usbd_cdc ⇄ usbd_cdc_if.c
                                                        │  ← 桥接点
                                                   bsp_usb (Board)
                                                        │
                                              Shell/应用层数据路由
```

- **数据入口**：`CDC_Receive_FS(uint8_t* buf, uint32_t *len)`（ST 生成、用户填）
  → 调 `bsp_usb_on_rx(buf, len)`（Board 层环形缓冲 + 上层回调）
- **数据出口**：`CDC_Transmit_FS(buf, len)`（ST 提供）→ 封装为 `bsp_usb_send()`
- **port 层角色弱化**：ST 栈已接管 PCD/中断，Interface 层不设 port_usb，
  仅保留 `BSP/Board/bsp_usb.c/h` 作为路由黑盒（符合"HAL 类型不出 Interface"——
  生成代码本身就在 ST 层）
- **坑位**：`APP_RX_DATA_SIZE`/`APP_TX_DATA_SIZE` 默认 4/4KB，与现有 SRAM
  账本核对后可裁到 512/512；usbd_desc.c 的 VID/PID 建议改成自有值避免与
  开发板批量设备冲突

## 6. 验收计划（42 例对标）

| 里程碑 | 内容 | 对标 |
|---|---|---|
| M45a | CubeMX regen + CDC 透传（PC 收发回环） | rocketpi_usb_cdc |
| M45b | bsp_usb 路由接入 Shell/Logger 双通道 | — |
| M45c | MSC：SD 卡变读卡器（需 MSC class + FatFS 挂起互斥） | rocketpi_usb_msc |

MSC 与 FatFS 的**介质互斥**（SD 卡不能同时被 FatFS 和 USB 主机写）是 M45c
的核心教学点：挂载状态切换需在 bsp_storage 层做会话仲裁。

## 7. 已知风险

- PLLQ 变更后 SDIO/RNG 回归（见 §2）
- usbd_cdc_if.c 的接收缓冲在 USB 中断上下文，路由必须走轻量标志 + 主循环消费
  （与 port_uart 的 ISR 纪律一致）
- Windows 端 CDC 驱动免驱（win10+），枚举失败优先查描述符与 PLLQ
