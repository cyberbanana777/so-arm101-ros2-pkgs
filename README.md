# SOARM101 ROS2 Workspace

Репозиторий содержит программный стек для взаимодействия с роботизированной рукой **SOARM101** в среде ROS2 Humble. Решение построено на базе фреймворка `ros2_control`, позволяет управлять физическими сервоприводами, интегрировано с MoveIt2 и предоставляет расширенную телеметрию. Для калибровки используются оригинальные калибровочные файлы lerobot и переводятся в кастомный формат.  

> **Документация двухуровневая:** в каждом пакете присутствует собственная документация, дополняющая этот общий README.


---

## Структура репозитория

| Пакет | Назначение |
|-------|------------|
| **`soarm101_interfaces`** | Кастомные сообщения для телеметрии моторов (`MotorState`, `MotorStates`). |
| **`soarm101_description`** | URDF-описание робота, геометрия, кинематика, визуализация (STL-меши). |
| **`soarm101_hardware`** | Аппаратный компонент `ros2_control` для управления сервоприводами через SDK. |
| **`soarm101_ros2_control`** | Конфигурационные файлы `ros2_control` (реальный робот и симуляция). |
| **`soarm101_telemetry_controller`** | Кастомный контроллер для публикации расширенной телеметрии (температура, напряжение, ток, флаг движения). |
| **`soarm101_moveit_config`** | Конфигурация MoveIt2 для планирования траекторий. |
| **`soarm101_bringup`** | Пакет-агрегатор для единого запуска всей системы (реальный робот или Gazebo). |
| **`soarm101_examples`** | Примеры узлов для управления суставами и схватом через action-интерфейсы. |
| **`converter_calibration_data`** | Инструмент для конвертации калибровочных данных из формата lerobot в кастомный YAML. |
| **`scservo_sdk`** | SDK для работы с сервоприводами Feetech (SCSCL, SMS_STS, HLSCL, SCS0009). |

---

## Требования к системе

- **Операционная система**: Ubuntu 22.04 (рекомендуется)  
- **ROS2**: Humble (протестировано на Humble)  
- **Компилятор**: C++17, Python 3.8+  
- **Аппаратное обеспечение**: роборука SOARM101 (подключённая по USB/последовательному порту)  
- **Дополнительно**:  
  - Установленный python3-пакет `lerobot` (для калибровки) – см. [huggingface/lerobot](https://github.com/huggingface/lerobot)  
  - Для работы с MoveIt2: `moveit2`, `moveit_visual_tools` и др. (будут добавлены в зависимости позже)

---

## Процесс развёртывания и запуска

1. **Установите lerobot** (следуйте инструкциям официального репозитория) [Ссылка на инструкцию](https://huggingface.co/docs/lerobot/installation).  
2. **Включите робота** и подключите к компьютеру.  
3. **Проведите калибровку** согласно [официальлной инструкции lerobot](https://huggingface.co/docs/lerobot/so101?calibrate_leader=Command)
4. Создайте рабочее пространство и скачайте наш репозиторий.
   ```bash
   mkdir -p so-arm101-ros2-pkgs_ws/src
   cd so-arm101-ros2-pkgs_ws/src
   git clone https://github.com/cyberbanana777/so-arm101-ros2-pkgs.git .
   ```  
5. **Преобразуйте калибровочные данные** в нужный формат:  
   ```bash
      cd converter_calibration_data
      pip install -r pip_requirements.txt
      python3 lerobot_to_custom_format.py </global/path/to/lerobot_calib.json> ../config/motor_calibration.yaml
   ```    
6. **Соберите workspace** (из корня рабочей области):  
   ```bash 
   cd ../..
   colcon build
   ```  
7. **Запустите полный стек** через bringup:  
   ```bash
   source install/setup.bash
   ros2 launch soarm101_bringup bringup.launch.py use_sim:=false
   ```
   Для симуляции в Gazebo замените `use_sim:=false` на `use_sim:=true`.

---

## Внешние интерфейсы стека (Public API)

После запуска системы внешние пользователи и узлы могут взаимодействовать с роботом через следующие ROS2-интерфейсы:

**Топики (Topics):**
- `/joint_states` (`sensor_msgs/msg/JointState`) — текущие положения, скорости и усилия всех сочленений (публикуется стандартным контроллером).
- `/soarm101_telemetry_controller/motor_states` (`soarm101_interfaces/msg/MotorStates`) — расширенные данные телеметрии: температура сервоприводов, напряжение, флаги ошибок (публикуется кастомным телеметрийным контроллером).
- `/tf` — трансформации между звеньями робота (для визуализации в RViz и навигации).

**Действия (Actions):**
- `/joint_trajectory_controller/follow_joint_trajectory` (стандартный action из `control_msgs`) — основной интерфейс для выполнения траекторий (используется MoveIt2 и примерами).
- `/gripper_controller/gripper_cmd (control_msgs/action/GripperCommand)` — управление схватом.

Примечание: точные названия топиков и типов сообщений могут уточняться в документации к соответствующим пакетам.

> **Примечание:**  точные названия топиков и типов сообщений могут уточняться в документации к соответствующим пакетам.

---

## Ссылки на источники и заимствования

- **lerobot** – оригинальная библиотека для калибровки и управления роботами: https://github.com/huggingface/lerobot

- **SC-SERVO SDK** – адаптирован из репозитория [adityakamath/SCServo_Linux](https://github.com/adityakamath/SCServo_Linux)

- **MoveIt2** – фреймворк для планирования движений: https://moveit.ai/

- **TheRobotStudio/SO-ARM100** – оригинальный URDF и меши: https://github.com/TheRobotStudio/SO-ARM100

---

## Лицензия

Распространяется под лицензией **MIT** (см. [LICENSE](LICENSE) в корне репозитория) (если не указано иное в отдельных пакетах).

---

## Контакты и поддержка

- Вопросы и предложения оформляйте через [Issues](../../issues).  

---



## Полезные ссылки

- Приложение для отладки Feetech-сервоприводов (Qt): [FT_SCServo_Debug_Qt](https://github.com/Kotakku/FT_SCServo_Debug_Qt)