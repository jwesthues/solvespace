#!/bin/sh -xe

if [ "$1" = "ci" ]; then
    curl -L https://bintray.com/homebrew/bottles/download_file?file_path=libomp-11.0.1.arm64_big_sur.bottle.tar.gz --output /tmp/libomp-arm64.tar.gz
    mkdir /tmp/libomp-arm64 || true
    tar -xzvf /tmp/libomp-arm64.tar.gz -C /tmp/libomp-arm64
    curl -L https://bintray.com/homebrew/bottles/download_file?file_path=libomp-11.0.1.big_sur.bottle.tar.gz --output /tmp/libomp-x86_64.tar.gz
    mkdir /tmp/libomp-x86_64 || true
    tar -xzvf /tmp/libomp-x86_64.tar.gz -C /tmp/libomp-x86_64
else
    brew install libomp
fi

git submodule update --init extlib/cairo extlib/freetype extlib/libdxfrw extlib/libpng extlib/mimalloc extlib/pixman extlib/zlib extlib/libeigen
