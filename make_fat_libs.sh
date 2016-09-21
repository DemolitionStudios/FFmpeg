ARCHS="x86_64 i386"

STATIC_LIBS="libavcodec.a libavdevice.a libavfilter.a libavformat.a libavutil.a libswresample.a libswscale.a"
SHARED_LIBS="libavcodec.57.dylib libavdevice.57.dylib libavfilter.6.dylib libavformat.57.dylib libavutil.55.dylib libswresample.2.dylib libswscale.4.dylib"

BUILDS_DIR="builds"
OUTPUT_DIR="$BUILDS_DIR/universal"

mkdir -p $OUTPUT_DIR/lib
mkdir -p $OUTPUT_DIR/include

for LIB in $STATIC_LIBS $SHARED_LIBS; do 
  LIPO_CREATE=""
  for ARCH in $ARCHS; do
    LIPO_CREATE="$LIPO_CREATE-arch $ARCH $BUILDS_DIR/$ARCH/lib/$LIB "
  done
  OUTPUT="$OUTPUT_DIR/lib/$LIB"
  echo "Creating: $OUTPUT"
  #echo $LIPO_CREATE  
  lipo -create $LIPO_CREATE -output $OUTPUT
  lipo -info $OUTPUT
done


echo "Copying headers from ffmpeg-i386..."
cp -R $BUILDS_DIR/$ARCH/include/* $OUTPUT_DIR/include
