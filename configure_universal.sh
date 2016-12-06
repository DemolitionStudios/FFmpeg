#--disable-programs

./configure  	--prefix=builds/universal/ --enable-shared \
		--disable-sdl --disable-ffplay \
		--enable-rpath \
		--sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.9.sdk

#		--disable-avdevice --disable-avcodec --disable-avfilter --disable-avformat --disable-swresample --disable-swscale --disable-postproc
