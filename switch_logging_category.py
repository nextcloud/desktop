import os
import sys
import re

def replace_logging_category(root_dir, from_prefix, to_prefix):
    pattern = re.compile(r'(Q_LOGGING_CATEGORY\s*\(\s*\w+\s*,\s*")' + re.escape(from_prefix) + r'(\.[^"]*")')
    total_replacements = 0
    for subdir, _, files in os.walk(root_dir):
        for file in files:
            if file.endswith(('.cpp', '.h', '.hpp', '.cxx', '.cc')):
                file_path = os.path.join(subdir, file)
                try:
                    with open(file_path, 'r', encoding='utf-8') as f:
                        content = f.read()
                    new_content, num_replacements = pattern.subn(r'\1' + to_prefix + r'\2', content)
                    if num_replacements > 0:
                        with open(file_path, 'w', encoding='utf-8') as f:
                            f.write(new_content)
                        total_replacements += num_replacements
                except Exception as e:
                    pass  # Ignore files that can't be processed
    print(f"Total replacements: {total_replacements}")

if __name__ == "__main__":
    if len(sys.argv) != 3 or sys.argv[1] not in ("to-nextcloud", "to-hidrivenext"):
        print("Usage: python switch_logging_category.py [to-nextcloud|to-hidrivenext] <root_dir>")
        sys.exit(1)
    mode = sys.argv[1]
    root_dir = sys.argv[2]
    if mode == "to-nextcloud":
        replace_logging_category(root_dir, "hidrivenext", "nextcloud")
    else:
        replace_logging_category(root_dir, "nextcloud", "hidrivenext")
