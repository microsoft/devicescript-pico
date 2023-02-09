#!/bin/sh

set -x
set -e

BUILD_TYPE=$1
UF2=$2
BUILD=build/$BUILD_TYPE
CLI="node devicescript/cli/devicescript"
JD_DCFG_BASE_ADDR=0x100dc000
UF2_PREF=$BUILD/devicescript-rp2040-$BUILD_TYPE

cp $UF2 $UF2_PREF-generic.uf2
cp `echo $UF2 | sed -e 's/\.uf2/.elf/'` $UF2_PREF-generic.elf

for f in boards/$BUILD_TYPE/*.json ; do
  BN=$(basename $f .json)
  $CLI dcfg $f --output $BUILD/settings-$BN.bin
  ./uf2/utils/uf2conv.py \
      --family RP2040 --base $JD_DCFG_BASE_ADDR  \
      --convert $BUILD/settings-$BN.bin \
      --output $BUILD/settings-$BN.uf2
  ./uf2/utils/uf2conv.py \
      --convert --join $UF2 $BUILD/settings-$BN.uf2 \
      --output $UF2_PREF-$BN.uf2
done
