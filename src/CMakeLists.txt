file(GLOB RUNNER_FILES *.c)

add_executable(devsrunner
        ${RUNNER_FILES}
        ${CMAKE_BINARY_DIR}/version.c
)

pico_generate_pio_header(devsrunner ${CMAKE_CURRENT_LIST_DIR}/sws.pio)

include_directories(AFTER
        "."
)

# pull in common dependencies
target_link_libraries(devsrunner
        jacdac
        devicescript
        pico_stdlib
        pico_unique_id
        hardware_pwm
        hardware_pio
        hardware_dma
        hardware_irq
        cmsis_core
        tinyusb_device
)

if(PICO_CYW43_SUPPORTED)
        target_link_libraries(devsrunner pico_cyw43_arch_lwip_poll)
endif()

# create map/bin/hex file etc.
pico_add_extra_outputs(devsrunner)
