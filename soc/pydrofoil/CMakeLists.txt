# Tell Zephyr to use the standard RISC-V linker script
set(SOC_LINKER_SCRIPT ${ZEPHYR_BASE}/include/zephyr/arch/riscv/common/linker.ld CACHE INTERNAL "")

# Note: We omitted add_subdirectory(common) because your custom 
# bare-metal SoC doesn't need QEMU's extra C initialization code!