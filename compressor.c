#include "compressor.h"
#include "algorithms.h"


void header_pbm_remove(FILE *input, uint32_t* width, uint32_t* height) {
	fscanf(input, "P4\n%d %d\n", width, height);
}

uint8_t header_pbm_add(FILE *output, uint32_t width, uint32_t height) {
	uint32_t position0 = ftell(output);
	fprintf(output, "P4\n%d %d\n", width, height);
	return ftell(output) - position0;
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
		case algo_strip:
			printf(", strip");
			compress_fn = copy_buffers;
			decompress_fn = copy_buffers;
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
	printf("   * uzlibfull : DEFLATE (dictionary+Huffman) with header, CRC and length bytes\n");
	printf("   * uzlibraw  : DEFLATE (dictionary+Huffman) without header, CRC and length bytes\n");
	printf("   * heatshrink: LZSS (dictionary+breakeven) for embedded devices with little RAM\n");
	printf("   * strip     : only remove/restore pbm image header\n");
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
	else if (strcmp(argv[3], "--strip") == 0) {
		file->algo = algo_strip;
		strcat(extension, ".stripped");
		strcat(suffix, "_destripped");
	}
	else {
		printf("Invalid compression algorithm: %s\n", argv[3]);
		printf("options: --packbits, --uzlibfull, --uzlibraw, --heatshrink or --strip\n");
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
		if (file->algo == algo_strip) {
			printf("Only images can be stripped of their pbm header!\n");
			return 1;
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
	if (!strcmp(argv[1], "--help")) {
		print_help();
		return 0;
	}
	if (test_arguments_n_min(argc, 5)) {
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
