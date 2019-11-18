# cd /c/Users/lev/projects/UnityHapPlugin/hapunityplugin/3rdparty/ffmpeg
# ./make-vs2015-env.bat
# <execute each export command>
# export PATH="/h/media-autobuild_suite/msys64/mingw64/bin/":$PATH
# bash ./configure_win_x86_64_msvc.sh


./configure     --prefix=builds/win_x86_msvc \
		--enable-shared --disable-static \
		--enable-asm --enable-yasm \
		--arch=x86 --target-os=win32 --toolchain=msvc \
        --disable-outdev=sdl --disable-ffplay --disable-ffprobe \
		--disable-bzlib --disable-libopenjpeg --disable-iconv --disable-zlib 
# FIXME: see x86_64

