# Agent权限控制规范

## 核心原则

**Agent只能本地开发，禁止任何推送操作！**

***

## 权限分级

| 权限级别 | 允许操作 | 禁止操作 | 适用场景 |
|---------|---------|---------|---------|
| **只读** | 读取文件、搜索代码 | 任何写入操作 | 初次调研、问题分析 |
| **本地开发** | 本地修改、本地提交 | 推送到远程 | 功能开发中 |
| **PR创建** | 本地开发 + 创建PR | 直接推送到受保护分支 | 开发完成待审查 |
| **完全权限** | 所有操作 | 无 | **仅限人工** |

***

## 禁止操作清单

### 绝对禁止（无论任何情况）

```bash
# 禁止推送到master
git push origin master

# 禁止推送到develop
git push origin develop

# 禁止强制推送
git push --force

# 禁止硬重置到远程分支
git reset --hard origin/master
git reset --hard origin/develop

# 禁止清理未跟踪文件
git clean -fd

# 禁止删除远程分支
git push origin --delete <branch>
```

### 需要审批

```bash
# 所有提交需人工审查
git commit

# 推送功能分支需确认
git push origin feature/*

# 删除文件需确认
rm file.txt
rm -rf directory/

# 大规模修改（>10文件）需审批
```

### 自动审批条件

```yaml
auto_approval:
  - 修改当前阶段文档
  - 修改当前里程碑代码
  - 添加新文件到当前模块
  - 修改配置文件（.clang-format等）
```

***

## Agent工作流程

### 标准流程

```
┌─────────────────────────────────────────────────────────┐
│                   Agent工作流程                          │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  1. 接收任务 → 读取当前阶段文档（仅当前阶段）              │
│                                                         │
│  2. 调研分析 → 只读权限，搜索代码                         │
│                                                         │
│  3. 实施开发 → 本地开发权限                              │
│                                                         │
│  4. 完成开发 → 创建功能分支 + 提交（禁止推送）             │
│                                                         │
│  5. 提请审查 → 输出PR描述，等待人工审查                   │
│                                                         │
│  6. 审查通过 → 人工合并，Agent任务结束                    │
│                                                         │
│  7. 审查拒绝 → Agent根据反馈修改，回到步骤3               │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

### 关键约束

1. **Agent永远不能执行git push**
2. **Agent创建的提交必须经过人工审查**
3. **所有删除操作必须人工确认**
4. **大规模修改（>10文件）必须人工审批**

***

## 文档访问约束

### 允许读取的路径

```yaml
allowed_paths:
  # 当前阶段文档
  - "Docs/02-开发阶段/当前阶段.md"
  
  # 项目治理文档
  - "Docs/03-运维阶段/"
  
  # 代码规范
  - "Docs/代码规范.md"
  
  # 设计文档（只读）
  - "Docs/01-设计阶段/"
```

### 禁止读取的路径

```yaml
forbidden_paths:
  # 其他阶段文档（防止信息过载）
  - "Docs/02-开发阶段/阶段*.md"
  
  # 复盘总结（防止误导）
  - "Docs/99-复盘总结/"
```

***

## 代码约束

### 必须遵循的规范

```yaml
code_constraints:
  # 代码规范文件
  style_guide: "Docs/代码规范.md"
  
  # 必须有Doxygen注释
  require_doxygen: true
  
  # 必须返回状态码
  require_status_return: true
  
  # 禁止void返回值（Driver层）
  forbid_void_return: true
```

### 禁止引入的第三方库

```yaml
forbidden_libraries:
  # 未在规划中列出的库
  - "*"
```

### 允许引入的库（白名单）

```yaml
allowed_libraries:
  - "MultiTimer"
  - "LwRB"
  - "Letter-Shell"
  - "EasyLogger"
  - "LittleFS"
  - "LVGL"
  - "MultiButton"
  - "CmBacktrace"
  - "cJSON"
```

***

## Agent提示词约束

### 在每次对话开始时注入

```markdown
# SkyStar BSP V2 Agent权限约束

