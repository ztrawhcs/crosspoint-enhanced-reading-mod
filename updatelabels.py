import re

filepath = 'src/activities/reader/EpubReaderMenuActivity.h'

with open(filepath, 'r', encoding='utf-8') as f:
    content = f.read()

# Swap the old strings with the new descriptive ones
new_content = re.sub(
    r'\{"Bottom=Format",\s*"Bottom=Nav"\}',
    r'{"Bottom: Formatting", "Bottom: Page Turns"}',
    content
)

if new_content != content:
    with open(filepath, 'w', encoding='utf-8', newline='\n') as f:
        f.write(new_content)
    print("\n[SUCCESS] Menu labels updated perfectly!")
else:
    print("\n[!] ERROR: Could not find the target text to replace.")
