# soarm101_examples

Пакет содержит примеры узлов для управления роботом SOARM101 через стандартные action-интерфейсы ROS2. Предназначен для демонстрации возможностей стека, тестирования контроллеров и быстрой отладки движений робота.

---

## Назначение

- Предоставить готовые примеры для отправки траекторий движения суставам робота.
- Показать, как взаимодействовать с `joint_trajectory_controller` через action `FollowJointTrajectory`.
- Показать, как управлять схватом через action `GripperCommand`.
- Обеспечить быстрый старт для новых разработчиков (можно сразу запустить и увидеть движение робота).

---

## Структура пакета

```
soarm101_examples/
├── src/
│   ├── joint_goal_sender.cpp          # Отправка цели для суставов (5 DOF)
│   └── gripper_goal_sender.cpp        # Отправка цели для схвата
├── launch/
│   └── rqt_joint_trajectory_controller.launch.py  # Запуск GUI-плагина rqt
├── CMakeLists.txt
├── package.xml
├── LICENSE
└── (тесты – при необходимости)
```

---

## Примеры узлов

### 1. `joint_goal_sender`

Узел, который отправляет одну цель движения для группы суставов робота (без учёта схвата). Использует action `/joint_trajectory_controller/follow_joint_trajectory` (стандартный интерфейс `control_msgs/action/FollowJointTrajectory`).

**Параметры (ROS-параметры):**

| Параметр | Тип | По умолчанию | Описание |
|----------|-----|--------------|----------|
| `shoulder_pan` | double | 0.0 | Целевое положение сустава `shoulder_pan_joint`, радианы |
| `shoulder_lift` | double | 0.0 | Целевое положение сустава `shoulder_lift_joint`, радианы |
| `elbow_flex` | double | 0.0 | Целевое положение сустава `elbow_flex_joint`, радианы |
| `wrist_flex` | double | 0.0 | Целевое положение сустава `wrist_flex_joint`, радианы |
| `wrist_roll` | double | 0.0 | Целевое положение сустава `wrist_roll_joint`, радианы |
| `goal_time` | double | 2.0 | Время выполнения движения, секунды |

**Использование:**

```bash
# Базовый запуск (все позиции = 0)
ros2 run soarm101_examples joint_goal_sender

# С указанием целевых углов (например, вертикальная поза)
ros2 run soarm101_examples joint_goal_sender \
  --ros-args -p shoulder_lift:=0.5 -p elbow_flex:=-0.8 \
  -p goal_time:=3.0
```

**Что происходит внутри:**
1. Узел подключается к action-серверу контроллера.
2. Формирует сообщение `JointTrajectory` с одной точкой (конечная позиция).
3. Отправляет цель и ожидает результат.
4. После завершения движения узел завершает работу.

---

### 2. `gripper_goal_sender`

Узел для управления схватом робота через action `/gripper_controller/gripper_cmd` (тип `control_msgs/action/GripperCommand`).

**Параметры (ROS-параметры):**

| Параметр | Тип | По умолчанию | Описание |
|----------|-----|--------------|----------|
| `position` | double | 0.0 | Целевой угол (-0.07 – полностью закрыт, ~1.16 – открыт) |
| `max_effort` | double | 10.0 | Максимальное усилие при сжатии |

**Использование:**

```bash
# Закрыть схват
ros2 run soarm101_examples gripper_goal_sender

# Открыть схват
ros2 run soarm101_examples gripper_goal_sender \
  --ros-args -p position:=1.16

# С заданным усилием
ros2 run soarm101_examples gripper_goal_sender \
  --ros-args -p position:=0.02 -p max_effort:=5.0
```

---

### 3. Launch-файл: `rqt_joint_trajectory_controller.launch.py`

Запускает графический интерфейс `rqt_joint_trajectory_controller` для ручного управления суставами через слайдеры. Удобно для тестирования и визуальной отладки.

**Использование:**

```bash
ros2 launch soarm101_examples rqt_joint_trajectory_controller.launch.py
```

После запуска откроется окно rqt с панелью управления `joint_trajectory_controller`, где можно:
- Выбрать суставы для управления,
- Задать целевые позиции ползунками,
- Отправить траекторию на выполнение.

---

## Взаимодействие с системой

Все примеры используют стандартные action-интерфейсы, которые предоставляются контроллерами из пакета `soarm101_ros2_control`:
- **`joint_trajectory_controller`** – управляет группой суставов (5 DOF).
- **`gripper_controller`** – управляет схватом (1 DOF).

Перед запуском примеров убедитесь, что:
1. Запущен полный стек (через `soarm101_bringup`).
2. Контроллеры активированы (их состояние можно проверить через `ros2 control list_controllers`).
3. Для реального робота – он подключён и откалиброван.

---

## Примеры использования в скриптах

### Python (через `rclpy.action`)

```python
import rclpy
from rclpy.action import ActionClient
from control_msgs.action import FollowJointTrajectory
from trajectory_msgs.msg import JointTrajectory, JointTrajectoryPoint

node = rclpy.create_node('example_client')
client = ActionClient(node, FollowJointTrajectory, '/joint_trajectory_controller/follow_joint_trajectory')

goal = FollowJointTrajectory.Goal()
goal.trajectory.joint_names = ['shoulder_pan_joint', 'shoulder_lift_joint', ...]
point = JointTrajectoryPoint()
point.positions = [0.5, 0.0, -1.0, 0.0, 0.0]
point.time_from_start = rclpy.duration.Duration(seconds=2.0)
goal.trajectory.points.append(point)

client.wait_for_server()
client.send_goal_async(goal)
```

---

## Дальнейшее развитие

Пакет может быть расширен следующими улучшениями:
- Добавление узлов для циклических движений (например, "помахать рукой").
- Интеграция с MoveIt для выполнения сложных траекторий.
- Примеры работы с сервисами (включение/отключение момента).
- Запись и воспроизведение траекторий из файла.

---

## Зависимости

- `rclcpp`, `rclcpp_action` – базовые компоненты ROS2.
- `trajectory_msgs` – сообщения для траекторий.
- `control_msgs` – action-интерфейсы контроллеров.
- `rqt_joint_trajectory_controller` – GUI-плагин для ручного управления.

---

## Лицензия

Пакет распространяется под лицензией **MIT** (см. файл [LICENSE](LICENSE) в корне пакета).

---

## Поддержка

Вопросы и предложения оформляйте через [Issues](https://github.com/cyberbanana777/so-arm101-ros2-pkgs/issues).