/* miyabi-os/kernel/boot.S — старт на двух MIPS-машинах 2004.
 * SGI IP32: ARMON PROM грузит ECOFF по адресу 0x400000 (A_KSEG0), a0=memsize.
 * Cobalt Qube: YAMON грузит ELF bin, entry 0x80400000, a2=cmdline ptr.
 * Одна и та же точка входа годится для обеих: читаем a0 как memsize. */

        .set      mips3
        .set      noat
        .globl  _start
        .ent    _start

_start:
        /* отключить прерывания и TLB-заполнение в kernel mode */
        mtc0    zero, Cause
        mtc0    zero, Status                /* BEV=0 позже; пока всё off */

        /* SP = фиксированный верх стека ядра */
        la      $sp, kstack_top

        /* a0 = размер памяти от PROM'а (ARMON отдаёт в a0, YAMON в a1) —
         * нормализуем: если a0==0 берём a1, иначе a0. Никакой автодетекти. */
        bnez    $a0, 1f
        move    $a0, $a1
1:
        jal     kmain
        nop

        /* сюда не возвращаемся */
2:      b       2b
        nop

        .end    _start

/* ---- switch_context(old_sp_ptr, new_sp, new_pc) ---- */
        .globl  switch_context
switch_context:
        /* сохраняем callee-saved + ra в текущий стек, записываем sp в *a0 */
        sw      $ra, -4($sp)
        sw      $s0, -8($sp)
        sw      $s1, -12($sp)
        sw      $gp, -16($sp)
        sw      $sp, 0($a0)
        /* восстанавливаем из нового стека */
        move    $sp, $a1
        lw      $ra, -4($sp)
        lw      $s0, -8($sp)
        lw      $s1, -12($sp)
        lw      $gp, -16($sp)
        j       $a2                       /* new_pc; arg уже на вершине стека spawn'а */
        nop

/* ---- syscall trap: sys_syscall(nr,a0,a1,a2,a3) dispatch ---- */
        .globl  t_syscall
t_syscall:
        /* cp0 causes: Context/EntryHi уже трогали; regs на месте */
        mfc0    $k0, Cause
        /* syscall_dispatch(a0=nr? нет: MIPS ABI v0=syscall nr, a0..a3 args) */
        move    $a4, $v0                  /* nr */
        /* вызов dispatch(nr, a0,a1,a2,a3): подвинем: делаем обёртку на C с 5 args
           через стек — gcc n32/o32: a0..a3 + stack. Используем o32: 5-й аргумент в стек. */
        subu    $sp, $sp, 8
        sw      $a3, 4($sp)
        sw      $a2, 0($sp)               /* упрощённо: dispatch читает a2/a3 из стека
                                             (см. main.c: a2,a3 unused anyway) */
        jal     syscall_dispatch_wrap
        nop
        addu    $sp, $sp, 8
        mtc0    $v0, EPC                  /* вернуть в user pc+8 (delay slot пропущен) */
        /* restore user regs & eret делает trap_low epilogue */
        j       t_ret
        nop

syscall_dispatch_wrap:
        /* nr в $a4 -> переложить в a0, остальные сдвинуть (o32 5-arg hack) */
        move    $t0, $a0
        move    $a0, $a4
        move    $a1, $t0
        jr      syscall_dispatch
        nop

/* ---- TLB refill: крошечный путь, identity map низких 16MB ---- */
        .align  5
        .globl  t_tlbrefill
t_tlbrefill:
        mfc0    $k0, BadVAddr             /* виноватый адрес */
        li      $k1, 0xFFF00000
        and     $k1, $k0, $k1             /* vpn2: страница выровнена */
        ori     $k1, $k1, 0x00000080      /* ASID=0 (UP, один адресный мир) */
        mtc0    $k1, EntryHi
        li      $k1, 0x00FFFFFF
        and     $k1, $k0, $k1             /* phys == младшие биты (identity) */
        ori     $k1, $k1, 0x3F            /* EntryLo: V|D|M|C0..C3 cached */
        mtc0    $k1, EntryLo0
        mtc0    $zero, Index              /* перезаписать случайный entry: нас 16, хватает */
        tlbwi
        eret

        .section .bss
        .align  4
kstack:
        .space  8192
kstack_top:
