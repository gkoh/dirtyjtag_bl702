cmake_minimum_required(VERSION 3.15)

set(CONFIG_FREERTOS 1)
set(CONFIG_CHERRYUSB 1)
set(CONFIG_CHERRYUSB_DEVICE 1)
set(CONFIG_CHERRYUSB_DEVICE_CDC_ACM 1)

find_package(bouffalo_sdk REQUIRED HINTS $ENV{BL_SDK_BASE})

sdk_add_include_directories(. src)

target_sources(app PRIVATE
    src/usb_descriptors.c
    src/dirtyjtag.c
    src/jtag_gpio.c
    src/uart_bridge.c
)

sdk_set_main_file(src/main.c)

# Board config (boot2 / partition table / factory DTS) is identical to the SDK's
# stock bl702dk, so point bflb_fw_post_proc at that copy instead of duplicating it.
# This property only affects the fw-tools config lookup (bflb_flash.cmake brdcfgdir);
# our custom board.c / linker script still come from src/board/tang_primer_20k.
set_property(GLOBAL PROPERTY FW_TOOL_CUSTOM_BOARD_DIR
             $ENV{BL_SDK_BASE}/bsp/board/bl702dk)

project(dirtyjtag_tangprimer20k)
