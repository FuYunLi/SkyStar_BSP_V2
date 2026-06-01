import os
import xml.etree.ElementTree as ET

def main():
    project_dir = r"d:\Keil5\STM32Projects\SkyStar\BSP\V2\SkyStar_BSP_HAL\MDK-ARM"
    project_file = os.path.join(project_dir, "SkyStar_BSP_HAL.uvprojx")
    
    # Register namespaces to prevent ns0 prefixes
    ET.register_namespace('xsi', 'http://www.w3.org/2001/XMLSchema-instance')
    
    # Parse the XML file
    tree = ET.parse(project_file)
    root = tree.getroot()
    
    # Manually ensure the xmlns:xsi attribute is preserved
    root.set('xmlns:xsi', 'http://www.w3.org/2001/XMLSchema-instance')
            
    # Find the Groups element
    groups_elem = root.find('.//Groups')
    if groups_elem is None:
        print("Error: <Groups> tag not found in project file.")
        return
        
    # Print all groups
    print("Existing groups in Keil project:")
    for group in groups_elem.findall('Group'):
        name_elem = group.find('GroupName')
        if name_elem is not None:
            print(f" - {name_elem.text}")
            
    # Check if APP/demos or APP/Demos group exists, otherwise use APP
    group_name = None
    for group in groups_elem.findall('Group'):
        name_elem = group.find('GroupName')
        if name_elem is not None:
            text = name_elem.text.strip().lower()
            if text in ["app/demos", "app/demo", "app/Demos", "app"]:
                group_name = name_elem.text
                break
                
    if group_name is None:
        group_name = "APP/demos"
        
    print(f"Targeting group: {group_name}")
    
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
        print(f"Group {group_name} found.")
        files_elem = target_group.find('Files')
        if files_elem is None:
            files_elem = ET.SubElement(target_group, "Files")
            
    # Define files to add
    files_to_add = [
        {"name": "app_uart_demo.c", "type": "1", "path": "../APP/demos/app_uart_demo.c"},
        {"name": "app_uart_demo.h", "type": "5", "path": "../APP/demos/app_uart_demo.h"},
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
            
    # Save back the updated project file
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
