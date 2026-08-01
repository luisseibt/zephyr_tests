# SPDX-License-Identifier: Apache-2.0

# 1. Require a modern CMake version (matches Zephyr's requirements)
cmake_minimum_required(VERSION 3.20.0)

# 2. Inject your custom out-of-tree hardware modules BEFORE finding Zephyr.
# This ensures Zephyr's build system discovers your custom_uart Kconfig 
# and CMakeLists.txt during its initial workspace scan.
list(APPEND EXTRA_ZEPHYR_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/../zephyr_modules/custom_uart)


# 3. Pull in the Zephyr OS build system
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

# 4. Name your application project
project(pydrofoil_app)

# 5. Add your source files to the Zephyr 'app' target
target_sources(app PRIVATE src/main.c)