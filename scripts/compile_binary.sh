#!/bin/sh

node-gyp rebuild
cd build
if [[ $OSTYPE = darwin* ]]; then
  echo "crazy stuff not needed on darwin"
else
  # Had to mess around here - libfontconfig was not in /usr/lib. Had to also ensure that openssl was statically linked
  flock ./Release/linker.lock g++ -shared -pthread -rdynamic -m64  -Wl,-soname=chimera.node -o Release/obj.target/chimera.node -Wl,--start-group Release/obj.target/chimera/src/top.o Release/obj.target/chimera/src/cookiejar.o Release/obj.target/chimera/src/chimera.o Release/obj.target/chimera/src/browser.o -Wl,--end-group  ../qt_compiled/lib/libQtXml.a ../qt_compiled/lib/libQtNetwork.a ../qt_compiled/lib/libQtWebKit.a ../qt_compiled/lib/libwebcore.a ../qt_compiled/lib/libjscore.a -Wl,--whole-archive ../qt_compiled/lib/libQtCore.a ../qt_compiled/lib/libQtGui.a  -Wl,--no-whole-archive -Wl,-Bsymbolic /usr/lib/x86_64-linux-gnu/libfontconfig.so -Bstatic -L../openssl -lssl -lcrypto
  make
fi
cp Release/chimera.node ../lib
