# west build command
cd ~/thesis/zephyrproject/zephyr
west build -b pydrofoil_32 /home/seibt/thesis/zephyr_tests/pydrofoil_app --pristine -DBOARD_ROOT=/home/seibt/thesis/zephyr_tests -DSOC_ROOT=/home/seibt/thesis/zephyr_tests -d /home/seibt/thesis/zephyr_tests/benchmark/build
# gdb command:
cd /home/seibt/thesis/multicore_test/zephyr_testing/benchmark
gdb-multiarch -x gdb_cmd.gdb build/zephyr/zephyr.elf

