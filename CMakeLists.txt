cmake_minimum_required(VERSION 3.20.0)
set(KCONFIG_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/Kconfig)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(zephyr_psoc6)
# This tells Zephyr where to find your custom Kconfig definitions
target_sources(app PRIVATE src/main.c src/wifi_module.c src/mqtt_module.c)