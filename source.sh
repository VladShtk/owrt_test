#!/bin/sh

set -ex

export SDK=openwrt-sdk-19.07.0-ar71xx-mikrotik_gcc-7.5.0_musl.Linux-x86_64

curl -SL https://downloads.openwrt.org/releases/19.07.0/targets/ar71xx/mikrotik/$SDK.tar.xz | tar xJ

mkdir $SDK/package/owrt_module
cp -avr src $SDK/package/owrt_module/src
cp -avR files $SDK/package/owrt_module/files
cp -avR opc_certs $SDK/package/owrt_module/opc_certs
cp -av Makefile $SDK/package/owrt_module

cd $SDK

./scripts/feeds update -a

./scripts/feeds install libconfig
./scripts/feeds install libmbedtls
./scripts/feeds install libopenssl
./scripts/feeds install libbz2
./scripts/feeds install libmodbus
./scripts/feeds install libmosquitto
# ./scripts/feeds install -a

make defconfig

build-wrapper-linux-x86-64 --out-dir  /drone/src/bw-output make package/owrt_module/compile

tree bin/packages