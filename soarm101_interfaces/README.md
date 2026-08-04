# soarm101_interfaces

Пакет содержит кастомные сообщения ROS2 для публикации данных телеметрии всех моторов робота.  
Он определяет формат данных для обмена между узлами стека и используется контроллерами, hardware-компонентами и узлами визуализации.

---

## Сообщения (Messages)

### `MotorState.msg`
Сообщение о состоянии одного мотора.

| Поле | Тип | Описание |
|------|-----|----------|
| `motor_id` | `int32` | Идентификатор мотора (даётся при первичной настройке) |
| `joint_name` | `string` | Имя сочленения (например, `"shoulder_pan_joint"`) |
| `position` | `float64` | Текущее положение (радианы) |
| `velocity` | `float64` | Текущая скорость (рад/с) |
| `effort` | `float64` | Текущее усилие (Н·м) |
| `temperature` | `float64` | Температура мотора (°C) |
| `voltage` | `float64` | Напряжение питания (В) |
| `current` | `float64` | Ток (А) |
| `moving_flag` | `bool` | Состояние мотора: `true` – мотор движется, `false` – остановлен |
| `max_torque` | `float64` | Максимальный момент для данного сустава (Н·м), экспортируется из state-интерфейсов `soarm101_hardware` |
| `enable_torque` | `bool` | Состояние включения момента: `true` – момент включён, `false` – выключен |

### `MotorStates.msg`
Массив состояний всех моторов, содержит в себе сообщения типа `MotorState.msg`:

```text
soarm101_interfaces/MotorState[] motors
```

---

## Использование в других пакетах

**Добавьте зависимость в `package.xml`:**

```xml
<depend>soarm101_interfaces</depend>
```

**В `CMakeLists.txt`:**

```cmake
find_package(soarm101_interfaces REQUIRED)
```

### Пример подписки на топик (Python)

```python
from soarm101_interfaces.msg import MotorStates

def callback(msg: MotorStates):
    for motor in msg.motors:
        print(f"Motor {motor.motor_id}: pos={motor.position:.3f}, "
              f"temp={motor.temperature:.1f}°C, "
              f"max_torque={motor.max_torque:.2f} N·m")

sub = node.create_subscription(MotorStates, '/soarm101_telemetry_controller/motor_states', callback, 10)
```

---

## Топик

В составе полного стека SOARM101 пакет публикует телеметрию в топик:

**`/soarm101_telemetry_controller/motor_states`** – тип `soarm101_interfaces/msg/MotorStates`

---

## Лицензия

Распространяется под лицензией **MIT** (см. [LICENSE](LICENSE) в корне репозитория).

---

## Версия

**2.0.0** – добавлены поля `max_torque` и `enable_torque`.

---

## Поддержка

Вопросы и предложения оформляйте через [Issues](https://github.com/cyberbanana777/so-arm101-ros2-pkgs/issues).
