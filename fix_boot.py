import re

filepath = 'src/activities/boot_sleep/BootActivity.cpp'

with open(filepath, 'r', encoding='utf-8') as f:
    content = f.read()

# Swap the long line for the perfectly wrapped version clang-format wants
new_content = re.sub(
    r'renderer\.drawCenteredText\(UI_10_FONT_ID, pageHeight / 2 \+ 70, "Crosspoint: Enhanced Reading Mod", true, EpdFontFamily::BOLD\);',
    r'renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 70, "Crosspoint: Enhanced Reading Mod", true,\n                            EpdFontFamily::BOLD);',
    content
)

if new_content != content:
    with open(filepath, 'w', encoding='utf-8', newline='\n') as f:
        f.write(new_content)
    print("\n[*] Boom! Line wrapped perfectly for clang-format.")
else:
    print("\n[!] Error: Could not find the line to replace.")
