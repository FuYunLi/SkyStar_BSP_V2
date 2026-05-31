---
name: keil_path
description: 当需要在 Keil 工程中添加源文件、包含路径或管理虚拟目录结构时使用。由 Agent 直接分析代码依赖并精确修改工程文件。
---

# Keil 工程路径管理

## 适用场景

- 用户新增了 BSP 驱动文件，需要添加到 Keil 工程
- 用户需要添加包含路径（Include Path）
- 用户需要管理 Keil 工程的虚拟目录（Groups）结构
- 用户提到 "添加到工程"、"包含路径"、"虚拟目录"、"Groups"

## 核心原则

**此 Skill 由 Agent 直接处理，不使用脚本。**

原因：
1. 需要理解代码上下文，判断哪些路径是必需的
2. 需要创建合理的虚拟目录结构
3. 需要交互确认用户意图
4. 脚本无法精确判断依赖关系

## 执行步骤

### 1. 分析代码依赖

当用户新增源文件时：

```
1. 读取新增的源文件
2. 分析 #include 语句，识别依赖的头文件
3. 过滤掉系统头文件（stdint.h, stdbool.h 等）
4. 记录用户头文件的文件名
```

### 2. 定位头文件位置

```
1. 在工作区中搜索这些头文件
2. 记录头文件所在的目录
3. 如果同名头文件存在多个位置，询问用户选择
```

### 3. 检查现有包含路径

```
1. 读取 .uvprojx 工程文件
2. 解析现有的 IncludePath 元素
3. 对比需要添加的目录是否已存在
```

### 4. 修改工程文件

#### 添加包含路径

工程文件中的包含路径结构：

```xml
<Cads>
  <VariousControls>
    <IncludePath>../Core/Inc;../Drivers/STM32F1xx_HAL_Driver/Inc</IncludePath>
  </VariousControls>
</Cads>
```

操作步骤：
1. 读取现有 IncludePath 文本
2. 追加新路径（用分号分隔）
3. 写回工程文件

#### 添加源文件到虚拟目录

虚拟目录结构：

```xml
<Groups>
  <Group>
    <GroupName>Application/User/Core</GroupName>
    <Files>
      <File>
        <FileName>main.c</FileName>
        <FileType>1</FileType>
        <FilePath>../Core/Src/main.c</FilePath>
      </File>
    </Files>
  </Group>
</Groups>
```

操作步骤：
1. 确定目标虚拟目录名（如 `BSP/Interface`）
2. 查找或创建对应的 Group 元素
3. 添加 File 子元素

#### 文件类型代码

| 类型 | 代码 |
|------|------|
| .c | 1 |
| .h | 5 |
| .s (汇编) | 2 |
| .lib | 4 |
| .o | 3 |

### 5. 备份原文件

修改前必须备份：

```
原文件: SkyStar_BSP_HAL.uvprojx
备份:   SkyStar_BSP_HAL.uvprojx.bak
```

## XML 操作示例

### 添加包含路径

```python
# 读取工程文件
tree = ET.parse(project_path)
root = tree.getroot()

# 找到 IncludePath 元素
include_elem = root.find(".//Cads/VariousControls/IncludePath")

# 追加新路径
existing = include_elem.text or ""
new_paths = "../BSP/Inc;../BSP/Interface"
include_elem.text = f"{existing};{new_paths}" if existing else new_paths

# 保存
tree.write(project_path, encoding="UTF-8", xml_declaration=True)
```

### 创建虚拟目录并添加文件

```python
# 找到 Groups 元素
groups_elem = root.find(".//Groups")

# 创建新 Group
new_group = ET.SubElement(groups_elem, "Group")
name_elem = ET.SubElement(new_group, "GroupName")
name_elem.text = "BSP/Interface"

# 创建 Files 元素
files_elem = ET.SubElement(new_group, "Files")

# 添加文件
file_elem = ET.SubElement(files_elem, "File")
ET.SubElement(file_elem, "FileName").text = "port_gpio.c"
ET.SubElement(file_elem, "FileType").text = "1"
ET.SubElement(file_elem, "FilePath").text = "../BSP/Interface/port_gpio.c"
```

## 虚拟目录命名规范

建议的目录结构：

```
Application/
  └── User/
      └── Core/          # main.c, stm32f1xx_it.c 等

Drivers/
  └── STM32F1xx_HAL_Driver/  # HAL 库文件

BSP/
  ├── Inc/              # BSP 头文件
  ├── Interface/        # 接口层实现
  └── Device/           # 设备驱动
```

## 注意事项

1. **路径格式**：Keil 使用相对路径，相对于 .uvprojx 文件所在目录
2. **路径分隔符**：使用 `/` 或 `\\`，不要用单个 `\`
3. **编码**：工程文件使用 UTF-8 编码
4. **备份**：修改前必须备份原文件
5. **验证**：修改后建议在 Keil 中打开验证

## 强制规范

**以下规则必须遵守，不可跳过**：

### 1. 头文件必须纳入虚拟目录

添加 .c 文件时，**必须同时添加对应的 .h 文件**：

```
示例：添加 bsp_board.c 时，必须同时添加：
- bsp_board.c (FileType=1)
- bsp_board.h (FileType=5)
```

**原因**：
- 方便在 Keil IDE 中查看和编辑头文件
- 保持工程文件完整性
- 符合 Keil 工程管理最佳实践

### 2. 虚拟目录结构必须与物理目录一致

| 物理目录 | 虚拟目录名 | 包含文件 |
|---------|-----------|---------|
| BSP/Board/ | BSP/Board | bsp_board.c, bsp_board.h, bsp_config.h |
| BSP/Interface/ | BSP/Interface | port_tick.c, port_tick.h, bsp_interface.h |
| BSP/Driver/ | BSP/Driver | dev_xxx.c, dev_xxx.h |
| App/ | App | app_main.c, app_main.h |
| Middleware/xxx/ | Middleware/xxx | xxx.c, xxx.h |

### 3. 文件类型代码必须正确

| 类型 | FileType | 说明 |
|------|----------|------|
| .c | 1 | C源文件 |
| .h | 5 | 头文件 |
| .s | 2 | 汇编文件 |
| .lib | 4 | 库文件 |

---

## 交互确认

**仅以下特殊情况需要询问用户**：

1. 同名头文件存在多个位置（需用户选择）
2. 虚拟目录结构不明确（需用户确认）

**注意**："是否添加头文件"不是交互项，头文件必须添加。

## 与其他 Skill 的关系

- 独立使用
- 可在添加新驱动文件后调用
- 修改后可用 clangd_config 重新生成 IDE 配置
