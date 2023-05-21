#!/bin/sh

make clean
make



#./compressor --image --compress --algorithm input.file
#./compressor --image --decompress --algorithm input.file width height

./compressor --image --compress --packbits bomb.pbm			# into bomb.i.packed
./compressor --image --decompress --packbits bomb.i.packed 32 32	# into bomb_i_unpacked.pbm

./compressor --image --compress --uzlibfull bomb.pbm			# into bomb.i.uzf
./compressor --image --decompress --uzlibfull bomb.i.uzf 32 32		# into bomb_i_deuzf.pbm

./compressor --image --compress --uzlibraw bomb.pbm			# into bomb.i.uzr
./compressor --image --decompress --uzlibraw bomb.i.uzr 32 32		# into bomb_i_deuzr.pbm

./compressor --image --compress --heatshrink bomb.pbm			# into bomb.i.heatshrunk
./compressor --image --decompress --heatshrink bomb.i.heatshrunk 32 32	# into bomb_i_unheatshrunk.pbm

./compressor --image --compress --strip bomb.pbm			# into bomb.i.stripped
./compressor --image --decompress --strip bomb.i.stripped 32 32		# into bomb_i_destripped.pbm



#./compressor --file --direction --algorithm input.file output_extension

./compressor --file --compress --packbits bomb.pbm			# into bomb.f.packed
./compressor --file --decompress --packbits bomb.f.packed pbm		# into bomb_f_unpacked.pbm

./compressor --file --compress --uzlibfull bomb.pbm			# into bomb.f.uzf
./compressor --file --decompress --uzlibfull bomb.f.uzf pbm		# into bomb_f_de-uzf.pbm

./compressor --file --compress --uzlibraw bomb.pbm			# into bomb.f.uzr
./compressor --file --decompress --uzlibraw bomb.f.uzr pbm		# into bomb_f_de-uzr.pbm

./compressor --file --compress --heatshrink bomb.pbm			# into bomb.f.heatshrunk
./compressor --file --decompress --heatshrink bomb.f.heatshrunk pbm	# into bomb_f_unheatshrunk.pbm

echo ""
echo ""
echo "now calling --help"
./compressor --help

sleep 5
