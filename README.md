# 6zip micro compressor
This program serves to store compressed 1-bit-per-pixel images / generic files on a micro controller, in compressed form.  

There are 3 algorithms to choose from:
 * the [skirridsystems](https://github.com/skirridsystems) [implementation](https://github.com/skirridsystems/packbits) of the [packbits](https://web.archive.org/web/20080705155158/http://developer.apple.com/technotes/tn/tn1023.html) run-length (de)compression algorithm. largest compressed size, smallest code space use for decompression.
  * the [pfalcon](https://github.com/pfalcon) [radically debloated implementation](https://github.com/pfalcon/uzlib) of the DEFLATE (dictionary + huffman trees) (de)compression algorithm named (uzlib), there are two flavors:
    * uzlibfull, which produces a file with compression header and trailer (CRC, length of contents)
    * uzlibraw, which has the header and trailer stripped away (smaller size)
  * the [atomicobject](https://github.com/atomicobject) [heatshrink](https://github.com/atomicobject/heatshrink) LZSS (dictionary + breakeven) compression algorithm for embedded devices with very limited RAM



# install
```
git clone https://github.com/recallmenot/uc_image_compressor.git
cd uc_image_compressor
git clone https://github.com/skirridsystems/packbits.git
git clone https://github.com/pfalcon/uzlib.git
git clone https://github.com/atomicobject/heatshrink.git
make
```



# conversion

running `./compressor --help` will show usage instructions.  
`tests.sh` offers an extensive collection of examples.  

For organisatory purposes it is heavily reccomended you keep the compressed files in these file extensions:
 * `.packed` for bitpacked files
 * `.uzf` for uzlibfull compressed files
 * `.uzr` for uzlibraw compressed files
 * `.heatshrunk` for heatshrink compressed files

## compressing images

1. Use an image editing tool like pinta, paint.net or GIMP to turn your image into the right size (e.g. 32 x 32 pixels) and convert it to monochrome (black and white). We want a 1 bit per pixel image in the right size. Save it as a TIFF or BMP (lossless).
2. Use imagemagick to convert the image to the pbm format `convert -monochrome image.tiff image.pbm`
3. strip the remaining pbm header and compress using `./compressor --image --compress`
4. convert the packed data to an array in a .h file `xxd -i image.[ext] > image.h`, where [ext] is one of the compressed extensions above

The resulting .h file can be #included in your C / C++ project.  

`xxd` will not declare the array `const`, you may want to do this in the resulting .h file so the compiler will definitely write it to FLASH.

## decompressing images

All 4 "formats" can also be decompressed, this will restore pbm headers but requires you to specify width and height.  

## compressing files

You can compress files, they will be compressed in full (no headers are stripped)  

## decompressing files

You can decompress files, no headers will be added.  

