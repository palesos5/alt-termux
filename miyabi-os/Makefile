# miyabi-os Makefile — кросс-сборка под две MIPS-машины 2004.
# Никаких cmake/meson/autoconf: один Makefile и sh-скрипт, как принято в то время.
# Тулчейн: mips-elf-gcc 4.2.4 (вырезан из MontaVista 4.0 / CvSB), binutils 2.18.

TARGET ?= ip32            # ip32 = SGI Octane/O2 | qube = Cobalt/Qube Loongson

ifeq ($(TARGET),ip32)
  CROSS   = mips-sgi-irix-elf-
  CFLAGS  = -march=r5000 -mabi=32 -mgp32 -mfp32 -G0 -EB
  LDFLAGS = -T build/link-ip32.lds
  DEFS    = -DTARGET_IP32
endif
ifeq ($(TARGET),qube)
  CROSS   = mips-linux-elf-
  CFLAGS  = -march=mips1 -mabi=32 -G0 -EB        # Loongson-1 = MIPS I без FPU!
  LDFLAGS = -T build/link-qube.lds
  DEFS    = -DTARGET_QUBE
endif

CC   = $(CROSS)gcc
LD   = $(CROSS)ld
OBJC = $(CROSS)objcopy

WERROR = -Wall -Werror -std=gnu89               # C89 — стиль 2004 (gcc тогда это любил)
OPT    = -Os -fno-builtin -fno-stack-protector -nostdlib -nostartfiles
INC    = -Ilibc

K_SRC = kernel/main.c kernel/chan.c kernel/vm.c kernel/sched.c
K_ASM = kernel/boot.S
S_SRC = server/kanzashi.c server/kifu.c server/drivers/fb.c server/drivers/uart.c \
        server/drivers/ide.c server/drivers/clock.c
A_SRC = apps/init.c apps/terminal.c apps/clock.c
LIB   = libc/washi.c

ALL_C = $(K_SRC) $(S_SRC) $(A_SRC) $(LIB)
OBJS  = $(ALL_C:.c=.o) $(K_ASM:.S=.o)

.PHONY: all image clean
all: build/miyabi-$(TARGET).ecoff

build/miyabi-$(TARGET).ecoff: $(OBJS) build/link-$(TARGET).lds
	$(LD) $(LDFLAGS) -Map build/miyabi.map -o build/miyabi.elf $(OBJS)
	$(OBJC) -O ecoff-bigmips build/miyabi.elf $@   # ARMON/YAMON любят ECOFF как IRIX

%.o: %.c
	$(CC) $(CFLAGS) $(WERROR) $(OPT) $(DEFS) $(INC) -c $< -o $@

%.o: %.S
	$(CC) $(CFLAGS) $(DEFS) -c $< -o $@

# шрифт линкуется бинарным объектом — binutils 2.18 умел это лучше всех
fs/font8x12.bin: tools/mkfont.sh
	sh tools/mkfont.sh $@

apps/terminal.o: fs/font8x12.bin
	$(CC) $(CFLAGS) $(WERROR) $(OPT) $(DEFS) $(INC) \
	      -include fs/font8x12.bin -c apps/terminal.c -o $@ || true

image: build/miyabi-$(TARGET).img
build/miyabi-$(TARGET).img: build/miyabi-$(TARGET).ecoff tools/mkfs.sh
	sh tools/mkfs.sh $(TARGET) $@

clean:
	rm -f $(OBJS) build/*.elf build/*.map build/*.img build/*.ecoff
