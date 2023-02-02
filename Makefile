_IGNORE0 := $(shell test -f Makefile.user || cp sample-Makefile.user Makefile.user)

include Makefile.user
BRAIN_ID ?= 59
BUILD = build/$(BRAIN_ID)
JDC = devicescript/runtime/jacdac-c

ELF = $(BUILD)/src/jacscript.elf

all: submodules refresh-version
	cd $(BUILD) && cmake ../..
	$(MAKE) -j16 -C $(BUILD)

r: flash
f: flash

flash: all boot
	cp $(BUILD)/src/jacscript.uf2 /Volumes/RPI-RP2

submodules: pico-sdk/lib/tinyusb/README.rst $(JDC)/jacdac/README.md $(BUILD)/config.cmake

# don't do --recursive - we don't want all tinyusb submodules

$(JDC)/jacdac/README.md:
	git submodule update --init
	cd devicescript && git submodule update --init --recursive

pico-sdk/lib/tinyusb/README.rst:
	git submodule update --init
	cd pico-sdk && git submodule update --init

$(BUILD)/config.cmake: Makefile Makefile.user
	mkdir -p $(BUILD)
	echo "add_compile_options($(COMPILE_OPTIONS) -DBRAIN_ID=$(BRAIN_ID))" > $@

FW_VERSION = $(shell sh $(JDC)/scripts/git-version.sh)

refresh-version:
	@mkdir -p $(BUILD)
	echo 'const char app_fw_version[] = "v$(FW_VERSION)";' > $(BUILD)/version-tmp.c
	@diff $(BUILD)/version.c $(BUILD)/version-tmp.c >/dev/null 2>/dev/null || \
		(echo "refresh version"; cp $(BUILD)/version-tmp.c $(BUILD)/version.c)

clean:
	rm -rf build

prep-build-gdb:
	echo > $(BUILD)/debug.gdb
	echo "file $(ELF)" >> $(BUILD)/debug.gdb
	echo "target extended-remote $(BMP_PORT)" >> $(BUILD)/debug.gdb
	echo "monitor connect_srst disable" >> $(BUILD)/debug.gdb
	echo "monitor swdp_scan" >> $(BUILD)/debug.gdb
	echo "attach 1" >> $(BUILD)/debug.gdb

boot: prep-build-gdb
	echo 'mon reset_usb_boot' >> $(BUILD)/debug.gdb
	echo "quit" >> $(BUILD)/debug.gdb
	arm-none-eabi-gdb --command=$(BUILD)/debug.gdb $(ELF) < /dev/null
	sleep 2

gdb: prep-build-gdb
	arm-none-eabi-gdb --command=$(BUILD)/debug.gdb $(ELF)

bump:
	./$(JDC)/scripts/bump.sh

# also keep ELF file for addr2line
.PHONY: dist
sub-dist: all
	mkdir -p dist
	cp $(BUILD)/src/jacscript.uf2 dist/jacscript-rp2040-msr$(BRAIN_ID).uf2
	cp $(BUILD)/src/jacscript.elf dist/jacscript-rp2040-msr$(BRAIN_ID).elf

dist:
	$(MAKE) BRAIN_ID=59 sub-dist
	$(MAKE) BRAIN_ID=124 sub-dist
	ls -l dist/

st:
	node $(JDC)/scripts/map-file-stats.js $(BUILD)/src/jacscript.elf.map

stf:
	node $(JDC)/scripts/map-file-stats.js $(BUILD)/src/jacscript.elf.map -fun