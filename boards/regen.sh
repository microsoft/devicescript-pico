#!/bin/sh

set -e
set -x
npx typescript-json-schema --noExtraProps --required --defaultNumberType integer \
		rp2040.d.ts RP2040DeviceConfig --out rp2040deviceconfig.schema.json
npx typescript-json-schema --noExtraProps --required --defaultNumberType integer \
		rp2040.d.ts RP2040ArchConfig --out rp2040archconfig.schema.json
