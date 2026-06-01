import os

def main():
    project_dir = r"d:\Keil5\STM32Projects\SkyStar\BSP\V2\SkyStar_BSP_HAL\MDK-ARM"
    project_file = os.path.join(project_dir, "SkyStar_BSP_HAL.uvprojx")
    
    with open(project_file, 'r', encoding='utf-8') as f:
        content = f.read()
        
    # Replace duplicate attribute
    duplicate_str = 'xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="project_projx.xsd" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"'
    correct_str = 'xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="project_projx.xsd"'
    
    if duplicate_str in content:
        content = content.replace(duplicate_str, correct_str)
        print("Duplicate attribute replaced using exact match.")
    else:
        # Fallback regex-free replacement
        content = content.replace(' xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"', '', 1)
        content = content.replace('<Project', '<Project xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"', 1)
        print("Fallback replacement done.")
        
    with open(project_file, 'w', encoding='utf-8', newline='\r\n') as f:
        f.write(content)
        
    print("Project file fixed.")

if __name__ == "__main__":
    main()
