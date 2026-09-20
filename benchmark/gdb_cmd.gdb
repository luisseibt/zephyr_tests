# Run with gdb-multiarch -x gdb_cmd.gdb <path to the elf>
set architecture riscv:rv32
target remote :5555
#b /home/seibt/thesis/zephyrproject/zephyr/arch/riscv/core/reset.S:199
#b /home/seibt/thesis/zephyrproject/zephyr/arch/riscv/core/prep_c.c:49
#b /home/seibt/thesis/zephyrproject/zephyr/include/zephyr/arch/riscv/arch_inlines.h:34
#b /home/seibt/thesis/zephyrproject/zephyr/kernel/init.c:230
#b /home/seibt/thesis/zephyrproject/zephyr/kernel/init.c:576
b /home/seibt/thesis/zephyrproject/zephyr/drivers/timer/riscv_machine_timer.c:161
b /home/seibt/thesis/zephyrproject/zephyr/drivers/timer/riscv_machine_timer.c:65
b /home/seibt/thesis/zephyrproject/zephyr/drivers/timer/riscv_machine_timer.c:66
b /home/seibt/thesis/zephyrproject/zephyr/drivers/timer/riscv_machine_timer.c:67
b /home/seibt/thesis/zephyrproject/zephyr/arch/riscv/core/isr.S:811
b /home/seibt/thesis/zephyrproject/zephyr/arch/riscv/core/isr.S:382