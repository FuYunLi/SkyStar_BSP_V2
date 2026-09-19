---
description: 分析 map 文件的 Flash/RAM 占用与增长点
---

分析本工程最新的 .map 文件(在 MDK-ARM 输出目录下查找,若无则先用 /build 编译生成),给出:

1. 总占用:Flash(ROM)与 RAM 的总大小、上限(STM32F407VET6: 512K Flash / 128K RAM + 64K CCM)与剩余余量;
2. 占用 Top10:按 section 大小排序最大的 10 个函数/对象,标注所属模块(APP/Middleware/BSP);
3. 增长分析:若 git 仓库中存在旧版 map 或上次分析记录,对比并列出本次增长最大的符号;没有旧版则跳过本项;
4. 结论:一句话评估当前余量是否健康,有无明显可优化项(如未用到的中间件段)。

只分析,不修改任何代码。
