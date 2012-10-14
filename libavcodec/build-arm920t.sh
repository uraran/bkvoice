#!/bin/bash
export PATH=/opt/FriendlyARM/toolschain/4.4.3/bin:$PATH

export SDK_DIR=$PWD/install_sdk/arm920t
echo "编解码SDK安装路径: $SDK_DIR"

#清除include与lib文件夹
echo "删除 include与lib文件夹"
rm -rf $PWD/../bin
rm -rf $PWD/../include
rm -rf $PWD/../lib
rm -rf $PWD/../share
rm -rf $SDK_DIR/bin
rm -rf $SDK_DIR/include
rm -rf $SDK_DIR/lib

#编译ogg
echo "编译 libogg-1.3.0"
pushd libogg-1.3.0
make distclean
chmod +x configure
./configure --prefix=$SDK_DIR --enable-shared --enable-static --host=arm-none-linux-gnueabi
make -j5
make install
popd

#编译speex
echo "编译 speex-1.2rc1"
pushd speex-1.2rc1
make distclean
chmod +x configure
./configure --prefix=$SDK_DIR --enable-shared --enable-static --host=arm-none-linux-gnueabi --enable-fixed-point --enable-arm4-asm
make -j5
make install
popd


#编译silk
echo "编译 SILK_SDK_SRC_ARM_v1.0.8"
pushd SILK_SDK_SRC_ARM_v1.0.8
#FOR S3C2440
make clean
make ARM920T=yes TOOLCHAIN_PREFIX=arm-none-linux-gnueabi-

if [ ! -d  $SDK_DIR/include ]; then
  mkdir -p $SDK_DIR/include
  echo "$SDK_DIR/include 目录创建成功"
else
  echo "$SDK_DIR/include 目录已经存在"
fi





if [ ! -d  $SDK_DIR/include/SILK ]; then
  mkdir -p $SDK_DIR/include/SILK
  echo "$SDK_DIR/include/SILK 目录创建成功"
else
  echo "$SDK_DIR/include/SILK 目录已经存在"
fi



if [ ! -d  $SDK_DIR/lib ]; then
  mkdir -p $SDK_DIR/lib
  echo "$SDK_DIR/lib 目录创建成功"
else
  echo "$SDK_DIR/lib 目录已经存在"
fi


echo "复制SILK SDK"
cp ./src/*.h $SDK_DIR/include/SILK

if [ ! -d  $SDK_DIR/include/SILK/interface ]; then
  mkdir -p $SDK_DIR/include/SILK/interface
  echo "$SDK_DIR/include/SILK/interface 目录创建成功"
else
  echo "$SDK_DIR/include/SILK/interface 目录已经存在"
fi

cp ./interface/*.h $SDK_DIR/include/SILK/interface
cp ./libSKP_SILK_SDK.a $SDK_DIR/lib
popd
