#!/bin/bash
pushd libogg-1.3.0
chmod +x configure
./configure --prefix=$(pwd)/../../ --enable-shared --enable-static --target=arc-linux --host=arc-linux-uclibc --build=arc-linux
make -j5
make install

popd
pushd speex-1.2rc1
chmod +x configure
./configure --prefix=$(pwd)/../../ --enable-shared --enable-static --target=arc-linux --host=arc-linux-uclibc --build=arc-linux --enable-fixed-point
make -j5
make install
