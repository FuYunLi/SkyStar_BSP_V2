import os
import shutil
import xml.etree.ElementTree as ET

def main():
    project_dir = r"d:\Keil5\STM32Projects\SkyStar\BSP\V2\SkyStar_BSP_HAL\MDK-ARM"
    project_file = os.path.join(project_dir, "SkyStar_BSP_HAL.uvprojx")
    backup_file = project_file + ".bak"
    
    # 1. Restore from backup to start clean
    if os.path.exists(backup_file):
        print(f"Restoring clean project file from backup...")
        shutil.copyfile(backup_file, project_file)
    else:
        print(f"Backing up project file to {backup_file}...")
        shutil.copyfile(project_file, backup_file)
    
    # Register namespaces to prevent ns0 prefixes
    ET.register_namespace('xsi', 'http://www.w3.org/2001/XMLSchema-instance')
    
    # 2. Parse the XML file
    tree = ET.parse(project_file)
    root = tree.getroot()
    
    # Manually ensure the xmlns:xsi attribute is preserved
    root.set('xmlns:xsi', 'http://www.w3.org/2001/XMLSchema-instance')
    
    # 3. Add include paths to all Targets
    new_inc_path = "../Middleware/LwRB/include"
    for inc_elem in root.findall('.//VariousControls/IncludePath'):
        existing = inc_elem.text or ""
        if existing:
            paths = [p.strip() for p in existing.split(';')]
            if new_inc_path not in paths:
                inc_elem.text = existing.rstrip(';') + ';' + new_inc_path
                print(f"Appended include path. New value: {inc_elem.text}")
        else:
            inc_elem.text = new_inc_path
            print(f"Set empty include path to: {inc_elem.text}")
            
    # 4. Add Define macro to all Targets
    new_define = "LWRB_DISABLE_ATOMIC"
    for def_elem in root.findall('.//VariousControls/Define'):
        existing = def_elem.text or ""
        if existing:
            defines = [d.strip() for d in existing.split(',')]
            if new_define not in defines:
                def_elem.text = existing.rstrip(',') + ',' + new_define
                print(f"Appended define macro. New value: {def_elem.text}")
        else:
            def_elem.text = new_define
            print(f"Set empty define to: {def_elem.text}")
            
    # 5. Find the Groups element
    groups_elem = root.find('.//Groups')
    if groups_elem is None:
        print("Error: <Groups> tag not found in project file.")
        return
        
    # Check if Middleware/LwRB group already exists
    group_name = "Middleware/LwRB"
    lwrb_group = None
    for group in groups_elem.findall('Group'):
        name_elem = group.find('GroupName')
        if name_elem is not None and name_elem.text == group_name:
            lwrb_group = group
            break
            
    if lwrb_group is None:
        print(f"Creating new group: {group_name}")
        lwrb_group = ET.SubElement(groups_elem, "Group")
        name_elem = ET.SubElement(lwrb_group, "GroupName")
        name_elem.text = group_name
        files_elem = ET.SubElement(lwrb_group, "Files")
    else:
        print(f"Group {group_name} already exists.")
        files_elem = lwrb_group.find('Files')
        if files_elem is None:
            files_elem = ET.SubElement(lwrb_group, "Files")
            
    # 6. Define files to add
    files_to_add = [
        {"name": "lwrb.c", "type": "1", "path": "../Middleware/LwRB/lwrb/lwrb.c"},
        {"name": "lwrb_ex.c", "type": "1", "path": "../Middleware/LwRB/lwrb/lwrb_ex.c"},
        {"name": "lwrb.h", "type": "5", "path": "../Middleware/LwRB/include/lwrb/lwrb.h"},
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
            
    # 7. Save back the updated project file
    try:
        ET.indent(tree, space="  ", level=0)
    except AttributeError:
        pass
        
    # Write in-memory
    import io
    out = io.BytesIO()
    tree.write(out, encoding='utf-8', xml_declaration=True)
    xml_str = out.getvalue().decode('utf-8')
    
    # 8. Post-process to conform to exact Keil XML header
    # XML declaration replacement
    xml_str = xml_str.replace("<?xml version='1.0' encoding='utf-8'?>", '<?xml version="1.0" encoding="UTF-8"?>')
    
    # Write back to file
    with open(project_file, 'w', encoding='utf-8', newline='\r\n') as f:
        f.write(xml_str)
        
    print("Project file updated successfully with namespaces, defines, and double quotes preserved.")

if __name__ == "__main__":
    main()
