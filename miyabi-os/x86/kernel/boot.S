/* miyabi-os/x86/kernel/boot.S — старт x86-32 с BIOS (protected mode).
 * Точка входа — start32: legacy GRUB (0.95, эпоха 2004) грузит ELF-образ в
 * 0x100000 и прыгает сюда уже в protected mode с flat-сегментами. Никакого
 * multiboot-заговора и UEFI: только защищённый режим, PIC 8259A, PIT, CMOS.
 * Память определяем по CMOS (port 0x70/0x71), как делали мелкие ОС 2004 года. */

        .text
        .code32
        .globl start32

start32:
        cli                           /* мир ещё не готов к прерываниям */

        /* свой GDT вместо чужого: фасад не доверяет загрузчику */
        lgdt gdtr

        movw $0x10, %ax               /* data selector ring0 */
        movw %ax, %ds
        movw %ax, %es
        movw %ax, %ss
        xorw %ax, %ax
        movw %ax, %gs

        /* стек ядра: фиксированный верх kstack_top (см. линкер) */
        movl $kstack_top, %esp

        /* memsize из CMOS: bytes 0x15/0x16 = base memory в KB +
         * bytes 0x30/0x31 = extended memory в KB (>1MB). as in 2004. */
        movb $0x15, %al
        outb %al, $0x70
        inb  $0x71, %al
        movzbl %al, %esi
        movb $0x16, %al
        outb %al, $0x70
        inb  $0x71, %al
        movzbl %al, %edi
        shll $8, %edi
        orl  %edi, %esi               /* esi = base memory KB */
        movb $0x30, %al
        outb %al, $0x70
        inb  $0x71, %al
        movzbl %al, %edi
        movb $0x31, %al
        outb %al, $0x70
        inb  $0x71, %al
        movzbl %al, %ecx
        shll $8, %ecx
        orl  %ecx, %edi               /* edi = extended KB */
        shll $10, %edi                /* -> байты выше 1MB */
        addl %edi, %esi               /* esi = memtop */

        pushl %esi
        call paging_init              /* страницы ядра: identity low RAM + VGA + MMIO */
        call idt_init                 /* IDT: trap gates на t_syscall/t_irq* */
        popl %eax                     /* memsize для kmain */

        call kmain                    /* сюда не возвращаемся */

hltloop:
        hlt
        jmp hltloop

/* ---- switch_context(&old_esp, new_esp) — cdecl, 2 аргумента ----
 * Механика ret-based (классика i386-ядер): сохраняем callee-saved в текущий
 * стек, пишем esp в *old, восстанавливаем новый esp, pop'ы возвращают регистры,
 * затем RET берёт адрес с НОВОГО стека:
 *   - resume блокнутого процесса: на вершине его kstack лежит ret-адрес вызова
 *     switch_context внутри sched_tick -> исполнение продолжается после call ->
 *     return в block_on -> chan_read -> iretd в user;
 *   - первый вход: spawn_proc подкладывает на дно kstack адрес proc_entry,
 *     который сам собирает iretd-фрейм ring3. */
        .globl switch_context
switch_context:
        pushl %ebp
        pushl %ebx
        pushl %esi
        pushl %edi
        movl 20(%esp), %eax           /* old_esp_ptr: ret+4push+arg0 */
        movl %esp, (%eax)             /* сохранить sp ПОСЛЕ push'ов */
        movl 24(%esp), %esp           /* new esp: arg1 */
        popl %edi
        popl %esi
        popl %ebx
        popl %ebp
        ret

/* ---- syscall шлюз: int $0x40, eax=nr, args ebx,ecx,edx,esi ---- */
        .globl t_syscall
t_syscall:
        /* фрейм iretd снизу вверх: eip, cs, eflags, uesp, ss (ring change) */
        pushal                        /* edi..eax; eax на вершине (esp+0) */
        /* dispatch(nr, a0,a1,a2,a3): nr=eax, args ebx,ecx,edx,esi */
        movl (%esp), %eax             /* nr */
        pushl %esi                    /* a3 */
        pushl %edx                    /* a2 */
        pushl %ecx                    /* a1 */
        pushl %ebx                    /* a0 */
        pushl %eax                    /* nr первым */
        call syscall_dispatch
        addl $20, %esp
        movl %eax, (%esp)             /* результат поверх сохранённого eax (esp+0 после pushal) */
        popal
        iretd

/* ---- IRQ трамплины: линия параметром, общий epilogue с EOI ---- */
.macro IRQENTRY n
        .globl t_irq\n
t_irq\n:
        pushl $\n
        jmp irq_common
        .endm

irq_common:
        pushal                        /* все regs (2004 стиль: лениво и честно) */
        pushl 32(%esp)                /* line: поверх сохранённых regs для cdecl */
        call irq_dispatch
        addl $4, %esp
        popal
        iretd

        IRQENTRY 0                    /* PIT timer   -> clock server */
        IRQENTRY 1                    /* keyboard    -> console server */
        IRQENTRY 5                    /* aux / spare */
        IRQENTRY 6                    /* floppy      (не используем) */
        IRQENTRY 7                    /* LPT2        (не используем) */
        IRQENTRY 12                   /* PS/2 mouse  -> kanzashi */
        IRQENTRY 14                   /* primary IDE -> ide server */

/* ---- GDT: flat kernel + flat user + TSS slot ---- */
        .section .data
        .align 4
gdtr:
        .word gdtend - gdt - 1
        .long gdt
gdt:
        .quad 0                         /* 0x00 null */
        .quad 0x00CF9A000000FFFF        /* 0x08 code ring0 */
        .quad 0x00CF92000000FFFF        /* 0x10 data ring0 */
        .quad 0x00CFFA000000FFFF        /* 0x18 code ring3 */
        .quad 0x00CFF2000000FFFF        /* 0x20 data ring3 */
        .quad 0                           /* 0x28 TSS (low) — заполняет idt_init */
        .quad 0                           /* 0x30 TSS (high) */
gdtend:

/* ---- TSS: нужен только для esp0 (int-from-ring3) ---- */
        .section .bss
        .align 4
        .globl tss
tss:
        .space 104

        .section .bss
        .align 4
kstack:
        .space 16384
        .globl kstack_top
kstack_top:
