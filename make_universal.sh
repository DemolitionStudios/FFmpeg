./configure_i386.sh && make clean && make -j5 && make install && \
./configure_x86_64.sh && make clean && make -j5 && make install && \
./make_fat_libs.sh


# not working in ffmpeg, using lipo us the suggested way of producing universal binaries
#	make CFLAGS="-arch i386 -arch x86_64" LDFLAGS="-arch i386 -arch x86_64" -j5 && \
#	make install
