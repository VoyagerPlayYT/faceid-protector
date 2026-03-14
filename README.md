# 🔐 FaceID Protector

**🇷🇺 Русский** | **🇬🇧 English**

---

## 🇷🇺 Описание

Программа для защиты компьютера с помощью распознавания лица. Если система не видит владельца перед камерой — компьютер автоматически блокируется. При попытке взлома отправляет фото злоумышленника в Telegram.

## 🇬🇧 Description

A desktop security tool that uses face recognition to protect your computer. If the system does not detect the authorized face, the computer locks automatically. On failed login attempts, it sends the intruder's photo to Telegram.

---

## 📌 Возможности / Features

🇷🇺 | 🇬🇧
---|---
🔐 Блокировка при отсутствии лица | 🔐 Locks when no face is detected
🎥 Распознавание через веб-камеру | 🎥 Webcam face recognition
⚡ Работа в реальном времени | ⚡ Real-time processing
🧠 OpenCV LBPH алгоритм | 🧠 Powered by OpenCV LBPH
🖥 Поддержка Windows 10/11 | 🖥 Windows 10/11 support
🚀 Автозапуск с системой | 🚀 Auto-start with Windows
🤖 Управление через Telegram | 🤖 Remote control via Telegram bot
🌐 Веб-панель в реальном времени | 🌐 Real-time web dashboard
🚨 Режим Паника | 🚨 Panic mode
💻 Удалённый диспетчер задач | 💻 Remote task manager
📁 Файловый менеджер | 📁 File manager
🔍 Поиск файлов на ПК | 🔍 File search on PC
🎙 Запись аудио с микрофона | 🎙 Microphone audio recording
🔌 Блокировка USB-портов | 🔌 USB port blocking
📷 Авто-скриншоты | 📷 Auto-screenshots
🔒 Зашифрованный UUID | 🔒 Encrypted device UUID
🔑 Одноразовый токен подключения | 🔑 One-time connection token

---

## 🤖 Telegram Bot

**[@FaceIDProtectorbot](https://t.me/FaceIDProtectorbot)**

---

## ⬇️ Скачать / Download

[**⬇️ FaceIDProtector.msi — latest release**](https://github.com/VoyagerPlayYT/faceid-protector/releases/latest)

---

## 🛠 Установка / Installation

🇷🇺
1. Скачайте файл `.msi`
2. Откройте установщик
3. Следуйте инструкциям
4. После установки программа запустится автоматически

🇬🇧
1. Download the `.msi` installer
2. Run the installer
3. Follow the setup instructions
4. The program will start automatically after installation

---

## ⚙️ Как работает / How It Works

🇷🇺 Программа использует камеру компьютера для анализа изображения в реальном времени. Если лицо пользователя не обнаружено в течение определённого времени — система блокирует Windows и уведомляет владельца через Telegram.

🇬🇧 The program uses your computer's camera to analyze the video feed in real time. If the user's face is not detected for a certain period, the system locks Windows and notifies the owner via Telegram.

---

## 🔒 Безопасность / Security

🇷🇺
- UUID устройства зашифрован ключом вашей машины
- Подключение только через одноразовый токен (16 символов)
- Верификация через 16-символьный код в Telegram
- Программа не отправляет данные на сторонние серверы
- Все вычисления выполняются локально
- Видео с камеры не сохраняется

🇬🇧
- Device UUID is encrypted with your machine key
- Connection only via one-time token (16 characters)
- Verification via 16-character Telegram code
- No data is sent to third-party servers
- All processing happens locally
- Camera footage is not stored

---

## 📦 Технологии / Technologies

- **C++20** — основное приложение / main application
- **Qt 6** — графический интерфейс / GUI
- **OpenCV 4** — распознавание лица / face recognition
- **Windows API** — системные функции / system functions
- **WinHTTP** — HTTP клиент / HTTP client
- **Python** — Telegram бот / Telegram bot
- **aiohttp** — веб-сервер бота / bot web server

---

## ⚠️ Требования / Requirements

- Windows 10 / Windows 11 (x64)
- Веб-камера / Webcam
- Права администратора / Administrator rights
- Интернет для Telegram / Internet for Telegram

---

## 🧪 Статус / Project Status

🇷🇺 Проект в активной разработке. Новые функции и улучшения добавляются регулярно.

🇬🇧 The project is under active development. New features and improvements are added regularly.

---

## 🤝 Участие / Contributing

🇷🇺
1. Сделайте Fork
2. Создайте новую ветку (`git checkout -b feature/name`)
3. Внесите изменения
4. Откройте Pull Request

🇬🇧
1. Fork the repository
2. Create a new branch (`git checkout -b feature/name`)
3. Make your changes
4. Open a Pull Request

---

## 📜 Лицензия / License

MIT License — see [LICENSE](LICENSE)

---

## 👤 Автор / Author

Created by **Asadbek**

- GitHub: [@VoyagerPlayYT](https://github.com/VoyagerPlayYT)
- Telegram Bot: [@FaceIDProtectorbot](https://t.me/FaceIDProtectorbot)
