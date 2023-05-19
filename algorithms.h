#ifndef algorithms_h
#define algorithms_h


/* maintenance defines, copy to your .c (before include)
#define COMP_PACKBITS 1
#define COMP_HEATSHRINK 1
#define COMP_UZLIB 1
*/

#if COMP_PACKBITS==1
#include "packbits/packbits.h"
#endif

#if COMP_UZLIB==1
#define UZ_OUT_CHUNK_SIZE 1
#include "uzlib/src/uzlib.h"
#endif

#if COMP_HEATSHRINK==1
#include "heatshrink/heatshrink_common.h"
#include "heatshrink/heatshrink_config.h"
#include "heatshrink/heatshrink_encoder.h"
#include "heatshrink/heatshrink_decoder.h"
#endif



#if COMP_PACKBITS==1
int compress_packbits(data_object* data) {
	data->output_length = packbits(data->input_data, data->output_data, data->input_length, data->output_length);

	return 0;
}

int decompress_packbits(data_object* data) {
	data->output_length = unpackbits(data->input_data, data->output_data, data->input_length, data->output_length);

	return 0;
}
#endif



#if COMP_UZLIB==1
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
#endif



#if COMP_HEATSHRINK==1
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
#endif

#endif // algorithms_h
