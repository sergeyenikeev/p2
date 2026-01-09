# P2 Screenshot (Windows 11)

Консольная утилита на C++ для быстрого захвата всех дисплеев в JPG с максимальным сжатием.
Основной путь: Desktop Duplication (DXGI/D3D11), при ошибке используется резервный путь на GDI.

## Сборка (Visual Studio 2026)

1) `cmake -S . -B build -G "Visual Studio 18 2026" -A x64`
2) `cmake --build build --config Release`

## Запуск

`p2_screenshot --out "D:\\Screens"`

Если `--out` не указан, используется текущая папка запуска.

### Структура вывода

```
<root>\<PC_USER>\<YYYY-MM>\<YYYY-MM-DD>\
```

### Имена файлов

`<ИмяКомпьютера>_<ИмяПользователя>_<YYYY-MM-DD>_<HH-MM-SS>.jpg`

Если дисплеев несколько:

`..._DisplayNN.jpg` (NN = 01, 02, 03...)

### Лог-файл

`<root>\<PC_USER>\<YYYY-MM>\<YYYY-MM-DD>\<YYYY-MM-DD>.log`

## Параметры

- `--out <путь>` — корневая папка сохранения (необязательный; по умолчанию текущая папка).
- `--test-image` — синтетические кадры вместо реального захвата (для тестов/CI).
- `--simulate-displays N` — количество синтетических дисплеев (включает `--test-image`).

## Проверка тестов

`ctest --test-dir build -C Release --output-on-failure`