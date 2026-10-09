# Building and Debugging zephyr.elf

This guide describes how to build the Zephyr application for the `pydrofoil_32` board and debug it using GDB.

### 1. Activate Zephyr Virtual Environment
Before running any Zephyr commands, activate the project's Python virtual environment.
```bash
cd <path-to-zephyrproject>
source .venv/bin/activate
```
### 2. Build the Application
Use west to compile the application. This command specifies the custom board and SoC roots and outputs the build files to the benchmark directory.
Note: The specific Zephyr configuration is located in the benchmark folder.

```bash
cd zephyr
west build -b pydrofoil_32 <path-to-zephyr_tests>/pydrofoil_app \
  --pristine \
  -DBOARD_ROOT=<path-to-zephyr_tests> \
  -DSOC_ROOT=<path-to-zephyr_tests> \
  -d <path-to-zephyr_tests>/benchmark/build
```

### 2b. Build the IS benchmark for all classes and core counts
`build_is.sh` builds `pydrofoil_app` for every problem class and core count and
copies the ELFs to `vcml-pydrofoil/benchmark/zephyr_is/build/` as
`zephyr_is_<class>_<n>cores.elf`. Per build it sets the core count for the
devicetree (`NRCPU`), Kconfig (`CONFIG_MP_MAX_NUM_CPUS`) and `main.c`
(`NUM_CORES`); the build directories are kept in `benchmark/build_is/`.
```bash
./build_is.sh                      # classes M S P W A, 1/2/4/8 cores
./build_is.sh -c "S W" -n "1 4"    # a subset
```
A single build can set the same with `-- -DIS_CLASS=W -DIS_NUM_CORES=4
-DCONFIG_MP_MAX_NUM_CPUS=4 -DDTS_EXTRA_CPPFLAGS=-DNRCPU=4` at the end of the
`west build` command above.

### 3. Debug the Application
```bash
cd <path-to-zephyr_tests>/benchmark
gdb-multiarch -x gdb_cmd.gdb build/zephyr/zephyr.elf
```

### On my machine
west build command
```bash
cd ~/thesis/zephyrproject/zephyr
west build -b pydrofoil_32 /home/seibt/thesis/zephyr_tests/pydrofoil_app --pristine -DBOARD_ROOT=/home/seibt/thesis/zephyr_tests -DSOC_ROOT=/home/seibt/thesis/zephyr_tests -d /home/seibt/thesis/zephyr_tests/benchmark/build
# gdb command:
cd ~/thesis/zephyr_tests/benchmark
gdb-multiarch -x gdb_cmd.gdb build/zephyr/zephyr.elf
```

