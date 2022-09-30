#REPO_ROOT=/c/Users/lev/projects/UnityHapPlugin/hapunityplugin
REPO_ROOT=/d/projects/DemolitionStudios/hapunityplugin

./configure   --prefix=builds/win_x86_64_msvc_debug \
		--enable-shared --disable-static \
		--enable-asm --enable-x86asm \
		--arch=x86_64 --target-os=win64 --toolchain=msvc \
		--extra-cflags="-MDd" --extra-ldflags="-nodefaultlib:LIBCMT" --enable-debug \
		--disable-encoders --disable-decoders --disable-hwaccels --disable-muxers --disable-demuxers \
		--disable-parsers --disable-bsfs --disable-protocols --disable-indevs --disable-outdevs \
		--disable-devices --disable-ffprobe \
		--enable-avcodec --enable-avformat --enable-swresample --enable-swscale --enable-avfilter \
		--enable-protocol=file --enable-demuxer=mov,avi --enable-muxer=mov,avi \
		--enable-filters \
		--enable-libsnappy --enable-liblz4 --enable-liblizard --enable-libzstd \
		--enable-decoder=hap --enable-encoder=hap \
		--extra-cflags="-I ${REPO_ROOT}/3rdparty/snappy-windows-1.1.1.8/include/" \
		--extra-ldflags="-LIBPATH:${REPO_ROOT}/3rdparty/snappy-windows-1.1.1.8/native/" \
		--extra-cflags="-I ${REPO_ROOT}/3rdparty/lz4-windows/include/" \
		--extra-ldflags="-LIBPATH:${REPO_ROOT}/3rdparty/lz4-windows/lib/x64/" \
		--extra-cflags="-I ${REPO_ROOT}/3rdparty/lizard-windows/include/" \
		--extra-ldflags="-LIBPATH:${REPO_ROOT}/3rdparty/lizard-windows/lib/x64/" \
		--extra-cflags="-I ${REPO_ROOT}/3rdparty/zstd-windows/include/" \
		--extra-ldflags="-LIBPATH:${REPO_ROOT}/3rdparty/zstd-windows/lib/x64/" 
