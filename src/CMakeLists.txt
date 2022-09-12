add_executable(jacscript
        main.c
        dmesg.c
        platform.c
        pins.c
        tim.c
        uart.c
        dma.c
        flash.c
        hid.c
        usb.c
        usb_descriptors.c

        "../build/version.c"
)

pico_generate_pio_header(jacscript ${CMAKE_CURRENT_LIST_DIR}/sws.pio)

include_directories(AFTER
        "."
)


# pull in common dependencies
target_link_libraries(jacscript
        jacdac
        pico_stdlib
        pico_unique_id
        hardware_pwm
        hardware_pio
        hardware_dma
        hardware_irq
        cmsis_core
        tinyusb_device
)

# create map/bin/hex file etc.
pico_add_extra_outputs(jacscript)
