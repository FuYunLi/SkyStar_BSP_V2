---
name: clangd_config
description: 当需要生成 compile_commands.json 或 .clangd 配置文件以支持 IDE 代码跳转和智能提示时使用。
---

# clangd 配置生成

## 适用场景

- 用户需要在非 Keil IDE（如 VSCode、CLion、Vim）中进行代码跳转
- 用户需要配置 clangd LSP 进行智能提示
- 用户提到 "compile_commands.json" 或 ".clangd" 或 "代码跳转配置"
- 工程使用 Keil MDK 构建，但需要在其他编辑器中开发

## 必要输入

- Keil 工程文件路径（.uvprojx 或 .uvproj）
- 目标名称（可选，默认使用第一个目标）

## 执行步骤

1. 确认工程文件路径，若用户未指定则在工作区中搜索 .uvprojx 文件
2. 执行脚本生成配置文件：

```bash
# 生成所有配置文件
python scripts/clangd_config.py --project <工程文件> --target <目标名> --export-all

# 仅生成 compile_commands.json
python scripts/clangd_config.py --project <工程文件> --target <目标名> --export-compile-commands

# 仅生成 .clangd
python scripts/clangd_config.py --project <工程文件> --target <目标名> --export-clangd
```

3. 确认输出文件位置（默认在工程根目录）

## 输出约定

脚本执行完成后，报告以下信息：

```
✅ 生成 compile_commands.json: /path/to/compile_commands.json
✅ 生成 .clangd: /path/to/.clangd

📁 输出目录: /path/to/project/root
```

## 配置文件说明

### compile_commands.json

JSON Compilation Database 格式，包含每个源文件的编译命令：
- 编译器路径
- 包含路径（-I）
- 宏定义（-D）
- 源文件路径

被 clangd、VSCode C++ 扩展、CLion 等工具广泛支持。

### .clangd

clangd LSP 的配置文件（YAML 格式）：
- CompileFlags：指定编译选项
- Diagnostics：诊断配置（禁用不适用于嵌入式开发的警告）

## 注意事项

- 配置文件生成在工程根目录（.uvprojx 所在目录的上级目录）
- 如果工程包含路径有变化，需要重新生成配置文件
- 脚本会自动将相对路径转换为绝对路径

## 与其他 Skill 的关系

- 独立使用，不依赖其他 skill
- 可在 build-keil 编译后使用，也可单独使用
- 生成的配置文件供 IDE/编辑器使用，不影响 Keil 编译
