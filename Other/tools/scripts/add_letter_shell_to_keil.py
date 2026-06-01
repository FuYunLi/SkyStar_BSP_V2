import os
import xml.etree.ElementTree as ET

def main():
    project_dir = r"d:\Keil5\STM32Projects\SkyStar\BSP\V2\SkyStar_BSP_HAL\MDK-ARM"
    project_file = os.path.join(project_dir, "SkyStar_BSP_HAL.uvprojx")
    backup_file = project_file + ".bak"
    
    # Register namespaces to prevent ns0 prefixes
    ET.register_namespace('xsi', 'http://www.w3.org/2001/XMLSchema-instance')
    
    # Parse the XML file
    tree = ET.parse(project_file)
    root = tree.getroot()
    
    # Manually ensure the xmlns:xsi attribute is preserved
    root.set('xmlns:xsi', 'http://www.w3.org/2001/XMLSchema-instance')
    
    # 1. Add include path to all Targets
    new_inc_path = "../Middleware/letter-shell-shell3.1/src"
    for inc_elem in root.findall('.//VariousControls/IncludePath'):
        existing = inc_elem.text or ""
        if existing:
            paths = [p.strip() for p in existing.split(';')]
            if new_inc_path not in paths:
                inc_elem.text = existing.rstrip(';') + ';' + new_inc_path
                print(f"Appended include path: {new_inc_path}")
        else:
            inc_elem.text = new_inc_path
            print(f"Set empty include path to: {new_inc_path}")
            
    # 2. Find or create the Groups element
    groups_elem = root.find('.//Groups')
    if groups_elem is None:
        print("Error: <Groups> tag not found in project file.")
        return
        
    group_name = "Middleware/LetterShell"
    target_group = None
    for group in groups_elem.findall('Group'):
        name_elem = group.find('GroupName')
        if name_elem is not None and name_elem.text == group_name:
            target_group = group
            break
            
    if target_group is None:
        print(f"Creating new group: {group_name}")
        target_group = ET.SubElement(groups_elem, "Group")
        name_elem = ET.SubElement(target_group, "GroupName")
        name_elem.text = group_name
        files_elem = ET.SubElement(target_group, "Files")
    else:
        print(f"Group {group_name} already exists.")
        files_elem = target_group.find('Files')
        if files_elem is None:
            files_elem = ET.SubElement(target_group, "Files")
            
    # 3. Define files to add (4 .c and 3 .h)
    files_to_add = [
        {"name": "shell.c", "type": "1", "path": "../Middleware/letter-shell-shell3.1/src/shell.c"},
        {"name": "shell_cmd_list.c", "type": "1", "path": "../Middleware/letter-shell-shell3.1/src/shell_cmd_list.c"},
        {"name": "shell_companion.c", "type": "1", "path": "../Middleware/letter-shell-shell3.1/src/shell_companion.c"},
        {"name": "shell_ext.c", "type": "1", "path": "../Middleware/letter-shell-shell3.1/src/shell_ext.c"},
        {"name": "shell.h", "type": "5", "path": "../Middleware/letter-shell-shell3.1/src/shell.h"},
        {"name": "shell_cfg.h", "type": "5", "path": "../Middleware/letter-shell-shell3.1/src/shell_cfg.h"},
        {"name": "shell_ext.h", "type": "5", "path": "../Middleware/letter-shell-shell3.1/src/shell_ext.h"},
    ]
    
    # Add files if not already present
    existing_filepaths = []
    for file_elem in files_elem.findall('File'):
        path_elem = file_elem.find('FilePath')
        if path_elem is not None and path_elem.text:
            existing_filepaths.append(path_elem.text.replace('\\', '/').strip())
            
    for f in files_to_add:
        normalized_path = f["path"].replace('\\', '/').strip()
        if normalized_path not in existing_filepaths:
            print(f"Adding file to group: {f['name']} ({normalized_path})")
            file_elem = ET.SubElement(files_elem, "File")
            
            name_elem = ET.SubElement(file_elem, "FileName")
            name_elem.text = f["name"]
            
            type_elem = ET.SubElement(file_elem, "FileType")
            type_elem.text = f["type"]
            
            path_elem = ET.SubElement(file_elem, "FilePath")
            path_elem.text = f["path"]
        else:
            print(f"File {f['name']} already exists in group.")
            
    # 4. Save back the updated project file
    try:
        ET.indent(tree, space="  ", level=0)
    except AttributeError:
        pass
        
    # Write in-memory
    import io
    out = io.BytesIO()
    tree.write(out, encoding='utf-8', xml_declaration=True)
    xml_str = out.getvalue().decode('utf-8')
    
    # Post-process to conform to exact Keil XML header
    xml_str = xml_str.replace("<?xml version='1.0' encoding='utf-8'?>", '<?xml version="1.0" encoding="UTF-8"?>')
    
    # Write back to file
    with open(project_file, 'w', encoding='utf-8', newline='\r\n') as f:
        f.write(xml_str)
        
    print("Project file updated successfully.")

if __name__ == "__main__":
    main()
