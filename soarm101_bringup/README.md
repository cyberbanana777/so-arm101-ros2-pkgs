# soarm101_bringup

Пакет-агрегатор, который обеспечивает **единый запуск** всего программного стека робота SOARM101. Он объединяет запуск `robot_state_publisher`, `ros2_control_node` (или Gazebo) и всех необходимых контроллеров в зависимости от выбранного режима: **симуляция** или **реальный робот**.

---

## Назначение

- Предоставить одну точку входа для запуска всей системы.
- Автоматически подбирать конфигурацию `ros2_control` (реальное железо или Gazebo) на основе аргумента `use_sim`.
- Загружать и активировать все контроллеры (`joint_state_broadcaster`, `joint_trajectory_controller`, `gripper_controller`, а для реального робота – ещё и `soarm101_telemetry_controller`).
- Обрабатывать xacro-файл, подставляя нужные include для `ros2_control` и разрешая пути к мешам (заменяя `package://` на абсолютные пути).

---

## Структура пакета

```
soarm101_bringup/
├── launch/
│   └── bringup.launch.py          # Главный launch-файл
├── urdf/
│   └── full.xacro                 # Сборка полного URDF с выбором ros2_control
├── test/                          # Тесты (flake8, pep257, copyright)
├── package.xml
├── setup.py
├── setup.cfg
├── LICENSE
└── resource/                      # Файл для ament_index
```

---

## Launch-файл: `bringup.launch.py`

### Аргументы

| Аргумент | Тип | По умолчанию | Описание |
|----------|-----|--------------|----------|
| `use_sim` | bool | `false` | Если `true` – запускается Gazebo с пустым миром и симуляционные контроллеры; если `false` – запускается `ros2_control_node` для реального робота. |

### Логика работы

1. **Разбор xacro** – читает `urdf/full.xacro`, передаёт в него значение `use_sim`, получает полную URDF-строку.
2. **Замена `package://`** – все пути вида `package://soarm101_description/meshes/...` заменяются на абсолютные пути с префиксом `file://`, чтобы Gazebo и другие компоненты могли загрузить меши.
3. **Запуск `robot_state_publisher`** – публикует `/robot_description` и транслирует `tf` на основе `joint_state` (которые будут приходить от контроллеров).
4. **Ветвление по `use_sim`**:

#### Режим симуляции (`use_sim:=true`)
- Запускает **Gazebo** через `ros_gz_sim` (пустой мир).
- Спавнит робота через `ros_gz_sim create`.
- Загружает контроллеры из `sim_controllers.yaml` (пакет `soarm101_ros2_control`):
  - `joint_state_broadcaster`
  - `joint_trajectory_controller`
  - `gripper_controller`

#### Режим реального робота (`use_sim:=false`)
- Запускает **`ros2_control_node`** с параметрами:
  - `robot_description` (обработанный URDF)
  - `real_controllers.yaml` (из `soarm101_ros2_control`)
- Спавнит контроллеры:
  - `joint_state_broadcaster`
  - `joint_trajectory_controller`
  - `gripper_controller`
  - `soarm101_telemetry_controller` – кастомный контроллер для публикации расширенной телеметрии.

---

## Полный URDF: `urdf/full.xacro`

Файл включает:
- Основной URDF робота (`soarm101_description/urdf/soarm101.xacro`).
- В зависимости от аргумента `use_sim` подключает либо:
  - `ros2_control_sim.xacro` – плагин `gz_ros2_control` для Gazebo,
  - `ros2_control_real.xacro` – плагин `soarm101_hardware/SOARM101SystemHardware` для реального железа.

Это позволяет использовать одно и то же описание робота для обоих режимов, меняя только низкоуровневый hardware-интерфейс.

---

## Использование

### Запуск реального робота

```bash
ros2 launch soarm101_bringup bringup.launch.py use_sim:=false
```

После запуска:
- Подключается к шине моторов (порт и скорость задаются в `ros2_control_real.xacro`).
- Активируются все контроллеры.
- Робот готов к приёму траекторий через `joint_trajectory_controller` или команд через другие интерфейсы.

### Запуск симуляции в Gazebo

```bash
ros2 launch soarm101_bringup bringup.launch.py use_sim:=true
```

После запуска:
- Открывается окно Gazebo с пустым миром.
- Робот появляется в мире с начальной позицией.
- Контроллеры работают в симуляционном режиме (используется `gz_ros2_control`).

### Дополнительные опции

Можно добавить аргументы для изменения параметров, например, указать другой файл калибровки или порт, но они уже заданы в xacro-файлах `soarm101_ros2_control`. При необходимости их можно переопределить через launch-аргументы (сейчас не реализовано, но легко добавить).

---

## Зависимости

Пакет явно зависит от:

- `robot_state_publisher` – публикация состояния робота.
- `controller_manager` – управление контроллерами.
- `soarm101_description` – URDF-описание.
- `soarm101_hardware` – аппаратный компонент для реального робота.
- `soarm101_ros2_control` – конфигурации `ros2_control`.
- `soarm101_telemetry_controller` – кастомный контроллер телеметрии.

Для симуляции дополнительно требуются:
- `ros_gz_sim` – интеграция Gazebo с ROS2.
- `gz_ros2_control` – плагин управления в Gazebo.

---

## Примечания

- В реальном режиме контроллеры загружаются через spawner, а не через `ros2_control_node` напрямую, что обеспечивает более гибкое управление и возможность перезагрузки отдельных контроллеров.

---

## Лицензия

Пакет распространяется под лицензией **MIT** (см. файл [LICENSE](LICENSE) в корне пакета).

---

## Поддержка

Вопросы и предложения оформляйте через [Issues](https://github.com/cyberbanana777/so-arm101-ros2-pkgs/issues).