# Install & run MSYS2
# Install cmp needed by configure script
#	pacman -Suy
#	pacman -S base-devel --needed 

# cd /c/Users/lev/projects/UnityHapPlugin/hapunityplugin/3rdparty/ffmpeg
#	 /d/projects/DemolitionStudios/hapunityplugin/3rdparty/ffmpeg

# Add nasm.exe to PATH
#	export PATH="/h/media-autobuild_suite/msys64/mingw64/bin/":$PATH
#	or install nasm from https://www.nasm.us/pub/nasm/releasebuilds/?C=M;O=D and 
#		export PATH="/c/Program Files/NASM":$PATH

# ./make-vs2015-x86_64-env.bat
# <execute each export command>

# bash ./configure_win_x86_64_msvc.sh
# time make -j9 && make install


# --enable-indevs: lavfi (https://lists.ffmpeg.org/pipermail/ffmpeg-user/2012-February/004618.html)
# todo: --disable-debug ?
# --disable-devices

# https://trac.ffmpeg.org/wiki/FancyFilteringExamples

#REPO_ROOT=/c/Users/lev/projects/UnityHapPlugin/hapunityplugin
REPO_ROOT=/d/projects/DemolitionStudios/hapunityplugin

./configure   --prefix=builds/win_x86_64_msvc \
		--enable-shared --disable-static \
		--enable-asm --enable-x86asm \
		--arch=x86_64 --target-os=win64 --toolchain=msvc \
		--disable-encoders --disable-decoders --disable-hwaccels --disable-muxers --disable-demuxers \
		--disable-parsers --disable-bsfs --disable-protocols --enable-indevs --disable-outdevs \
		--disable-ffprobe \
		--enable-avcodec --enable-avformat --enable-swresample --enable-swscale --enable-avfilter \
		--enable-protocol=file --enable-demuxer=mov,avi,h264 --enable-muxer=mov,avi,h264 \
		--enable-filters \
		--enable-gpl --enable-parser=h264 --enable-decoder=h264 --enable-decoder=rawvideo  \
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
