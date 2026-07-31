sdk_add_include_directories(.)

target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/board.c)

sdk_set_linker_script(bl702_flash.ld)

# No fw_header.c or -ufw_header: blisp injects its own boot header at flash time.
# The .bin is raw code linked at XIP 0x23000000, like the old bl_mcu_sdk.
