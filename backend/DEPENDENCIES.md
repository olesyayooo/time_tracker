# Зависимости бэкенда

Основной вариант — API и PostgreSQL через Docker Compose. Для него достаточно
Docker Desktop/Engine с Compose и Git: перечисленные ниже C++-зависимости
автоматически устанавливаются при сборке образа. Тестовый образ также содержит
Python 3 и PostgreSQL-клиент; устанавливать их на компьютер не нужно.

Ниже — зависимости для альтернативной локальной сборки бэкенда на macOS.

| Зависимость | Назначение | Как подключается |
|---|---|---|
| Компилятор C++20 | Сборка сервера | Xcode Command Line Tools на macOS |
| CMake 3.24+ | Конфигурация сборки | Установить локально |
| Git | Загрузка исходников Drogon и подмодулей | Установить локально |
| Drogon 1.9.11 | HTTP-сервер, JSON, работа с БД | CMake скачивает и собирает автоматически |
| libpq | Клиентская библиотека PostgreSQL | Установить локально до сборки Drogon |
| JsonCpp | JSON для Drogon | Установить локально |
| OpenSSL | Зависимость сетевого стека | Установить локально |
| zlib | Зависимость Drogon | Предоставляется macOS SDK |
| Docker Desktop с Compose | Запуск PostgreSQL 16 | Установить и запустить приложение |

Trantor загружается как подмодуль Drogon. Ставить PostgreSQL-сервер на компьютер не нужно.
Для первой конфигурации CMake нужен интернет. Каталог build/_deps содержит загруженные зависимости.

## macOS / Homebrew

Установить Xcode Command Line Tools, если ещё нет:

```sh
xcode-select --install
```

Установить библиотеки и инструменты:

```sh
brew install cmake git libpq jsoncpp openssl@3 libsodium
```

Docker Desktop устанавливается отдельно с https://www.docker.com/products/docker-desktop/.
Если он уже установлен, достаточно запустить приложение.

Drogon отдельно через brew устанавливать не нужно: проект собирает закреплённую версию
с поддержкой PostgreSQL. CMake получает путь к libpq через команду из README.

## Авторизация

libsodium используется для Argon2id. jwt-cpp 0.7.1 автоматически скачивается CMake,
проверяет и подписывает access-токены HS256 через OpenSSL 3.
В настройке CMake указывайте путь к openssl@3: установленная параллельно OpenSSL 4
несовместима с закреплённой версией сетевого стека Drogon.
Qt относится к десктопному клиенту и для бэкенда не нужен.

## Источники

- Drogon: https://github.com/drogonframework/drogon/wiki/ENG-02-Installation
- libpq: https://formulae.brew.sh/formula/libpq
- CMake FetchContent: https://cmake.org/cmake/help/latest/module/FetchContent.html
