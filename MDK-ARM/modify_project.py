#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Keil 工程文件路径管理脚本
功能：添加包含路径和源文件到虚拟目录
"""

import xml.etree.ElementTree as ET
import os

def modify_keil_project():
    project_path = r"d:\Keil5\STM32Projects\SkyStar\BSP\V2\SkyStar_BSP_HAL\MDK-ARM\SkyStar_BSP_HAL.uvprojx"
    
    ET.register_namespace('xsi', 'http://www.w3.org/2001/XMLSchema-instance')
    
    tree = ET.parse(project_path)
    root = tree.getroot()
    
    include_elem = root.find(".//Cads/VariousControls/IncludePath")
    if include_elem is not None:
        current_paths = include_elem.text or ""
        new_path = "../Middleware/LwRB/include"
        
        if new_path not in current_paths:
            if current_paths:
                include_elem.text = f"{current_paths};{new_path}"
            else:
                include_elem.text = new_path
            print(f"✓ 已添加包含路径: {new_path}")
        else:
            print(f"○ 包含路径已存在: {new_path}")
    
    groups_elem = root.find(".//Groups")
    if groups_elem is None:
        print("✗ 未找到 Groups 元素")
        return
    
    bsp_interface_group = None
    for group in groups_elem.findall("Group"):
        group_name = group.find("GroupName")
        if group_name is not None and group_name.text == "BSP/Interface":
            bsp_interface_group = group
            break
    
    if bsp_interface_group is None:
        bsp_interface_group = ET.SubElement(groups_elem, "Group")
        name_elem = ET.SubElement(bsp_interface_group, "GroupName")
        name_elem.text = "BSP/Interface"
        files_elem = ET.SubElement(bsp_interface_group, "Files")
        print("✓ 已创建虚拟目录: BSP/Interface")
    else:
        files_elem = bsp_interface_group.find("Files")
        if files_elem is None:
            files_elem = ET.SubElement(bsp_interface_group, "Files")
    
    files_to_add = [
        {"name": "port_uart.c", "type": "1", "path": "../BSP/Interface/port_uart.c"},
        {"name": "port_uart.h", "type": "5", "path": "../BSP/Interface/port_uart.h"}
    ]
    
    existing_files = set()
    for file_elem in files_elem.findall("File"):
        file_name = file_elem.find("FileName")
        if file_name is not None:
            existing_files.add(file_name.text)
    
    for file_info in files_to_add:
        if file_info["name"] not in existing_files:
            new_file = ET.SubElement(files_elem, "File")
            ET.SubElement(new_file, "FileName").text = file_info["name"]
            ET.SubElement(new_file, "FileType").text = file_info["type"]
            ET.SubElement(new_file, "FilePath").text = file_info["path"]
            print(f"✓ 已添加文件: {file_info['name']}")
        else:
            print(f"○ 文件已存在: {file_info['name']}")
    
    tree.write(project_path, encoding="UTF-8", xml_declaration=True)
    print(f"\n✓ 工程文件已更新: {project_path}")

if __name__ == "__main__":
    modify_keil_project()
