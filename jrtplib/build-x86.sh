export SDK_DIR=$PWD/install_sdk/x86/
echo "jrtplib安装路径: $SDK_DIR"


#清除include与lib文件夹
echo "删除 include与lib文件夹"
rm -rf $PWD/../bin
rm -rf $PWD/../include
rm -rf $PWD/../lib
rm -rf $PWD/../share
rm -rf $SDK_DIR/bin
rm -rf $SDK_DIR/include
rm -rf $SDK_DIR/lib

#编译 jthread-1.2.1
echo "编译 jthread-1.2.1"
pushd jthread-1.2.1
make distclean
chmod +x configure
./configure --prefix=$SDK_DIR --enable-shared --enable-static
make -j5
make install
popd





#编译 jrtplib-3.7.1
echo "编译 jrtplib-3.7.1"
pushd jrtplib-3.7.1
make distclean
chmod +x configure
./configure --prefix=$SDK_DIR --enable-shared --enable-static
make -j5
make install
popd




