add_executable(jacscript
        main.c
        )

# pull in common dependencies
target_link_libraries(jacscript pico_stdlib)
# create map/bin/hex file etc.
pico_add_extra_outputs(jacscript)
