obj-$(CONFIG_HPET_TIMER) += hpet.o
obj-$(CONFIG_LOAPIC_TIMER) += loapic_timer.o
obj-$(CONFIG_ARCV2_TIMER) += arcv2_timer0.o
obj-$(CONFIG_NIOS2_AVALON_TIMER) += nios2_avalon_timer.o
obj-$(CONFIG_RISCV_SYSTIMER) += riscv_systimer.o

_CORTEX_M_SYSTICK_AND_GDB_INFO_yy = y
obj-$(CONFIG_CORTEX_M_SYSTICK) += cortex_m_systick.o
obj-$(_CORTEX_M_SYSTICK_AND_GDB_INFO_$(CONFIG_CORTEX_M_SYSTICK)$(CONFIG_GDB_INFO)) += cortex_m_systick_gdb.o

obj-y += sys_clock_init.o
