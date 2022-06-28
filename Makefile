BMP_PORT ?= $(shell ls -1 /dev/cu.usbmodem????????1 | head -1)
ELF = build/src/jacscript.elf

all: submodules refresh-version
	cd build && cmake ..
	$(MAKE) -j16 -C build

r: flash
f: flash

flash: all boot
	cp build/src/jacscript.uf2 /Volumes/RPI-RP2

submodules: pico-sdk/lib/tinyusb/README.rst jacdac-c/jacdac/README.md

# don't do --recursive - we don't want all tinyusb submodules

jacdac-c/jacdac/README.md:
	git submodule update --init
	cd jacdac-c && git submodule update --init

pico-sdk/lib/tinyusb/README.rst:
	git submodule update --init
	cd pico-sdk && git submodule update --init

refresh-version:
	@mkdir -p build
	echo 'const char app_fw_version[] = "v$(FW_VERSION)";' > build/version-tmp.c
	@diff build/version.c build/version-tmp.c >/dev/null 2>/dev/null || \
		(echo "refresh version"; cp build/version-tmp.c build/version.c)

clean:
	rm -rf build


prep-build-gdb:
	echo > build/debug.gdb
	echo "file $(ELF)" >> build/debug.gdb
	echo "target extended-remote $(BMP_PORT)" >> build/debug.gdb
	echo "monitor connect_srst disable" >> build/debug.gdb
	echo "monitor swdp_scan" >> build/debug.gdb
	echo "attach 1" >> build/debug.gdb

boot: prep-build-gdb
	echo 'set $$pc=0x25b5' >> build/debug.gdb
	echo 'set $$r0=0' >> build/debug.gdb
	echo 'set $$r1=0' >> build/debug.gdb
	echo "quit" >> build/debug.gdb
	arm-none-eabi-gdb --command=build/debug.gdb $(ELF) < /dev/null
	sleep 2

gdb: prep-build-gdb
	arm-none-eabi-gdb --command=build/debug.gdb $(ELF)
