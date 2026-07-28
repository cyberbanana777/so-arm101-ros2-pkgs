# soarm101_telemetry_controller

Пакет предоставляет кастомный контроллер для сбора и публикации телеметрии сервоприводов робота SOARM101. Он читает state-интерфейсы из `ros2_control` (экспортируемые компонентом `soarm101_hardware`) и формирует удобное агрегированное сообщение `MotorStates`, содержащее все измеряемые параметры каждого мотора.

---

## Назначение

- Получение данных о состоянии каждого сустава через state-интерфейсы (position, velocity, effort, temperature, voltage, current, moving_flag).
- Публикация их в одном топике с типом `soarm101_interfaces/msg/MotorStates` для удобного мониторинга и логирования.
- Отделение телеметрии от управляющих контроллеров (например, `joint_trajectory_controller`), что упрощает архитектуру.
- Предоставляет гибкую настройку через параметры: можно выбрать набор публикуемых полей, задать ID моторов и т.д.

---

## Параметры конфигурации

Контроллер настраивается через ROS-параметры (обычно в файле конфигурации `ros2_control` или в launch-файле). Поддерживаются:

| Параметр | Тип | Описание | Пример |
|----------|-----|----------|--------|
| `joints` | массив строк | Имена суставов в порядке, соответствующем порядку моторов в аппаратном интерфейсе. | `["shoulder_pan_joint", "shoulder_lift_joint", ...]` |
| `motor_ids` | массив целых чисел | Идентификаторы моторов (ID в протоколе Feetech) для каждого сустава. Должен совпадать по размеру с `joints`. Если не задан, используются индексы 1,2,3,... | `[1, 2, 3, 4, 5, 6]` |
| `interface_names` | массив строк | Какие именно state-интерфейсы читать. По умолчанию: `["position", "velocity", "effort", "temperature", "voltage", "current", "moving_flag"]`. Можно сократить, если нужны не все. | `["position", "temperature", "current"]` |

Пример фрагмента конфигурации для `controller_manager` (файл `soarm101_controllers.yaml`):

```yaml
controller_manager:
  ros__parameters:
    update_rate: 100  # Гц

    soarm101_telemetry_controller:
      type: soarm101_telemetry_controller/ServoTelemetryController
      joints:
        - shoulder_pan_joint
        - shoulder_lift_joint
        - elbow_flex_joint
        - wrist_flex_joint
        - wrist_roll_joint
        - gripper_jaw_joint
      motor_ids: [1, 2, 3, 4, 5, 6]
      state_interfaces:
        - position
        - velocity
        - effort
        - temperature
        - voltage
        - current
        - moving_flag
```

---

## Публикуемый топик

- **Топик:** `~/motor_states` (при использовании внутри компонента будет резолвиться как `<node_namespace>/motor_states`).
- **Тип:** `soarm101_interfaces/msg/MotorStates`.
- **QoS:** `best_effort`, `SystemDefaultQoS`.

Сообщение содержит массив структур `MotorState` для каждого сустава, где каждое поле соответствует значению из state-интерфейса.

---

## Использование в системе

Контроллер активируется вместе с другими контроллерами через `controller_manager`. Обычно он запускается в launch-файле `soarm101_bringup`.

После активации в топике `/soarm101_telemetry_controller/motor_states` появляются данные, которые можно просматривать:

```bash
ros2 topic echo /soarm101_telemetry_controller/motor_states
```

---

## Дальнейшее развитие

В пакете присутствует файл `IMPROVEMENTS.md`, в котором перечислены возможные улучшения:
- Публикация стандартного `sensor_msgs/JointState` для совместимости.
- Фильтрация данных (сглаживание).
- Настраиваемая частота публикации.
- Выборочная публикация полей.
- Диагностика и мониторинг пределов.
- Обработка ошибок и переподключение.
- Агрегированная статистика.
- Сервисы управления.
- Сохранение в файл.
- Преобразование в одометрию (forward kinematics).

Приоритетные улучшения отмечены в документе.

---

## Зависимости

- `controller_interface` – базовый класс для контроллеров ROS2.
- `pluginlib` – регистрация плагина.
- `rclcpp`, `rclcpp_lifecycle` – жизненный цикл.
- `realtime_tools` – вспомогательные утилиты (не используется явно, но указано в package.xml).
- `soarm101_interfaces` – кастомные сообщения.

---

## Лицензия

Пакет распространяется под лицензией **MIT** (см. файл [LICENSE](LICENSE) в корне пакета).

---

## Поддержка

Вопросы и предложения оформляйте через [Issues](https://github.com/cyberbanana777/so-arm101-ros2-pkgs/issues).