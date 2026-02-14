import subprocess

print("Searching your Git history to rescue the original CrossPointState.cpp...")

good_content = None
# Look backwards in time through your last 50 commits
for i in range(50):
    commit = f"HEAD~{i}"
    try:
        # Ask Git to read the file from this specific past commit
        content = subprocess.check_output(
            ['git', 'show', f"{commit}:src/CrossPointState.cpp"], 
            stderr=subprocess.DEVNULL
        ).decode('utf-8')
        
        # If the file does NOT contain the Settings function, we found the uncorrupted version!
        if "CrossPointSettings::getReaderFontId" not in content and "CrossPointState" in content:
            print(f"[*] Found the original, perfect file in commit {commit}!")
            good_content = content
            break
    except Exception:
        continue

if good_content:
    with open("src/CrossPointState.cpp", "w", encoding="utf-8", newline='\n') as f:
        f.write(good_content)
    print("\n[SUCCESS] CrossPointState.cpp has been fully restored!")
else:
    print("[!] ERROR: Could not find the original file in the import subprocess

print("Searching your Git history to rescue the original CrossPointState.cpp...")

good_content = None
# Look backwards in time through your last 50 commits
for i in range(50):
    commit = f"HEAD~{i}"
    try:
        # Ask Git to read the file from this specific past commit
        content = subprocess.check_output(
            ['git', 'show', f"{commit}:src/CrossPointState.cpp"], 
            stderr=subprocess.DEVNULL
        ).decode('utf-8')
        
        # If the file does NOT contain the Settings function, we found the uncorrupted version!
        if "CrossPointSettings::getReaderFontId" not in content and "CrossPointState" in content:
            print(f"[*] Found the original, perfect file in commit {commit}!")
            good_content = content
            break
    except Exception:
        continue

if good_content:
    with open("src/CrossPointState.cpp", "w", encoding="utf-8", newline='\n') as f:
        f.write(good_content)
    print("\n[SUCCESS] CrossPointState.cpp has been fully restored!")
else:
    print("[!] ERROR: Could not find the original file in the last 50 commits.")
