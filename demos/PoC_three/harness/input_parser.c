#include "input_parser.h"

size_t decode_ops(const uint8_t *data, size_t len, decoded_op_t *ops, size_t max_ops) {
    size_t available = len / SYSTEMCTL_OP_WIDTH;
    size_t count = available < max_ops ? available : max_ops;

    for (size_t i = 0; i < count; ++i) {
        size_t off = i * SYSTEMCTL_OP_WIDTH;
        ops[i].operation = data[off];
        ops[i].selector = data[off + 1];
        ops[i].flags = data[off + 2];
        ops[i].repeat = data[off + 3];
    }

    return count;
}
