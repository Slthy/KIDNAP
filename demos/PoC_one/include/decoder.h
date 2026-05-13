#ifndef DECODER_H
#define DECODER_H

#include <stddef.h>
#include <stdint.h>

#include "ops.h"

#define OP_SIZE 4

fuzz_op_t decode_op(const uint8_t *buf);
size_t read_input(const char *path, uint8_t *buf, size_t cap);

#endif
