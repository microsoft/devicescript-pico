_IGNORE0 := $(shell test -f Makefile.user || cp sample-Makefile.user Makefile.user)

include Makefile.user
BUILD_ARCH ?= rp2040w
BOARD ?= $(shell basename `cd boards/$(BUILD_ARCH); ls *.board.json | head -1` .board.json)

BUILD = build/$(BUILD_ARCH)
JDC = devicescript/runtime/jacdac-c
CLI = node devicescript/cli/devicescript 
EXE = devsrunner
ELF = $(BUILD)/src/$(EXE).elf
UF2 = $(BUILD)/src/$(EXE).uf2

ifeq ($(BUILD_ARCH),rp2040w)
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
	cp dist/devicescript-$(BUILD_ARCH)-$(BOARD).uf2 /Volumes/RPI-RP2

submodules: pico-sdk/lib/tinyusb/README.rst $(JDC)/jacdac/README.md \
	$(BUILD)/config.cmake \
	boards/$(BUILD_ARCH) \
	devicescript/cli/built/devicescript-cli.cjs

devicescript/cli/built/devicescript-cli.cjs: $(JDC)/jacdac/README.md
	cd devicescript && yarn
	cd devicescript && yarn build-fast

$(JDC)/jacdac/README.md:
	git submodule update --init
	cd devicescript && git submodule update --init --recursive

# don't do top-level --recursive - we don't want all tinyusb submodules
pico-sdk/lib/tinyusb/README.rst:
	git submodule update --init
	cd pico-sdk && git submodule update --init

$(BUILD)/config.cmake: Makefile Makefile.user
	mkdir -p $(BUILD)
	echo "add_compile_options($(COMPILE_OPTIONS))" > $@

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

update-devs: devicescript/cli/built/devicescript-cli.cjs
	node devicescript/scripts/bumparch.mjs --update

bump: update-devs
	node devicescript/scripts/bumparch.mjs

concat-configs:
	mkdir -p dist
	$(CLI) binpatch --slug microsoft/devicescript-pico --generic --uf2 $(UF2) --outdir dist boards/$(BUILD_ARCH)/*.board.json

# also keep ELF file for addr2line
.PHONY: dist
sub-dist: all

dist:
	rm -rf dist/
	set -e; for f in `ls boards` ; do \
	  if test -f boards/$$f/arch.json; then $(MAKE) BUILD_ARCH=$$f sub-dist ; fi ; \
	done
	ls -l dist/

st:
	node $(JDC)/scripts/map-file-stats.js $(ELF).map

stf:
	node $(JDC)/scripts/map-file-stats.js $(ELF).map -fun
