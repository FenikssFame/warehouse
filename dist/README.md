# Склад магазина — готовая сборка

Вариант 3: C++20, Win32 GUI, PostgreSQL. Суммы — в белорусских рублях (BYN, обозначение Br).

## Запуск

1. Создайте базу и примените схему:

```powershell
$pg = "C:\Program Files\PostgreSQL\16\bin"
$env:Path = "$pg;$env:Path"

createdb -U postgres warehouse
psql -U postgres -d warehouse -v ON_ERROR_STOP=1 -f .\db\schema.sql
psql -U postgres -d warehouse -v ON_ERROR_STOP=1 -f .\db\seed.sql
```

2. Запустите `WarehouseApp.exe`. При первом запуске откроется диалог
   подключения; параметры сохранятся в `warehouse.ini` рядом с `.exe`.

3. Либо скопируйте `warehouse.ini.example` в `warehouse.ini` и заполните
   параметры вручную.

## Что внутри

- `WarehouseApp.exe` — приложение;
- `libpq.dll`, `libssl-3-x64.dll`, `libcrypto-3-x64.dll`, `libintl-9.dll`,
  `libiconv-2.dll`, `libwinpthread-1.dll` — клиент PostgreSQL;
- `db/schema.sql`, `db/seed.sql` — схема и демонстрационные данные;
- `warehouse.ini.example` — пример настроек.

Пароль в `warehouse.ini` хранится в открытом виде; это учебная сборка.

Исходный код, UML-диаграммы и инструкция по сборке находятся в исходном
архиве проекта.
