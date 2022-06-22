all: submodules refresh-version
	cd build && cmake ..
	$(MAKE) -j16 -C build

r: flash
f: flash

flash: all
	cp build/src/jacscript.uf2 /Volumes/RPI-RP2

submodules: pico-sdk/lib/tinyusb/README.md jacdac-c/jacdac/README.md

# don't do --recursive - we don't want all tinyusb submodules

jacdac-c/jacdac/README.md:
	git submodule update --init
	cd jacdac-c && git submodule update --init

pico-sdk/lib/tinyusb/README.md:
	git submodule update --init
	cd pico-sdk && git submodule update --init

refresh-version:
	@mkdir -p build
	echo 'const char app_fw_version[] = "v$(FW_VERSION)";' > build/version-tmp.c
	@diff build/version.c build/version-tmp.c >/dev/null 2>/dev/null || \
		(echo "refresh version"; cp build/version-tmp.c build/version.c)
