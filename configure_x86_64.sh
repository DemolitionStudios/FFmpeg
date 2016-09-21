#./configure  --prefix=builds/x86_64/    --enable-shared --disable-sdl  --disable-programs --disable-outdev=sdl --disable-ffplay

./configure     --prefix=builds/x86_64 --enable-shared \
                --disable-sdl --disable-outdev=sdl --disable-ffplay \
                --enable-rpath \
                --sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.9.sdk 
