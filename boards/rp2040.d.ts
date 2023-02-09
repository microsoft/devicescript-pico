/// <reference path="../devicescript/runtime/jacdac-c/dcfg/srvcfg.d.ts" />

import { DeviceConfig, Pin } from "@devicescript/srvcfg"

interface RP2040DeviceConfig extends DeviceConfig {
    pinPwrNILimHi?: Pin
}
