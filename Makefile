_IGNORE0 := $(shell test -f Makefile.user || cp sample-Makefile.user Makefile.user)

include Makefile.user
BUILD_TYPE ?= cyw43

BUILD = build/$(BUILD_TYPE)
JDC = devicescript/runtime/jacdac-c

EXE = devsrunner
ELF = $(BUILD)/src/$(EXE).elf
UF2 = $(BUILD)/src/$(EXE).uf2

ifeq ($(BUILD_TYPE),cyw43)
CMAKE_OPTIONS += -DPICO_BOARD=pico_w
endif

all: submodules refresh-version
	cd $(BUILD) && cmake ../.. $(CMAKE_OPTIONS)
	$(MAKE) -j16 -C $(BUILD)
	$(MAKE) concat-configs

r: flash
f: flash

flash: all boot
	sleep 2
	cp $(UF2) /Volumes/RPI-RP2

submodules: pico-sdk/lib/tinyusb/README.rst $(JDC)/jacdac/README.md $(BUILD)/config.cmake boards/$(BUILD_TYPE)

# don't do --recursive - we don't want all tinyusb submodules

$(JDC)/jacdac/README.md:
	git submodule update --init
	cd devicescript && git submodule update --init --recursive

pico-sdk/lib/tinyusb/README.rst:
	git submodule update --init
	cd pico-sdk && git submodule update --init

$(BUILD)/config.cmake: Makefile Makefile.user
	mkdir -p $(BUILD)
	echo "add_compile_options($(COMPILE_OPTIONS) -DBRAIN_ID=BRAIN_ID_$(BRAIN_ID))" > $@

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

load-gdb: prep-build-gdb
	echo 'load' >> $(BUILD)/debug.gdb
	echo "quit" >> $(BUILD)/debug.gdb
	arm-none-eabi-gdb --command=$(BUILD)/debug.gdb $(ELF) < /dev/null

rg: all
	$(MAKE) boot
	$(MAKE) load-gdb

gdb: prep-build-gdb
	arm-none-eabi-gdb --command=$(BUILD)/debug.gdb $(ELF)

bump:
	./$(JDC)/scripts/bump.sh

concat-configs:
	./scripts/concat-configs.sh $(BUILD_TYPE) $(UF2)

# also keep ELF file for addr2line
.PHONY: dist
sub-dist: all
	mkdir -p dist
	cp $(BUILD)/devicescript-rp2040-* dist/
	cp $(BUILD)/settings-*.uf2 dist/

dist:
	$(MAKE) BUILD_TYPE=base sub-dist
	$(MAKE) BUILD_TYPE=cyw43 sub-dist
	ls -l dist/

st:
	node $(JDC)/scripts/map-file-stats.js $(ELF).map

stf:
	node $(JDC)/scripts/map-file-stats.js $(ELF).map -fun
