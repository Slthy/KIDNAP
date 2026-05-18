#ifndef INPUT_PARSER_H
#define INPUT_PARSER_H

#include <stddef.h>
#include <stdint.h>

#define SYSTEMCTL_MAX_OPS 64
#define SYSTEMCTL_OP_WIDTH 4

typedef struct {
    uint8_t operation;
    uint8_t selector;
    uint8_t flags;
    uint8_t repeat;
} decoded_op_t;

size_t decode_ops(const uint8_t *data, size_t len, decoded_op_t *ops, size_t max_ops);

#endif
