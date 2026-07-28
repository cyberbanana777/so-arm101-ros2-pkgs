# soarm101_interfaces

Пакет содержит кастомные сообщения ROS2 для обмена данными с роботизированной рукой SOARM101.  
Он определяет формат телеметрии для всех моторов и используется всеми узлами стека.

---

## Сообщения (Messages)

### `MotorState.msg`
Детальное состояние одного сервопривода.

| Поле | Тип | Описание |
|------|-----|----------|
| `motor_id` | `int32` | Идентификатор мотора (номер в протоколе) |
| `joint_name` | `string` | Имя сочленения (например, `"joint1"`) |
| `position` | `float64` | Текущее положение (радианы) |
| `velocity` | `float64` | Текущая скорость (рад/с) |
| `effort` | `float64` | Текущее усилие (условные ед. см. подробно документацию к сервоприводам) |
| `temperature` | `float64` | Температура мотора (°C) |
| `voltage` | `float64` | Напряжение питания (В) |
| `current` | `float64` | Ток (А) |
| `moving_flag` | `bool` | `true` – мотор движется, `false` – остановлен |

### `MotorStates.msg`
Массив состояний всех моторов:

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
        print(f"Motor {motor.motor_id}: pos={motor.position:.3f}")

sub = node.create_subscription(MotorStates, '/soarm101_telemetry_controller/motor_states', callback, 10) # 'motor_states' как пример топика
```

---

## Топик

В составе полного стека SOARM101 пакет публикует телеметрию в топик:

- **`/soarm101_telemetry_controller/motor_states`** – тип `soarm101_interfaces/msg/MotorStates`

---

## Лицензия

Распространяется под лицензией **MIT** (см. [LICENSE](LICENSE) в корне репозитория).

---

## Поддержка

Вопросы и предложения оформляйте через [Issues](https://github.com/cyberbanana777/so-arm101-ros2-pkgs/issues) (ссылка актуальна для вашего репозитория).
