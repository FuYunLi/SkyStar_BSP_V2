# Git 规范

> 版本：1.1.0  
> 更新日期：2026-05-31  
> 参考：Conventional Commits、Git Flow

---

## 目录

1. [提交消息规范](#1-提交消息规范)
2. [分支命名规范](#2-分支命名规范)
3. [分支管理策略](#3-分支管理策略)
4. [提交前检查清单](#4-提交前检查清单)
5. [常见场景处理](#5-常见场景处理)
6. [规划迭代与偏差处理](#6-规划迭代与偏差处理)
7. [参考资源](#7-参考资源)

---

## 1. 提交消息规范

### 1.1 消息结构

```
<type>(<scope>): <subject>

[可选 body]

[可选 footer]
```

### 1.2 标题行（必填）

**格式**：`<type>(<scope>): <中文简述>`

**type 类型**：

| 类型 | 说明 | 示例 |
|------|------|------|
| `feat` | 新功能 | 新增驱动、接口、Demo |
| `fix` | Bug 修复 | 修复 DMA 竞态、参数校验遗漏 |
| `refactor` | 重构（不改变功能） | 代码结构优化、命名统一 |
| `docs` | 文档变更 | 更新 README、代码规范 |
| `test` | 测试相关 | 新增测试用例、测试框架 |
| `chore` | 构建/工具变更 | 更新 .clang-format、Makefile |
| `style` | 代码格式（不影响逻辑） | 格式化、缩进修正 |
| `perf` | 性能优化 | 算法优化、内存占用降低 |

**scope 范围**：

| 范围 | 说明 |
|------|------|
| `arch` | 架构、目录结构、规范 |
| `interface` | Interface 层（port_spi, port_i2c...） |
| `driver` | Driver 层（dev_st7789, dev_w25q...） |
| `board` | Board 层（bsp_lcd, bsp_key...） |
| `middleware` | 中间件（Letter Shell, EasyLogger...） |
| `demo` | Demo 示例 |
| `app` | 应用层任务 |

**subject 要求**：
- 使用中文简述
- 不超过 50 字符
- 不以句号结尾
- 使用祈使语气（"新增"而非"新增了"）

### 1.3 正文（可选）

**格式要求**：
- 与标题空一行
- 每行不超过 72 字符
- 说明 **做了什么**、**为什么做**、**影响范围**

**内容模板**：

```markdown
<type>(<scope>): <subject>

变更内容：
- 新增 xxx 功能
- 修正 xxx 问题
- 重构 xxx 模块

影响范围：
- 影响模块 A、模块 B
- 需要同步更新 xxx 配置

技术细节：
- 使用 xxx 方案解决 xxx 问题
- 参考 [链接/文档]
```

### 1.4 Footer（可选）

**用途**：
- **Breaking Changes**：不兼容变更说明
- **Issue 引用**：关联 Issue 编号
- **验收命令**：Shell 验收命令

**格式**：

```markdown
BREAKING CHANGE: <不兼容变更说明>

Closes #123
Refs #456

验收: shell> version
```

### 1.5 完整示例

**示例 1：新功能（简单）**

```
feat(interface): port_gpio - GPIO 逻辑映射层
```

**示例 2：新功能（带正文）**

```
feat(driver): dev_ws2812 - PWM+DMA RGB LED 驱动

变更内容：
- 新增 WS2812B RGB LED 驱动
- 支持 RGB565 颜色格式
- 实现彩虹渐变、呼吸灯效果

技术细节：
- 使用 TIM PWM+DMA 方案，CPU 占用率 < 1%
- 修正 V1 颜色宏复合字面量兼容问题

验收: shell> rgb_rainbow
```

**示例 3：Bug 修复**

```
fix(driver): dev_ws2812 - DMA 传输完成标志竞态

问题：
DMA 传输完成回调中直接修改状态标志，与主循环检测存在竞态，
导致偶发性颜色错乱。

修复：
- 使用 volatile 修饰状态标志
- 回调中通过队列通知主循环
- 增加传输超时保护

验收: shell> rgb_test 100  // 连续运行 100 次无异常
```

### 1.6 提交粒度建议

| 粒度 | 说明 | 示例 |
|------|------|------|
| **单功能单提交** | 一个提交只做一件事 | 新增驱动、修复 Bug 分开提交 |
| **接口→驱动→Demo 分离** | 同一总线分三个提交 | port_spi / dev_st7789 / demo_lcd |
| **可编译可验收** | 每个提交都能编译通过 | 禁止中间态提交 |

### 1.7 禁止事项

| 禁止 | 原因 |
|------|------|
| `update`、`fix bug` 等模糊描述 | 无法追溯变更内容 |
| 一个提交包含多个不相关功能 | 难以回滚和审查 |
| 提交无法编译的代码 | 破坏主分支可用性 |
| 提交敏感信息 | 安全风险 |

---

## 2. 分支命名规范

### 2.1 分支类型

| 分支类型 | 命名格式 | 示例 | 说明 |
|---------|---------|------|------|
| **主分支** | `master` / `main` | `master` | 生产环境，受保护 |
| **开发分支** | `develop` | `develop` | 集成分支，受保护 |
| **功能分支** | `feature/<name>` | `feature/M1-uart-interface` | 新功能开发 |
| **发布分支** | `release/<version>` | `release/v0.1` | 发布准备 |
| **热修分支** | `hotfix/<name>` | `hotfix/dma-race` | 紧急修复 |
| **备份分支** | `backup/<desc>-<date>` | `backup/30号-串口完成版-20250531` | 备份存档 |

### 2.2 功能分支命名

**格式**：`feature/<里程碑>-<功能描述>`

**示例**：

| 里程碑 | 功能 | 分支名 |
|--------|------|--------|
| M1 | UART接口层 | `feature/M1-uart-interface` |
| M2 | Letter Shell | `feature/M2-letter-shell` |
| M3 | EasyLogger | `feature/M3-easy-logger` |
| M4 | SPI接口层 | `feature/M4-spi-interface` |

### 2.3 热修分支命名

**格式**：`hotfix/<问题简述>`

**示例**：

| 问题 | 分支名 |
|------|--------|
| DMA竞态 | `hotfix/dma-race` |
| 内存泄漏 | `hotfix/memory-leak` |
| 参数校验缺失 | `hotfix/param-check` |

---

## 3. 分支管理策略

### 3.1 分支结构

```
master (受保护)
  │
  ├── develop (集成分支)
  │     │
  │     ├── feature/M1-uart-interface
  │     ├── feature/M2-letter-shell
  │     ├── feature/M3-easy-logger
  │     └── ...
  │
  ├── release/v0.1 (阶段零发布)
  ├── release/v0.2 (阶段一发布)
  └── ...
  │
  └── hotfix/紧急修复分支
```

### 3.2 分支保护规则

**master 分支（严格保护）**：

| 规则 | 说明 |
|------|------|
| 禁止直接推送 | 必须通过PR合并 |
| 禁止强制推送 | 保护历史记录 |
| 必须PR审查 | 至少1人审查 |
| 必须CI通过 | 编译测试通过 |

**develop 分支（适度保护）**：

| 规则 | 说明 |
|------|------|
| 禁止直接推送 | 必须通过PR合并 |
| 允许功能分支合并 | 功能开发完成后合并 |
| 可选PR审查 | 小型项目可省略 |

**feature 分支（无保护）**：

| 规则 | 说明 |
|------|------|
| 允许直接推送 | 开发者自由操作 |
| 允许强制推送 | 开发者自行决定 |
| 完成后可删除 | 合并后清理 |

### 3.2 工作流程

**功能开发流程**：

```
1. 从 develop 创建功能分支
   git checkout develop
   git pull origin develop
   git checkout -b feature/M1-uart-interface

2. 在功能分支开发
   git add .
   git commit -m "feat(interface): port_uart - UART接口层"
   git push origin feature/M1-uart-interface

3. 创建 PR 到 develop
   gh pr create --base develop --head feature/M1-uart-interface

4. 审查通过后合并
   gh pr merge

5. 删除功能分支
   git branch -d feature/M1-uart-interface
   git push origin --delete feature/M1-uart-interface
```

**发布流程**：

```
1. 从 develop 创建发布分支
   git checkout develop
   git checkout -b release/v0.1

2. 发布准备（版本号、文档等）
   git commit -m "chore: 准备 v0.1 发布"

3. 合并到 master 和 develop
   git checkout master
   git merge release/v0.1
   git checkout develop
   git merge release/v0.1

4. 打标签
   git tag -a v0.1 -m "Release v0.1"
   git push origin v0.1

5. 删除发布分支
   git branch -d release/v0.1
```

**热修流程**：

```
1. 从 master 创建热修分支
   git checkout master
   git checkout -b hotfix/dma-race

2. 修复问题
   git commit -m "fix(driver): DMA竞态修复"

3. 合并到 master 和 develop
   git checkout master
   git merge hotfix/dma-race
   git checkout develop
   git merge hotfix/dma-race

4. 删除热修分支
   git branch -d hotfix/dma-race
```

---

## 4. 提交前检查清单

### 4.1 代码检查

```
□ 代码符合 Docs/代码规范.md
□ 命名规范正确（snake_case / PascalCase）
□ 注释完整（Doxygen格式）
□ 无硬编码常量（使用宏定义）
□ 无编译警告
□ 无内存泄漏风险
```

### 4.2 功能检查

```
□ 功能完整实现
□ 编译通过
□ 基本测试通过
□ Demo可运行（如有）
```

### 4.3 提交检查

```
□ 提交消息符合规范
□ 一个提交只做一件事
□ 提交可编译
□ 提交可验收
```

### 4.4 分支检查

```
□ 在正确的分支上
□ 分支命名符合规范
□ 无冲突文件
□ 已拉取最新代码
```

---

## 5. 常见场景处理

### 5.1 提交错分支

**场景**：在master上误提交

**恢复**：

```bash
# 1. 撤销提交（保留修改）
git reset --soft HEAD~1

# 2. 创建正确分支
git checkout -b feature/xxx

# 3. 重新提交
git commit -m "feat: xxx"

# 4. 推送到远程
git push origin feature/xxx
```

### 5.2 需要修改最后一次提交

**场景**：提交消息写错或遗漏文件

**恢复**：

```bash
# 修改提交消息
git commit --amend -m "正确的提交消息"

# 添加遗漏文件
git add forgotten_file.c
git commit --amend --no-edit
```

### 5.3 需要撤销已推送的提交

**场景**：已推送到远程，但发现错误

**恢复**：

```bash
# 方案A：创建反向提交（推荐）
git revert <commit-hash>
git push origin <branch>

# 方案B：强制回退（仅限未公开项目）
git reset --hard <correct-commit>
git push origin <branch> --force
```

### 5.4 分支污染恢复

**场景**：分支被错误提交污染

**恢复**：

```bash
# 1. 创建备份分支
git branch backup-污染前-$(date +%Y%m%d)

# 2. 找到污染前的正确提交
git log --oneline -10

# 3. 重置到正确提交
git reset --hard <correct-commit>

# 4. 强制推送（如果已推送）
git push origin <branch> --force
```

---

## 6. 规划迭代与偏差处理

### 6.1 规划变更分类

| 变更类型 | 说明 | 版本号变化 | 示例 |
|----------|------|------------|------|
| **大重构** | 阶段重划分、架构变更 | 主版本+1 | 2.x → 3.0 |
| **阶段调整** | 新增/删除里程碑 | 次版本+1 | 2.0 → 2.1 |
| **小修补** | 错别字、补充说明 | 修订号+1 | 2.0.0 → 2.0.1 |

### 6.2 规划变更触发条件

| 条件 | 说明 | 处理方式 |
|------|------|----------|
| 其他阶段会遇到同样问题 | 设计决策变更 | 更新规划文档 |
| 技术选型与规划不同 | 方案变更 | 更新规划文档 |
| 规划遗漏依赖/接口 | 发现缺失 | 更新规划文档 |
| 实现细节与规划不同 | 临时调整 | 不改规划，Git提交记录 |
| 提交数量与预期不同 | 进度偏差 | 不改规划，阶段完成复盘 |

### 6.3 规划变更提交格式

**场景**：规划文档更新

```bash
docs(planning): 新增SPI接口层规划

变更内容：
- 新增 M5: SPI接口 + DMA
- 新增 M6: W25Q Flash驱动
- 新增 M7: ST7789 LCD驱动

影响：
- 阶段一提交数从4增加到7
- 版本号: 2.0 → 2.1

原因: 原规划遗漏SPI相关开发阶段
```

**场景**：技术选型变更

```bash
docs(planning): 日志系统从EasyLogger改为RTT

变更内容：
- 替换 EasyLogger → SEGGER RTT
- 更新开源库表格

原因:
- EasyLogger依赖printf，与Shell冲突
- RTT更轻量，支持多通道
- SEGGER官方维护，稳定性更好

版本号: 2.1 → 2.2
```

### 6.4 开发偏差处理

**偏差分类**：

| 偏差类型 | 说明 | 处理方式 | 示例 |
|----------|------|----------|------|
| **临时调整** | 实现细节不同，目标一致 | 不改规划 | 先阻塞模式，后续补DMA |
| **发现遗漏** | 规划未覆盖的场景 | 更新规划 | 新增port_critical |
| **技术变更** | 技术选型不同 | 更新规划 | EasyLogger → RTT |
| **进度偏差** | 提交数量不同 | 不改规划 | 4提交 → 7提交 |

**临时调整提交格式**：

```bash
feat(interface): port_uart - 阻塞模式实现

规划: DMA+环形缓冲区（M1）
实际: 阻塞模式实现

原因: DMA配置问题待调试，先用阻塞模式验收
后续: M1.1 将补充DMA+LwRB实现

验收: printf("Hello V2\r\n") 从串口输出
```

### 6.5 复盘记录格式

**阶段完成后复盘**：

```markdown
## 阶段一复盘（2026-06-XX）

### 规划与实际对比

| 项目 | 规划 | 实际 | 原因 |
|------|------|------|------|
| 提交数 | 4 | 7 | M1拆分+新增port_critical |
| 耗时 | 1周 | 2周 | Shell移植比预期复杂 |
| 技术选型 | EasyLogger | RTT | printf冲突问题 |

### 偏差原因分析

1. **粒度不够细**：M1应拆分为阻塞/DDMA两个里程碑
2. **遗漏依赖**：临界区保护未规划
3. **技术风险低估**：Shell移植复杂度

### 后续改进

- 规划粒度细化到单一技术点
- 开发前先做技术调研
- 每个里程碑预留20%缓冲时间
```

### 6.6 规划文档修订历史格式

**规划文档末尾**：

```markdown
## 修订历史

| 版本 | 日期 | 说明 |
|------|------|------|
| 2.2.0 | 2026-06-02 | 日志系统从EasyLogger改为RTT |
| 2.1.0 | 2026-05-31 | 补充LwRB环形缓冲区引入规划 |
| 2.0.0 | 2026-05-31 | 精简版，移除V1复盘与提交格式 |
```

---

## 7. 参考资源

| 资源 | 链接 |
|------|------|
| Conventional Commits | https://www.conventionalcommits.org/zh-hans/ |
| Git Flow | https://nvie.com/posts/a-successful-git-branching-model/ |
| GitHub Flow | https://docs.github.com/en/get-started/quickstart/github-flow |

---

## 8. 修订历史

| 版本 | 日期 | 说明 |
|------|------|------|
| 1.1.0 | 2026-05-31 | 新增规划迭代与偏差处理规范 |
| 1.0.0 | 2026-05-31 | 初始版本，从代码规范.md分离 |
