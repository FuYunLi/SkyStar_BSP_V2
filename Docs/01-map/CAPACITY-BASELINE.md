# SkyStar BSP V2 - 容量基线 (CAPACITY BASELINE)

> 快照时间：2026-09-24　基点：`zcode_bsp` @ `fded7b5`　工具链：ARMCC V5.06 update 5
> 数据来源：`MDK-ARM/SkyStar_BSP_HAL/SkyStar_BSP_HAL.map`，由 `Other/tools/MemoryMap/parse_map.ps1` 解析（非估算）
> 用途：回答"还剩多少空间给应用层"，并把每次改动的容量趋势固定下来。**每次新增静态缓冲/Demo 后复跑一次。**

---

## 1. 区域总账

| 区域 | 用途 | 已用 | 上限 | **剩余** | 占用率 |
|---|---|---|---|---|---|
| `ER_IROM1` 0x08000000 | Flash 代码+常量 | 364,584 B | 524,288 B | **159,704 B（156 KB）** | 69.5 % |
| `RW_IRAM1` 0x20000000 | 主 SRAM1 | 114,592 B | 114,688 B | **96 B** | **99.9 %** |
| `RW_IRAM2` 0x2001C000 | 主 SRAM2 | 16,280 B | 16,384 B | **104 B** | **99.4 %** |
| `RW_IRAM_CCM` 0x10000000 | CCM（仅 CPU 可访问） | 26,132 B | 65,536 B | **39,404 B（38.5 KB）** | 39.9 % |

RAM 合计已用 **157,004 B**。含栈 8,192 B（`Stack_Size 0x2000`，落在 SRAM1）与堆 4,096 B（`Heap_Size 0x1000`，落在 SRAM2）。

**一句话结论：不是"没空间"，是空间放错了地方——主 SRAM 只剩 200 B，而 CCM 还空着 38.5 KB。**

---

## 2. 硬约束（决定什么能挪、什么不能挪）

| 约束 | 依据 | 后果 |
|---|---|---|
| **CCM(0x10000000) 对所有 DMA 不可达** | F407 总线拓扑；`sd_diskio.c` 的 `SD_DmaBufOk()` 已把它列为拒绝条件 | 任何 DMA 源/目标缓冲**必须**留在主 SRAM：UART 环、SDIO 数据、I2S 音频双缓冲、SPI DMA（含 WS2812 与 LCD flush） |
| SDIO IDMA 只按 32 位取指，丢地址低 2 位 | 已定案（`Docs/40-records/串口框架与Ymodem移植记录-20260922.md` §3） | 交给 SDIO 的缓冲需满足 `buf ≡ 文件位置 (mod 4)`；已由 L1 拒绝 + L2 中转兜住 |
| LVGL 绘制缓冲被 `disp_flush` 优先按 **DMA** 送出 | `Middleware/lvgl/examples/porting/lv_port_disp.c` flush 分支 | ⇒ **绘制缓冲不能整体挪进 CCM**，只能缩行数或改单缓冲；若将来确定走轮询路径才可挪 |

---

## 3. 主 SRAM1 解剖（99.9 % 是谁吃的）

| 分类 | 对象（实测字节） | 小计 | 性质 |
|---|---|---|---|
| **演示/Demo** | `app_lcd_demo` 28,800（`s_test_fb[120*120]` 渐变测试图）、`app_fatfs_demo` 10,997、`app_lvgl_images_demo` 4,740、`app_ymodem_demo` 1,768 | **46,305 B** | **上线时可全部裁掉** |
| 演示（在 SRAM2） | `app_lcd_touch_demo` 5,132、`app_flash_demo` 768、`app_spi_demo` 64、`app_multibutton_demo` 48 | **6,012 B** | 同上 |
| **LVGL 移植层** | `lv_port_disp` 38,401（两半绘制缓冲 240×40×2 各 19,200） | 38,401 B | 可缩不可挪（见 §2 第 3 条） |
| **驱动必需（DMA）** | `bsp_audio` 8,784、`bsp_uart` 8,240、SRAM2 的 `dev_ws2812` 2,288 | 19,312 B | 必须留主 SRAM |
| 运行时 | 栈 8,192（SRAM1）、堆 4,096（SRAM2） | 12,288 B | 堆已近乎无用（LittleFS 已走专用池），栈未测水位 |
| 存储/Shell 零散 | `bsp_lfs` 1,152、`fatfs` 1,125、`bsp_shell` 620、`lv_init` 500、`spi` 272、`usart` 260 … | ≈ 4.7 KB | 正常 |

**⇒ "BSP 把空间吃光了"不成立：演示合计 ≈ 52.3 KB，是主 SRAM 的最大单一负载来源。**

---

## 4. 四条回收路径（收益/风险/判据，均为"待用户决定"，本文件不主张立即做）

