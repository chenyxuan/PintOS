#! /bin/bash

if [ $# -ne 1 ] || [ $1 == "-h" -o $1 == "--help" ]; then
  echo "Usage: $0 DSTDIR"
  exit 1
fi

CWD="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
if [ ! -f $CWD/bochs-2.6.2-jitter-plus-segv.patch -a -f $CWD/bochs-2.6.2-xrandr-pkgconfig.patch -a -f $CWD/bochs-2.6.2-banner-stderr.patch -a -f $CWD/bochs-2.6.2-block-device-check.patch -a -f $CWD/bochs-2.6.2-const-char.patch ]; then
  echo "Could not find the patch files for Bochs in $CWD."
  exit 1
fi

mkdir -p $1
if [ ! -d $1 ]; then
  echo "Could not find or create the destination directory"
  exit 1
fi
DSTDIR=$(cd $1 && pwd)

cd /tmp
mkdir $$
cd $$
cp /home/sjtu-ypm/system/pintos/src/misc/bochs-2.6.2.tar.gz ./
tar xzf bochs-2.6.2.tar.gz
cd bochs-2.6.2
cat $CWD/bochs-2.6.2-jitter-plus-segv.patch | patch -p1
cat $CWD/bochs-2.6.2-xrandr-pkgconfig.patch | patch -p1
cat $CWD/bochs-2.6.2-banner-stderr.patch | patch -p1
cat $CWD/bochs-2.6.2-block-device-check.patch | patch -p1
cat $CWD/bochs-2.6.2-const-char.patch | patch -p1
CFGOPTS="--with-x --with-x11 --with-term --with-nogui --prefix=$DSTDIR"
os="`uname`"
if [ $os == "Darwin" ]; then
  if [ ! -d /opt/X11 ]; then
    echo "Error: X11 directory does not exist. Have you installed XQuartz https://www.xquartz.org?" 
    exit 1
  fi
  # Bochs will have trouble finding X11 header files and library
  # We need to set the pkg config path explicitly.
  export PKG_CONFIG_PATH=/opt/X11/lib/pkgconfig
fi
WD=$(pwd)
mkdir plain && cd plain
../configure $CFGOPTS --enable-gdb-stub && make -j8
if [ $? -ne 0 ]; then
  echo "Error: build bochs failed"
  exit 1
fi
echo "Bochs plain successfully built"
make install
cd $WD
mkdir with-dbg && cd with-dbg 
../configure --enable-disasm --enable-all-optimizations --enable-readline  --enable-debugger-gui --enable-x86-debugger --enable-a20-pin --enable-fast-function-calls --enable-debugger $CFGOPTS 
pwd
read -rsp $'Press enter to continue...\n'
make -j8
if [ $? -ne 0 ]; then
  echo "Error: build bochs-dbg failed"
  exit 1
fi
cp bochs $DSTDIR/bin/bochs-dbg 
rm -rf /tmp/$$
echo "Done. bochs and bochs-dbg has been built and copied to $DSTDIR/bin"
