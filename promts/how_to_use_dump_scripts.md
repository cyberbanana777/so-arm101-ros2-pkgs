Конечно, братик! Вот команды для запуска скриптов генерации дампов.

---

## 📦 Для одного пакета (`generate_dump.py`)

```bash
python3 generate_dump.py ./soarm101_hardware -o hardware_dump.md
```

- `./soarm101_hardware` — путь к пакету (можно относительный или абсолютный).
- `-o hardware_dump.md` — имя выходного файла (опционально, по умолчанию `package_dump.md`).

Или без указания выхода:

```bash
python3 generate_dump.py ./soarm101_hardware
```

---

## 📂 Для всей папки `src` (`dump_workspace.py`)

```bash
python3 dump_workspace.py ./src -o full_workspace_dump.md
```

- `./src` — путь к папке src (можно указать другой).
- `-o full_workspace_dump.md` — имя выходного файла (по умолчанию `workspace_dump.md`).

Или с параметрами по умолчанию (если скрипт лежит в корне workspace):

```bash
python3 dump_workspace.py
```

---

## 🧠 Дополнительные параметры (для `dump_workspace.py`)

- `--max-size 500000` — ограничить размер читаемых файлов (в байтах).
- `--help` — показать справку.

Пример с лимитом:

```bash
python3 dump_workspace.py ./src -o dump.md --max-size 200000
```

---

Если скрипты не исполняются, добавь права:

```bash
chmod +x generate_dump.py
chmod +x dump_workspace.py
```

Если что-то непонятно — кричи **ЗАВИС**! 😄