#!/bin/sh

set -e
set -x
npx typescript-json-schema --noExtraProps --required \
		rp2040.d.ts RP2040DeviceConfig > rp2040deviceconfig-generated.schema.json
