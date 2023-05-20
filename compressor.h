#ifndef compressor_h
#define compressor_h

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FILE_NAME_LENGTH 256
#define LOG_LVL 0

enum directions{
	direction_compress,
	direction_decompress,
};

enum file_types{
	file_type_file,
	file_type_image_pbm,
};

enum algorithms{
	algo_packbits,
	algo_uzlib_full,
	algo_uzlib_raw,
	algo_heatshrink,
};

typedef struct {
	char* input_file;
	char* output_file;
	enum directions direction;
	enum algorithms algo;
	enum file_types file_type;
	uint32_t width;
	uint32_t height;
	uint32_t output_length;
} file_object;


typedef struct {
	FILE* input;
	FILE* output;
	uint8_t* input_data;
	uint32_t input_length;
	uint8_t* output_data;
	uint32_t output_length;
	uint32_t output_write_position;
	uint32_t width;
	uint32_t height;
	uint8_t enable_raw;
} data_object;

#define COMP_PACKBITS 1
#define COMP_HEATSHRINK 1
#define COMP_UZLIB 1



// recursive include guard
#ifndef algorithms_h
#include "algorithms.h"
#endif

#endif // compressor_h
