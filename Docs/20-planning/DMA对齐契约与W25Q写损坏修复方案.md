# DMA 对齐契约全局化 + W25Q 写损坏修复方案

> 版本:1.1.0
> 日期:2026-09-22（实施回填 2026-09-23）
> 上游依据:`Docs/40-records/串口框架与Ymodem移植记录-20260922.md`(Ymodem 损坏定案)、
> `Docs/40-records/阶段八音频调试记录-20260920.md`(2.7 节)、ARCHITECTURE.md 第 9 节
> 分支建议:`feature/dma-alignment-w25q`(自 zcode_bsp 创建,前置:hotfix/flash-bus-mutex 合入)

---

## 0. 实施结果与前提更正（2026-09-23，必读）

本方案已按 `feature/dma-alignment-contract` 实施完毕，实测见
`Docs/40-records/DMA对齐契约全局化-20260923.md`。三条与原计划不同的事实：

1. **本文的“W25Q 写损坏”前提已被推翻**（§1-2、§3、§5 的 H1-H4 全部作废）：裸 `dev_w25q_read` 77 次重复读 0 差异，
   真因是早期被 CAN 中止的传输留下的残留坏文件（数据块已释放并被其它文件复用）。因此本批次只交付了“对齐契约”部分；
   中止残留问题已由“临时名 + 校验后 rename 提交 + 失败丢弃”并在两后端验证（见 ARCHITECTURE 第 9 节已勾项）。
2. **L1 落点不是 `port_sdio`**：该层只有 init/在位/容量/错误码接口，SD 块读写实际在
   `FATFS/Target/sd_diskio.c` → `BSP_SD_*Blocks_DMA`，用户指针在那里才变成硬件地址；守卫已落在 disk 层。
3. **不变式不是“缓冲 4 字节对齐”**，而是 `buf ≡ 文件位置 (mod 4)`：FatFS 会把用户指针推进到扇区边界。
   按原计划只做“buf 非对齐才中转”会当场打断 WAV 播放（data 起始 78 字节 → 交出 `buf+434`）。

另新增一项未决缺陷（不在原方案内）：指针合法的 raw 路径仍会间歇复现 36 字节岛，
且连续单扇区写会确定性错 2 字节——已入 ARCHITECTURE 第 9 节独立待办。

---

## 1. 目标与范围

消灭"非对齐 DMA 访问导致数据静默位移"这一类缺陷:

1. **全局契约**:任何经 DMA 的存储/串行访问,缓冲区 4 字节对齐由架构保证,不再依赖调用方自觉;
2. **W25Q 定案**:查明 LittleFS 写路径 43 块中 blk7(0x7000)/blk9(0x9000) 稳定损坏的确切根因并修复,`fatfs_test crcmap` 全绿;
3. 不改变既有 API 签名语义(对上层零侵入或显式报错,不静默)。

## 2. 已知事实(定案与取证)

| 事实 | 来源 | 推论 |
|---|---|---|
| Ymodem 损坏根因:载荷指针非 4 字节对齐,SDIO IDMA 丢弃地址低 2 位,整块位移且 CRC 全程无错 | 串口框架记录(已定案) | 损坏是**确定性的地址截断**,不是随机丢字节 |
| W25Q:43 块中 blk7/blk9 稳定损坏,两趟读一致 | `fatfs_test crcmap` 实测 | 同为确定性损坏;SD 侧同文件正常 → 范围锁定 W25Q 写/擦路径 |
| `dev_w25q` 全部走 `port_spi_write/read` 阻塞轮询(HAL_SPI_Transmit/Receive) | 代码核实 | **轮询模式下不存在 IDMA 对齐问题**,W25Q 损坏另有根因(见 §3 假设矩阵) |
| 取证工具已就位:`fatfs_test crc/crcmap/dump`(VFS 通用) | ymodem 修复分支合入 | 无需新写工具 |

## 3. W25Q 损坏:假设矩阵与取证步骤(开工第一步,先诊断后动手)

blk7=0x7000、blk9=0x9000 均为 4KB 对齐地址,损坏 2/43 且稳定。按优先级验证:

| # | 假设 | 验证方法 | 判据 |
|---|---|---|---|
| H1 | **页编程边界**:lfs prog 尺寸/偏移跨 256B 页,`dev_w25q_write` 跨页循环有缺陷 | 用 `dump` 提取 blk7 损坏区字节,与 PC 端比对**位移方向和周期**:若按 256B 周期错位 → 页边界;若整块位移 4 字节 → 对齐类;若位翻转 → 信号 | 位移模式 |
| H2 | **擦除-编程竞态**:erase 后未等 WIP 清零就 prog(或反之),W25Q 内部拒绝编程 | 直接调 `dev_w25q_write` 写已知图案到 0x7000 → 读回;不经过 lfs。图案完好的话走 H3 | 分层定位:lfs 层 vs 驱动层 |
| H3 | **LittleFS 元数据与数据交错**:同扇区先 prog 数据后擦除(地址换算 `LFS_START_BLOCK_OFFSET` 冲突) | 对比 blk7/blk9 与 lfs 分配的元数据块位置 | 检查 lfs 超级块/元数据块号 |
| H4 | SPI 信号完整性(仅大块连续写时) | 降 `port_spi` 速率重跑 crcmap,若全绿 → 信号余量问题 | 速率相关性 |

