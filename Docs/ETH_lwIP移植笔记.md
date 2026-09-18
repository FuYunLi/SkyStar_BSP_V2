# 以太网 lwIP 移植笔记（M39/M40 规划稿）

> 状态：设计稿（未接入工程，等待 CubeMX regen）
> 硬件：LAN8720A PHY（RMII 接口），板载隔离变压器与 RJ45
> 【未验证】本文档为指导笔记，未在板上实测

---

## 1. 硬件资源（引脚分配总表摘录）

| MCU 引脚 | 复用功能 | 说明 |
|---|---|---|
| PA1 | ETH_REF_CLK | RMII 50MHz 参考时钟（**PHY 输出给 MCU**） |
| PA2 | ETH_MDIO | 管理数据（SMI 两线之一） |
| PA7 | ETH_CRS_DV | 载波侦听/数据有效 |
| PC1 | ETH_MDC | 管理时钟 |
| PC4/PC5 | ETH_RXD0/RXD1 | 接收数据 |
| PB11/PB12/PB13 | TX_EN/TXD0/TXD1 | 发送 |
| PE12 | ETH_NRST | PHY 硬件复位（GPIO，低有效） |

RMII 模式下 REF_CLK 必须 50MHz 常在，MCU 的 ETH MAC 全部时序由它驱动——
这是 RMII 与 MII 的核心差异。

## 2. CubeMX 配置清单

1. Connectivity → ETH：Mode = **RMII**（PHY 芯片选 LAN8720）
2. PHY 地址：LAN8720 的 PHYAD0 引脚 strap 决定 0 或 1——**查底板原理图
   确认**，CubeMX 默认 0，错则 PHY 完全无应答
3. Middleware → LWIP：
   - 裸机路线：NO_SYS 勾选（轮询式），lwIP 定时由 SysTick 驱动
   - 内存：MEM_SIZE 建议 8-16KB；PBUF 池按并发连接估算
   - 启用 ICMP（ping）/UDP/TCP 按 42 例目标递增
4. 生成文件：`ethernetif.c`（netif 对接层）、`lwip.c`（初始化与定时）、
   `Middlewares/Third_Party/LwIP/...`

## 3. PHY 驱动设计（dev_lan8720）

LAN8720 的寄存器经 SMI（MDIO/MDC）访问，标准 IEEE 802.3 页 + 厂商页：

| 寄存器 | 用途 | 要点 |
|---|---|---|
| 0x00 BCR | 复位/速率/双工/自动协商 | Bit15 复位后等待完成 |
| 0x01 BSR | 链路状态 | Bit2 = Link Up（锁存型，读法有讲究） |
| 0x1F 特殊模式 | 模式/strap 回读 | 地址 strap、时钟模式确认 |
| 31 PHYSR | 解析速率与双工结果 | 厂商页，链路建立后读取 |

SMI 读写复用 HAL ETH 的 `HAL_ETH_ReadPHYRegister/WritePHYRegister`——
所以 dev_lan8720 依赖 port_eth 先把 MAC 时钟与 SMI 配好。

## 4. BSP 对接架构

```
lwIP (netif/ethernetif.c)  ← CubeMX 生成，含 DMA 描述符环
        │
   port_eth (Interface)   ← 唯一新增 port 模块：MAC 启停、链路查询、
        │                    SMI 读写转发、ETH IRQ 托管
   dev_lan8720 (Driver)   ← PHY 复位时序(PE12)、自协商、链路参数
        │
   bsp_net (Board)        ← netif 生命周期、DHCP、ping/回环指令
```

- ethernetif.c 里 CubeMX 已把 HAL_ETH 的描述符环封装好，BSP 不重复造
- 移植工作量集中在：**PHY 复位时序**（PE12 拉低 ≥100µs 再释放，等上电
  稳定后才能读 BSR）与 **链路事件**（拨线/插线的重协商处理）

## 5. 验收计划（M39/M40）

1. `eth_link` 指令：读 PHY 状态打印速率/双工/链路
2. DHCP 或静态 IP 上线，PC `ping` 通（ICMP 走通 = 描述符环与收发链路全通）
3. UDP 回环（PC 发 UDP，板子原样回发）——对标 rocketpi 例程精神
4. TCP echo server —— socket API 学习
5. （远期）HTTP/MQTT → 与 mbedTLS 汇合进毕业设计

## 6. 已知风险与内存账

- **SRAM 紧张**：lwIP 堆 + PBUF + ETH DMA 描述符/缓冲 ≈ 20-40KB，叠加
  LVGL(约 40KB)与音频缓冲(8KB)后逼近 192KB 上限——建议 ETH 里程碑做时
  同步做一次全工程内存审计（沿用阶段 7 的方法）
- REF_CLK 走线方向是硬件既定，软件只管用；若 50MHz 缺失，MAC 完全不动
- 自动协商失败优先查：PHY 地址 strap、NRST 时序、双绞线线序
- 裸机 NO_SYS 路线下 lwIP 定时器靠主循环喂——遵守全回调架构：`eth_timer`
  挂 MultiTimer，收包轮询挂 app_main_process
