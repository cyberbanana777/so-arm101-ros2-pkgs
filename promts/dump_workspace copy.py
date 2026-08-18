#!/usr/bin/env python3
"""
Для корректной работы скрипт должен находиться в папке workspace. Он должен находится на одном уровне с папками src, build, install, log
Обходит папку src, собирает все текстовые файлы из всех пакетов
и формирует единый MD-дамп.

Запуск:
python3 dump_workspace.py ./src -o full_dump.md
"""

import os
import sys
import argparse
from pathlib import Path

# Расширения, которые считаем бинарными (не читаем)
BINARY_EXTS = {
    '.pyc', '.so', '.dll', '.exe', '.jpg', '.png', '.gif', '.bmp',
    '.zip', '.tar', '.gz', '.rar', '.7z', '.o', '.a', '.elf', '.bin',
    '.stl', '.dae', '.obj', '.ply', '.mesh', '.stp', '.step',
    '.bag', '.db', '.sqlite', '.log', '.pdf', '.doc', '.docx', '.xls'
}

# Расширения -> язык для подсветки в Markdown
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
    '.cfg': 'text',
    '.ini': 'text',
    '.csv': 'text',
    '.html': 'html',
    '.js': 'javascript',
    '.css': 'css',
}

# Максимальный размер файла для чтения (байт) — пропускаем большие
MAX_FILE_SIZE = 1024 * 1024  # 1 МБ


def is_binary(filepath):
    """Проверяет, является ли файл бинарным по расширению."""
    ext = Path(filepath).suffix.lower()
    return ext in BINARY_EXTS


def get_lang(filepath):
    """Определяет язык для подсветки синтаксиса."""
    path = Path(filepath)
    ext = path.suffix.lower()
    # Особые случаи
    if path.name.endswith('.launch.py'):
        return 'python'
    if path.name == 'CMakeLists.txt':
        return 'cmake'
    return LANG_MAP.get(ext, '')


def collect_files(root_dir):
    """
    Рекурсивно обходит root_dir и возвращает список путей ко всем файлам,
    исключая системные папки (__pycache__, .git, build, install, log).
    """
    ignore_dirs = {'.git', '__pycache__', 'build', 'install', 'log', '.vscode', '.idea'}
    for root, dirs, files in os.walk(root_dir):
        # Удаляем игнорируемые папки из обхода
        dirs[:] = [d for d in dirs if d not in ignore_dirs]
        for file in files:
            if file.startswith('.'):
                continue
            yield os.path.join(root, file)


def main():
    parser = argparse.ArgumentParser(
        description='Создаёт Markdown-дамп всех пакетов в папке src.'
    )
    parser.add_argument(
        'src_dir',
        nargs='?',
        default='./src',
        help='Путь к папке src (по умолчанию ./src)'
    )
    parser.add_argument(
        '-o', '--output',
        default='workspace_dump.md',
        help='Имя выходного файла (по умолчанию workspace_dump.md)'
    )
    parser.add_argument(
        '--max-size',
        type=int,
        default=MAX_FILE_SIZE,
        help=f'Максимальный размер файла для чтения в байтах (по умолчанию {MAX_FILE_SIZE})'
    )
    args = parser.parse_args()

    src_dir = os.path.abspath(args.src_dir)
    if not os.path.isdir(src_dir):
        print(f"❌ Ошибка: '{src_dir}' не является директорией", file=sys.stderr)
        sys.exit(1)

    output_file = args.output
    max_size = args.max_size

    # Собираем все пакеты (подпапки в src)
    packages = []
    for item in os.listdir(src_dir):
        item_path = os.path.join(src_dir, item)
        if os.path.isdir(item_path) and not item.startswith('.'):
            packages.append(item)

    packages.sort()
    print(f"🔍 Найдено пакетов: {len(packages)}")
    print(f"📁 Сканируем: {src_dir}")

    total_files = 0
    skipped_binary = 0
    skipped_size = 0

    with open(output_file, 'w', encoding='utf-8') as out:
        out.write(f"# Дамп всего workspace\n\n")
        out.write(f"Создан: {os.path.basename(src_dir)}\n\n")
        out.write("---\n\n")

        for pkg_name in packages:
            pkg_path = os.path.join(src_dir, pkg_name)
            out.write(f"## Пакет: {pkg_name}\n\n")

            # Проверяем, есть ли в пакете package.xml (формально это ROS-пакет)
            pkg_xml = os.path.join(pkg_path, 'package.xml')
            if not os.path.exists(pkg_xml):
                out.write(f"*(Нет package.xml, возможно, это не ROS-пакет)*\n\n")
                continue

            # Собираем все файлы в пакете
            files = sorted(collect_files(pkg_path))
            if not files:
                out.write("*(пакет пуст)*\n\n")
                continue

            for filepath in files:
                rel_path = os.path.relpath(filepath, pkg_path)
                if rel_path.startswith('.'):
                    continue
                out.write(f"### Файл: `{rel_path}`\n\n")

                # Проверка размера
                try:
                    file_size = os.path.getsize(filepath)
                    if file_size > max_size:
                        out.write(f"*(Файл превышает лимит размера ({max_size} байт), пропущен)*\n\n")
                        skipped_size += 1
                        continue
                except OSError:
                    out.write("*(Ошибка получения размера файла)*\n\n")
                    continue

                # Бинарные файлы
                if is_binary(filepath):
                    out.write("*(Бинарный файл, содержимое не отображается)*\n\n")
                    skipped_binary += 1
                    continue

                # Читаем содержимое
                lang = get_lang(filepath)
                out.write(f"```{lang}\n")
                try:
                    with open(filepath, 'r', encoding='utf-8') as f:
                        content = f.read()
                    out.write(content)
                    if not content.endswith('\n'):
                        out.write('\n')
                except UnicodeDecodeError:
                    # Пробуем другие кодировки
                    try:
                        with open(filepath, 'r', encoding='latin-1') as f:
                            content = f.read()
                        out.write(content)
                        if not content.endswith('\n'):
                            out.write('\n')
                    except Exception as e:
                        out.write(f"(Ошибка чтения: {e})")
                except Exception as e:
                    out.write(f"(Ошибка чтения: {e})")
                out.write("```\n\n")
                total_files += 1

    print(f"\n✅ Дамп сохранён в {output_file}")
    print(f"📊 Итоги:")
    print(f"   - Обработано файлов: {total_files}")
    print(f"   - Пропущено бинарных: {skipped_binary}")
    print(f"   - Пропущено из-за размера: {skipped_size}")
    print(f"   - Всего пакетов: {len(packages)}")


if __name__ == '__main__':
    main()