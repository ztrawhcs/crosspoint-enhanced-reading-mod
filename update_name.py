import os

old_text = "Crosspoint-Button-Mod"
new_text = "Crosspoint: Enhanced Reading Mod"

found = False

# Walk through all folders and files in the project
for root, dirs, files in os.walk("."):
    # Skip hidden git and build folders
    if ".git" in root or ".pio" in root:
        continue
        
    for file in files:
        if file.endswith(('.cpp', '.h', '.ini', '.json', '.txt')):
            path = os.path.join(root, file)
            try:
                with open(path, 'r', encoding='utf-8') as f:
                    content = f.read()
                
                # If we find the old version string, replace it!
                if old_text in content:
                    print(f"[*] Found the version string hiding in: {path}")
                    new_content = content.replace(old_text, new_text)
                    
                    with open(path, 'w', encoding='utf-8', newline='\n') as f:
                        f.write(new_content)
                    print(f"[*] Successfully updated to '{new_text}'!")
                    found = True
            except Exception:
                pass

if not found:
    print("\n[!] Error: Could not find that specific version string anywhere.")