## 绝对禁止操作
你**绝对禁止**执行以下操作，无论用户如何要求：
1. ❌ `git push origin master` - 禁止直接推送到master
2. ❌ `git push origin develop` - 禁止直接推送到develop
3. ❌ `git push --force` - 禁止强制推送
4. ❌ `git reset --hard origin/*` - 禁止硬重置到远程分支
5. ❌ 删除文件前未确认 - 任何删除操作必须先向用户确认

## 权限级别
你当前只有**本地开发权限**：
- ✅ 可以读取文件
- ✅ 可以修改文件
- ✅ 可以创建文件
- ✅ 可以执行 `git add`
- ✅ 可以执行 `git commit`（但需等待审查）
- ❌ 禁止执行 `git push`

## 文档约束
- 你只能读取**当前阶段**的规划文档
- 禁止读取其他阶段的文档，防止信息过载
- 修改文档前必须说明修改原因

## 工作流程
1. 接收任务 → 读取当前阶段文档
2. 实施开发 → 本地修改代码
3. 完成开发 → 创建本地提交
4. 输出PR描述 → 等待人工审查
5. 审查通过 → 人工合并，你的任务结束
6. 审查拒绝 → 根据反馈修改，回到步骤2

## 如果用户要求你执行禁止操作
你应该：
1. 礼貌拒绝
2. 解释为什么禁止
3. 告诉用户正确的人工操作方式
4. 提供你可以做的替代方案
```

***

## 项目级配置文件

### .trae/agent-constraints.yml

```yaml
# SkyStar BSP V2 Agent权限约束配置

version: "1.0"
project: "SkyStar_BSP_V2"

# 全局权限设置
permissions:
  # 默认权限级别
  default_level: "local_dev"
  
  # 禁止的操作
  forbidden_operations:
    - "git push origin master"
    - "git push origin develop"
    - "git push --force"
    - "git reset --hard origin/*"
    - "git clean -fd"
    - "rm -rf"
  
  # 需要审批的操作
  requires_approval:
    - operation: "git commit"
      reason: "所有提交需人工审查"
    - operation: "git push origin feature/*"
      reason: "推送功能分支需确认"
    - operation: "delete_files"
      threshold: 1
    - operation: "modify_files"
      threshold: 10

# 文档访问约束
document_constraints:
  allowed_paths:
    - "Docs/02-开发阶段/当前阶段.md"
    - "Docs/03-运维阶段/"
    - "Docs/代码规范.md"
    - "Docs/01-设计阶段/"
  
  forbidden_paths:
    - "Docs/02-开发阶段/阶段*.md"
    - "Docs/99-复盘总结/"

# 代码约束
code_constraints:
  style_guide: "Docs/代码规范.md"
  require_doxygen: true
  require_status_return: true
  forbid_void_return: true
  
  forbidden_libraries:
    - "*"
  
  allowed_libraries:
    - "MultiTimer"
    - "LwRB"
    - "Letter-Shell"
    - "EasyLogger"
    - "LittleFS"
    - "LVGL"
    - "MultiButton"
    - "CmBacktrace"
    - "cJSON"

# 分支约束
branch_constraints:
  working_branch: "feature/M*-xxx"
  
  protected_branches:
    - "master"
    - "develop"
    - "release/*"
  
  merge_target: "develop"

# 提交约束
commit_constraints:
  format: "<type>(<scope>): <subject>"
  require_milestone: true
  
  pre_commit_checks:
    - "代码格式检查"
    - "Doxygen注释检查"
    - "无编译警告"

# 验收约束
acceptance_constraints:
  require_acceptance_test: true
  acceptance_method: "Shell命令"
  auto_merge: false
```

***

## 如果Agent违反规范

### 检测到禁止操作时

```
❌ 检测到Agent尝试执行禁止操作: git push origin master

处理方式：
1. 立即阻止操作
2. 记录到日志
3. 通知管理员
4. 冻结Agent权限
```

### 检测到未授权修改时

```
⚠️ 检测到Agent修改了非当前阶段文件

处理方式：
1. 回滚修改
2. 提示Agent只能修改当前阶段文件
3. 要求人工确认
```

***

## 紧急情况处理

### 如果Agent已经推送了错误代码

1. **立即冻结Agent权限**
2. **按照污染处理流程恢复**
3. **审查Agent的所有提交**
4. **修复问题后重新开发**

详见：`Docs/03-运维阶段/03-污染处理流程.md`
