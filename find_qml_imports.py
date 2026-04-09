import os
import sys
import re

def find_imports_in_qml(root_dir, import_prefix):
    pattern = re.compile(r'^\s*import\s+' + re.escape(import_prefix) + r'.*$', re.MULTILINE)
    matches = []
    for subdir, _, files in os.walk(root_dir):
        for file in files:
            if file.endswith('.qml'):
                file_path = os.path.join(subdir, file)
                try:
                    with open(file_path, 'r', encoding='utf-8') as f:
                        content = f.read()
                    for match in pattern.finditer(content):
                        matches.append((file_path, match.group().strip()))
                except Exception:
                    pass  # Ignore files that can't be processed
    print(f"Found {len(matches)} matching import lines:")
    for file_path, line in matches:
        print(f"{file_path}: {line}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python find_qml_imports.py <import_prefix> <root_dir>")
        sys.exit(1)
    import_prefix = sys.argv[1]
    root_dir = sys.argv[2]
    find_imports_in_qml(root_dir, import_prefix)
