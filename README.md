# Jacdac+DeviceScript for RP2040

To build just run `make` - this will clone submodules and invoke `cmake` as appropriate.
Do not clone with `--recursive` unless you want to wait a long time - just run `make`.

You will need ARM GCC and node.js installed as well - some of our
[instructions for STM32 build](https://github.com/microsoft/jacdac-stm32x0/blob/main/README.md#setup)
may apply.

Once you build, copy `build/src/devsrunner.uf2` to the RPI-RP2 drive.

Debugging is set up for Black Magic Probe. 
You can convert a [$3 Bluepill board into Black Magic Probe](https://github.com/mmoskal/blackmagic-bluepill).
Run `make gdb` to run GDB.

One you have the firmware running, head to https://aka.ms/jacdac and press "CONNECT" in top-right corner,
and then "CONNECT SERIAL" (not "CONNECT USB").
Then head to "Device Dashboard" (Jacdac "logo" (connector icon) next to "CONNECT" button).
You should see widgets for power service, DeviceScript and HID services.

## TODO

* PICO_TIME_DEFAULT_ALARM_POOL_DISABLED

## Contributing

This project welcomes contributions and suggestions.  Most contributions require you to agree to a
Contributor License Agreement (CLA) declaring that you have the right to, and actually do, grant us
the rights to use your contribution. For details, visit https://cla.opensource.microsoft.com.

When you submit a pull request, a CLA bot will automatically determine whether you need to provide
a CLA and decorate the PR appropriately (e.g., status check, comment). Simply follow the instructions
provided by the bot. You will only need to do this once across all repos using our CLA.

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/).
For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or
contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.

## Trademarks

This project may contain trademarks or logos for projects, products, or services. Authorized use of Microsoft 
trademarks or logos is subject to and must follow 
[Microsoft's Trademark & Brand Guidelines](https://www.microsoft.com/en-us/legal/intellectualproperty/trademarks/usage/general).
Use of Microsoft trademarks or logos in modified versions of this project must not cause confusion or imply Microsoft sponsorship.
Any use of third-party trademarks or logos are subject to those third-party's policies.
