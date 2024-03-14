#include <stdint.h>
#include <stdlib.h>

// Source: https://stackoverflow.com/a/6782480

char *base64_encode(const char *data, size_t input_length, size_t *output_length);
unsigned char *base64_decode(const char *data, size_t input_length, size_t *output_length);
void build_decoding_table();
void base64_cleanup();