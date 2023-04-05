# Install & run MSYS2
# Install cmp needed by configure script
#	pacman -Suy
#	pacman -S base-devel --needed 

# Note: In something not working, make sure non of the paths below has changed

# cd /c/Users/lev/projects/UnityHapPlugin/hapunityplugin/3rdparty/ffmpeg
# cd /d/projects/DemolitionStudios/hapunityplugin/3rdparty/ffmpeg

# Add nasm.exe to PATH
#	export PATH="/d/media-autobuild_suite/msys64/mingw64/bin/":$PATH
#	or install nasm from https://www.nasm.us/pub/nasm/releasebuilds/?C=M;O=D and 
#		export PATH="/c/Program Files/NASM":$PATH

# Run ./make-vs2015-x86_64-env.bat
# <execute each export command>

# make clean && bash ./configure_win_x86_64_msvc_shutter_encoder.sh
# time make -j9 && make install
# rm -f libavcodec/hapenc.d && make -j9 && rm -f libavcodec/hapenc.d && make install

# If any errors occur:
# make SHELL="sh -x"

#REPO_ROOT=/c/Users/lev/projects/UnityHapPlugin/hapunityplugin
REPO_ROOT=/d/projects/DemolitionStudios/hapunityplugin

#--pkg-config=pkgconf  --extra-cxxflags=-fpermissive 
#--extra-cflags=-Wno-int-conversion 
#--disable-w32threads --enable-sdl2 
#--enable-version3 --enable-fontconfig --enable-iconv --enable-libass --enable-libdav1d --enable-libfreetype --enable-libmp3lame --enable-libopencore-amrnb --enable-libopencore-amrwb --enable-libopenjpeg --enable-libopus --enable-libsnappy --enable-libtheora --enable-libtwolame --enable-libvpx --enable-libwebp --enable-libx264 --enable-libx265 --enable-libzimg --enable-lzma --enable-zlib --enable-libvidstab --enable-libvorbis --enable-libvo-amrwbenc --enable-libxvid --enable-libgsm --enable-libsvtav1 --enable-libaom --enable-libmfx --enable-ffnvcodec --enable-cuda-llvm --enable-cuvid --enable-d3d11va --enable-nvenc --enable-nvdec --enable-dxva2 --enable-amf --enable-vulkan --enable-libglslang --enable-schannel 
#--extra-cflags=-DLIBTWOLAME_STATIC --extra-libs=-lstdc++ --extra-libs=-liconv --shlibdir=/local64/bin-video

        #--windres=/C/msys64/mingw64/bin/windres.exe \

./configure   --prefix=builds/win_x86_64_msvc \
		--enable-shared --disable-static \
		--enable-asm --enable-x86asm \
		--arch=x86_64 --target-os=win64 --toolchain=msvc \
		--disable-debug \
        --disable-autodetect \
        --enable-gpl --enable-version3 \
		--disable-bsfs --disable-protocols --disable-indevs --disable-outdevs \
		--disable-devices --enable-ffprobe \
		--enable-avcodec --enable-avformat --enable-swresample --enable-swscale --enable-avfilter \
		--enable-protocol=file --enable-muxer=mov,avi,h264 \
		--enable-filters \
		--enable-decoder=rawvideo \
		--enable-decoder=hap \
        --enable-encoder=hap \
        --enable-libsnappy \
        --enable-libgdeflate \
        --enable-libbc7e \
		--extra-cflags="-I ${REPO_ROOT}/3rdparty/snappy-windows-1.1.1.8/include" \
		--extra-ldflags="-LIBPATH:${REPO_ROOT}/3rdparty/snappy-windows-1.1.1.8/native" \
        --extra-cflags="-I ${REPO_ROOT}/3rdparty/gdeflate/include" \
		--extra-ldflags="-LIBPATH:${REPO_ROOT}/3rdparty/gdeflate/lib/x86_64" \
        --extra-cflags="-I ${REPO_ROOT}/3rdparty/ffmpeg/libavcodec/bc7e/build/include" \
		--extra-ldflags="-LIBPATH:${REPO_ROOT}/3rdparty/ffmpeg/libavcodec/bc7e/build/lib/x86_64"
        #--enable-libmp3lame --enable-libopus --enable-libtheora --enable-libtwolame --enable-libvorbis --enable-libgsm \
		
		