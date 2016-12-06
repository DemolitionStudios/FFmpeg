./configure     --prefix=builds/win_i686 --enable-shared \
		--enable-memalign-hack --arch=x86 --target-os=mingw32 --cross-prefix=i686-w64-mingw32- \
		--disable-w32threads \
		--pkg-config=pkg-config \
                --disable-sdl --disable-outdev=sdl --disable-ffplay
                
#--disable-avdevice --disable-avcodec --disable-avfilter --disable-avformat --disable-swresample --disable-swscale --disable-postproc
