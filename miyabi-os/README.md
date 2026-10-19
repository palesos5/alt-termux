# MIYABI OS — 雅びOS

Минималистичная экспериментальная ОС с GUI для рабочих станций **SGI IP32 (Octane/O2)**
и **Cobalt/Qube Loongson** (~2004 г.), т.е. двух японских MIPS-машин того периода.

Идеология: **архитектура «фасад» (façade / microkernel + user-space servers)**
как в Plan 9, и единый GUI-сервер с окнами-каналами как в BeOS.
Никаких современных технологий: без виртуализации, без TCP/IP-стека,
без динамического линковщика, без кэшей и JIT. Только ядро, серверы и каналы.

## Концепция

```
        +---------------------------------------------------+
        |  apps: terminal, draw, file, clock (каждый — клиент) |
        +---------------------------+-----------------------+
        |  gui-server ("kanzashi")  |  fs-server ("kifu")   |  <- серверы в user space
        |  window/compositing       |  файл = канал (9P-lite)|
        +---------------------------+-----------------------+
        |  kernel ("kokoro")        |  libc ("washi")       |  <- минимальное ядро ~2000 строк
        |  IPC-каналы, VM, PRAT     |  syscall trampoline   |
        +---------------------------+-----------------------+
        |  drivers: fb(NEC/Magnum), uart, ide, irq           |  <- все драйверы — серверы, не ядро
        +---------------------------+-----------------------+
        SGI IP32 (R5000/R10000, Magnum3B, 1280x1024-8)
        Cobalt/Qube (Loongson-1, SM712 FB, 800x600-16)
```

Правила «фасада»:

1. В ядре нет ни одной «взрослой» подсистемы: файловая сеть, GUI, драйверы — всё процессы-серверы.
2. Всё общение — **каналы** (`chan_read`/`chan_write`, урезанный 9P из Plan 9).
3. GUI — один сервер `kanzashi`. Окно = файл `/dev/win/<id>/expose|mouse|kbd`.
   Приложение рисует в свой буфер и шлёт команду `damage` — сервер композитит
   в framebuffers на лету, двойной буфер, палитра 8bpp (IP32) / 4bpp grayscale (Qube).
4. Большие модальности (large modality): каждое приложение имеет **три модальных
   слоя интерфейса**, переключаемых клавишей `XButton` мыши (3-кнопочная мышь SGI!):
   - `M0 direct`   — прямое manipulation (рисуем мышью);
   - `M1 panel`    — модальная панель инструментов поверх окна (BeOS-style tracker panel);
   - `M2 command`  — модалка командного языка (Plan 9 rio-style `edit`-строка внизу).
   Модальность не блокирует other windows: сервер шлёт `mode` событие всем подписчикам.

## Дерево исходников

```
miyabi-os/
├── README.md            этот файл
├── ARCHITECTURE.md      детали протоколов каналов и модальностей
├── Makefile             кросс-сборка mips-elf (gcc -march=r5000 / -march=loongson2)
├── toolchain.sh         установка мипсового тулчейна (old-school binutils 2.18)
├── kernel/              kokoro — ядро (boot, PM, VM, каналы, IRQ dispatch)
│   ├── boot.S           старт: SGI ARMON PROM entry / YAMON ELF entry
│   ├── main.c
│   ├── chan.c           каналы (IPC)
│   ├── vm.c             страничная память, MIPS R4k TLB
│   └── sched.c          round-robin, NICE как в BeOS
├── libc/washi.c         syscall trampoline + chan API
├── server/
│   ├── kanzashi.c       GUI-сервер (окна, события, композитинг)
│   ├── kifu.c           файловый сервер (план 9-подобный walk/open/read/write)
│   └── drivers/         fb_ip32.c, fb_qube.c, uart.c, ide.c — серверы-драйверы
├── apps/                терминал, draw, файлик, часы (клиенты kanzashi)
├── fs/                  загрузочный образ (tar-like архив «kifu.img»)
└── build/               скрипты сборки образа ROM/IDE
```

## Сборка

```sh
./toolchain.sh                 # binutils 2.18 + gcc 4.2.4 mips-elf (тогдашние!)
make TARGET=ip32               # SGI Octane/O2
make TARGET=qube               # Cobalt Qube/Loonson
make image TARGET=ip32         # -> build/miyabi-ip32.img (IDE-образ)
```

Запуск на настоящем железе: записать `build/*.img` на IDE-CF-карточку,
на SGI — загружать через `inst` c PROM или по сети TFTP-заменой (serial console).

## Лицензия
BSD-2-clause. Написано «как будто в 2004», с любовью к IRIX и Acme.
