#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "packbits/packbits.h"
#include "uzlib/src/uzlib.h"
#include "heatshrink/heatshrink_common.h"
#include "heatshrink/heatshrink_config.h"
#include "heatshrink/heatshrink_encoder.h"
#include "heatshrink/heatshrink_decoder.h"

#define UZ_OUT_CHUNK_SIZE 1
#define MAX_FILE_NAME_LENGTH 256

#define LOG_LVL 0

void header_pbm_remove(FILE *input, uint32_t* width, uint32_t* height) {
	fscanf(input, "P4\n%d %d\n", width, height);
}

uint8_t header_pbm_add(FILE *output, uint32_t width, uint32_t height) {
	uint32_t position0 = ftell(output);
	fprintf(output, "P4\n%d %d\n", width, height);
	return ftell(output) - position0;
}

enum directions{
	direction_compress,
	direction_decompress,
};

enum algorithms{
	algo_packbits,
	algo_uzlib_full,
	algo_uzlib_raw,
	algo_heatshrink,
};

enum file_types{
	file_type_file,
	file_type_image_pbm,
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



int compress_packbits(data_object* data) {
	data->output_length = packbits(data->input_data, data->output_data, data->input_length, data->output_length);

	return 0;
}

int decompress_packbits(data_object* data) {
	data->output_length = unpackbits(data->input_data, data->output_data, data->input_length, data->output_length);

	return 0;
}



int compress_uzlib(data_object* data) {
	// Initialize compression structure
	struct uzlib_comp comp = {0};
	comp.dict_size = 32768;
	comp.hash_bits = 12;
	//comp.outlen = output_max_size - 4;

	size_t hash_size = sizeof(uzlib_hash_entry_t) * (1 << comp.hash_bits);
	comp.hash_table = malloc(hash_size);
	memset(comp.hash_table, 0, hash_size);

	// Perform compression
	zlib_start_block(&comp);
	uzlib_compress(&comp, data->input_data, data->input_length);
	zlib_finish_block(&comp);

	size_t output_size = comp.outlen;
	if (data->enable_raw != 1) {
		output_size += 4; // add 4 bytes because of header
	}

	data->output_write_position = 0;

	if (data->enable_raw != 1) {
		//construct header
		memcpy(data->output_data + data->output_write_position, "\x1F", 1);
		data->output_write_position += 1;
		memcpy(data->output_data + data->output_write_position, "\x8B", 1);
		data->output_write_position += 1;
		memcpy(data->output_data + data->output_write_position, "\x08", 1);
		data->output_write_position += 1;
		memcpy(data->output_data + data->output_write_position, "\x00", 1);
		data->output_write_position += 1;
		int mtime = 0;
		memcpy(data->output_data + data->output_write_position, &mtime, sizeof(mtime));
		data->output_write_position += sizeof(mtime);
		memcpy(data->output_data + data->output_write_position, "\x04", 1); // XFL
		data->output_write_position += 1;
		memcpy(data->output_data + data->output_write_position, "\x03", 1); // OS
		data->output_write_position += 1;
	}

	memcpy(data->output_data + data->output_write_position, comp.outbuf, comp.outlen);
	data->output_write_position += comp.outlen;

	if (data->enable_raw != 1) {
		// construct footer
		unsigned crc = ~uzlib_crc32(data->output, data->input_length, ~0);
		memcpy(data->output_data + data->output_write_position, &crc, sizeof(crc));
		data->output_write_position += sizeof(crc);
		memcpy(data->output_data + data->output_write_position, &data->input_length, sizeof(data->input_length));
		data->output_write_position += sizeof(data->input_length);
	}

	data->output_length = data->output_write_position;

	return 0;
}

int decompress_uzlib(data_object* data) {
	uzlib_init();

	uint32_t output_data_length;
	if (data->enable_raw != 1) {
		output_data_length = *(uint32_t*)(&data->input_data[data->input_length - sizeof(uint32_t)]);
		data->output_length = output_data_length;
	}
	else {
		output_data_length = data->output_length;
	}

	/* there can be mismatch between length in the trailer and actual
	   data stream; to avoid buffer overruns on overlong streams, reserve
	   one extra byte */
	output_data_length++;

	struct uzlib_uncomp d_stream;
	
	uzlib_uncompress_init(&d_stream, NULL, 0);

	d_stream.source = data->input_data;
	d_stream.source_limit = data->input_data + data->input_length;
	if (data->enable_raw != 1) {
		d_stream.source_limit -= 4;
	}
	d_stream.source_read_cb = NULL;
	d_stream.dest_start = d_stream.dest = data->output_data;

	int res;
	if (data->enable_raw != 1) {
		res = uzlib_gzip_parse_header(&d_stream);
		if (res != TINF_OK) {
			printf("Error parsing header: %d\n", res);
			exit(1);
		}
	}

	while (output_data_length) {
		unsigned int chunk_len = output_data_length < UZ_OUT_CHUNK_SIZE ? output_data_length : UZ_OUT_CHUNK_SIZE;
		d_stream.dest_limit = d_stream.dest + chunk_len;
		if (data->enable_raw != 1) {
			res = uzlib_uncompress_chksum(&d_stream);
		}
		else if (data->enable_raw == 1) {
			res = uzlib_uncompress(&d_stream);
		}
		output_data_length -= chunk_len;
		if (res != TINF_OK) {
			break;
		}
	}
/*
	if (res != TINF_DONE) {
		printf("Error during decompression: %d\n", res);
		exit(-res);
	}
*/
	data->output_length = d_stream.dest - data->output_data;

	return 0;
}



int compress_heatshrink(data_object* data) {
	// Initialize heatshrink encoder
	heatshrink_encoder *hse = heatshrink_encoder_alloc(8, 4);
	if (!hse) {
		perror("Error allocating heatshrink encoder");
		free(data->input_data);
		free(data->output_data);
		return 1;
	}

	size_t input_index = 0;
	size_t output_index = 0;
	uint8_t buffer[128];
	HSE_sink_res sink_res;
	HSE_poll_res poll_res;
	HSE_finish_res finish_res;

	// Sink input data into the encoder
	while (input_index < data->input_length) {
		size_t input_consumed;
		sink_res = heatshrink_encoder_sink(hse, &data->input_data[input_index], data->input_length - input_index, &input_consumed);
		if (sink_res < 0) {
			perror("Error sinking data into heatshrink encoder");
			heatshrink_encoder_free(hse);
			free(data->input_data);
			free(data->output_data);
			return 1;
		}
		input_index += input_consumed;

		// Poll output data from the encoder
		do {
			size_t output_produced;
			poll_res = heatshrink_encoder_poll(hse, buffer, sizeof(buffer), &output_produced);
			if (poll_res < 0) {
				perror("Error polling data from heatshrink encoder");
				heatshrink_encoder_free(hse);
				free(data->input_data);
				free(data->output_data);
				return 1;
			}
			memcpy(&data->output_data[output_index], buffer, output_produced);
			output_index += output_produced;
		} while (poll_res == HSER_POLL_MORE);
	}

	// Finish the encoding process
	do {
		finish_res = heatshrink_encoder_finish(hse);
		if (finish_res < 0) {
			perror("Error finishing heatshrink encoding");
			heatshrink_encoder_free(hse);
			free(data->input_data);
			free(data->output_data);
			return 1;
		}

		size_t output_produced;
		poll_res = heatshrink_encoder_poll(hse, buffer, sizeof(buffer), &output_produced);
		memcpy(&data->output_data[output_index], buffer, output_produced);
		output_index += output_produced;
	} while (finish_res == HSER_FINISH_MORE);

	heatshrink_encoder_free(hse);

	data->output_length = output_index;

	return 0;
}

int decompress_heatshrink(data_object* data) {
	// Set up heatshrink decoder
	heatshrink_decoder *hsd;
	hsd = heatshrink_decoder_alloc(data->input_length, 8, 4); // Choose appropriate window_sz2 and lookahead_sz2 values
	if (!hsd) {
		perror("Error allocating heatshrink decoder");
		return 1;
	}
	heatshrink_decoder_reset(hsd);

	// Sink input data into the decoder
	size_t sunk = 0;
	size_t sunk_total = 0;
	while (heatshrink_decoder_sink(hsd, data->input_data + sunk_total, data->input_length - sunk_total, &sunk) == HSDR_SINK_OK) {
		sunk_total += sunk;
	}

	// Decompress data using heatshrink decoder
	size_t output_written = 0;
	uint8_t poll_byte;
	size_t poll_count = 0;
	HSD_poll_res poll_res;
	do {
		poll_res = heatshrink_decoder_poll(hsd, &poll_byte, 1, &poll_count);
		if (poll_count) {
			data->output_data[output_written++] = poll_byte;
		}
	} while (poll_res == HSDR_POLL_MORE);

	heatshrink_decoder_finish(hsd);
	heatshrink_decoder_free(hsd);

	data->output_length = output_written;

	return 0;
}



int process_file(file_object* file) {
	data_object data;
	data.input = fopen(file->input_file, "rb");
	if (!data.input) {
		perror("Error opening input file");
		return 1;
	}

	int row_bytes;

	switch (file->file_type) {
		case file_type_file:
			fseek(data.input, 0, SEEK_END);
			data.input_length = ftell(data.input);
			fseek(data.input, 0, SEEK_SET);
			data.output_length = file->output_length;
			printf("file mode");
			break;
		case file_type_image_pbm:
			switch (file->direction) {
				case direction_compress:
					header_pbm_remove(data.input, &file->width, &file->height);
					row_bytes = (file->width + 7) / 8;
					data.input_length = row_bytes * file->height;
					break;
				case direction_decompress:
					fseek(data.input, 0, SEEK_END);
					data.input_length = ftell(data.input);
					fseek(data.input, 0, SEEK_SET);
					row_bytes = (file->width + 7) / 8;
					data.output_length = row_bytes * file->height;
					break;
			}
			printf("image mode");
			break;
	}

	data.input_data = malloc(data.input_length);
	fread(data.input_data, 1, data.input_length, data.input);
	fclose(data.input);

	// allocate enough memory for absolute worst-case scenario
	data.output_data = malloc(data.input_length * 2);

	// modify data	#######################
	// ghost functions!
	int (*compress_fn)(data_object*) = NULL;
	int (*decompress_fn)(data_object*) = NULL;
	switch (file->algo) {
		case algo_packbits:
			printf(", packbits");
			compress_fn = compress_packbits;
			decompress_fn = decompress_packbits;
			data.output_length = data.input_length * 2;
			break;
		case algo_uzlib_full:
			printf(", uzlib full");
			data.enable_raw = 0;
			compress_fn = compress_uzlib;
			decompress_fn = decompress_uzlib;
			break;
		case algo_uzlib_raw:
			printf(", uzlib raw");
			data.enable_raw = 1;
			compress_fn = compress_uzlib;
			decompress_fn = decompress_uzlib;
			data.output_length = data.input_length * 2;
			break;
		case algo_heatshrink:
			printf(", heatshrink");
			compress_fn = compress_heatshrink;
			decompress_fn = decompress_heatshrink;
			break;
	}

#if LOG_LVL == 1
	printf("file->input_file		is %s\n", file->input_file);
	printf("file->output_file		is %s\n", file->output_file);
	printf("file->direction			is %u\n", file->direction);
	printf("file->algo			is %u\n", file->algo);
	printf("file->file_type			is %u\n", file->file_type);
	printf("file->width			is %u\n", file->width);
	printf("file->height			is %u\n", file->height);
	printf("data.input_data			is %p\n", data.input_data);
	printf("data.input_length		is %u\n", data.input_length);
	printf("data.output_data		is %p\n", data.output_data);
	printf("data.output_length		is %u\n", data.output_length);
#endif
	switch (file->direction) {
		case direction_compress:
			printf(", compression\n");
			compress_fn(&data);
			break;

		case direction_decompress:
			printf(", de-compression\n");
			decompress_fn(&data);
			break;
	}

	data.output = fopen(file->output_file, "wb");
	if (!data.output) {
		perror("Error opening output file");
		free(data.input_data);
		free(data.output_data);
		return 1;
	}

	uint32_t output_total_length = 0;
	switch (file->file_type) {
		case file_type_file:
			break;
		case file_type_image_pbm:
			switch (file->direction) {
				case direction_compress:
					break;
				case direction_decompress:
					output_total_length += header_pbm_add(data.output, file->width, file->height);
					break;
			}
			break;
	}


	fwrite(data.output_data, 1, data.output_length, data.output);
	fclose(data.output);
	free(data.input_data);
	free(data.output_data);
	output_total_length += data.output_length;

	float IO_ratio = (float)output_total_length / (float)data.input_length;
	printf("input %u bytes, output %u bytes, ratio %f\n", data.input_length, output_total_length, IO_ratio);

	return 0;
}



void print_help() {
	printf("#### USAGE INFO\n");
	printf("\n");
	printf("(de)compression, stripping/restoring the pbm image header\n");
	printf("./compressor --image --compress --algorithm input.file\n");
	printf("./compressor --image --decompress --algorithm input.file width height\n");
	printf("(de)compression\n");
	printf("./compressor --file --compress --algorithm input.file\n");
	printf("./compressor --file --decompress --algorithm input.file output_extension\n");
	printf("\navaliable algorithms:\n");
	printf("   * packbits  : run length encoding\n");
	printf("   * uzlibfull : DEFLATE with header, CRC and length bytes\n");
	printf("   * uzlibraw  : DEFLATE without header, CRC and length bytes\n");
	printf("   * heatshrink: LZSS-based for embedded devices\n");
}


int test_arguments_n(int argc, int expected_args) {
	if (argc == expected_args) {
		return 0;
	}
	else {
		printf("Invalid number of arguments, expected %u, got %u.", expected_args, argc);
		printf("terminating. please call ./compressor --help");
		return 1;
	}
}

int test_arguments_n_min(int argc, int expected_args) {
	if (argc >= expected_args) {
		return 0;
	}
	else {
		printf("Invalid number of arguments, expected %u, got %u.", expected_args, argc);
		printf("terminating. please call ./compressor --help");
		return 1;
	}
}

void file_without_extension(char* input, char* output) {
	int lastIndex = -1;
	for (int i = 0; input[i] != '\0'; ++i) {
		if (input[i] == '.') {
			lastIndex = i;
		}
	}
	if (lastIndex != -1) {
		strncpy(output, input, lastIndex);
		output[lastIndex] = '\0';
	}
	else {
		strcpy(output, input);
	}
}


int parameters_to_config(file_object* file, int argc, char* argv[]) {
	char suffix[16] = "";
	char extension[16] = "";

	if (strcmp(argv[1], "--file") == 0) {
		file->file_type = file_type_file;
		//strcpy(suffix, "_f");
		strcpy(extension, ".f");
	}
	else if (strcmp(argv[1], "--image") == 0) {
		file->file_type = file_type_image_pbm;
		//strcpy(suffix, "_i");
		strcpy(extension, ".i");
	}
	else {
		printf("Invalid file type selecotr: %s\n", argv[1]);
		printf("options: --file or --image\n");
		return 1;
	}

	if (strcmp(argv[2], "--compress") == 0) {
		file->direction = direction_compress;
	}
	else if (strcmp(argv[2], "--decompress") == 0) {
		file->direction = direction_decompress;
	}
	else {
		printf("Invalid direction selector: %s\n", argv[2]);
		printf("options: --compress or --decompress\n");
		return 1;
	}

	if (strcmp(argv[3], "--packbits") == 0) {
		file->algo = algo_packbits;
		strcat(extension, ".packed");
		strcat(suffix, "_unpacked");
	}
	else if (strcmp(argv[3], "--uzlibfull") == 0) {
		file->algo = algo_uzlib_full;
		strcat(extension, ".uzf");
		strcat(suffix, "_deuzf");
	}
	else if (strcmp(argv[3], "--uzlibraw") == 0) {
		file->algo = algo_uzlib_raw;
		strcat(extension, ".uzr");
		strcat(suffix, "_deuzr");
	}
	else if (strcmp(argv[3], "--heatshrink") == 0) {
		file->algo = algo_heatshrink;
		strcat(extension, ".heatshrunk");
		strcat(suffix, "_unheatshrunk");
	}
	else {
		printf("Invalid compression algorithm: %s\n", argv[3]);
		printf("options: --packbits, --uzlibfull, --uzlibraw or --heatshrink\n");
		return 1;
	}

	//file->input_file = malloc(MAX_FILE_NAME_LENGTH * sizeof(char));
	file->input_file = argv[4];
	file->output_file = malloc(MAX_FILE_NAME_LENGTH * sizeof(char));
	file_without_extension(file->input_file, file->output_file);

	int n_expected_arguments = 0;

	if (file->file_type == file_type_file) {
		if (file->direction == direction_compress) {
			strcat(file->output_file, extension);
			n_expected_arguments = 5;
		}
		else if (file->direction == direction_decompress) {
			strcat(file->output_file, suffix);
			strcat(file->output_file, ".");
			strcat(file->output_file, argv[5]);
			file->output_length = 0;
			n_expected_arguments = 6;
		}
	}
	else if (file->file_type == file_type_image_pbm ) {
		if (file->direction == direction_compress) {
			strcat(file->output_file, extension);
			n_expected_arguments = 5;
		}
		else if (file->direction == direction_decompress) {
			file->width = atoi(argv[5]);
			file->height = atoi(argv[6]);
			strcat(file->output_file, suffix);
			strcat(file->output_file, ".pbm");
			n_expected_arguments = 7;
		}
	}
#if LOG_LVL == 2
	printf("file->input_file		is %s\n", file->input_file);
	printf("file->output_file		is %s\n", file->output_file);
	printf("file->direction			is %u\n", file->direction);
	printf("file->algo			is %u\n", file->algo);
	printf("file->file_type			is %u\n", file->file_type);
	printf("file->width			is %u\n", file->width);
	printf("file->height			is %u\n", file->height);
#endif
	return test_arguments_n(argc, n_expected_arguments);
}



int main(int argc, char *argv[]) {
	printf("\n\n#### 6zip micro COMPRESSOR\n");
	if (test_arguments_n_min(argc, 5) || !strcmp(argv[1], "--help")) {
		return 1;
	}

	file_object file;

	if (parameters_to_config(&file, argc, argv)) {
		return 1;
	}

	if(process_file(&file)) {
		return 1;
	}

	return 0;
}
