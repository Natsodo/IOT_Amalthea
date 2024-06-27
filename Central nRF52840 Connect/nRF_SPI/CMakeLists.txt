cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(nRF_SPI)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(gpio_custom_dts_binding)
zephyr_library_include_directories(include)

FILE(GLOB app_sources src/*.c)

target_sources(app PRIVATE
  src/main.c
  src/env_client.c
  src/spi_communication.c
)
