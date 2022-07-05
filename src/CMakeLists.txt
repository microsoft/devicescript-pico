add_executable(jacscript
        main.c
        dmesg.c
        platform.c
        pins.c
        tim.c
        uart.c
        dma.c
        flash.c
        hf2.c
        hid.c
        usb.c
        usb_descriptors.c

        "../build/version.c"
        "../jacdac-c/source/jd_control.c"
        "../jacdac-c/source/jd_ipipe.c"
        "../jacdac-c/source/jd_opipe.c"
        "../jacdac-c/source/jd_physical.c"
        "../jacdac-c/source/jd_protocol.c"
        "../jacdac-c/source/jd_queue.c"
        "../jacdac-c/source/jd_send_util.c"
        "../jacdac-c/source/jd_services.c"
        "../jacdac-c/source/jd_util.c"
        "../jacdac-c/source/interfaces/event_queue.c"
        "../jacdac-c/source/interfaces/simple_rx.c"
        "../jacdac-c/source/interfaces/tx_queue.c"
        "../jacdac-c/services/jd_status_light.c"
        "../jacdac-c/services/power.c"
        "../jacdac-c/client/rolemgr.c"
        "../jacdac-c/client/routing.c"
        "../jacdac-c/jacscript/aggbuffer.c"
        "../jacdac-c/jacscript/buffer.c"
        "../jacdac-c/jacscript/fibers.c"
        "../jacdac-c/jacscript/jacscript.c"
        "../jacdac-c/jacscript/jacscriptmgr.c"
        "../jacdac-c/jacscript/jdiface.c"
        "../jacdac-c/jacscript/math.c"
        "../jacdac-c/jacscript/regcache.c"
        "../jacdac-c/jacscript/step.c"
        "../jacdac-c/jacscript/strformat.c"
        "../jacdac-c/jacscript/value.c"
        "../jacdac-c/jacscript/verify.c"
        "../jacdac-c/jacscript/jacscloud.c"
        "../jacdac-c/jacscript/tsagg.c"
        "../jacdac-c/jacdac/dist/c/jd_spec_pack.c"

        "../jacdac-c/storage/ff/ff.c"
        "../jacdac-c/storage/lstore.c"
        "../jacdac-c/storage/crc32.c"
)

pico_generate_pio_header(jacscript ${CMAKE_CURRENT_LIST_DIR}/sws.pio)

include_directories(AFTER
        "."
        "../jacdac-c/inc"
        "../jacdac-c/jacscript"
        "../jacdac-c"
)

# pull in common dependencies
target_link_libraries(jacscript 
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
