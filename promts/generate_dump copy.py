#!/usr/bin/env python3

'''
Для корректной работы этот скрипт должен находиться в папке src.

Запуск:
python3 generate_dump.py ./soarm101_hardware -o hardware_dump.md
'''

import os
import sys
import argparse
from pathlib import Path

# Расширения, которые считаем бинарными (чтобы не читать)
BINARY_EXTS = {'.pyc', '.so', '.dll', '.exe', '.jpg', '.png', '.gif', '.bmp', 
               '.zip', '.tar', '.gz', '.rar', '.7z', '.o', '.a', '.elf', '.bin'}

# Маппинг расширений -> язык для подсветки в Markdown
LANG_MAP = {
    '.xml': 'xml',
    '.launch': 'xml',
    '.xacro': 'xml',
    '.urdf': 'xml',
    '.py': 'python',
    '.cpp': 'cpp',
    '.cc': 'cpp',
    '.cxx': 'cpp',
    '.h': 'cpp',
    '.hpp': 'cpp',
    '.c': 'c',
    '.txt': 'text',
    '.md': 'markdown',
    '.cmake': 'cmake',
    '.yml': 'yaml',
    '.yaml': 'yaml',
    '.json': 'json',
    '.sh': 'bash',
    '.bash': 'bash',
    '.msg': 'text',
    '.srv': 'text',
    '.action': 'text',
    '.launch.py': 'python',
}

def is_binary(filepath):
    """Проверяет, является ли файл бинарным по расширению."""
    ext = Path(filepath).suffix.lower()
    return ext in BINARY_EXTS

def get_lang(filepath):
    """Определяет язык для подсветки синтаксиса."""
    ext = Path(filepath).suffix.lower()
    # Особые случаи для .launch.py и т.п.
    if filepath.endswith('.launch.py'):
        return 'python'
    return LANG_MAP.get(ext, '')

def collect_files(dirpath):
    """Рекурсивно обходит папку и возвращает пути ко всем файлам."""
    for root, dirs, files in os.walk(dirpath):
        # Игнорируем скрытые папки (например, .git, .vscode)
        dirs[:] = [d for d in dirs if not d.startswith('.')]
        for file in files:
            # Игнорируем скрытые файлы
            if file.startswith('.'):
                continue
            yield os.path.join(root, file)

def main():
    parser = argparse.ArgumentParser(description='Сборка текстового дампа папки для передачи ассистенту.')
    parser.add_argument('folder', help='Путь к папке пакета (например, soarm101_interfaces)')
    parser.add_argument('-o', '--output', default='package_dump.md', help='Имя выходного файла (по умолчанию package_dump.md)')
    args = parser.parse_args()

    folder = args.folder
    if not os.path.isdir(folder):
        print(f"Ошибка: '{folder}' не является директорией", file=sys.stderr)
        sys.exit(1)

    package_name = os.path.basename(folder)
    output_file = args.output

    with open(output_file, 'w', encoding='utf-8') as out:
        out.write(f"# Пакет: {package_name}\n\n")
        out.write(f"Это пакет **{package_name}**.\n\n")
        out.write("---\n\n")

        for filepath in sorted(collect_files(folder)):
            rel_path = os.path.relpath(filepath, folder)
            out.write(f"## Файл: `{rel_path}`\n\n")

            if is_binary(filepath):
                out.write("*(Бинарный файл, содержимое не отображается)*\n\n")
                continue

            lang = get_lang(filepath)
            out.write(f"```{lang}\n")
            try:
                with open(filepath, 'r', encoding='utf-8') as f:
                    content = f.read()
                out.write(content)
                if not content.endswith('\n'):
                    out.write('\n')
            except Exception as e:
                out.write(f"(Ошибка чтения: {e})")
            out.write("```\n\n")

    print(f"✅ Документация пакета сохранена в {output_file}")
    print("Теперь вы можете скопировать содержимое этого файла и отправить в чат.")

if __name__ == '__main__':
    main()
