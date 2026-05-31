#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
添加lwrb源文件到Keil工程
"""

import xml.etree.ElementTree as ET

def add_lwrb_to_project():
    project_path = r"d:\Keil5\STM32Projects\SkyStar\BSP\V2\SkyStar_BSP_HAL\MDK-ARM\SkyStar_BSP_HAL.uvprojx"
    
    ET.register_namespace('xsi', 'http://www.w3.org/2001/XMLSchema-instance')
    
    tree = ET.parse(project_path)
    root = tree.getroot()
    
    groups_elem = root.find(".//Groups")
    if groups_elem is None:
        print("✗ 未找到 Groups 元素")
        return
    
    middleware_group = None
    for group in groups_elem.findall("Group"):
        group_name = group.find("GroupName")
        if group_name is not None and group_name.text == "Middleware/LwRB":
            middleware_group = group
            break
    
    if middleware_group is None:
        middleware_group = ET.SubElement(groups_elem, "Group")
        name_elem = ET.SubElement(middleware_group, "GroupName")
        name_elem.text = "Middleware/LwRB"
        files_elem = ET.SubElement(middleware_group, "Files")
        print("✓ 已创建虚拟目录: Middleware/LwRB")
    else:
        files_elem = middleware_group.find("Files")
        if files_elem is None:
            files_elem = ET.SubElement(middleware_group, "Files")
    
    files_to_add = [
        {"name": "lwrb.c", "type": "1", "path": "../Middleware/lwrb/lwrb/lwrb.c"}
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
    print(f"\n✓ 工程文件已更新")

if __name__ == "__main__":
    add_lwrb_to_project()