| # | 动作 | 收益 | 风险 | 判据 |
|---|---|---|---|---|
| ① | **Demo 加裁剪机制**（编译期 `#if` 或工程级 exclude；`app_main.c:138-164` 无条件调用 **19 个** `app_*_demo_init()`（共 20 处，仅 `app_uart_demo_init()` 被注释），**全仓不存在任何 DEMO 门控宏**） | **≈ 52 KB 主 SRAM** | 低；注意 `SHELL_EXPORT_CMD` 走 linker section，整文件裁掉才会同时消失 | `parse_map.ps1` 中所有 `app_*.o` 的 RW 归零；shell 命令表相应减少 |
| ② | `app_lcd_demo.c:21` 的 `s_test_fb[120*120]`（28,800 B）改为复用 LVGL 绘制缓冲或分块生成 | **28.8 KB** | 低（纯演示用途） | 同上，`app_lcd_demo.o` RW 从 28,800 → ~0 |
| ③ | 绘制缓冲 40 行 → 20 行，或双缓冲 → 单缓冲 | 19.2 KB | 中：牺牲刷屏并发度（LVGL 无法预渲染下一块） | `lv_port_disp.o` 38,401 → 19,201；`lvgl_images`/`lcd_gestures` 帧率不显著恶化 |
| ④ | 栈按实测水位下调（填充法）+ 堆 4 KB → 2 KB（LittleFS 已不吃 C 堆，仅剩 `ff_memalloc`/LVGL FS 路径） | 4–6 KB | **中高**：栈裁过头 = 偶发死机（本仓已有教训），必须先测峰值 | 高水位任务栈填充 0xA5 遍历，空闲率 > 30 % 才可降 |
| ⑤ | LVGL 池（CCM 24,576 B）按需降到 16 KB | CCM 余量 → 47 KB | 中：受 `LFS_POOL`/LVGL 分配水位影响，`lfs_pool` 与新加的 LVGL 水位可查 | LVGL `lv_mem_monitor` 空闲率仍 > 30 % |

> 长期习惯：**新增任何大块静态数据前先问"它进 DMA 吗？"**——不进就放 CCM（现余 38.5 KB）。这条已写入 `ARCHITECTURE.md` 第 10 节。

---

## 5. Flash / OTA 的前瞻约束（现在不痛，做 OTA 时会痛）

ROM 剩 156 KB 看着宽裕，但**双镜像 OTA 装不下**：当前单镜像 ≈ 356 KB，2 × 356 KB > 512 KB。
可行形态只有：**单镜像 + 下载暂存到 W25Q128（后 12 MB LittleFS 现为空）或 SD 卡，由 bootloader 回写主 Flash**，并预留 bootloader 16–32 KB。分区方案要在做 OTA 之前定，之后改要重刷全链路。

ROM 侧最大占用者（供裁剪参考）：`lfs.o` 23,003、`lv_font_montserrat_14` 13,644、`ff.o` 7,272、`app_fatfs_demo` 8,296、`app_shell_demo` 6,159、`shell.o` 6,272，其余为 LVGL 模块逐个 2–7 KB 累加。

---

## 6. 怎么复跑（本文件的数据都是这么来的）

```powershell
# 区域余量 + 每区 top 14 对象（默认读 MDK-ARM/SkyStar_BSP_HAL/SkyStar_BSP_HAL.map）
powershell -ExecutionPolicy Bypass -File Other\tools\MemoryMap\parse_map.ps1

# 指定条目数并存快照（趋势对比用，快照按日期入库）
powershell -ExecutionPolicy Bypass -File Other\tools\MemoryMap\parse_map.ps1 -Top 20 -OutJson Other\tools\MemoryMap\baseline-YYYYMMDD.json

# 只看某几个区域
powershell -ExecutionPolicy Bypass -File Other\tools\MemoryMap\parse_map.ps1 -Regions RW_IRAM1,RW_IRAM_CCM
```

已知事项：`.agents/skills/memory-analysis` 自带的解析器**不能解析 ARMCC V5 的 .map**（实测输出为无意义值），故本仓使用上面的 `parse_map.ps1`。

---

## 7. 纠错记录（防止错误结论复述）

| 曾出现的说法 | 核实结果 |
|---|---|
| "20 个 `app_*_demo` 统一运行期门控 `BSP_DEMO_ENABLE()`" | **不存在**。全仓 grep `BSP_DEMO_ENABLE` = 0 命中；`app_main.c:138-164` 无条件调用 19 个 demo init（共 20 处，仅 `app_uart_demo_init()` 被注释），**没有任何编译期或运行期裁剪机制**。该说法来自历史摘要的转述，未验证即引用——本次已按实测更正，并列入 §4 路径① |
| "LVGL 缓冲区已经在 64 KB CCM 里，所以不缺内存" | 半对：**LVGL 堆（24,576 B）确实在 CCM**，但**两半绘制缓冲（38,401 B）仍在主 SRAM**（且因 flush 优先走 DMA，不能简单挪动）。这是主 SRAM 见底的直接原因之一 |

---

## 8. 维护约定

1. 新增/放大任何静态缓冲后，跑一次 §6 命令，把 `FREE` 数字变化写进提交正文；
2. `FREE < 1 KB` 的区（现在是 SRAM1/SRAM2）视为**红色水位**，任何新增占用必须先给出回收方案；
3. 快照文件按日期存于 `Other/tools/MemoryMap/baseline-YYYYMMDD.json`，里程碑处对比；
4. 本文件与 `ARCHITECTURE.md` 第 9 节（已知问题）、第 10 节（维护约定）互为索引。
