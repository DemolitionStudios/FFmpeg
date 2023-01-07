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

# make clean && bash ./configure_win_x86_64_msvc.sh
# time make -j9 && make install


#REPO_ROOT=/c/Users/lev/projects/UnityHapPlugin/hapunityplugin
REPO_ROOT=/d/projects/DemolitionStudios/hapunityplugin

./configure   --prefix=builds/win_x86_64_msvc \
		--enable-shared --disable-static \
		--enable-asm --enable-x86asm \
		--arch=x86_64 --target-os=win64 --toolchain=msvc \
		--disable-debug \
		--disable-encoders --disable-decoders --disable-hwaccels --disable-muxers --disable-demuxers \
		--disable-parsers --disable-bsfs --disable-protocols --disable-indevs --disable-outdevs \
		--disable-devices --disable-ffprobe \
		--enable-avcodec --enable-avformat --enable-swresample --enable-swscale --enable-avfilter \
		--enable-protocol=file --enable-demuxer=mov,avi,h264 --enable-muxer=mov,avi,h264 \
		--enable-filters \
		--enable-parser=h264 --enable-decoder=h264 --enable-decoder=rawvideo  \
		--enable-decoder=hap \
        --enable-encoder=hap \
        --enable-libsnappy \
        --enable-libgdeflate \
        --enable-libcompressonator \
		--extra-cflags="-I ${REPO_ROOT}/3rdparty/snappy-windows-1.1.1.8/include/" \
		--extra-ldflags="-LIBPATH:${REPO_ROOT}/3rdparty/snappy-windows-1.1.1.8/native/" \
        --extra-cflags="-I ${REPO_ROOT}/3rdparty/gdeflate/include/" \
		--extra-ldflags="-LIBPATH:${REPO_ROOT}/3rdparty/gdeflate/lib/x86_64/" \
        --extra-cflags="-I ${COMPRESSONATOR_SDK_PATH}/include/" \
		--extra-ldflags="-LIBPATH:${COMPRESSONATOR_SDK_PATH}/lib/VS2017/x64/" \
		--enable-decoder=8svx_exp,8svx_fib,aac,aac_fixed,aac_latm,ac3,ac3_fixed,adpcm_4xm,adpcm_adx,adpcm_afc,adpcm_agm,adpcm_aica,adpcm_ct,adpcm_dtk,adpcm_ea,adpcm_ea_maxis_xa,adpcm_ea_r1,adpcm_ea_r2,adpcm_ea_r3,adpcm_ea_xas,g722,g726,g726le,adpcm_ima_amv,adpcm_ima_apc,adpcm_ima_dat4,adpcm_ima_dk3,adpcm_ima_dk4,adpcm_ima_ea_eacs,adpcm_ima_ea_sead,adpcm_ima_iss,adpcm_ima_oki,adpcm_ima_qt,adpcm_ima_rad,adpcm_ima_smjpeg,adpcm_ima_wav,adpcm_ima_ws,adpcm_ms,adpcm_mtaf,adpcm_psx,adpcm_sbpro_2,adpcm_sbpro_3,adpcm_sbpro_4,adpcm_swf,adpcm_thp,adpcm_thp_le,adpcm_vima,adpcm_xa,adpcm_yamaha,amrnb,libopencore_amrnb,amrwb,libopencore_amrwb,ape,aptx,aptx_hd,atrac1,atrac3,atrac3al,atrac3plus,atrac3plusal,atrac9,on2avc,binkaudio_dct,binkaudio_rdft,bmv_audio,comfortnoise,cook,dolby_e,dsicinaudio,dss_sp,dst,dca,dvaudio,eac3,evrc,g723_1,g729,gremlin_dpcm,gsm,gsm_ms,hcom,iac,ilbc,imc,interplay_dpcm,interplayacm,mace3,mace6,metasound,mlp,mp1,mp1float,mp2,mp2float,mp3float,mp3,mp3adufloat,mp3adu,mp3on4float,mp3on4,als,mpc7,mpc8,nellymoser,opus,libopus,paf_audio,pcm_alaw,pcm_bluray,pcm_dvd,pcm_f16le,pcm_f24le,pcm_f32be,pcm_f32le,pcm_f64be,pcm_f64le,pcm_lxf,pcm_mulaw,pcm_s16be,pcm_s16be_planar,pcm_s16le,pcm_s16le_planar,pcm_s24be,pcm_s24daud,pcm_s24le,pcm_s24le_planar,pcm_s32be,pcm_s32le,pcm_s32le_planar,pcm_s64be,pcm_s64le,pcm_s8,pcm_s8_planar,pcm_u16be,pcm_u16le,pcm_u24be,pcm_u24le,pcm_u32be,pcm_u32le,pcm_u8,pcm_vidc,pcm_zork,qcelp,qdm2,qdmc,real_144,real_288,ralf,roq_dpcm,s302m,sbc,sdx2_dpcm,shorten,sipr,smackaud,sol_dpcm,libspeex,truehd,truespeech,twinvq,vmdaudio,vorbis,libvorbis,wavesynth,ws_snd1,wmalossless,wmapro,wmav1,wmav2,wmavoice,xan_dpcm,xma1,xma2 
		
		