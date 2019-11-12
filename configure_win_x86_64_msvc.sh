# cd /c/Users/lev/projects/UnityHapPlugin/hapunityplugin/3rdparty/ffmpeg
# ./make-vs2015-x86_64-env.bat
# <execute each export command>
# export PATH="/h/media-autobuild_suite/msys64/mingw64/bin/":$PATH
# bash ./configure_win_x86_64_msvc.sh
# time make -j9 && make install



# --enable-avdevice 
# --enable-filter=scale

./configure     --prefix=builds/win_x86_64_msvc \
		--enable-shared --disable-static \
		--enable-asm --enable-x86asm \
		--arch=x86_64 --target-os=win64 --toolchain=msvc \
		--disable-encoders --disable-decoders --disable-hwaccels --disable-muxers --disable-demuxers \
		--disable-parsers --disable-bsfs --disable-protocols --disable-indevs --disable-outdevs \
		--disable-devices --disable-ffprobe \
		--enable-avcodec --enable-avformat --enable-swresample --enable-swscale --enable-avfilter \
		--enable-protocol=file --enable-demuxer=mov,avi --enable-muxer=mov,avi \
		--enable-filters \
		--enable-libsnappy --enable-liblz4 \
		--enable-decoder=hap --enable-encoder=hap \
		--extra-cflags="-I /c/Users/lev/projects/UnityHapPlugin/hapunityplugin/3rdparty/snappy-windows-1.1.1.8/include/" \
		--extra-ldflags="-LIBPATH:/c/Users/lev/projects/UnityHapPlugin/hapunityplugin/3rdparty/snappy-windows-1.1.1.8/native/" \
		--extra-cflags="-I /c/Users/lev/projects/UnityHapPlugin/hapunityplugin/3rdparty/lz4-windows/include/" \
		--extra-ldflags="-LIBPATH:/c/Users/lev/projects/UnityHapPlugin/hapunityplugin/3rdparty/lz4-windows/lib/x64/" 

#		--extra-cflags="-I/c/Users/lev/projects/UnityHapPlugin/hapunityplugin/3rdparty/snappy-windows-1.1.1.8/include/" 
#		--extra-ldflags="-L/c/Users/lev/projects/UnityHapPlugin/hapunityplugin/3rdparty/snappy-windows-1.1.1.8/lib/"

