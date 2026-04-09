import os
import sys
import re

def replace_qml_imports(root_dir, from_prefix, to_prefix):
    pattern = re.compile(r'(^\s*import\s+)' + re.escape(from_prefix) + r'(\.[^\s]*)', re.MULTILINE)
    total_replacements = 0
    for subdir, _, files in os.walk(root_dir):
        for file in files:
            if file.endswith('.qml'):
                file_path = os.path.join(subdir, file)
                try:
                    with open(file_path, 'r', encoding='utf-8') as f:
                        content = f.read()
                    new_content, num_replacements = pattern.subn(r'\1' + to_prefix + r'\2', content)
                    if num_replacements > 0:
                        with open(file_path, 'w', encoding='utf-8') as f:
                            f.write(new_content)
                        total_replacements += num_replacements
                except Exception:
                    pass  # Ignore files that can't be processed
    print(f"Total replacements: {total_replacements}")

if __name__ == "__main__":
    if len(sys.argv) != 3 or sys.argv[1] not in ("to-nextcloud", "to-hidrivenext"):
        print("Usage: python switch_qml_imports.py [to-nextcloud|to-hidrivenext] <root_dir>")
        sys.exit(1)
    mode = sys.argv[1]
    root_dir = sys.argv[2]
    if mode == "to-nextcloud":
        replace_qml_imports(root_dir, "com.ionos.hidrivenext", "com.nextcloud")
    else:
        replace_qml_imports(root_dir, "com.nextcloud", "com.ionos.hidrivenext")
