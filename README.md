## Getting the zephyr.elf 
# Activate Zephyr venv
cd <path-to-zephyrproject>
source .venv/bin/activate

# Build the application
cd zephyr
west build -b pydrofoil_32 <path-to-zephyr_tests>/pydrofoil_app \
  --pristine \
  -DBOARD_ROOT=<path-to-zephyr_tests> \
  -DSOC_ROOT=<path-to-zephyr_tests> \
  -d <path-to-zephyr_tests>/benchmark/build

Please find the zephyr config in the benchmark folder

# Debug zephyr app
cd <path-to-zephyr_tests>/benchmark
gdb-multiarch -x gdb_cmd.gdb build/zephyr/zephyr.elf


Just for me: 
# west build command
cd ~/thesis/zephyrproject/zephyr
west build -b pydrofoil_32 /home/seibt/thesis/zephyr_tests/pydrofoil_app --pristine -DBOARD_ROOT=/home/seibt/thesis/zephyr_tests -DSOC_ROOT=/home/seibt/thesis/zephyr_tests -d /home/seibt/thesis/zephyr_tests/benchmark/build
# gdb command:
cd /home/seibt/thesis/multicore_test/zephyr_testing/benchmark
gdb-multiarch -x gdb_cmd.gdb build/zephyr/zephyr.elf

