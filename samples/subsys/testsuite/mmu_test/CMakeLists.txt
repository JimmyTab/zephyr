# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(integration)

include_directories(
    ${ZEPHYR_BASE}/kernel/include
    ${ZEPHYR_BASE}/kernel
    ${ARCH_DIR}/${ARCH}/include
)

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})