**产出**:取证结论写入 `Docs/40-records/`,按命中的假设落 §5 对应修复;若 H2/H3 命中,修复点在 `dev_w25q`/`bsp_lfs`;H4 命中则降速或加 FIFO。

## 4. DMA 对齐契约:三级防线设计

### 4.1 L1——port 层显式守卫(强制,先行合入)

**原则:非对齐 + DMA 的组合不允许静默发生。**

- `port_sdio`:DMA 直写路径(FatFS 底层 `sd_diskio` 的 read/write)入口校验缓冲对齐与长度;
- `port_spi` 异步 DMA 路径(`port_spi_read_dma/write_dma`):入口同校验;阻塞轮询路径不受限;
- 校验失败行为:`BSP_ASSERT`(debug 构建断言)+ 返回 `BSP_EINVAL`(release 构建显式失败)——**报错优于中转**,让调用方在开发期暴露;
- 契约写入 `bsp_interface.h` 头注释与 ARCHITECTURE 第 4 节:"凡传入 port 层 DMA 路径的缓冲,调用方保证 4 字节对齐;否则返回 EINVAL"。

### 4.2 L2——bsp_file 兜底中转(便捷,第二提交)

`bsp_file_read/bsp_file_write` 入口:若后端为 FatFS 且(指针非 4 字节对齐 **或** 长度非 4 的倍数),走内部静态对齐中转缓冲(4KB,文件级静态,禁止上栈)分块搬运。理由:

- bsp_file 是 VFS 唯一入口,拦住 100% 的上层调用(Ymodem 已点的规避可顺势迁移至此,删 app 层特判);
- LittleFS 后端经 lfs 内部缓冲,天然对齐,无需中转;
- 代价:仅非对齐调用方承担一次拷贝,常规路径零开销。

### 4.3 L3——契约文档化

- ARCHITECTURE.md 第 4 节新增契约第 4 条:DMA 缓冲 4 字节对齐;
- 第 9 节勾掉"对齐契约点状规避"项;Ymodem 记录补"已升格全局"批注。

## 5. 修复点映射(按 §3 取证结果执行)

| 命中假设 | 修复位置 | 修复动作 |
|---|---|---|
| H1 页边界 | `dev_w25q.c` `w25q_page_program` | 跨页循环逐页对齐审查;prog 前强制 `w25q_wait_busy()` |
| H2 擦写竞态 | `dev_w25q.c` | erase 后 wait_busy 提前到写使能前;写使能失败路径回滚 |
| H3 lfs 地址换算 | `bsp_lfs.c` | 修正 `LFS_START_BLOCK_OFFSET` 与元数据块分配的冲突 |
| H4 信号 | `port_spi.c` | SPI 分频降档或启用 FIFO;记录到板级注意事项 |

## 6. 提交拆分(单功能单提交,均可编译)

```
1  docs(records): W25Q 损坏取证结论(§3 产出,先发)
2  fix(interface): port_sdio/port_spi DMA 路径对齐守卫(L1)
3  fix(board): bsp_file 非对齐中转兜底(L2)+ Ymodem 迁移
4  fix(driver|board): W25Q/LittleFS 按取证结论修复(§5 对应项)
5  docs(map): 契约第 4 条 + 第 9 节勾项
```

## 7. 验收清单

- [x] 两后端整文件 CRC 与 PC 基准一致且可重复：`0:/tour.wav` = `flash/Tour_France.wav` = `7A6FD6F4`，`short=0`（连读两次一致）
- [x] `crcmap 0:/tour.wav 43 块双遍：`read-unstable=0`、`pc-diverged=0`，内容与 PC 完全一致
- [x] `ymodem_recv` 传输 tour.wav 板端 CRC 与 PC 一致（删除 app 层点状缓冲后改由 L2 接管，仍字节级正确）
- [x] `play_wav` 播放回归：`Playing … → Playback finished`（980ms），且 `Current owner: NONE`（总线正常交还）；
      该文件 data 起始 78 字节，是 L2 新不变式的直接受益者
- [x] 音频互斥回归：`audio_bus_switch i2s` 后 `flash_id`/`lfs_boot_count`/`crc flash/…` 均快速失败
      `ret=-5 (BSP_BUSY)` 不卡死；`audio_bus_switch spi` 后完全恢复
- [x] 非对齐负向测试：`fatfs_test align` 双向矩阵 9 轮全 PASS——raw 非对齐被 L1 拒（`rejects=+2`），
      经 VFS 的非对齐由 L2 中转后逐字节正确（`staged=+3`）
- [ ] 待验：指针合法 raw 路径的 36 字节岛与单扇区错 2 字节（原计划未列，见第 0 节与 ARCHITECTURE 第 9 节）
- [x] 全程 Shell 交互正常

## 8. 风险与对策

1. **中转缓冲 RAM 占用**:4KB 静态缓冲,固件 RAM 余量充足(RAM ≈ 138.5KB/192KB),编译后核对 map;
2. **L1 拒绝策略的破坏性**:若既有代码存在隐性非对齐调用,L1 会让其显式报错——这正是目的,但合入前先全仓 grep DMA 路径调用点逐一核对;
3. **取证不充分风险**:若 H1-H4 全部排除,扩充假设(H5:W25Q 电源/时钟边际)再议,禁止无根因盲改。
