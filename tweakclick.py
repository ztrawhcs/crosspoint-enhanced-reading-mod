filepath = 'src/activities/reader/EpubReaderActivity.cpp'

with open(filepath, 'r', encoding='utf-8') as f:
    content = f.read()

# Swap 300ms for 400ms
new_content = content.replace(
    'constexpr unsigned long doubleClickMs = 300;',
    'constexpr unsigned long doubleClickMs = 400;'
)

if new_content != content:
    with open(filepath, 'w', encoding='utf-8', newline='\n') as f:
        f.write(new_content)
    print("\n[*] Success: Double-click window increased to 400ms!")
else:
    print("\n[!] Error: Could not find the double-click variable.")
