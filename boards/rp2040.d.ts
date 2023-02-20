import { Pin } from "@devicescript/srvcfg"
import {
    ArchConfig,
    DeviceConfig,
} from "../devicescript/compiler/src/archconfig"

interface RP2040DeviceConfig extends DeviceConfig {
    pinPwrNILimHi?: Pin
}

interface RP2040ArchConfig extends ArchConfig {}
