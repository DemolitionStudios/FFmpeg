#./configure  --prefix=builds/i386    --enable-shared --cc='clang -m32'  --disable-sdl  --disable-programs --disable-outdev=sdl --disable-ffplay

./configure     --prefix=builds/i386 --enable-shared \
		--cc='clang -m32' \
                --disable-sdl --disable-outdev=sdl --disable-ffplay \
                --enable-rpath \
                --sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.9.sdk
                
#--disable-avdevice --disable-avcodec --disable-avfilter --disable-avformat --disable-swresample --disable-swscale --disable-postproc
