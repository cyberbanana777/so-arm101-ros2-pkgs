# converter_calibration_data

Пакет содержит утилиту для преобразования калибровочных данных робота SOARM101 из формата библиотеки **lerobot** (JSON) в кастомный YAML-формат, а также готовые примеры конфигурационных файлов для лидера и фолловера.

---

## Назначение

После калибровки робота с помощью `lerobot-calibrate` вы получаете JSON-файл с параметрами каждого сустава. Данный пакет преобразует этот файл в структурированный YAML-конфиг, который затем используется:
- компонентом `soarm101_hardware` для инициализации сервоприводов.

---

## Установка зависимостей

```bash
pip install -r pip_requirements.txt
```

Устанавливает библиотеки:
- `pyyaml`.

---

## Использование

### 1. Проведите калибровку робота через lerobot

Пример команды ([из документации lerobot](https://huggingface.co/docs/lerobot/so101?calibrate_leader=Command)):

```bash
lerobot-calibrate \
    --teleop.type=so101_leader \
    --teleop.port=/dev/ttyACM0 \
    --teleop.id=my_awesome_leader_arm
```

На выходе получите JSON-файл, например `my_awesome_follower_arm.json`.

### 2. Запустите конвертер

```bash
cd converter_calibration_data/converter_calibration_data
python3 lerobot_to_custom_format.py ./my_awesome_follower_arm.json ../config/motor_calibration.yaml leader
```

**Аргументы:**
- `./my_awesome_follower_arm.json` – путь к JSON-файлу от lerobot.
- `../config/motor_calibration.yaml` – путь, куда сохранить итоговый YAML-конфиг (для корректной работы всего стека необходимо класть в `../config/` относительно директории скрипта).
- `leader` – префикс, который будет добавлен к имени выходного файла (например, `leader_motor_calibration.yaml`). Это позволяет различать конфигурации для лидера и фолловера. Должен быть или `leader`, или `follower`

После успешного выполнения вы получите файл вида (в примере — `leader_motor_calibration.yaml`):

```yaml
shoulder_pan_joint:
  drive_mode: 0
  range_min: 732
  range_max: 3459
shoulder_lift_joint:
  drive_mode: 0
  range_min: 824
  range_max: 3209
...
```

Для фолловера достаточно указать другой префикс, например `follower`, и выходной файл будет назван `follower_motor_calibration.yaml`.

### 3. Соберите workspace

```bash
cd ../../..
colcon build --packages-select converter_calibration_data
source install/local_setup.bash
```

Теперь калибровочные данные доступны для использования другими компонентами ПО.

---

## Формат выходного YAML

Файл содержит словарь, где ключи — имена суставов (с суффиксом `_joint`), а значения — параметры:

| Поле | Тип | Описание |
|------|-----|----------|
| `drive_mode` | `int` | Режим привода (0 — стандартный, другие значения зависят от протокола) |
| `range_min` | `int` | Минимальное значение положения (в сырых отсчётах энкодера) |
| `range_max` | `int` | Максимальное значение положения |

Эти диапазоны используются для пересчёта сырых значений в радианы и обратно.

---

## Лицензия

Пакет распространяется под лицензией **MIT** (см. файл [LICENSE](LICENSE) в корне пакета).

---

## Версия

**2.0.0** – изменён интерфейс командной строки: добавлен обязательный аргумент `prefix` для формирования имени выходного файла; обновлена документация.

---

## Поддержка

Вопросы и предложения оформляйте через [Issues](https://github.com/cyberbanana777/so-arm101-ros2-pkgs/issues).
